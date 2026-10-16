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

#include "Simulation.h"

#include "Parameters.h"
#include "util/ModelOptions.h"
#include "util/ModelNameProvider.h"
#include "util/random.h"

#include "mon/init.h"

#include "interventions/InterventionManager.h"
#include "Clinical/ClinicalModel.h"

#include "Host/NeonatalMortality.h"

#include <utility>

namespace OM {

Simulation::Simulation(std::unique_ptr<scnXml::Scenario> scenario)
    : scenario_xml(std::move(scenario)),
      only_new_episode(scenario_xml->getMonitoring().getSurveyOptions().getOnlyNewEpisode()) {
    // 1) elements with no dependencies on other elements initialised here:
    WithinHost::Genotypes::init(*scenario_xml);
    util::master_RNG.seed(scenario_xml->getModel().getComputationParameters().getIseed(), 0); // Init RNG with Iseed
    const util::ModelNameProvider model_name_provider(scenario_xml->getModel());

    // 2) elements depending on only elements initialised in (1):
    sim::init(*scenario_xml, model_name_provider); // Also reads survey dates.

    // 3) elements depending on only elements initialised in (2).
    const Parameters parameters(scenario_xml->getModel().getParameters(),
                                model_name_provider); // Depends on ModelNameProvider.
    util::ModelOptions::init(scenario_xml->getModel().getModelOptions(),
                             model_name_provider); // Depends on ModelNameProvider.

    // 4) elements depending on only elements initialised in (3).
    WithinHost::diagnostics::init(parameters, *scenario_xml); // Depends on Parameters.

    // 5) elements depending on only elements initialised in (4).
    mon::initReporting(*scenario_xml); // Reporting init depends on diagnostics and monitoring

    // Init models used by humans
    Transmission::PerHost::init(scenario_xml->getModel().getHuman().getAvailabilityToMosquitoes());
    Host::InfectionIncidenceModel::init(parameters);
    WithinHost::WHInterface::init(parameters, *scenario_xml);
    Clinical::ClinicalModel::init(parameters, *scenario_xml);
    Host::NeonatalMortality::init(scenario_xml->getModel().getClinical());
    AgeStructure::init(scenario_xml->getDemography());

    // 3) elements depending on other elements; dependencies on (1) are not mentioned:
    // Transmission model initialisation depends on Transmission::PerHost and
    // genotypes (both from Human, from Population::init()) and
    // monitoring age groups (from Surveys.init()):
    // Note: PerHost dependency can be postponed; it is only used to set adultAge
    const size_t pop_size = scenario_xml->getDemography().getPopSize();

    population_model = std::make_unique<Population>(pop_size);
    transmission_model = std::unique_ptr<Transmission::TransmissionModel>(
        Transmission::createTransmissionModel(scenario_xml->getEntomology(), static_cast<int>(pop_size)));

    registerContinousPopulationCallbacks();

    // Depends on transmission model (for species indexes):
    // MDA1D may depend on health system (too complex to verify)
    interventions::InterventionManager::init(scenario_xml->getInterventions(), *population_model, *transmission_model);
    Clinical::ClinicalModel::setHS(scenario_xml->getHealthSystem()); // Depends on interventions, PK/PD (from humanPop)
    mon::initCohorts(scenario_xml->getMonitoring());                 // Depends on interventions

    sim::s_t0 = sim::zero();
    sim::s_t1 = sim::zero();

    // Make sure warmup period is at least as long as a human lifespan, as the
    // length required by vector warmup, and is a whole number of years.
    SimTime warmup = sim::maxHumanAge();
    if (transmission_model->interventionMode != Transmission::forcedEIR)
        warmup = max(warmup, sim::fromYearsI(55)); // Data is summed over 5 years; add an extra 50 for stabilization.

    warmup_length = sim::fromYearsI(static_cast<int>(ceil(sim::inYears(warmup))));
}

}
