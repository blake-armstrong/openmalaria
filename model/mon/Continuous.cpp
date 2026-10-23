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

#include "mon/Continuous.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/UnitParse.h"
#include "schema/monitoring.h"

#include <vector>
#include <map>
#include <fstream>

namespace OM::mon {
using util::xml_scenario_error;

/* Initialise: enable outputs registered and requested in XML.
         * Search for Continuous::registerCallback to see outputs available. */
void Continuous::init(const scnXml::Monitoring& monitoring, bool is_checkpoint, const string& ctsout_name) {
    const scnXml::Monitoring::ContinuousOptional& cts_opt = monitoring.getContinuous();
    if (!cts_opt.present()) {
        cts_period = Sim::zero();
        return;
    }
    const auto& options = cts_opt.get();
    try {
        //NOTE: if changing XSD, this should not have a default unit:
        cts_period = UnitParse::readShortDuration(options.getPeriod(), UnitParse::STEPS);
        if (cts_period < Sim::one_ts())
            throw util::format_error("must be >= 1 time step");
    } catch (const util::format_error& e) {
        throw xml_scenario_error(string("monitoring/continuous/period: ").append(e.message()));
    }

    if (options.getDuringInit().present())
        during_init = options.getDuringInit().get();

    for (const auto& option : options.getOption()) {
        auto registered_output = registered.find(option.getName());
        if (registered_output == registered.end()) {
            throw xml_scenario_error("monitoring.continuous: no output " + string(option.getName()));
        }
        if (option.getValue())
            to_report.push_back(registered_output->second);
    }

    if (is_checkpoint) {
        // When loading a check-point, we resume reporting to this file.
        cts_stream.open(ctsout_name, ios::binary | ios::ate | ios::in | ios::out);
        if (cts_stream.fail())
            throw util::checkpoint_error("Continuous: resume error (no file)");
        return; // checkpoint() sets the saved output position later
    }

    cts_stream.open(ctsout_name, ios::binary | ios::out);
    cts_stream << "##\t##\n"; // live-graph needs a delimiter specifier when it is not a comma
    if (during_init)
        cts_stream << "simulation time\t";
    cts_stream << "timestep"; // TODO: change to days or remove or leave?
    for (const auto& report : to_report)
        cts_stream << std::get<0>(report);
    cts_stream << '\n' << flush;
    stream_offset = cts_stream.tellp();
}

void Continuous::checkpoint(ostream& stream) {
    if (cts_period == Sim::zero())
        return; // output disabled

    stream_offset & stream;
}
void Continuous::checkpoint(istream& stream) {
    if (cts_period == Sim::zero())
        return; // output disabled

    /* We attempt to resume output correctly after a reload by recording
            * the last position, and relocating there.
            * 
            * (Keeping results in memory until end of sim would be another,
            * slightly safer, option, but loses real-time output.) */
    stream_offset & stream;
    // We skip back to the last write-point, so anything written after the
    // last checkpoint will be repeated:
    cts_stream.seekp(stream_offset, ios_base::beg);

    if (cts_stream.fail())
        throw util::checkpoint_error("Continuous: resume error (bad pos/file)");
}

void Continuous::register_callback(const string& opt_name, const string& titles, const function<void(ostream&)>& f) {
    assert(registered.count(opt_name) == 0); // name clash/registered twice?
    const function<void(const Population&, ostream&)> wrapped = [f](const Population&, ostream& ostream) {
        f(ostream);
    };
    registered[opt_name] = {titles, wrapped};
}

void Continuous::register_callback(const string& opt_name, const string& titles,
                                   const function<void(const vector<Host::Human>&, ostream&)>& f) {
    assert(registered.count(opt_name) == 0); // name clash/registered twice?
    const function<void(const Population&, ostream&)> wrapped = [f](const Population& p, ostream& ostream) {
        f(p.humans, ostream);
    };
    registered[opt_name] = {titles, wrapped};
}

void Continuous::register_callback(const string& opt_name, const string& titles,
                                   const function<void(Population&, ostream&)>& f) {
    assert(registered.count(opt_name) == 0); // name clash/registered twice?
    registered[opt_name] = {titles, f};
}

void Continuous::update(Population& population) {
    if (cts_period == Sim::zero())
        return; // output disabled
    if (!during_init) {
        if (Sim::interv_time() < Sim::zero() || mod_nn(Sim::interv_time(), cts_period) != Sim::zero())
            return;
    } else {
        if (mod_nn(Sim::now(), cts_period) != Sim::zero())
            return;
        cts_stream << Sim::in_steps(Sim::now()) << '\t';
    }

    if (during_init && Sim::interv_time() < Sim::zero()) {
        cts_stream << "nan";
    } else {
        // NOTE: we could switch this to output dates, but (1) it would be
        // breaking change and (2) it may be harder to use.
        cts_stream << Sim::in_steps(Sim::interv_time());
    }
    for (const auto& report : to_report)
        std::get<1>(report)(population, cts_stream);

    // We must flush often to avoid temporarily outputting partial lines
    // (resulting in incorrect real-time graphs).
    cts_stream << '\n' << flush;

    stream_offset = cts_stream.tellp();
}
} // namespace OM::mon
