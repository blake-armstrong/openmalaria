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

#ifndef HMOD_CLOCK
#define HMOD_CLOCK

#include "Global.h"
#include "util/ModelNameProvider.h"
#include "util/mod.h"

#include <cassert>
#include <cstddef>
#include <iosfwd>

class UnittestUtil;

namespace scnXml {
class Scenario;
}

namespace OM {
namespace util {
class CommandLineOptions;
}

class Clock {
public:
    void init(const scnXml::Scenario& scenario, const util::ModelNameProvider& mnp,
              const util::CommandLineOptions& command_line);

    SimTime one_ts() const { return interval; }
    int in_steps(SimTime d) const { return d / interval; }
    int modulo_steps(SimTime d, int denominator) const { return util::mod_nn(d / interval, denominator); }
    int modulo_year_steps(SimTime d) const { return util::mod_nn(d / interval, static_cast<int>(steps_in_year)); }
    SimTime from_ts(int ts) const { return interval * ts; }
    SimTime from_years_n(double years) const { return round_to_ts_from_days(Sim::DaysInYear * years); }
    SimTime from_years_d(double years) const {
        return from_ts(floor_to_int(static_cast<double>(steps_in_year) * years));
    }
    SimTime round_to_ts_from_days(double days) const { return from_ts(floor_to_int((days / interval) + 0.5)); }
    size_t steps_per_year() const { return steps_in_year; }
    double years_per_step() const { return years_in_step; }

    SimTime max_human_age() const { return max_age; }
    SimTime start_date() const { return start; }
    SimTime end_date() const { return end; }

    SimTime ts0() const {
        assert(in_update);
        return t0;
    }
    SimTime ts1() const {
        assert(in_update);
        return t1;
    }
    SimTime now() const {
        assert(!in_update);
        return t0;
    }
    SimTime now_or_ts0() const { return t0; }
    SimTime now_or_ts1() const { return t1; }
    SimTime latest_ts0() const { return t1 - one_ts(); }
    SimTime interv_time() const { return interv; }
    SimTime interv_date() const { return start + interv; }

    void reset_time() {
        t0 = Sim::zero();
        t1 = Sim::zero();
    }
    void start_intervention_period() { interv = Sim::zero(); }

    void start_update() {
        t1 = t1 + one_ts();
#ifndef NDEBUG
        in_update = true;
#endif
    }
    void end_update() {
#ifndef NDEBUG
        in_update = false;
#endif
        t0 = t1;
        interv = interv + one_ts();
    }

    void checkpoint_interv_time(std::istream& stream);
    void checkpoint_interv_time(std::ostream& stream);
    void checkpoint_step_times(std::istream& stream);
    void checkpoint_step_times(std::ostream& stream);

private:
    void init_interval(const scnXml::Scenario& scenario, util::ModelNameProvider mnp);

    friend class ::UnittestUtil;

    int interval = 0;
    size_t steps_in_year = 0;
    double years_in_step = 0.0;
    SimTime start = Sim::never();
    SimTime end = Sim::never();
    SimTime max_age = Sim::never();
    SimTime t0 = Sim::never();
    SimTime t1 = Sim::never();
    SimTime interv = Sim::never();
#ifndef NDEBUG
    bool in_update = false;
#endif
};

namespace bridge {

inline Clock* current_clock = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline Clock& clock() {
    assert(current_clock != nullptr);
    return *current_clock;
}

}

class ClockBinding {
public:
    explicit ClockBinding(Clock& clock) : previous(bridge::current_clock) { bridge::current_clock = &clock; }
    ClockBinding(const ClockBinding&) = delete;
    ClockBinding& operator=(const ClockBinding&) = delete;
    ClockBinding(ClockBinding&&) = delete;
    ClockBinding& operator=(ClockBinding&&) = delete;
    ~ClockBinding() { bridge::current_clock = previous; }

private:
    Clock* previous;
};

inline int Sim::in_steps(SimTime d) { return bridge::clock().in_steps(d); }
inline int Sim::modulo_steps(SimTime d, int denominator) { return bridge::clock().modulo_steps(d, denominator); }
inline int Sim::modulo_year_steps(SimTime d) { return bridge::clock().modulo_year_steps(d); }
inline SimTime Sim::one_ts() { return bridge::clock().one_ts(); }
inline SimTime Sim::from_ts(int ts) { return bridge::clock().from_ts(ts); }
inline SimTime Sim::from_years_n(double years) { return bridge::clock().from_years_n(years); }
inline SimTime Sim::from_years_d(double years) { return bridge::clock().from_years_d(years); }
inline SimTime Sim::round_to_ts_from_days(double days) { return bridge::clock().round_to_ts_from_days(days); }
inline size_t Sim::steps_per_year() { return bridge::clock().steps_per_year(); }
inline double Sim::years_per_step() { return bridge::clock().years_per_step(); }
inline SimTime Sim::max_human_age() { return bridge::clock().max_human_age(); }
inline SimTime Sim::start_date() { return bridge::clock().start_date(); }
inline SimTime Sim::end_date() { return bridge::clock().end_date(); }
inline SimTime Sim::ts0() { return bridge::clock().ts0(); }
inline SimTime Sim::ts1() { return bridge::clock().ts1(); }
inline SimTime Sim::now() { return bridge::clock().now(); }
inline SimTime Sim::now_or_ts0() { return bridge::clock().now_or_ts0(); }
inline SimTime Sim::now_or_ts1() { return bridge::clock().now_or_ts1(); }
inline SimTime Sim::latest_ts0() { return bridge::clock().latest_ts0(); }
inline SimTime Sim::interv_time() { return bridge::clock().interv_time(); }
inline SimTime Sim::interv_date() { return bridge::clock().interv_date(); }

}

#endif
