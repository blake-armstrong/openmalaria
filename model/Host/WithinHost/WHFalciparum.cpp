/* This file is part of OpenMalaria.
 * 
 * Copyright (C) 2005-2026 Swiss Tropical and Public Health Institute
 * Copyright (C) 2005-2015 Liverpool School Of Tropical Medicine
 * Copyright (C) 2020-2026 University of Basel
 * Copyright (C) 2025-2026 The Kids Research Institute Australia
 *
 * OpenMalaria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "Host/WithinHost/WHFalciparum.h"
#include "Host/WithinHost/DescriptiveWithinHost.h"
#include "Host/WithinHost/CommonWithinHost.h"
#include "Host/WithinHost/Infection/DummyInfection.h"
#include "Host/WithinHost/Infection/EmpiricalInfection.h"
#include "Host/WithinHost/Infection/MolineauxInfection.h"
#include "Host/WithinHost/Infection/PennyInfection.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "Host/WithinHost/Diagnostic.h"
#include "Host/WithinHost/Treatments.h"
#include "Host/WithinHost/Genotypes.h"
#include "mon/Monitoring.h"
#include "util/random.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "util/StreamValidator.h"
#include "util/checkpoint_containers.h"
#include "util/UnitParse.h"
#include "schema/scenario.h"

#include <cmath>
#include <gsl/gsl_cdf.h>

namespace OM::WithinHost {

using namespace OM::util;

FalciparumParams::FalciparumParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                                   const GenotypeRegistry& genotypes, const DiagnosticRegistry& diagnostics)
    : sigma_i(sqrt(parameters[Parameter::SigmaISq])), imm_penalty_22(1 - exp(parameters[Parameter::ImmunityPenalty])),
      imm_effector_remain(exp(-parameters[Parameter::ImmuneEffectorDecay])),
      asex_imm_remain(exp(-parameters[Parameter::AsexualImmunityDecay])),
      inv_cumulative_y_star(1.0 / parameters[Parameter::CumulativeYStar]),
      inv_cumulative_h_star(1.0 / parameters[Parameter::CumulativeHStar]),
      alpha_m(1.0 - exp(-parameters[Parameter::NegLogOneMinusAlphaM])), decay_m(parameters[Parameter::DecayM]),
      y_lag_len(Sim::in_steps(Sim::from_days(20)) + 1),
      report_infections_by_genotype(mon::is_used(mon::measure("nInfectByGenotype")) ||
                                    mon::is_used(mon::measure("nPatentByGenotype")) ||
                                    mon::is_used(mon::measure("logDensByGenotype"))),
      vaccine_genotype(options.option(util::VaccineGenotype)), genotypes(&genotypes), diagnostics(&diagnostics) {}

// -----  Non-static  -----

WHFalciparum::WHFalciparum(const FalciparumParams& params, const Pathogenesis::PathogenesisParams& pathogenesis,
                           const TreatmentRegistry& treatments, LocalRng& rng, double comorbidity_factor)
    : WHInterface(),
      // NOTE: negating a Gaussian sample with mean 0 is pointless — except that
      // the individual samples change. In any case the overhead is negligible.
      //FIXME: Should this be allowed to be greater than 1?
      // Oldest code on GoogleCode: _innateImmunity=(double)(W_GAUSS((0), (sigma_i)));
      innate_imm_surv_fact(exp(-rng.gauss(0.0, params.sigma_i))),
      pathogenesis_model(pathogenesis.create(comorbidity_factor)), falciparum_params(&params), treatments(&treatments) {
    y_lag_imported.resize(params.y_lag_len * params.genotypes->count());
    y_lag_local.resize(params.y_lag_len * params.genotypes->count());
}

WHFalciparum::~WHFalciparum() = default;

double WHFalciparum::immunity_survival_factor(double age_in_years, double cumulative_exposure_j) {
    if (std::isnan(age_in_years) || std::isnan(cumulative_exposure_j) || std::isnan(cumulative_h) ||
        std::isnan(cumulative_y)) {
        throw base_exception("nan in immunitySurvivalFactor");
    }

    // Documentation: AJTMH pp22-23

    // Effect of cumulative Parasite density (named Dy in AJTM)
    double d_y = 1.0;
    // Effect of number of infections experienced since birth (named Dh in AJTM)
    double d_h = 1.0;
    if (cumulative_h > 1.0) {
        d_h = 1.0 / (1.0 + (cumulative_h - 1.0) * params().inv_cumulative_h_star);
        d_y = 1.0 / (1.0 + (cumulative_y - cumulative_exposure_j) * params().inv_cumulative_y_star);
    }

    // Effect of age-dependent maternal immunity (named Dm in AJTM)
    const double d_a = 1.0 - params().alpha_m * exp(-params().decay_m * age_in_years);

    return util::streamValidate(std::min(d_y * d_h * d_a, 1.0));
}

// Infectiousness parameters: see AJTMH p.33; tau=1/sigmag**2
const double PtmBeta1 = 1.0;
const double PtmBeta2 = 0.46;
const double PtmBeta3 = 0.17;
const double PtmTau = 0.066;
const double PtmTauPrime = 1.0 / sqrt(1.0 / PtmTau);
const double PtmMu = -8.1;

inline double compute_y_lag(const vector<double>& y_lag, int y_lag_len, vector<double>& y_lag_g) {
    const size_t n = y_lag_g.size();

    // Take weighted sum of total asexual blood stage density 10, 15 and 20 days
    // before. Add y_lag_len to index to ensure positive.
    const size_t d10 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(10)), y_lag_len);
    const size_t d15 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(15)), y_lag_len);
    const size_t d20 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(20)), y_lag_len);

    double y10_sum = 0.0, y15_sum = 0.0, y20_sum = 0.0;
    for (size_t g = 0; g < n; ++g) {
        const double y10 = y_lag[d10 * n + g];
        const double y15 = y_lag[d15 * n + g];
        const double y20 = y_lag[d20 * n + g];
        y10_sum += y10;
        y15_sum += y15;
        y20_sum += y20;
        y_lag_g[g] = PtmBeta1 * y10 + PtmBeta2 * y15 + PtmBeta3 * y20;
    }

    // Weighted sum:
    return PtmBeta1 * y10_sum + PtmBeta2 * y15_sum + PtmBeta3 * y20_sum;
}

double WHFalciparum::prob_transmission_to_mosquito(vector<double>& prob_trans_genotype_i,
                                                   vector<double>& prob_trans_genotype_l) const {
    // This model (often referred to as the gametocyte model) was designed for
    // 5-day time steps. We use the same model (sampling 10, 15 and 20 days
    // ago) for 1-day time steps to avoid having to design and analyse a new
    // model. Description: AJTMH pp.32-33 and p9.

    // Note: we don't allow for gametocydal treatments (e.g. Primaquine).
    const size_t n = genotypes().count();

    // Sum lagged densities across genotypes:
    vector<double> y_lag_g_i(n), y_lag_g_l(n);
    const double y_lag_sum_i = compute_y_lag(y_lag_imported, params().y_lag_len, y_lag_g_i);
    const double y_lag_sum_l = compute_y_lag(y_lag_local, params().y_lag_len, y_lag_g_l);
    const double y_lag_sum = y_lag_sum_i + y_lag_sum_l;

    if (y_lag_sum < 0.001)
        return 0.0; // cut off for uninfectious humans

    // Get a zval, convert to equivalent Normal sample:
    const double zval = (log(y_lag_sum) + PtmMu) * PtmTauPrime;
    const double pone = gsl_cdf_ugaussian_P(zval);
    double p_transmit = pone * pone;

    // pTransmit has to be between 0 and 1:
    p_transmit = std::max(p_transmit, 0.0);
    p_transmit = std::min(p_transmit, 1.0);

    if (p_transmit <= 0.0)
        return p_transmit;

    for (size_t g = 0; g < n; ++g) {
        prob_trans_genotype_i[g] = p_transmit * y_lag_g_i[g] / y_lag_sum;
        prob_trans_genotype_l[g] = p_transmit * y_lag_g_l[g] / y_lag_sum;
    }

    // Include here the effect of transmission-blocking vaccination:
    util::streamValidate(p_transmit);
    return p_transmit;
}

bool WHFalciparum::diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const {
    return diagnostic.is_positive(rng, total_density, hrp2_density);
}

void WHFalciparum::treatment(Host::Human& human, TreatmentId treat_id) {
    const Treatments& treat = treatments->select(treat_id);
    treat_simple(human, treat.liver_effect(), treat.blood_effect());

    // triggered intervention deployments:
    treat.deploy(human, mon::DeployMethod::Treat, interventions::VaccineLimits(/*default initialise: no limits*/));
}
bool WHFalciparum::treat_simple(Host::Human& human, SimTime time_liver, SimTime time_blood) {
    if (time_liver != Sim::zero()) {
        if (time_liver < Sim::zero())
            clear_infections(Treatments::LIVER);
        else
            treat_expiry_liver = max(treat_expiry_liver, Sim::now_or_ts1() + time_liver);
        mon::record_event(mon::measure("nLiverStageTreatments"), human);
    }
    if (time_blood != Sim::zero()) {
        if (time_blood < Sim::zero())
            clear_infections(Treatments::BLOOD);
        else
            treat_expiry_blood = max(treat_expiry_blood, Sim::now_or_ts1() + time_blood);
        return true; // blood stage treatment
    }
    return false; // no blood stage treatment
}

