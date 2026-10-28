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
#include "util/ModelOptions.h"
#include "Clock.h"
#include "util/CommandLine.h"
#include "util/UnitParse.h"
#include "schema/scenario.h"
#include "mon/Monitoring.h"

#include <cstdlib>
#include <iomanip>
#include <regex>

// ———  Unit parsing stuff  ———
namespace OM::unit_parse {

template <typename T> int cast_to_int(T x) {
    const int y = static_cast<int>(x);
    if (static_cast<T>(y) != x) {
        throw util::format_error("underflow/overflow");
    }
    return y;
}

SimTime read_short_duration(const std::string& str, DefaultUnit def_unit, const Clock& clock,
                            const util::CommandLineOptions& command_line) {
    const char* s = str.c_str();
    char* end = nullptr;

    const long v = std::strtol(s, &end, 10);
    const size_t len = end - s;
    if (len == str.size()) {
        // no unit given; examine our policy:
        if (v == 0) { // don't need a unit in this case
            return Sim::zero();
        } else if (def_unit == NONE) {
            throw util::format_error("unit required but not given (try e.g. 5d or 12t)");
        } else {
            if (command_line.option(util::CommandLine::DeprecationWarnings)) {
                cerr << "Deprecation warning: duration \"" << str
                     << "\" specified without unit; it is recommended to do so (e.g. 5d or 1t)" << endl;
            }
            if (def_unit == DAYS) {
                return clock.round_to_ts_from_days(static_cast<double>(v));
            } else if (def_unit == STEPS) {
                return clock.from_ts(cast_to_int(v));
            } else {
                throw SWITCH_DEFAULT_EXCEPTION;
            }
        }
    } else if (len + 1 == str.size()) {
        // one extra character found; is this a unit?
        const char u = str[len];
        if (u == 'd' || u == 'D') {
            return clock.round_to_ts_from_days(static_cast<double>(v));
        } else if (u == 't' || u == 'T') {
            return clock.from_ts(cast_to_int(v));
        }
        // otherwise, fall through to below
    }
    throw util::format_error(string("bad format: '").append(str).append("' (try e.g. 1 or 2d or 3s)"));
}

// Parses str, returns v and may set unit
double parse_duration_and_unit(const std::string& str, DefaultUnit& unit,
                               const util::CommandLineOptions& command_line) {
    const char* s = str.c_str();
    char* end = nullptr;

    const double v = std::strtod(s, &end);
    const size_t len = end - s;
    if (len + 1 == str.size()) {
        // one extra character found; is this a unit?
        const char u = str[len];
        if (u == 'y' || u == 'Y')
            unit = YEARS;
        else if (u == 'd' || u == 'D')
            unit = DAYS;
        else if (u == 't' || u == 'T')
            unit = STEPS;
        else {
            throw util::format_error(string("unknown unit: '").append(str).append("' (try e.g. 1 or 2d or 3s or 4y)"));
        }
    } else if (len == str.size()) {
        if (v == 0.0)
            unit = DAYS; // special case: 0 does not require a unit; pretend the unit is days
        else if (unit == NONE /*no default set for this value*/) {
            throw util::format_error("unit required but not given (try e.g. 5d or 12t or 2.3y)");
        } else if (command_line.option(util::CommandLine::DeprecationWarnings)) {
            cerr << "Deprecation warning: duration \"" << str
                 << "\" specified without unit; it is recommended to do so (e.g. 5d or 1t or 0.5y)" << endl;
        }
    } else {
        throw util::format_error(string("bad format: '").append(str).append("' (try e.g. 1 or 2d or 3s or 4y)"));
    }
    return v;
}

SimTime read_duration(const std::string& str, DefaultUnit unit, const Clock& clock,
                      const util::CommandLineOptions& command_line) {
    const double v = parse_duration_and_unit(str, unit, command_line);

    if (unit == YEARS)
        return clock.from_years_n(v);
    else if (v != std::floor(v)) {
        throw util::format_error("fractional values are only allowed when the unit is years (e.g. 0.25y)");
    } else if (unit == DAYS)
        return clock.round_to_ts_from_days(v);
    else if (unit == STEPS)
        return clock.from_ts(cast_to_int(v));
    else
        throw SWITCH_DEFAULT_EXCEPTION;
}

double duration_to_days(const std::string& str, DefaultUnit unit, const Clock& clock,
                        const util::CommandLineOptions& command_line) {
    const double v = parse_duration_and_unit(str, unit, command_line);

    if (unit == YEARS)
        return v * Sim::one_year();
    else if (unit == DAYS)
        return v;
    else if (unit == STEPS)
        return v * clock.one_ts();
    else
        throw SWITCH_DEFAULT_EXCEPTION;
}

/** Returns sim::never() when it doesn't recognise a date. Throws when it does
 * but encounters definite format errors. */
SimTime parse_date(const std::string& str, const Clock& clock) {
    const std::regex date_rx(R"((\d{4})-(\d{1,2})-(\d{1,2}))");
    std::smatch rx_what;

    if (regex_match(str, rx_what, date_rx)) {
        int year = std::atoi(rx_what[1].str().c_str()), month = std::atoi(rx_what[2].str().c_str()),
            day = std::atoi(rx_what[3].str().c_str());
        if (month < 1 || month > 12 || day < 1 || day > MonthLen[month - 1]) {
            throw util::format_error(string(str).append(
                " does not look like a date (expected YYYY-MM-DD with 1≤MM≤12 and 1≤DD≤(days in month))"));
        }
        // We can't overflow: the largest year possible is 9999 which is about
        // 3.6 million days; an int is good to more than 1e9.
        // We should also round to the nearest step
        // Inconsistency: time "zero" is 0000-01-01, not 0001-01-01. Since
        // dates are always relative to another date, the extra year doesn't
        // actually affect anything.
        return Sim::origin() + Sim::from_years_i(year) + clock.round_to_ts_from_days(MonthStart[month - 1] + day - 1);
    } else {
        return Sim::never();
    }
}

SimTime read_date(const std::string& str, DefaultUnit def_unit, const Clock& clock,
                  const util::CommandLineOptions& command_line) {
    const SimTime date = parse_date(str, clock);
    if (date != Sim::never()) {
        if (date > clock.start_date() + Sim::from_years_i(500)) {
            cerr << "Warning: date is a long time after start date. Did you forget to set monitoring/startDate?"
                 << endl;
        } else if (date < clock.start_date()) {
            throw util::format_error("date of event is before the start of monitoring");
        }
        return date;
    } else {
        if (command_line.option(util::CommandLine::DeprecationWarnings)) {
            cerr << "Deprecation warning: time specified via duration \"" << str
                 << "\" where a date could be used; recommended to use a date (e.g. 2011-12-20)" << endl;
        }
        return clock.start_date() + read_duration(str, def_unit, clock, command_line);
    }
}

} // end of UnitParse namespace
