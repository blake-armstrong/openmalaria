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

#ifndef HMOD_SIMULATION
#define HMOD_SIMULATION

#include "Global.h"
#include "Population.h"
#include "Transmission/transmission.h"

#include "schema/scenario.h"

#include <memory>

namespace OM {

class Simulation {
public:
    explicit Simulation(std::unique_ptr<scnXml::Scenario> scenario);

    const scnXml::Scenario& scenario() const { return *scenario_xml; }
    Population& population() { return *population_model; }
    Transmission::TransmissionModel& transmission() { return *transmission_model; }
    SimTime human_warmup_length() const { return warmup_length; }
    bool survey_only_new_ep() const { return only_new_episode; }

private:
    std::unique_ptr<scnXml::Scenario> scenario_xml;
    std::unique_ptr<Population> population_model;
    std::unique_ptr<Transmission::TransmissionModel> transmission_model;
    SimTime warmup_length = 0;
    bool only_new_episode = false;
};

}

#endif
