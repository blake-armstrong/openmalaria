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

#ifndef Hmod_util_CommandLine
#define Hmod_util_CommandLine

#include "Global.h"
#include <string>
#include <set>
#include <bitset>
#include <cassert>
#include <iosfwd>
#include <limits>
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
        PRINT_MODEL_OPTIONS = 0,
        /// Verbose output
        VERBOSE,
        /// Show progress
        PROGRESS,
        /// Forces checkpointing just before starting the main phase.
        CHECKPOINT,
        /// Exit after writing checkpoint
        CHECKPOINT_STOP,
        /** Do initialisation and error checks, but don't run simulation. */
        SKIP_SIMULATION,
        /** Compress main survey output. */
        COMPRESS_OUTPUT,
        /** Print the annual EIR. */
        PRINT_ANNUAL_EIR,
        /** Outputs samples from the active interpolation methods of all
             * age-group data suitible for graphing. */
        SAMPLE_INTERPOLATIONS,
        /** Show details of vector-parameter fitting.
             * 
             * The fitting methods used aren't guaranteed to work. If they don't, this output should help work out why. */
        DEBUG_VECTOR_FITTING,
        /** Print out details about interventions. */
        PRINT_INTERVENTIONS,
        /** Warn on use of deprecated features; that is recommend the use
             * of replacement features. */
        DEPRECATION_WARNINGS,
        /** Print times of all surveys. */
        PRINT_SURVEY_TIMES,
        PRINT_GENOTYPES,
        NUM_OPTIONS
    };

    enum class OutputFormat { TXT, BIN };

    /** Looks through all command line options.
	*
	* @returns The options, including the name of the scenario XML file to use.
	*
	* Throws cmd_exit in the case a help message is printed. Help
	* is printed to cout. */
    static CommandLineOptions parse(int argc, char* argv[]);

    /** Return true if given option (from CommandLine::Options) is active. */
    static bool option(size_t code);

    /** If path is relative, prepend it with clResourcePath. */
    static string lookupResource(const string& path);

    /** Get the name of the output file. */
    static string getOutputName();

    static OutputFormat getOutputFormat();

    /** Get the name of the ctsout file. */
    static string getCtsoutName();

    /** Get the name of the checkpoint file. */
    static string getCheckpointName();
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

    std::bitset<CommandLine::NUM_OPTIONS> flags;
    std::string resource_path;
    std::string scenario;
    std::string output;
    CommandLine::OutputFormat format = CommandLine::OutputFormat::TXT;
    std::string ctsout;
    std::string checkpoint_file;
};
}

namespace bridge {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline const util::CommandLineOptions* current_command_line = nullptr;

inline const util::CommandLineOptions& command_line() {
    assert(current_command_line != nullptr);
    return *current_command_line;
}

}

class CommandLineBinding {
public:
    explicit CommandLineBinding(const util::CommandLineOptions& options) : previous(bridge::current_command_line) {
        bridge::current_command_line = &options;
    }
    CommandLineBinding(const CommandLineBinding&) = delete;
    CommandLineBinding& operator=(const CommandLineBinding&) = delete;
    CommandLineBinding(CommandLineBinding&&) = delete;
    CommandLineBinding& operator=(CommandLineBinding&&) = delete;
    ~CommandLineBinding() { bridge::current_command_line = previous; }

private:
    const util::CommandLineOptions* previous;
};

namespace util {
inline bool CommandLine::option(size_t code) { return bridge::command_line().option(code); }
inline string CommandLine::lookupResource(const string& path) { return bridge::command_line().lookup_resource(path); }
inline string CommandLine::getOutputName() { return bridge::command_line().output_name(); }
inline CommandLine::OutputFormat CommandLine::getOutputFormat() { return bridge::command_line().output_format(); }
inline string CommandLine::getCtsoutName() { return bridge::command_line().ctsout_name(); }
inline string CommandLine::getCheckpointName() { return bridge::command_line().checkpoint_name(); }
}
}
#endif
