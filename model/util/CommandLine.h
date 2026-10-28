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

#ifndef HMOD_UTIL_COMMAND_LINE
#define HMOD_UTIL_COMMAND_LINE

#include "Global.h"
#include <string>
#include <set>
#include <bitset>
#include <iosfwd>
#include <limits>
#include <span>
using namespace std;

namespace OM {
namespace util {
class CommandLineOptions;

/// Command line options and processing
class CommandLine {
public:
    /// Boolean command-line options
    enum Options {
        /** Outputs non-default "ModelOptions" values in a human-readable form. */
        PrintModelOptions = 0,
        /// Verbose output
        Verbose,
        /// Show progress
        Progress,
        /// Forces checkpointing just before starting the main phase.
        Checkpoint,
        /// Exit after writing checkpoint
        CheckpointStop,
        /** Do initialisation and error checks, but don't run simulation. */
        SkipSimulation,
        /** Compress main survey output. */
        CompressOutput,
        /** Print the annual EIR. */
        PrintAnnualEir,
        /** Outputs samples from the active interpolation methods of all
             * age-group data suitible for graphing. */
        SampleInterpolations,
        /** Show details of vector-parameter fitting.
             * 
             * The fitting methods used aren't guaranteed to work. If they don't, this output should help work out why. */
        DebugVectorFitting,
        /** Print out details about interventions. */
        PrintInterventions,
        /** Warn on use of deprecated features; that is recommend the use
             * of replacement features. */
        DeprecationWarnings,
        /** Print times of all surveys. */
        PrintSurveyTimes,
        PrintGenotypes,
        NumOptions
    };

    enum class OutputFormat { Txt, Bin };

    /** Looks through all command line options.
	*
	* @returns The options, including the name of the scenario XML file to use.
	*
	* Throws cmd_exit in the case a help message is printed. Help
	* is printed to cout. */
    static CommandLineOptions parse(std::span<char* const> args);
};

class CommandLineOptions {
public:
    bool option(size_t code) const { return flags.test(code); }
    std::string lookup_resource(const std::string& path) const;
    const std::string& scenario_path() const { return scenario; }
    const std::string& output_name() const { return output; }
    CommandLine::OutputFormat output_format() const { return format; }
    const std::string& ctsout_name() const { return ctsout; }
    const std::string& checkpoint_name() const { return checkpoint_file; }

    void checkpoint(std::istream& stream) const;
    void checkpoint(std::ostream& stream) const;

private:
    friend class CommandLine;

    std::bitset<CommandLine::NumOptions> flags;
    std::string resource_path;
    std::string scenario;
    std::string output;
    CommandLine::OutputFormat format = CommandLine::OutputFormat::Txt;
    std::string ctsout;
    std::string checkpoint_file;
};
}

}
#endif
