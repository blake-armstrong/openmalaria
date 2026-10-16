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

#ifndef HMOD_OM_SIM_TIME
#define HMOD_OM_SIM_TIME

#include <cmath>
#include <cstddef>
#include <memory>

namespace OM {

inline int floor_to_int(double x) { return static_cast<int>(std::floor(x)); }

/******************************************************************************
 * Since version 43.1, SimTime is an integer instead of a class. Note: a 
 * class can be used again simply by replacing the following typedef 
 * definition by the old SimTime class.
 * 
 * SimTime is now always a number of days.
 * 
 * Time steps, days and dates are derived from this. The values and units of
 * internal variables are an implementation detail (i.e. code outside of
 * sim should not need to know).
 * 
 * The simulation always starts at time zero. "Intervention time" is the 
 * beginning of the scenario and monitoring period.
 * 
 * Granularity: 1 day.
 *****************************************************************************/
typedef int SimTime;

/** Encapsulates static variables: sim time. */
class Sim {
public:
    ///@brief Simulation constants
    //@{
    /// Number of days in a year; defined as 365 (leap years are not simulated).
    static const int DaysInYear = 365;

    ///@brief Conversions to other types/units
    //@{
    /// Convert to years
    static inline double in_years(SimTime d) { return d * (1.0 / DaysInYear); }

    /// Convert to time steps (rounding down)
    static inline int in_steps(SimTime d);
    //@}

    /** Return this time in time steps modulo some positive integer. */
    static inline int modulo_steps(SimTime d, int denominator);

    /** Return this time in time steps modulo some positive integer. */
    static inline int modulo_year_steps(SimTime d);

    /** Duration zero and the time at the start of the simulation. */
    static inline SimTime zero() { return SimTime(0); }

    static inline SimTime origin() { return SimTime(0); }

    /** Special value representing a time point always in the past, such that
     * never() + x < zero() and x - never() will not to overflow for all valid
     * simulation times x (including any value now() may take as well as
     * never() and future()). */
    static inline SimTime never() { return SimTime(-0x3FFFFFFF); }

    /** Special value representing a time point always in the future, such that
     * now() < future() and now() + future() does not overflow. */
    static inline SimTime future() { return SimTime(0x3FFFFFFF); }

    /** One day. */
    static inline SimTime one_day() { return SimTime(1); }

    /** One year. See DAYS_IN_YEAR. */
    static inline SimTime one_year() { return SimTime(DaysInYear); }

    /** One time step (currently either one or five days). */
    static inline SimTime one_ts();
    //@}

    ///@brief Parameterised constructors
    //@{
    /** Convert. */
    static inline SimTime from_ts(int ts);

    /** Duration in days. Should be fast (currently no conversion required). */
    static inline SimTime from_days(int days) { return SimTime(days); }

    /** Convert from a whole number of years. */
    static inline SimTime from_years_i(int years) { return SimTime(DaysInYear * years); }

    /** Convert from years to nearest time step. */
    static inline SimTime from_years_n(double years);

    /** Convert from years, rounding down to the next time step. */
    static inline SimTime from_years_d(double years);

    /** Round to the nearest time-step, where input is in days. */
    static inline SimTime round_to_ts_from_days(double days);
    //@}

    /** The number of time steps in one year. */
    static inline size_t steps_per_year();

    /** A cached value: one year divided by one time step. */
    static inline double years_per_step();

    /// Maximum possible age of a human.
    static inline SimTime max_human_age();

    /// The starting date of the simulation
    static inline SimTime start_date();

    /// The ending date of the simulation
    static inline SimTime end_date();
    //@}

    ///@brief Access simulation time variables
    //@{
    /** Time at the beginning of a time step update.
     *
     * This is what is mostly used during an update. It is never negative and
     * increases throughout the simulation. */
    static inline SimTime ts0();
    /** Time at the end of a time step update.
     * 
     * During an update, ts0() + oneTS() = ts1(). Neither this nor ts0 should
     * be used outside of updates. */
    static inline SimTime ts1();
    /**
     * Time steps are mid-day to mid-day, and this is the time at mid-day (i.e.
     * this equals ts1 from the last step and ts0 from the next one).
     * 
     * This is for monitoring and intervention deployment which happens between
     * updates. Cannot be used during human or vector update.
     */
    static inline SimTime now();
    /** During updates, this is ts0; between, this is now. */
    static inline SimTime now_or_ts0();
    /** During updates, this is ts1; between, this is now. */
    static inline SimTime now_or_ts1();
    /** During updates, this is ts0; between, it is now - 1. */
    static inline SimTime latest_ts0();
    //@}

    ///@brief Access intervention-time variables
    //@{
    /// Time relative to the start of the intervention period.
    ///
    /// This equals (intervDate() - startDate()), but happens to be the most
    /// common way that intervention-period dates are used.
    static inline SimTime interv_time();

    /// The current date.
    ///
    /// Only valid during the intervention phase, since the duration required
    /// for warm-up is not known in advance. (In prior phases, this function
    /// returns a large negative value.)
    ///
    /// Intervention deployment times are relative to this date.
    static inline SimTime interv_date();
    //@}
};

}
#endif
