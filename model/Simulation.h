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
#include "Parameters.h"
#include "Population.h"
#include "Transmission/transmission.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/random.h"

#include "schema/scenario.h"

#include <memory>
#include <string>

namespace OM {

struct CheckpointOptions {
    std::string file_name = "checkpoint";
    bool enabled = false;
    bool stop_after_write = false;
};

class Simulation {
public:
    Simulation(std::unique_ptr<scnXml::Scenario> scenario, util::CommandLineOptions options);
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&&) = delete;
    Simulation& operator=(Simulation&&) = delete;
    ~Simulation();

    void run(const CheckpointOptions& checkpointing);
    void run_phase(const std::string& phase, SimTime end);
    void run_until(SimTime end);
    void step();

    const scnXml::Scenario& scenario() const { return *scenario_xml; }
    Population& population() { return *population_model; }
    Transmission::TransmissionModel& transmission() { return *transmission_model; }
    SimTime human_warmup_length() const { return warmup_length; }
    bool survey_only_new_ep() const { return only_new_episode; }

private:
    void start_new(const CheckpointOptions& checkpointing);
    void resume(const std::string& checkpoint_file);
    void finish();

    util::CommandLineOptions command_line;
    CommandLineBinding command_line_binding{command_line};
    Clock clock;
    ClockBinding clock_binding{clock};
    util::MasterRngs master_rngs;
    MasterRngsBinding master_rngs_binding{master_rngs};
    util::ModelOptionFlags model_options;
    ModelOptionsBinding model_options_binding{model_options};
    Parameters parameters;
    std::unique_ptr<scnXml::Scenario> scenario_xml;
    std::unique_ptr<Population> population_model;
    std::unique_ptr<Transmission::TransmissionModel> transmission_model;
    SimTime warmup_length = 0;
    SimTime end_time = 0;
    SimTime est_end_time = 0;
    int last_percent = -1;
    bool only_new_episode;
};

}

#endif
