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

#include "Global.h"
#include "interventions/IRS.h"
#include "Host/Human.h"
#include "util/errors.h"
#include "util/SpeciesIndexChecker.h"
#include "util/CommandLine.h"
#include "R_nmath/qnorm.h"
#include <cmath>

namespace OM::interventions {

IRSComponent::IRSComponent(ComponentId id, const scnXml::IRSDescription& elt,
                           const map<string, size_t>& species_name_map)
    : Transmission::HumanVectorInterventionComponent(id) {
    initial_insecticide.setParams(elt.getInitialInsecticide());
    const double max_prop = 0.999; //NOTE: this could be exposed in XML, but probably doesn't need to be
    max_insecticide = R::qnorm5(max_prop, initial_insecticide.getMu(), initial_insecticide.getSigma(), true, false);
    insecticide_decay =
        DecayFunction::makeObject(elt.getInsecticideDecay(), "interventions.human.IRS.description.insecticideDecay");
    // assume usage modifier is 100% if none is specified
    double prop_use = NAN;
    if (elt.getUsage().present()) {
        prop_use = elt.getUsage().get().getValue();
    } else {
        prop_use = 1.0;
    }
    if (!(prop_use >= 0.0 && prop_use <= 1.0)) {
        throw util::xml_scenario_error("ITN.description.usage: must be within range [0,1]");
    }

    using AP = scnXml::IRSDescription::AnophelesParamsSequence;
    const AP& ap = elt.getAnophelesParams();
    species.resize(species_name_map.size());
    util::SpeciesIndexChecker checker("IRS intervention", species_name_map);
    for (const auto& it : ap) {
        species[checker.getIndex(it.getMosquito())].init(it, prop_use, max_insecticide);
    }
    checker.checkNoneMissed();
}

void IRSComponent::deploy(Host::Human& human, mon::deploy::Method method, VaccineLimits) const {
    human.perHostTransmission.deploy_component(human.intervention_rng, *this);
    mon::record_deploy(mon::measure("nMassIRS"), mon::measure("nCtsIRS"), human, method);
}

component_types::Type IRSComponent::component_type() const { return component_types::IRS; }

void IRSComponent::print_details(std::ostream& out) const { out << id().id << "\tIRS"; }

unique_ptr<PerHostInterventionData> IRSComponent::make_human_part(LocalRng& rng) const {
    return std::make_unique<HumanIRS>(rng, *this);
}
unique_ptr<PerHostInterventionData> IRSComponent::make_human_part(istream& stream, ComponentId id) const {
    return std::make_unique<HumanIRS>(stream, id, *this);
}

void IRSComponent::IRSAnopheles::init(const scnXml::IRSDescription::AnophelesParamsType& elt, double proportion_use,
                                      double max_insecticide) {
    rel_attractiveness.init(elt.getDeterrency());
    preprandial_killing_effect.init(elt.getPreprandialKillingEffect(), false, max_insecticide);
    postprandial_killing_effect.init(elt.getPostprandialKillingEffect(), true, max_insecticide);
    if (elt.getFecundityReduction().present()) {
        rel_fecundity_effect.init(elt.getFecundityReduction().get(), false /*TODO: err msg*/, max_insecticide);
    } else {
        rel_fecundity_effect.init1();
    }
    assert(proportion_use >= 0.0 && proportion_use <= 1.0);
    // Simpler version of ITN usage/action:
    const double prop_active = elt.getPropActive();
    if (prop_active != 1.0 && prop_active != 0.0)
        throw util::xml_scenario_error("GVI.description.propActive: must be either 0 or 1");

    assert(prop_active >= 0.0 && prop_active <= 1.0);
    proportion_protected = proportion_use * prop_active;
    proportion_unprotected = 1.0 - proportion_protected;
}

IRSComponent::IRSAnopheles::RelativeAttractiveness::RelativeAttractiveness()
    : l_pf(numeric_limits<double>::signaling_NaN()), insecticide_scaling(numeric_limits<double>::signaling_NaN()) {}
void IRSComponent::IRSAnopheles::RelativeAttractiveness::init(const scnXml::IRSDeterrency& elt) {
    const double pf = elt.getInsecticideFactor();
    insecticide_scaling = elt.getInsecticideScalingFactor();
    if (!(pf > 0.0)) {
        throw util::xml_scenario_error(
            "IRS.description.anophelesParams.relativeAvailability: expected insecticideFactor to be positive.");
    }

    /* We need to ensure the relative availability is non-negative. However,
     * since it's an exponentiated value, it always will be.
     * 
     * If we don't want IRS to be able to increase transmission, the following
     * limits could also be applied. In general, however, there is no reason
     * IRS couldn't make individuals more attractive to mosquitoes.
     * 
     * To ensure relative availability is at most one: relative availability is
     *  exp( log(PF)*p ) = PF^p
     * where PF is the insecticide factor, with p∈[0,1] defined as:
     *  p=1−exp(-insecticideContent*insecticideScalingFactor).
     * We therefore just need PF ≤ 1. */
    //     if( !( PF <= 1.0 ) ) {
    // Potentially warn about this... but not necessary since making humans
    // more attractive isn't really an issue.
    //     }
    assert((std::isnan)(l_pf)); // double init
    l_pf = log(pf);
}
IRSComponent::IRSAnopheles::SurvivalFactor::SurvivalFactor()
    : bf(numeric_limits<double>::signaling_NaN()), pf(numeric_limits<double>::signaling_NaN()),
      insecticide_scaling(numeric_limits<double>::signaling_NaN()),
      inv_base_survival(numeric_limits<double>::signaling_NaN()) {}
void IRSComponent::IRSAnopheles::SurvivalFactor::init(const scnXml::IRSKillingEffect& elt, bool post_prandial,
                                                      double max_insecticide) {
    bf = elt.getBaseFactor();
    pf = elt.getInsecticideFactor();
    insecticide_scaling = elt.getInsecticideScalingFactor();
    inv_base_survival = 1.0 / (1.0 - bf);
    if (!(bf >= 0.0 && bf < 1.0)) {
        ostringstream msg;
        msg << "IRS.description.anophelesParams." << (post_prandial ? "post" : "pre")
            << "killingFactor: expected baseFactor to be in range [0,1]";
        throw util::xml_scenario_error(msg.str());
    }
    if (!(insecticide_scaling >= 0.0)) {
        ostringstream msg;
        msg << "IRS.description.anophelesParams." << (post_prandial ? "post" : "pre")
            << "killingFactor: expected scaling factor to be non-negative";
        throw util::xml_scenario_error(msg.str());
    }

    /* We want the calculated survival factor (1−K)/(1−BF) to be in the range
    [0,1] where K is the killing factor: K=BF+PF×p, with p defined as:
    p=1−exp(-insecticideContent×insecticideScalingFactor).
    
    By its nature, insecticideContent ≥ 0. We restrict
        insecticideScalingFactor ≥ 0
    which implies that p lies in the range [0,1]. We also know the base
    survival factor, 1−BF, is in the range [0,1].
    
    To make sure the survival factor is not negative we need (1−K)/(1−BF) ≥ 0.
    Since 1−BF > 0 we need 1−K ≥ 0, which, substituting K, gives us
        BF + PF×p ≤ 1	(1)
    We also want to make sure the survival factor is not greater than one (since
    IRS shouldn't increase mosquito survival), (1−K)/(1−BF) ≤ 1 or equivalently
    1-K ≤ 1-BF or K ≥ BF, which, substituting K, yields
        PF×p ≥ 0		(2)
    Since p ≥ 0, ensuring PF is non-negative is enough for (2).
    
    The maximum value for p depends on the maximum insecticide content; denote
    pmax = max(p). In this extreme case (1) becomes:
        BF + PF×pmax ≤ 1	(3)
    
    As with the ITN model, we impose a maximum value on the initial insecticide
    content, Pmax, such that the probability of sampling a value from our
    parameterise normal distribution greater than Pmax is 0.001. */
    const double pmax = 1.0 - exp(-max_insecticide * insecticide_scaling);
    if (!(pf >= 0.0 && bf + pf * pmax <= 1.0)) {
        ostringstream msg;
        msg << "IRS.description.anophelesParams." << (post_prandial ? "post" : "pre")
            << "killingFactor: expected insecticideFactor≥0, baseFactor+" << pmax << "×insecticideFactor≤1";
        throw util::xml_scenario_error(msg.str());
    }
}
void IRSComponent::IRSAnopheles::SurvivalFactor::init1() {
    bf = 0.0;
    pf = 0.0;
    insecticide_scaling = 0.0;
    inv_base_survival = 1.0;
}
double IRSComponent::IRSAnopheles::RelativeAttractiveness::relative_attractiveness(double insecticide_content) const {
    const double insecticide_component = 1.0 - exp(-insecticide_content * insecticide_scaling);
    const double rel_avail = exp(l_pf * insecticide_component);
    assert(rel_avail >= 0.0);
    return rel_avail;
}
double IRSComponent::IRSAnopheles::SurvivalFactor::survival_factor(double insecticide_content) const {
    const double insecticide_component = 1.0 - exp(-insecticide_content * insecticide_scaling);
    const double killing_effect = bf + pf * insecticide_component;
    const double survival_factor = (1.0 - killing_effect) * inv_base_survival;
    assert(survival_factor >= 0.0);
    assert(survival_factor <= 1.0);
    return survival_factor;
}

double IRSComponent::sample_initial_insecticide(LocalRng& rng) const {
    double sample = initial_insecticide.sample(rng);
    if (sample < 0.0)
        sample = 0.0; // avoid negative samples
    if (sample > max_insecticide)
        sample = max_insecticide;
    return sample;
}

// ———  per-human data  ———
HumanIRS::HumanIRS(LocalRng& rng, const IRSComponent& params)
    : PerHostInterventionData(params.id()), initial_insecticide(params.sample_initial_insecticide(rng)),
      component(&params) {
    // Varience factor of decay is sampled once per human: human is assumed
    // to account for most variance.
    insecticide_decay_het = params.insecticide_decay->hetSample(rng);
}

void HumanIRS::redeploy(LocalRng& rng, const OM::Transmission::HumanVectorInterventionComponent& params) {
    deploy_time = Sim::now_or_ts1();
    const auto* irs_params = dynamic_cast<const IRSComponent*>(&params);
    assert(irs_params != nullptr); // code error if this fails
    initial_insecticide = irs_params->sample_initial_insecticide(rng);
}

void HumanIRS::update(Host::Human& human) {}

double HumanIRS::relative_attractiveness(size_t species_index) const {
    const IRSComponent& params = *component;
    const IRSComponent::IRSAnopheles& anoph = params.species[species_index];
    const double effect = anoph.relative_attractiveness(get_insecticide_content(params));
    return anoph.by_protection(effect);
}

double HumanIRS::preprandial_survival_factor(size_t species_index) const {
    const IRSComponent& params = *component;
    const IRSComponent::IRSAnopheles& anoph = params.species[species_index];
    const double effect = anoph.preprandial_survival_factor(get_insecticide_content(params));
    return anoph.by_protection(effect);
}

double HumanIRS::postprandial_survival_factor(size_t species_index) const {
    const IRSComponent& params = *component;
    const IRSComponent::IRSAnopheles& anoph = params.species[species_index];
    const double effect = anoph.postprandial_survival_factor(get_insecticide_content(params));
    return anoph.by_protection(effect);
}
double HumanIRS::rel_fecundity(size_t species_index) const {
    const IRSComponent& params = *component;
    const IRSComponent::IRSAnopheles& anoph = params.species[species_index];
    const double effect = anoph.fecundity_effect(get_insecticide_content(params));
    return anoph.by_protection(effect);
}

void HumanIRS::checkpoint(ostream& stream) {
    deploy_time & stream;
    initial_insecticide & stream;
    component->insecticide_decay->checkpointSample(insecticide_decay_het, stream);
}
HumanIRS::HumanIRS(istream& stream, ComponentId id, const IRSComponent& params)
    : PerHostInterventionData(id), component(&params) {
    deploy_time & stream;
    initial_insecticide & stream;
    component->insecticide_decay->checkpointSample(insecticide_decay_het, stream);
}

}
