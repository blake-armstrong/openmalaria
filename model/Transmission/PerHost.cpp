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

#include "Transmission/PerHost.h"
#include "Transmission/VectorModel.h"
#include "interventions/InterventionManager.h"
#include "util/errors.h"
#include "util/checkpoint.h"

namespace OM {
namespace Transmission {
using namespace OM::util;

PerHostAnophParams::PerHostAnophParams(const scnXml::Mosq& mosq) {
    if (mosq.getAvailability().getDistr() != "const" &&
        (util::ModelOptions::option(NegativeBinomialMassAction) || util::ModelOptions::option(LognormalMassAction) ||
         util::ModelOptions::option(ComorbTransHet)))
        throw util::xml_scenario_error(
            "PerHostAnophParams::PerHostAnophParams(): ModelOptions NEGATIVE_BINOMIAL_MASS_ACTION, "
            "LOGNORMAL_MASS_ACTION, COMORB_TRANS_HET are not compatible with ento-availability heterogeneity "
            "distributions other than \"const\"");

    ento_availability = util::createSampler(1.0, mosq.getAvailability());
    prob_mosq_biting.setParams(mosq.getMosqProbBiting());
    prob_mosq_find_rest_site.setParams(mosq.getMosqProbFindRestSite());
    prob_mosq_survival_resting.setParams(mosq.getMosqProbResting());
}

PerHostParams::PerHostParams(const scnXml::AgeGroupValues& availability_to_mosquitoes) {
    rel_avail_age.set(availability_to_mosquitoes, "availabilityToMosquitoes");
}

void PerHostParams::calc_availability_percentiles() {
    int n_samples = 100000;
    vector<double> samples(n_samples, 0.0);

    // Using a different seed to make sure old results will not change
    // Does it really matter?
    util::MasterRng seed(0, 0);
    seed.seed(0, 0);

    util::LocalRng rng(seed);

    // Sample
    for (int i = 0; i < n_samples; i++) {
        double avail = 0.0;
        for (size_t s = 0; s < num_species(); ++s)
            avail += get(s).ento_availability_factor * get(s).ento_availability->sample(rng);
        samples[i] = avail;
    }

    // Sort samples
    sort(samples.begin(), samples.end());

    ento_availability_percentiles.resize(100);

    // Calc percetiles threshold values
    for (int i = 0; i < 100; i++)
        ento_availability_percentiles[i] = samples[int(i * n_samples / 100)];
    ento_availability_percentiles[0] = 0.0;
}

double PerHostParams::ento_availability_percentile(int p) const {
    if (p >= 0 && p < 100)
        return ento_availability_percentiles[p];
    else if (p == 100)
        return std::numeric_limits<double>::max();
    else
        throw util::xml_scenario_error(
            "Availability percentiles in intervention specifications can only be in [0 and 100]");
}

// -----  PerHost non-static -----

PerHost::PerHost() : outside_transmission(false), relative_availability_het(numeric_limits<double>::signaling_NaN()) {}

void PerHost::initialise(const PerHostParams& per_host_params, LocalRng& rng, double availability_factor) {
    params = &per_host_params;
    relative_availability_het = availability_factor;
    anoph_ento_availability_raw.resize(params->num_species());
    anoph_ento_availability.resize(params->num_species());
    anoph_prob_mosq_biting.resize(params->num_species());
    anoph_prob_mosq_resting.resize(params->num_species());

    for (size_t i = 0; i < params->num_species(); ++i) {
        const PerHostAnophParams& base = params->get(i);
        anoph_ento_availability_raw[i] = base.ento_availability->sample(rng);
        anoph_ento_availability[i] =
            base.ento_availability_factor * anoph_ento_availability_raw[i] * availability_factor;
        anoph_prob_mosq_biting[i] = base.prob_mosq_biting.sample(rng);
        auto p_rest1 = base.prob_mosq_find_rest_site.sample(rng);
        auto p_rest2 = base.prob_mosq_survival_resting.sample(rng);
        anoph_prob_mosq_resting[i] = p_rest1 * p_rest2;
    }
}

void PerHost::update(Host::Human& human) {
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        (*iter)->update(human);
    }
}

void PerHost::deploy_component(LocalRng& rng, const HumanVectorInterventionComponent& params) {
    // This adds per-host per-intervention details to the host's data set.
    // This data is never removed since it can contain per-host heterogeneity samples.
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        if ((*iter)->id() == params.id()) {
            // already have a deployment for that description; just update it
            (*iter)->redeploy(rng, params);
            return;
        }
    }
    // no deployment for that description: must make a new one
    active_components.push_back(params.make_human_part(rng));
}

// Note: in the case an intervention is not present, we can use the approximation
// of Weibull decay over the time span now - sim::never()
// (easily large enough for conceivable Weibull params that the value is 0.0 when
// rounded to a double. Performance-wise it's perhaps slightly slower than using
// an if() when interventions aren't present.
double PerHost::ento_availability_het_vec_itv(size_t species) const {
    double alpha_i = anoph_ento_availability[species];
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        alpha_i *= (*iter)->relative_attractiveness(species);
    }
    return alpha_i;
}
double PerHost::prob_mosq_biting(size_t species) const {
    double p_b_i = anoph_prob_mosq_biting[species];
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        p_b_i *= (*iter)->preprandial_survival_factor(species);
    }
    return p_b_i;
}
double PerHost::prob_mosq_resting(size_t species) const {
    double p_rest = anoph_prob_mosq_resting[species];
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        p_rest *= (*iter)->postprandial_survival_factor(species);
    }
    return p_rest;
}

double PerHost::rel_mosq_fecundity(size_t species) const {
    double rel_fecundity = 1.0;
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        rel_fecundity *= (*iter)->rel_fecundity(species);
    }
    return rel_fecundity;
}

bool PerHost::has_active_interv(interventions::Component::Type type) const {
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        if ((*iter)->is_deployed()) {
            if (interventions::InterventionManager::getComponent((*iter)->id()).componentType() == type)
                return true;
        }
    }
    return false;
}

void PerHost::checkpoint_intervs(ostream& stream) {
    active_components.size() & stream;
    for (auto iter = active_components.begin(); iter != active_components.end(); ++iter) {
        *iter& stream;
    }
}
void PerHost::checkpoint_intervs(istream& stream) {
    size_t l;
    l & stream;
    validateListSize(l);
    active_components.clear();
    for (size_t i = 0; i < l; ++i) {
        interventions::ComponentId id(stream);
        try {
            const interventions::HumanInterventionComponent& gen_params =
                interventions::InterventionManager::getComponent(id); // may throw
            const HumanVectorInterventionComponent* params =
                dynamic_cast<const HumanVectorInterventionComponent*>(&gen_params);
            if (params == 0)
                throw util::base_exception(""); // see catch block below
            active_components.push_back(params->make_human_part(stream, id));
        } catch (util::base_exception& e) {
            // two causes, both boil down to index being wrong
            throw util::checkpoint_error("bad value in checkpoint file");
        }
    }
}

}
}
