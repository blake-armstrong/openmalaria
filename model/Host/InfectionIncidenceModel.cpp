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

namespace OM {
namespace Host {
using namespace OM::util;

InfectionIncidenceParams::InfectionIncidenceParams(const Parameters& parameters,
                                                   const util::ModelOptionFlags& model_options) {
    baseline_avail_shape_param = parameters[Parameter::BaselineAvailabilityShape];
    baseline_avail_offset = 0.5 * pow(baseline_avail_shape_param, 2);

    gamma_p = parameters[Parameter::GammaP];
    s_inf = 1 - exp(-parameters[Parameter::NegLogOneMinusSinf]);
    s_imm = parameters[Parameter::Simm];
    e_star_inv = 1.0 / parameters[Parameter::EStar];
    x_star_p_inv = 1.0 / parameters[Parameter::XStarP];

    //! constant defining the constraint for the Gamma shape parameters
    /// Used for the case where availability is assumed gamma distributed
    double r_square_Gamma;
    /*
    //! Expected number of inoculations
    /// Product of measured EIR, susceptibility and length of time Global::interval
    double gsi = 1.0;
    
    r_square_Gamma=(totalInfectionrateVariance**2-gsi*BaselineAvailabilityMean)/(gsi*BaselineAvailabilityMean)**2
    r_square_Gamma must be greater than zero, so r_square_LogNormal is also. 
    */
    r_square_Gamma = 0.649; //such that r_square_LogNormal =0.5

    no_pre_erythrocytic = model_options.option(util::NoPreErythrocytic);
    vaccine_genotype = model_options.option(util::VaccineGenotype);

    if (model_options.option(util::NegativeBinomialMassAction)) {
        kind = Kind::NegativeBinomial;
        inf_rate_shape_param = (baseline_avail_shape_param + 1.0) / (r_square_Gamma * baseline_avail_shape_param - 1.0);
        inf_rate_shape_param = std::max(inf_rate_shape_param, 0.0);
    } else if (model_options.option(util::LognormalMassAction)) {
        kind = Kind::LogNormal;

        //! constant defining the constraint for the log Normal variance
        /// Used for the case where availability is assumed log Normally distributed
        double r_square_LogNormal = log(1.0 + r_square_Gamma);

        inf_rate_shape_param = sqrt(r_square_LogNormal - 1.86 * pow(baseline_avail_shape_param, 2));
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

InfectionIncidenceModel::InfectionIncidenceModel(const InfectionIncidenceParams& params)
    : params(&params), m_pInfected(0.0), m_cumulativeEIRa(0.0) {}

// -----  non-static methods  -----

double InfectionIncidenceModel::getAvailabilityFactor(LocalRng& rng, double baseAvailability) {
    return baseAvailability;
}
double NegBinomMAII::getAvailabilityFactor(LocalRng& rng, double baseAvailability) {
    // Gamma sample with k=BaselineAvailabilityShapeParam; mean is baseAvailability
    return rng.gamma(params->baseline_avail_shape_param, baseAvailability / params->baseline_avail_shape_param);
}
double LogNormalMAII::getAvailabilityFactor(LocalRng& rng, double baseAvailability) {
    // given BaselineAvailabilityShapeParam = sqrt (log (1 + variance/mean²))
    // and baseAvailability = mean, this is a draw from the log-normal distribution.
    if (baseAvailability != 1.0) {
        // NOTE: shouldn't the normal_mean parameter be adjusted when baseAvailability != 1.0?
        throw TRACED_EXCEPTION_DEFAULT("LogNormalMAII::getAvailabilityFactor");
    }
    return rng.log_normal(log(baseAvailability) - params->baseline_avail_offset, params->baseline_avail_shape_param);
}

void InfectionIncidenceModel::summarize(const Host::Human& human) {
    mon::recordStat(mon::measure("nExpectd"), human, m_pInfected);
}

double InfectionIncidenceModel::getModelExpectedInfections(LocalRng& rng, double effectiveEIR,
                                                           const Transmission::PerHost& phTrans) {
    // First two lines are availability adjustment: S_1(i,t) from AJTMH 75 (suppl 2) p12 eqn. (5)
    // Note that NegBinomMAII and LogNormalMAII supercede this model; see below
    return (params->s_inf + (1 - params->s_inf) / (1 + effectiveEIR / Sim::one_ts() * params->e_star_inv)) *
           susceptibility() * effectiveEIR;
}
double HeterogeneityWorkaroundII::getModelExpectedInfections(LocalRng& rng, double effectiveEIR,
                                                             const Transmission::PerHost& phTrans) {
    return (params->s_inf +
            (1 - params->s_inf) /
                (1 + effectiveEIR / (Sim::one_ts() * phTrans.relative_availability_het) * params->e_star_inv)) *
           susceptibility() * effectiveEIR;
}
double NegBinomMAII::getModelExpectedInfections(LocalRng& rng, double effectiveEIR, const Transmission::PerHost&) {
    // Documentation: http://www.plosmedicine.org/article/fetchSingleRepresentation.action?uri=info:doi/10.1371/journal.pmed.1001157.s009
    return rng.gamma(params->inf_rate_shape_param, effectiveEIR * susceptibility() / params->inf_rate_shape_param);
}
double LogNormalMAII::getModelExpectedInfections(LocalRng& rng, double effectiveEIR, const Transmission::PerHost&) {
    // Documentation: http://www.plosmedicine.org/article/fetchSingleRepresentation.action?uri=info:doi/10.1371/journal.pmed.1001157.s009
    if (effectiveEIR < 1e-10)
        return 0.0;
    double meanlog = log(effectiveEIR * susceptibility()) - params->inf_rate_offset;
    return rng.log_normal(meanlog, params->inf_rate_shape_param);
}

double InfectionIncidenceModel::susceptibility() {
    if (params->no_pre_erythrocytic) {
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
        return params->s_imm +
               (1.0 - params->s_imm) / (1.0 + pow(m_cumulativeEIRa * params->x_star_p_inv, params->gamma_p));
    }
}

double InfectionIncidenceModel::expectedNumNewInfections(OM::Host::Human& human, double effectiveEIR) {
    // error check (should be OK if kappa is checked, for nonVector model):
    if (!(std::isfinite)(effectiveEIR)) {
        ostringstream out;
        out << "effectiveEIR is not finite: " << effectiveEIR << endl;
        throw TRACED_EXCEPTION(out.str(), util::Error::EffectiveEIR);
    }

    double expected_num_infections =
        getModelExpectedInfections(human.host_rng, effectiveEIR, human.perHostTransmission);

    // Only to be consistent with old simulation runs when set to false
    // Setting this option to true will only affect reporting
    if (params->vaccine_genotype == false)
        //Introduce the effect of vaccination. Note that this does not affect cumEIR.
        expected_num_infections *= human.vaccine.getFactor(interventions::Vaccine::PEV);

    //Update pre-erythrocytic immunity
    m_cumulativeEIRa += effectiveEIR;

    m_pInfected = 1.0 - exp(-expected_num_infections) * (1.0 - m_pInfected);
    if (m_pInfected < 0.0)
        m_pInfected = 0.0;
    else if (m_pInfected > 1.0)
        m_pInfected = 1.0;

    return expected_num_infections;
}

int InfectionIncidenceModel::numNewInfections(Human& human, double expectedNumInfections) {
    if (expectedNumInfections > 0.0000001) {
        int n = human.host_rng.poisson(expectedNumInfections);

        if (n > WithinHost::WHInterface::MaxInfections)
            n = WithinHost::WHInterface::MaxInfections;
        return n;
    }

    if ((std::isnan)(expectedNumInfections)) { // check for not-a-number
        // bad Params::BASELINE_AVAILABILITY_SHAPE ?
        throw TRACED_EXCEPTION("numNewInfections: NaN", util::Error::NumNewInfections);
    }
    return 0;
}

void InfectionIncidenceModel::reportNumNewInfections(Human& human, int newNumInfections_i, int newNumInfections_l,
                                                     NewInfections& new_infections) {
    mon::recordEvent(mon::measure("nNewInfections"), human, newNumInfections_i + newNumInfections_l);
    mon::recordEvent(mon::measure("nNewInfections_Introduced"), human, newNumInfections_i);
    mon::recordEvent(mon::measure("nNewInfections_Indigenous"), human, newNumInfections_l);
    new_infections.add(newNumInfections_i + newNumInfections_l);
}

}
}
