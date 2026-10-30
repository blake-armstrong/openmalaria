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

#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
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

std::unique_ptr<OM::Simulation> make_simulation(const std::vector<char*>& args) {
    OM::util::CommandLineOptions command_line = OM::util::CommandLine::parse(args);
    std::unique_ptr<scnXml::Scenario> scenario = OM::util::loadScenario(command_line.scenario_path());
    OM::util::XMLChecker().PerformPostValidationChecks(*scenario);
    return std::make_unique<OM::Simulation>(std::move(scenario), std::move(command_line));
}

void run_one(const std::vector<char*>& args) { make_simulation(args)->run(OM::CheckpointOptions{}); }

class Turns {
public:
    explicit Turns(size_t count) : done(count, false) {}

    void wait(size_t index) {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return turn == index; });
    }

    void pass(size_t index) {
        std::unique_lock lock(mutex);
        give_on(index);
        changed.wait(lock, [&] { return turn == index; });
    }

    void finish(size_t index) {
        const std::scoped_lock lock(mutex);
        done[index] = true;
        give_on(index);
    }

private:
    void give_on(size_t index) {
        for (size_t step = 1; step <= done.size(); ++step) {
            const size_t next = (index + step) % done.size();
            if (!done[next]) {
                turn = next;
                break;
            }
        }
        changed.notify_all();
    }

    std::mutex mutex;
    std::condition_variable changed;
    std::vector<bool> done;
    size_t turn = 0;
};

void run_interleaved(const std::vector<std::vector<char*>>& runs) {
    Turns turns(runs.size());
    std::vector<std::exception_ptr> errors(runs.size());
    std::vector<std::thread> threads;
    threads.reserve(runs.size());
    for (size_t index = 0; index < runs.size(); ++index) {
        threads.emplace_back([&, index] {
            turns.wait(index);
            try {
                std::unique_ptr<OM::Simulation> simulation = make_simulation(runs[index]);
                simulation->set_step_hook([&] { turns.pass(index); });
                simulation->run(OM::CheckpointOptions{});
            } catch (...) {
                errors[index] = std::current_exception();
            }
            turns.finish(index);
        });
    }
    for (std::thread& thread : threads)
        thread.join();
    for (const std::exception_ptr& error : errors) {
        if (error)
            std::rethrow_exception(error);
    }
}

}

int main(int argc, char* argv[]) {
    try {
        OM::util::set_gsl_handler();
        std::span<char*> args{argv, static_cast<size_t>(argc)};
        const bool interleave = args.size() > 1 && std::string_view(args[1]) == "--interleave";
        if (interleave) {
            args[1] = args[0];
            args = args.subspan(1);
        }
        const std::vector<std::vector<char*>> runs = split_runs(args);
        if (interleave)
            run_interleaved(runs);
        else {
            for (const std::vector<char*>& run_args : runs)
                run_one(run_args);
        }
    } catch (const OM::util::base_exception& e) {
        std::cerr << "Error: " << e.message() << '\n';
        return e.getCode();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
