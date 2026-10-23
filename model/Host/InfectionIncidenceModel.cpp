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

#include "Host/InfectionIncidenceModel.h"
#include "Host/Human.h"
#include "Transmission/PerHost.h"
#include "Host/WithinHost/WHInterface.h"
#include "Parameters.h"
#include "util/ModelOptions.h"
#include "util/random.h"
#include "util/errors.h"

#include <stdexcept>
#include <cmath>
#include <format>

namespace OM::Host {
using namespace OM::util;

InfectionIncidenceParams::InfectionIncidenceParams(const Parameters& parameters,
                                                   const util::ModelOptionFlags& model_options)
    : baseline_avail_shape_param(parameters[Parameter::BaselineAvailabilityShape]),
      baseline_avail_offset(0.5 * pow(baseline_avail_shape_param, 2)), gamma_p(parameters[Parameter::GammaP]),
      s_inf(1 - exp(-parameters[Parameter::NegLogOneMinusSinf])), s_imm(parameters[Parameter::Simm]),
      e_star_inv(1.0 / parameters[Parameter::EStar]), x_star_p_inv(1.0 / parameters[Parameter::XStarP]),
      no_pre_erythrocytic(model_options.option(util::NoPreErythrocytic)),
      vaccine_genotype(model_options.option(util::VaccineGenotype)) {
    //! constant defining the constraint for the Gamma shape parameters
    /// Used for the case where availability is assumed gamma distributed
    /*
    //! Expected number of inoculations
    /// Product of measured EIR, susceptibility and length of time Global::interval
    double gsi = 1.0;
    
    r_square_Gamma=(totalInfectionrateVariance**2-gsi*BaselineAvailabilityMean)/(gsi*BaselineAvailabilityMean)**2
    r_square_Gamma must be greater than zero, so r_square_LogNormal is also. 
    */
    const double r_square_gamma = 0.649; //such that r_square_LogNormal =0.5

    if (model_options.option(util::NegativeBinomialMassAction)) {
        kind = Kind::NegativeBinomial;
        inf_rate_shape_param = (baseline_avail_shape_param + 1.0) / (r_square_gamma * baseline_avail_shape_param - 1.0);
        inf_rate_shape_param = std::max(inf_rate_shape_param, 0.0);
    } else if (model_options.option(util::LognormalMassAction)) {
        kind = Kind::LogNormal;

        //! constant defining the constraint for the log Normal variance
        /// Used for the case where availability is assumed log Normally distributed
        const double r_square_log_normal = log(1.0 + r_square_gamma);

        inf_rate_shape_param = sqrt(r_square_log_normal - 1.86 * pow(baseline_avail_shape_param, 2));
        inf_rate_shape_param = std::max(inf_rate_shape_param, 0.0);
        inf_rate_offset = 0.5 * pow(inf_rate_shape_param, 2);
        if ((std::isnan)(inf_rate_shape_param)) {
            throw util::xml_scenario_error("bad parameter 16 (BASELINE_AVAILABILITY_SHAPE)");
        }
    } else {
        if (model_options.option(util::TransHet) || model_options.option(util::ComorbTransHet) ||
            model_options.option(util::TransTreatHet) || model_options.option(util::TripleHet)) {
            kind = Kind::HeterogeneityWorkaround;
            cerr << "Warning: will use heterogeneity workaround." << endl;
        }
    }
}

std::unique_ptr<InfectionIncidenceModel> InfectionIncidenceParams::create() const {
    switch (kind) {
    case Kind::NegativeBinomial:
        return std::make_unique<NegBinomMAII>(*this);
    case Kind::LogNormal:
        return std::make_unique<LogNormalMAII>(*this);
    case Kind::HeterogeneityWorkaround:
        return std::make_unique<HeterogeneityWorkaroundII>(*this);
    case Kind::Default:
        break;
    }
    return std::make_unique<InfectionIncidenceModel>(*this);
}

InfectionIncidenceModel::InfectionIncidenceModel(const InfectionIncidenceParams& params) : incidence_params(&params) {}

// -----  non-static methods  -----

double InfectionIncidenceModel::get_availability_factor(LocalRng& rng, double base_availability) {
    return base_availability;
}
double NegBinomMAII::get_availability_factor(LocalRng& rng, double base_availability) {
    // Gamma sample with k=BaselineAvailabilityShapeParam; mean is baseAvailability
    return rng.gamma(params().baseline_avail_shape_param, base_availability / params().baseline_avail_shape_param);
}
double LogNormalMAII::get_availability_factor(LocalRng& rng, double base_availability) {
    // given BaselineAvailabilityShapeParam = sqrt (log (1 + variance/mean²))
    // and baseAvailability = mean, this is a draw from the log-normal distribution.
    if (base_availability != 1.0) {
        // NOTE: shouldn't the normal_mean parameter be adjusted when baseAvailability != 1.0?
        throw TRACED_EXCEPTION_DEFAULT("LogNormalMAII::getAvailabilityFactor");
    }
    return rng.log_normal(log(base_availability) - params().baseline_avail_offset, params().baseline_avail_shape_param);
}

void InfectionIncidenceModel::summarize(const Host::Human& human) {
    mon::recordStat(mon::measure("nExpectd"), human, p_infected);
}

double InfectionIncidenceModel::get_model_expected_infections(LocalRng& rng, double effective_eir,
                                                              const Transmission::PerHost& ph_trans) {
    // First two lines are availability adjustment: S_1(i,t) from AJTMH 75 (suppl 2) p12 eqn. (5)
    // Note that NegBinomMAII and LogNormalMAII supercede this model; see below
    return (params().s_inf + (1 - params().s_inf) / (1 + effective_eir / Sim::one_ts() * params().e_star_inv)) *
           susceptibility() * effective_eir;
}
double HeterogeneityWorkaroundII::get_model_expected_infections(LocalRng& rng, double effective_eir,
                                                                const Transmission::PerHost& ph_trans) {
    return (params().s_inf +
            (1 - params().s_inf) /
                (1 + effective_eir / (Sim::one_ts() * ph_trans.relative_availability_het) * params().e_star_inv)) *
           susceptibility() * effective_eir;
}
double NegBinomMAII::get_model_expected_infections(LocalRng& rng, double effective_eir, const Transmission::PerHost&) {
    // Documentation: http://www.plosmedicine.org/article/fetchSingleRepresentation.action?uri=info:doi/10.1371/journal.pmed.1001157.s009
    return rng.gamma(params().inf_rate_shape_param, effective_eir * susceptibility() / params().inf_rate_shape_param);
}
double LogNormalMAII::get_model_expected_infections(LocalRng& rng, double effective_eir, const Transmission::PerHost&) {
    // Documentation: http://www.plosmedicine.org/article/fetchSingleRepresentation.action?uri=info:doi/10.1371/journal.pmed.1001157.s009
    if (effective_eir < 1e-10)
        return 0.0;
    const double meanlog = log(effective_eir * susceptibility()) - params().inf_rate_offset;
    return rng.log_normal(meanlog, params().inf_rate_shape_param);
}

double InfectionIncidenceModel::susceptibility() {
    if (params().no_pre_erythrocytic) {
        //! The average proportion of bites from sporozoite positive mosquitoes resulting in infection.
        /*! 
    This is computed as 0.19 (the value S from a neg bin mass action model fitted 
    to Saradidi data, divided by 0.302 (the ratio of body surface area in a 
    0.5-6 year old child (as per Saradidi) to adult) 
    \sa getExpectedNumberOfInfections() 
    */
        return 0.702;
    } else {
        // S_2(i,t) from AJTMH 75 (suppl 2) p12 eqn. (7)
        return params().s_imm +
               (1.0 - params().s_imm) / (1.0 + pow(cumulative_eir_a * params().x_star_p_inv, params().gamma_p));
    }
}

double InfectionIncidenceModel::expected_num_new_infections(OM::Host::Human& human, double effective_eir) {
    // error check (should be OK if kappa is checked, for nonVector model):
    if (!(std::isfinite)(effective_eir)) {
        throw TRACED_EXCEPTION(std::format("effectiveEIR is not finite: {:g}\n", effective_eir),
                               util::Error::EffectiveEIR);
    }

    double expected_num_infections =
        get_model_expected_infections(human.host_rng, effective_eir, human.perHostTransmission);

    // Only to be consistent with old simulation runs when set to false
    // Setting this option to true will only affect reporting
    if (params().vaccine_genotype == false)
        //Introduce the effect of vaccination. Note that this does not affect cumEIR.
        expected_num_infections *= human.vaccine.getFactor(interventions::Vaccine::PEV);

    //Update pre-erythrocytic immunity
    cumulative_eir_a += effective_eir;

    p_infected = 1.0 - exp(-expected_num_infections) * (1.0 - p_infected);
    if (p_infected < 0.0)
        p_infected = 0.0;
    else if (p_infected > 1.0)
        p_infected = 1.0;

    return expected_num_infections;
}

int InfectionIncidenceModel::num_new_infections(Human& human, double expected_num_infections) {
    if (expected_num_infections > 0.0000001) {
        int n = human.host_rng.poisson(expected_num_infections);

        if (n > WithinHost::WHInterface::MaxInfections)
            n = WithinHost::WHInterface::MaxInfections;
        return n;
    }

    if ((std::isnan)(expected_num_infections)) { // check for not-a-number
        // bad Params::BASELINE_AVAILABILITY_SHAPE ?
        throw TRACED_EXCEPTION("numNewInfections: NaN", util::Error::NumNewInfections);
    }
    return 0;
}

void InfectionIncidenceModel::report_num_new_infections(Human& human, int new_num_infections_i,
                                                        int new_num_infections_l, NewInfections& new_infections) {
    mon::recordEvent(mon::measure("nNewInfections"), human, new_num_infections_i + new_num_infections_l);
    mon::recordEvent(mon::measure("nNewInfections_Introduced"), human, new_num_infections_i);
    mon::recordEvent(mon::measure("nNewInfections_Indigenous"), human, new_num_infections_l);
    new_infections.add(new_num_infections_i + new_num_infections_l);
}

}
