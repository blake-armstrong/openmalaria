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

namespace OM {
namespace WithinHost {

using namespace OM::util;

SimTime Infection::s_latentP = Sim::never();

FalciparumParams::FalciparumParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options)
    : sigma_i(sqrt(parameters[Parameter::SigmaISq])), imm_penalty_22(1 - exp(parameters[Parameter::ImmunityPenalty])),
      imm_effector_remain(exp(-parameters[Parameter::ImmuneEffectorDecay])),
      asex_imm_remain(exp(-parameters[Parameter::AsexualImmunityDecay])),
      inv_cumulative_y_star(1.0 / parameters[Parameter::CumulativeYStar]),
      inv_cumulative_h_star(1.0 / parameters[Parameter::CumulativeHStar]),
      alpha_m(1.0 - exp(-parameters[Parameter::NegLogOneMinusAlphaM])), decay_m(parameters[Parameter::DecayM]),
      y_lag_len(Sim::in_steps(Sim::from_days(20)) + 1),
      report_infections_by_genotype(mon::isUsed(mon::measure("nInfectByGenotype")) ||
                                    mon::isUsed(mon::measure("nPatentByGenotype")) ||
                                    mon::isUsed(mon::measure("logDensByGenotype"))),
      vaccine_genotype(options.option(util::VaccineGenotype)) {}

// -----  Non-static  -----

WHFalciparum::WHFalciparum(const FalciparumParams& params, LocalRng& rng, double comorbidity_factor)
    : WHInterface(),
      // NOTE: negating a Gaussian sample with mean 0 is pointless — except that
      // the individual samples change. In any case the overhead is negligible.
      //FIXME: Should this be allowed to be greater than 1?
      // Oldest code on GoogleCode: _innateImmunity=(double)(W_GAUSS((0), (sigma_i)));
      _innateImmSurvFact(exp(-rng.gauss(0.0, params.sigma_i))), m_cumulative_h(0.0), m_cumulative_Y(0.0),
      m_cumulative_Y_lag(0.0), totalDensity(0.0), hrp2Density(0.0), timeStepMaxDensity(0.0),
      pathogenesisModel(Pathogenesis::PathogenesisModel::createPathogenesisModel(comorbidity_factor)),
      treatExpiryLiver(0), treatExpiryBlood(0), falciparum_params(&params) {
    m_y_lag_i.resize(params.y_lag_len * Genotypes::N());
    m_y_lag_l.resize(params.y_lag_len * Genotypes::N());
}

WHFalciparum::~WHFalciparum() {}

double WHFalciparum::immunitySurvivalFactor(double ageInYears, double cumulativeExposureJ) {
    if (std::isnan(ageInYears) || std::isnan(cumulativeExposureJ) || std::isnan(m_cumulative_h) ||
        std::isnan(m_cumulative_Y)) {
        throw base_exception("nan in immunitySurvivalFactor");
    }

    // Documentation: AJTMH pp22-23

    // Effect of cumulative Parasite density (named Dy in AJTM)
    double d_y = 1.0;
    // Effect of number of infections experienced since birth (named Dh in AJTM)
    double d_h = 1.0;
    if (m_cumulative_h > 1.0) {
        d_h = 1.0 / (1.0 + (m_cumulative_h - 1.0) * params().inv_cumulative_h_star);
        d_y = 1.0 / (1.0 + (m_cumulative_Y - cumulativeExposureJ) * params().inv_cumulative_y_star);
    }

    // Effect of age-dependent maternal immunity (named Dm in AJTM)
    const double d_a = 1.0 - params().alpha_m * exp(-params().decay_m * ageInYears);

    return util::streamValidate(std::min(d_y * d_h * d_a, 1.0));
}

// Infectiousness parameters: see AJTMH p.33; tau=1/sigmag**2
const double PTM_beta1 = 1.0;
const double PTM_beta2 = 0.46;
const double PTM_beta3 = 0.17;
const double PTM_tau = 0.066;
const double PTM_tau_prime = 1.0 / sqrt(1.0 / PTM_tau);
const double PTM_mu = -8.1;

inline double compute_y_lag(const vector<double>& y_lag, int y_lag_len, vector<double>& y_lag_g) {
    const size_t n = Genotypes::N();

    // Take weighted sum of total asexual blood stage density 10, 15 and 20 days
    // before. Add y_lag_len to index to ensure positive.
    size_t d10 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(10)), y_lag_len);
    size_t d15 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(15)), y_lag_len);
    size_t d20 = mod_nn(y_lag_len + Sim::in_steps(Sim::ts1() - Sim::from_days(20)), y_lag_len);

    double y10_sum = 0.0, y15_sum = 0.0, y20_sum = 0.0;
    for (size_t g = 0; g < n; ++g) {
        double y10 = y_lag[d10 * n + g];
        double y15 = y_lag[d15 * n + g];
        double y20 = y_lag[d20 * n + g];
        y10_sum += y10;
        y15_sum += y15;
        y20_sum += y20;
        y_lag_g[g] = PTM_beta1 * y10 + PTM_beta2 * y15 + PTM_beta3 * y20;
    }

    // Weighted sum:
    return PTM_beta1 * y10_sum + PTM_beta2 * y15_sum + PTM_beta3 * y20_sum;
}

