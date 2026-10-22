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

#include "Host/WithinHost/Pathogenesis/Submodels.h"
#include "Host/Human.h"
#include "mon/Monitoring.h"
#include "util/random.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "util/AgeGroupInterpolation.h"
#include "Parameters.h"
#include "schema/healthSystem.h"

#include <cmath>
using namespace std;

namespace OM {
namespace WithinHost {
namespace Pathogenesis {

using namespace OM::util;

PathogenesisParams::PathogenesisParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                                       const scnXml::Clinical& clinical, bool nmf_only) {
    if (options.option(util::NonMalariaFevers)) {
        if (!clinical.getNonMalariaFevers().present()) {
            throw util::xml_scenario_error("NonMalariaFevers element of model->clinical required");
        }
        const scnXml::Clinical::NonMalariaFeversType& nmf_desc = clinical.getNonMalariaFevers().get();
        nmf_incidence.set(nmf_desc.getIncidence(), "incidence");
    }
    if (nmf_only)
        return;

    indir_risk_co_factor = 1 - exp(-parameters[Parameter::IndirectRiskCofactor]);
    severe_mal_threshold = parameters[Parameter::SevereMalariaThreshhold] + 1;
    comorb_intercept = 1 - exp(-parameters[Parameter::ComorbidityIntercept]);
    inv_crit_age_comorb = 1 / parameters[Parameter::CriticalAgeForComorbidity];

    if (options.option(util::PredeterminedEpisodes)) {
        kind = Kind::Predetermined;
        //no separate init:
        pyrogen = PyrogenParams(parameters);
    } else {
        if (options.option(util::MuellerPresentationModel)) {
            kind = Kind::Mueller;
            mueller = MuellerParams(parameters);
        } else {
            pyrogen = PyrogenParams(parameters);
        }
    }
}

std::unique_ptr<PathogenesisModel> PathogenesisParams::create(double comorbidity_factor) const {
    switch (kind) {
    case Kind::Predetermined:
        return std::make_unique<PredetPathogenesis>(*this, comorbidity_factor);
    case Kind::Mueller:
        return std::make_unique<MuellerPathogenesis>(*this, comorbidity_factor);
    case Kind::Pyrogen:
        return std::make_unique<PyrogenPathogenesis>(*this, comorbidity_factor);
    }
    throw TRACED_EXCEPTION_DEFAULT("unknown pathogenesis model kind");
}

Pathogenesis::State PathogenesisParams::sample_nmf(LocalRng& rng, double age_years) const {
    if (nmf_incidence.isSet()) {
        double p_nmf = nmf_incidence.eval(age_years);
        if (rng.bernoulli(p_nmf))
            return Pathogenesis::STATE_NMF;
    }
    return Pathogenesis::NONE;
}

PathogenesisModel::PathogenesisModel(const PathogenesisParams& params, double cF)
    : params(&params), _comorbidityFactor(cF) {}

Pathogenesis::StatePair PathogenesisModel::determineState(Host::Human& human, double ageYears,
                                                          double timeStepMaxDensity, double endDensity, bool isDoomed) {
    double pMalariaFever = getPEpisode(timeStepMaxDensity, endDensity);
    StatePair result;
    //TODO(performance): would using a single RNG sample and manipulating probabilities be faster?
    //Decide whether a clinical episode occurs and if so, which type
    if (human.host_rng.bernoulli(pMalariaFever)) {
        const double prSevereEpisode = timeStepMaxDensity / (timeStepMaxDensity + params->severe_mal_threshold);
        const double comorb_factor = _comorbidityFactor / (1.0 + ageYears * params->inv_crit_age_comorb);
        const double pCoinfection = params->comorb_intercept * comorb_factor;

        // Expectation of a severe bout:
        const double exSevere = prSevereEpisode + (1.0 - prSevereEpisode) * pCoinfection;
        mon::recordStat(mon::measure("expectedSevere"), human, exSevere);
        mon::recordStat(mon::measure("expectedSevereWithoutComorbidities"), human, prSevereEpisode);

        if (human.host_rng.bernoulli(prSevereEpisode))
            result.state = STATE_SEVERE;
        else {
            if (human.host_rng.bernoulli(pCoinfection))
                result.state = STATE_COINFECTION;
            else
                result.state = STATE_MALARIA;
        }

        // Indirect mortality:
        // IndirectRisk is the probability of dying from the indirect effects
        // of malaria conditional on not having an acute attack of malaria
        double indirectRisk = params->indir_risk_co_factor * comorb_factor;
        if (!isDoomed) {
            mon::recordStat(mon::measure("expectedIndirectDeaths"), human, indirectRisk);
        }
        //NOTE: we could only evaluate indirectMortality if not already doomed,
        // except that (a) it affects random numbers, and (b) it affects
        // evaluation of uncomplicated cases with the 5-day HS when
        // indirectMortBugfix is not enabled.
        if (human.host_rng.bernoulli(indirectRisk))
            result.indirectMortality = true;
    } else {
        result.state = params->sample_nmf(human.host_rng, ageYears);
    }
    return result;
}

void PathogenesisModel::checkpoint(istream& stream) { _comorbidityFactor & stream; }
void PathogenesisModel::checkpoint(ostream& stream) { _comorbidityFactor & stream; }

}
}
}
