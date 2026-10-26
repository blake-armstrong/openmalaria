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
#include "interventions/GVI.h"
#include "Host/Human.h"
#include "util/SpeciesIndexChecker.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include <cmath>

namespace OM::interventions {

GVIComponent::GVIComponent(ComponentId id, const scnXml::GVIDescription& elt,
                           const map<string, size_t>& species_name_map)
    : Transmission::HumanVectorInterventionComponent(id) {
    decay = DecayFunction::makeObject(elt.getDecay(), "interventions.human.vector.decay");
    // assume usage modifier is 100% if none is specified
    double prop_use = NAN;
    if (elt.getUsage().present()) {
        prop_use = elt.getUsage().get().getValue();
    } else {
        prop_use = 1.0;
    }
    if (!(prop_use >= 0.0 && prop_use <= 1.0)) {
        throw util::xml_scenario_error("GVI.description.usage: must be within range [0,1]");
    }

    using AP = scnXml::GVIDescription::AnophelesParamsSequence;
    const AP& ap = elt.getAnophelesParams();
    species.resize(species_name_map.size());
    util::SpeciesIndexChecker checker("GVI intervention", species_name_map);
    for (const auto& it : ap) {
        species[checker.getIndex(it.getMosquito())].init(it, prop_use);
    }
    checker.checkNoneMissed();
}

void GVIComponent::deploy(Host::Human& human, mon::DeployMethod method, VaccineLimits) const {
    human.per_host_transmission.deploy_component(human.intervention_rng, *this);
    mon::record_deploy(mon::measure("nMassGVI"), mon::measure("nCtsGVI"), human, method);
}

component_types::Type GVIComponent::component_type() const { return component_types::GVI; }

void GVIComponent::print_details(std::ostream& out) const { out << id().id << "\tGVI"; }

unique_ptr<PerHostInterventionData> GVIComponent::make_human_part(LocalRng& rng) const {
    return std::make_unique<HumanGVI>(rng, *this);
}
unique_ptr<PerHostInterventionData> GVIComponent::make_human_part(istream& stream, ComponentId id) const {
    return std::make_unique<HumanGVI>(stream, id, *this);
}

void GVIComponent::GVIAnopheles::init(const scnXml::GVIDescription::AnophelesParamsType& elt, double proportion_use) {
    assert((std::isnan)(deterrency)); // double init
    deterrency = elt.getDeterrency().present() ? elt.getDeterrency().get().getValue() : 0.0;
    preprandial_killing =
        elt.getPreprandialKillingEffect().present() ? elt.getPreprandialKillingEffect().get().getValue() : 0.0;
    postprandial_killing =
        elt.getPostprandialKillingEffect().present() ? elt.getPostprandialKillingEffect().get().getValue() : 0.0;
    fecundity_reduction = elt.getFecundityReduction().present() ? elt.getFecundityReduction().get().getValue() : 0.0;
    // Simpler version of ITN usage/action:
    const double prop_active = elt.getPropActive();
    if (prop_active != 1.0 && prop_active != 0.0)
        throw util::xml_scenario_error("GVI.description.propActive: must be either 0 or 1");

    assert(proportion_use >= 0.0 && proportion_use <= 1.0);
    assert(prop_active >= 0.0 && prop_active <= 1.0);
    proportion_protected = proportion_use * prop_active;
    proportion_unprotected = 1.0 - proportion_protected;
}

// ———  per-human data  ———
HumanGVI::HumanGVI(LocalRng& rng, const GVIComponent& params)
    : PerHostInterventionData(params.id()), component(&params) {
    // Varience factor of decay is sampled once per human: human is assumed
    // to account for most variance.
    decay_het = params.decay->hetSample(rng);
}

void HumanGVI::redeploy(LocalRng& rng, const Transmission::HumanVectorInterventionComponent&) {
    deploy_time = Sim::now_or_ts1();
}

void HumanGVI::update(Host::Human& human) {}

double HumanGVI::relative_attractiveness(size_t species_index) const {
    const GVIComponent& params = *component;
    const GVIComponent::GVIAnopheles& anoph = params.species[species_index];
    const double effect = (1.0 - anoph.deterrency * get_effect_survival(params));
    return anoph.by_protection(effect);
}

double HumanGVI::preprandial_survival_factor(size_t species_index) const {
    const GVIComponent& params = *component;
    const GVIComponent::GVIAnopheles& anoph = params.species[species_index];
    const double effect = (1.0 - anoph.preprandial_killing * get_effect_survival(params));
    return anoph.by_protection(effect);
}

double HumanGVI::postprandial_survival_factor(size_t species_index) const {
    const GVIComponent& params = *component;
    const GVIComponent::GVIAnopheles& anoph = params.species[species_index];
    const double effect = (1.0 - anoph.postprandial_killing * get_effect_survival(params));
    return anoph.by_protection(effect);
}
double HumanGVI::rel_fecundity(size_t species_index) const {
    const GVIComponent& params = *component;
    const GVIComponent::GVIAnopheles& anoph = params.species[species_index];
    const double effect = (1.0 - anoph.fecundity_reduction * get_effect_survival(params));
    return anoph.by_protection(effect);
}

void HumanGVI::checkpoint(ostream& stream) {
    deploy_time & stream;
    component->decay->checkpointSample(decay_het, stream);
}
HumanGVI::HumanGVI(istream& stream, ComponentId id, const GVIComponent& params)
    : PerHostInterventionData(id), component(&params) {
    deploy_time & stream;
    component->decay->checkpointSample(decay_het, stream);
}

}