double WHFalciparum::probTransmissionToMosquito(vector<double>& probTransGenotype_i,
                                                vector<double>& probTransGenotype_l) const {
    // This model (often referred to as the gametocyte model) was designed for
    // 5-day time steps. We use the same model (sampling 10, 15 and 20 days
    // ago) for 1-day time steps to avoid having to design and analyse a new
    // model. Description: AJTMH pp.32-33 and p9.

    // Note: we don't allow for gametocydal treatments (e.g. Primaquine).
    const size_t n = Genotypes::N();

    // Sum lagged densities across genotypes:
    vector<double> y_lag_g_i(n), y_lag_g_l(n);
    const double y_lag_sum_i = compute_y_lag(m_y_lag_i, params().y_lag_len, y_lag_g_i);
    const double y_lag_sum_l = compute_y_lag(m_y_lag_l, params().y_lag_len, y_lag_g_l);
    const double y_lag_sum = y_lag_sum_i + y_lag_sum_l;

    if (y_lag_sum < 0.001)
        return 0.0; // cut off for uninfectious humans

    // Get a zval, convert to equivalent Normal sample:
    const double zval = (log(y_lag_sum) + PTM_mu) * PTM_tau_prime;
    const double pone = gsl_cdf_ugaussian_P(zval);
    double pTransmit = pone * pone;

    // pTransmit has to be between 0 and 1:
    pTransmit = std::max(pTransmit, 0.0);
    pTransmit = std::min(pTransmit, 1.0);

    if (pTransmit <= 0.0)
        return pTransmit;

    for (size_t g = 0; g < n; ++g) {
        probTransGenotype_i[g] = pTransmit * y_lag_g_i[g] / y_lag_sum;
        probTransGenotype_l[g] = pTransmit * y_lag_g_l[g] / y_lag_sum;
    }

    // Include here the effect of transmission-blocking vaccination:
    util::streamValidate(pTransmit);
    return pTransmit;
}

bool WHFalciparum::diagnosticResult(LocalRng& rng, const Diagnostic& diagnostic) const {
    return diagnostic.isPositive(rng, totalDensity, hrp2Density);
}

void WHFalciparum::treatment(Host::Human& human, TreatmentId treatId) {
    const Treatments& treat = Treatments::select(treatId);
    treatSimple(human, treat.liverEffect(), treat.bloodEffect());

    // triggered intervention deployments:
    treat.deploy(human, mon::Deploy::TREAT, interventions::VaccineLimits(/*default initialise: no limits*/));
}
bool WHFalciparum::treatSimple(Host::Human& human, SimTime timeLiver, SimTime timeBlood) {
    if (timeLiver != Sim::zero()) {
        if (timeLiver < Sim::zero())
            clearInfections(Treatments::LIVER);
        else
            treatExpiryLiver = max(int(treatExpiryLiver), int(Sim::now_or_ts1()) + timeLiver);
        mon::recordEvent(mon::measure("nLiverStageTreatments"), human);
    }
    if (timeBlood != Sim::zero()) {
        if (timeBlood < Sim::zero())
            clearInfections(Treatments::BLOOD);
        else
            treatExpiryBlood = max(int(treatExpiryBlood), int(Sim::now_or_ts1()) + timeBlood);
        return true; // blood stage treatment
    }
    return false; // no blood stage treatment
}

Pathogenesis::StatePair WHFalciparum::determineMorbidity(Host::Human& human, double ageYears, bool isDoomed) {
    Pathogenesis::StatePair result =
        pathogenesisModel->determineState(human, ageYears, timeStepMaxDensity, totalDensity, isDoomed);

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

void WHFalciparum::updateImmuneStatus() {
    const FalciparumParams& p = params();
    if (p.imm_effector_remain < 1) {
        m_cumulative_h *= p.imm_effector_remain;
        m_cumulative_Y *= p.imm_effector_remain;
    }
    if (p.asex_imm_remain < 1) {
        m_cumulative_h *=
            p.asex_imm_remain / (1 + (m_cumulative_h * (1 - p.asex_imm_remain) * p.inv_cumulative_h_star));
        m_cumulative_Y *=
            p.asex_imm_remain / (1 + (m_cumulative_Y * (1 - p.asex_imm_remain) * p.inv_cumulative_y_star));
    }
    m_cumulative_Y_lag = m_cumulative_Y;
}

// -----  Checkpointing  -----

void WHFalciparum::checkpoint(istream& stream) {
    WHInterface::checkpoint(stream);
    _innateImmSurvFact & stream;
    m_cumulative_h & stream;
    m_cumulative_Y & stream;
    m_cumulative_Y_lag & stream;
    totalDensity & stream;
    hrp2Density & stream;
    timeStepMaxDensity & stream;
    m_y_lag_i & stream;
    m_y_lag_l & stream;
    (*pathogenesisModel) & stream;
    treatExpiryLiver & stream;
    treatExpiryBlood & stream;
}
void WHFalciparum::checkpoint(ostream& stream) {
    WHInterface::checkpoint(stream);
    _innateImmSurvFact & stream;
    m_cumulative_h & stream;
    m_cumulative_Y & stream;
    m_cumulative_Y_lag & stream;
    totalDensity & stream;
    hrp2Density & stream;
    timeStepMaxDensity & stream;
    m_y_lag_i & stream;
    m_y_lag_l & stream;
    (*pathogenesisModel) & stream;
    treatExpiryLiver & stream;
    treatExpiryBlood & stream;
}

}
}
