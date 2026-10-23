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
#include "util/CommandLine.h"
#include "util/DocumentLoader.h"
#include "util/XMLChecker.h"
#include "util/errors.h"

#include "schema/scenario.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<std::vector<char*>> split_runs(std::span<char*> args) {
    std::vector<std::vector<char*>> runs{{args.front()}};
    for (char* arg : args.subspan(1)) {
        if (std::string_view(arg) == "--next")
            runs.push_back({args.front()});
        else
            runs.back().push_back(arg);
    }
    return runs;
}

void run_one(const std::vector<char*>& args) {
    OM::util::CommandLineOptions command_line = OM::util::CommandLine::parse(args);
    std::unique_ptr<scnXml::Scenario> scenario = OM::util::loadScenario(command_line.scenario_path());
    OM::util::XMLChecker().PerformPostValidationChecks(*scenario);
    OM::Simulation simulation(std::move(scenario), std::move(command_line));
    simulation.run(OM::CheckpointOptions{});
}

}

int main(int argc, char* argv[]) {
    try {
        OM::util::set_gsl_handler();
        for (const std::vector<char*>& args : split_runs({argv, static_cast<size_t>(argc)}))
            run_one(args);
    } catch (const OM::util::base_exception& e) {
        std::cerr << "Error: " << e.message() << '\n';
        return e.getCode();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
