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
#include "Simulator.h"

#include "util/errors.h"
#include "util/CommandLine.h"

#include "schema/scenario.h"

#include <cerrno>

using namespace OM;

// The main() function parses argv, loads the scenario XML and runs the
// simulation via Simulator::run(), which writes output.txt/ctsout.txt. The
// orchestration itself lives in Simulator.cpp, shared with the Python
// bindings (python/src/bindings.cpp); this file only owns argv parsing
// and translating exceptions to an exit code
int main(int argc, char* argv[])
{
    int exitStatus = EXIT_SUCCESS;
    string scenarioFile;

    try {
        scenarioFile = util::CommandLine::parse (argc, argv);

        Simulator::RunConfig config;
        config.scenario.isPath = true;
        config.scenario.path = scenarioFile;
        config.outputMode = Simulator::OutputMode::WriteFiles;
        Simulator::run(config);
    } catch (const OM::util::cmd_exception& e) {
        if( e.getCode() == 0 ){
            // this is not an error, but exiting due to command line
            std::cerr << e.what() << "; exiting..." << endl;
        }else{
            std::cerr << "Command-line error: "<<e.what();
            exitStatus = e.getCode();
        }
    } catch (const ::xsd::cxx::tree::exception<char>& e) {
        if (errno == 2) // Errno value 2 corresponds to "No such file or directory" i.e. this is not a mismatch between scenario contents and schema rules.
        {
            // We don't print the content of the xsdcxx exception since the error messages are not relevant to the user in this case.
            std::cerr << "Parsing scenario file failed.\n\tLikely the XSD schema file is not present at an expected location/with expected filename." << std::endl;
        }
        else
        {
            // e.what() returns a concise error message e.g. "instance document parsing failed".
            // Whereas inserting e into a stream inserts the (possibly long and repetitive) list of error messages arising from the parsing attempt.
            std::cerr << "XSD error: " << e.what() << '\n' << e << endl;

            // We print this *after* the above since users are more likely to take note of the tail of stderr/stdout output than the head.
            std::cerr << "Parsing scenario file failed.\n\tLikely the XSD schema is named/located correctly but the scenario does not conform to the schema." << std::endl;
        }
        exitStatus = OM::util::Error::XSD;
    } catch (const OM::util::checkpoint_error& e) {
        std::cerr << "Checkpoint error: " << e.what() << endl;
        std::cerr << e << flush;
        exitStatus = e.getCode();
    } catch (const OM::util::traced_exception& e) {
        std::cerr << "Code error: " << e.what() << endl;
        std::cerr << e << flush;
        std::cerr << "This is likely an error in the C++ code. Please report!" << endl;
        exitStatus = e.getCode();
    } catch (const OM::util::xml_scenario_error& e) {
        std::cerr << "Error: " << e.what() << endl;
        std::cerr << "In: " << scenarioFile << endl;
        exitStatus = e.getCode();
    } catch (const OM::util::base_exception& e) {
        std::cerr << "Error: " << e.message() << endl;
        exitStatus = e.getCode();
    } catch (const exception& e) {
        std::cerr << "Error: " << e.what() << endl;
        exitStatus = EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown error" << endl;
        exitStatus = EXIT_FAILURE;
    }

    // If we get to here, we already know an error occurred.
    if( errno != 0 )
        std::perror( "OpenMalaria" );

    return exitStatus;
}
