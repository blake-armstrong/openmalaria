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
using SimTime = int;

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
    //@}

    /** Duration zero and the time at the start of the simulation. */
    static inline SimTime zero() { return 0; }

    static inline SimTime origin() { return 0; }

    /** Special value representing a time point always in the past, such that
     * never() + x < zero() and x - never() will not to overflow for all valid
     * simulation times x (including any value now() may take as well as
     * never() and future()). */
    static inline SimTime never() { return -0x3FFFFFFF; }

    /** Special value representing a time point always in the future, such that
     * now() < future() and now() + future() does not overflow. */
    static inline SimTime future() { return 0x3FFFFFFF; }

    /** One day. */
    static inline SimTime one_day() { return 1; }

    /** One year. See DAYS_IN_YEAR. */
    static inline SimTime one_year() { return DaysInYear; }
    //@}

    ///@brief Parameterised constructors
    //@{
    /** Duration in days. Should be fast (currently no conversion required). */
    static inline SimTime from_days(int days) { return days; }

    /** Convert from a whole number of years. */
    static inline SimTime from_years_i(int years) { return DaysInYear * years; }
    //@}
};

}
#endif
