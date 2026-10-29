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
namespace mon {
struct MonitoringState;
}
namespace util {
class CommandLineOptions;
}

class Clock {
public:
    void init(const scnXml::Scenario& scenario, const util::ModelNameProvider& mnp,
              const util::CommandLineOptions& command_line, mon::MonitoringState& monitoring);

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

    /** Time at the beginning of a time step update.
     *
     * This is what is mostly used during an update. It is never negative and
     * increases throughout the simulation. */
    SimTime ts0() const {
        assert(in_update);
        return t0;
    }
    /** Time at the end of a time step update.
     * 
     * During an update, ts0() + oneTS() = ts1(). Neither this nor ts0 should
     * be used outside of updates. */
    SimTime ts1() const {
        assert(in_update);
        return t1;
    }
    /**
     * Time steps are mid-day to mid-day, and this is the time at mid-day (i.e.
     * this equals ts1 from the last step and ts0 from the next one).
     * 
     * This is for monitoring and intervention deployment which happens between
     * updates. Cannot be used during human or vector update.
     */
    SimTime now() const {
        assert(!in_update);
        return t0;
    }
    /** During updates, this is ts0; between, this is now. */
    SimTime now_or_ts0() const { return t0; }
    /** During updates, this is ts1; between, this is now. */
    SimTime now_or_ts1() const { return t1; }
    /** During updates, this is ts0; between, it is now - 1. */
    SimTime latest_ts0() const { return t1 - one_ts(); }
    /// Time relative to the start of the intervention period.
    ///
    /// This equals (intervDate() - startDate()), but happens to be the most
    /// common way that intervention-period dates are used.
    SimTime interv_time() const { return interv; }
    /// The current date.
    ///
    /// Only valid during the intervention phase, since the duration required
    /// for warm-up is not known in advance. (In prior phases, this function
    /// returns a large negative value.)
    ///
    /// Intervention deployment times are relative to this date.
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

}

#endif