Pathogenesis::StatePair WHFalciparum::determine_morbidity(Host::Human& human, double age_years, bool is_doomed) {
    Pathogenesis::StatePair result =
        pathogenesis_model->determine_state(human, age_years, time_step_max_density, total_density, is_doomed);

    /* Note: this model can easily be re-enabled, but is not used and not considered to be a good model.
    if( (result.state & Pathogenesis::MALARIA) && util::ModelOptions::option( util::PENALISATION_EPISODES ) ){
        // This does immunity penalisation:
        m_cumulative_Y = m_cumulative_Y_lag - immPenalty_22*(m_cumulative_Y-m_cumulative_Y_lag);
        if (m_cumulative_Y < 0) {
            m_cumulative_Y=0.0;
        }
    }*/

    return result;
}

// -----  immunity  -----

void WHFalciparum::update_immune_status() {
    const FalciparumParams& p = params();
    if (p.imm_effector_remain < 1) {
        cumulative_h *= p.imm_effector_remain;
        cumulative_y *= p.imm_effector_remain;
    }
    if (p.asex_imm_remain < 1) {
        cumulative_h *= p.asex_imm_remain / (1 + (cumulative_h * (1 - p.asex_imm_remain) * p.inv_cumulative_h_star));
        cumulative_y *= p.asex_imm_remain / (1 + (cumulative_y * (1 - p.asex_imm_remain) * p.inv_cumulative_y_star));
    }
    cumulative_y_lag = cumulative_y;
}

// -----  Checkpointing  -----

void WHFalciparum::checkpoint(istream& stream) {
    WHInterface::checkpoint(stream);
    innate_imm_surv_fact & stream;
    cumulative_h & stream;
    cumulative_y & stream;
    cumulative_y_lag & stream;
    total_density & stream;
    hrp2_density & stream;
    time_step_max_density & stream;
    y_lag_imported & stream;
    y_lag_local & stream;
    (*pathogenesis_model) & stream;
    treat_expiry_liver & stream;
    treat_expiry_blood & stream;
}
void WHFalciparum::checkpoint(ostream& stream) {
    WHInterface::checkpoint(stream);
    innate_imm_surv_fact & stream;
    cumulative_h & stream;
    cumulative_y & stream;
    cumulative_y_lag & stream;
    total_density & stream;
    hrp2_density & stream;
    time_step_max_density & stream;
    y_lag_imported & stream;
    y_lag_local & stream;
    (*pathogenesis_model) & stream;
    treat_expiry_liver & stream;
    treat_expiry_blood & stream;
}

}
