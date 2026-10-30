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

#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/DocumentLoader.h"
#include "util/XMLChecker.h"
#include "util/logging.h"

#include "Simulation.h"

#include "schema/scenario.h"

#include <cerrno>
#include <sstream>
#include <string>

using namespace OM;

namespace {

template <typename Exception> std::string streamed(const Exception& e) {
    std::ostringstream stream;
    stream << e;
    return stream.str();
}

std::string without_final_newline(std::string text) {
    if (text.ends_with('\n'))
        text.pop_back();
    return text;
}

CheckpointOptions checkpoint_options(const util::CommandLineOptions& command_line) {
    CheckpointOptions options;
    const string& name = command_line.checkpoint_name();
    if (!name.empty())
        options.file_name = name;
    options.enabled = command_line.option(util::CommandLine::Checkpoint);
    options.stop_after_write = command_line.option(util::CommandLine::CheckpointStop);
    return options;
}

}

/// main() — loads scenario XML and runs simulation
int main(int argc, char* argv[]) { // NOLINT(bugprone-exception-escape)
    int exit_status = EXIT_SUCCESS;
    string scenario_file;

    try {
        util::set_gsl_handler();

        util::CommandLineOptions command_line = util::CommandLine::parse({argv, static_cast<size_t>(argc)});
        scenario_file = command_line.scenario_path();
        unique_ptr<scnXml::Scenario> scenario = util::loadScenario(scenario_file);

        util::XMLChecker().PerformPostValidationChecks(*scenario);

        const CheckpointOptions checkpointing = checkpoint_options(command_line);
        Simulation simulation(std::move(scenario), std::move(command_line));
        simulation.run(checkpointing);
    } catch (const OM::util::cmd_exception& e) {
        if (e.getCode() == 0) {
            // this is not an error, but exiting due to command line
            logging::warn("{}; exiting...", e.what());
        } else {
            logging::error("Command-line error: {}", e.what());
            exit_status = e.getCode();
        }
    } catch (const ::xsd::cxx::tree::exception<char>& e) {
        if (errno ==
            2) // Errno value 2 corresponds to "No such file or directory" i.e. this is not a mismatch between scenario contents and schema rules.
        {
            // We don't print the content of the xsdcxx exception since the error messages are not relevant to the user in this case.
            logging::error("Parsing scenario file failed.\n\tLikely the XSD schema file is not present at an expected "
                           "location/with expected filename.");
        } else {
            // e.what() returns a concise error message e.g. "instance document parsing failed".
            // Whereas inserting e into a stream inserts the (possibly long and repetitive) list of error messages arising from the parsing attempt.
            logging::error("XSD error: {}\n{}", e.what(), streamed(e));

            // We print this *after* the above since users are more likely to take note of the tail of stderr/stdout output than the head.
            logging::error("Parsing scenario file failed.\n\tLikely the XSD schema is named/located correctly but the "
                           "scenario does not conform to the schema.");
        }
        exit_status = OM::util::Error::XSD;
    } catch (const OM::util::checkpoint_error& e) {
        logging::error("Checkpoint error: {}", e.what());
        logging::error("{}", without_final_newline(streamed(e)));
        exit_status = e.getCode();
    } catch (const OM::util::traced_exception& e) {
        logging::error("Code error: {}", e.what());
        logging::error("{}", without_final_newline(streamed(e)));
        logging::error("This is likely an error in the C++ code. Please report!");
        exit_status = e.getCode();
    } catch (const OM::util::xml_scenario_error& e) {
        logging::error("Error: {}", e.what());
        logging::error("In: {}", scenario_file);
        exit_status = e.getCode();
    } catch (const OM::util::base_exception& e) {
        logging::error("Error: {}", e.message());
        exit_status = e.getCode();
    } catch (const exception& e) {
        logging::error("Error: {}", e.what());
        exit_status = EXIT_FAILURE;
    } catch (...) {
        logging::error("Unknown error");
        exit_status = EXIT_FAILURE;
    }

    return exit_status;
}
