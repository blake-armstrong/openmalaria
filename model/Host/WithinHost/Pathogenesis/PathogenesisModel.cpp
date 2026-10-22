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
            return Pathogenesis::StateNmf;
    }
    return Pathogenesis::NONE;
}

PathogenesisModel::PathogenesisModel(const PathogenesisParams& params, double comorbidity_factor)
    : params(&params), comorbidity_factor(comorbidity_factor) {}

Pathogenesis::StatePair PathogenesisModel::determine_state(Host::Human& human, double age_years,
                                                           double time_step_max_density, double end_density,
                                                           bool is_doomed) {
    double p_malaria_fever = get_p_episode(time_step_max_density, end_density);
    StatePair result;
    //TODO(performance): would using a single RNG sample and manipulating probabilities be faster?
    //Decide whether a clinical episode occurs and if so, which type
    if (human.host_rng.bernoulli(p_malaria_fever)) {
        const double pr_severe_episode = time_step_max_density / (time_step_max_density + params->severe_mal_threshold);
        const double comorb_factor = comorbidity_factor / (1.0 + age_years * params->inv_crit_age_comorb);
        const double p_coinfection = params->comorb_intercept * comorb_factor;

        // Expectation of a severe bout:
        const double ex_severe = pr_severe_episode + (1.0 - pr_severe_episode) * p_coinfection;
        mon::recordStat(mon::measure("expectedSevere"), human, ex_severe);
        mon::recordStat(mon::measure("expectedSevereWithoutComorbidities"), human, pr_severe_episode);

        if (human.host_rng.bernoulli(pr_severe_episode))
            result.state = StateSevere;
        else {
            if (human.host_rng.bernoulli(p_coinfection))
                result.state = StateCoinfection;
            else
                result.state = StateMalaria;
        }

        // Indirect mortality:
        // IndirectRisk is the probability of dying from the indirect effects
        // of malaria conditional on not having an acute attack of malaria
        double indirect_risk = params->indir_risk_co_factor * comorb_factor;
        if (!is_doomed) {
            mon::recordStat(mon::measure("expectedIndirectDeaths"), human, indirect_risk);
        }
        //NOTE: we could only evaluate indirectMortality if not already doomed,
        // except that (a) it affects random numbers, and (b) it affects
        // evaluation of uncomplicated cases with the 5-day HS when
        // indirectMortBugfix is not enabled.
        if (human.host_rng.bernoulli(indirect_risk))
            result.indirect_mortality = true;
    } else {
        result.state = params->sample_nmf(human.host_rng, age_years);
    }
    return result;
}

void PathogenesisModel::checkpoint(istream& stream) { comorbidity_factor & stream; }
void PathogenesisModel::checkpoint(ostream& stream) { comorbidity_factor & stream; }

}
}
}
