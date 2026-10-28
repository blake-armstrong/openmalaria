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

#ifndef HMOD_AGE_DISTRIBUTION
#define HMOD_AGE_DISTRIBUTION

#include "Global.h"

namespace scnXml {
class AgeGroupValues;
}
namespace OM {
class Clock;
namespace util {
class CommandLineOptions;

/** This struct is not for external use. Use AgeGroupInterpolator instead.
 * 
 * It is here to allow the trivial functions below to be inlined. */
struct AgeGroupInterpolation {
    virtual ~AgeGroupInterpolation() = default;

    virtual double eval(double age_years) const = 0;
    virtual void scale(double factor) = 0;
    virtual double first_global_maximum() = 0;

protected:
    /** Sample interpolator between 0 and max age, outputting to a csv file
     * called name.csv. */
    void output_samples(const string& name, const Clock& clock, const CommandLineOptions& command_line);
};

/** A class representing deterministic interpolation of data collected
 * according to age groups. Derived classes implement the actual interpolation.
 * 
 * Current version does not store an age index, thus an order log(n) lookup
 * must occur each time a value is looked up.
 ********************************************/
struct AgeGroupInterpolator {
    /** Create. set() must be called before further use to avoid exceptions. */
    AgeGroupInterpolator();
    AgeGroupInterpolator(const AgeGroupInterpolator&) = delete;
    AgeGroupInterpolator& operator=(const AgeGroupInterpolator&) = delete;
    AgeGroupInterpolator(AgeGroupInterpolator&& other) noexcept;
    AgeGroupInterpolator& operator=(AgeGroupInterpolator&& other) noexcept;
    ~AgeGroupInterpolator() { reset(); }

    /** Set age-group interpolation data from an XML fragment.
     * 
     * @param ageGroups XML element of per-age-group values
     * @param eltName Name of XML element (for reasonable error reporting)
     */
    void set(const scnXml::AgeGroupValues& age_groups, const char* elt_name, const Clock& clock,
             const CommandLineOptions& command_line);

    /** Set back to initial state. */
    void reset();

    /// Return true if set() was ever called.
    bool isSet() const; // NOLINT(readability-identifier-naming)

    /** Return a value interpolated for age ageYears. */
    inline double eval(double age_years) const { return obj->eval(age_years); }

    /** Scale function by factor. */
    inline void scale(double factor) { obj->scale(factor); }

    /** Find the youngest age which is the global maximum (i.e. the age at
     * which individuals are considered adults where all adults have the same
     * same value). */
    inline double first_global_maximum() const { return obj->first_global_maximum(); }

private:
    AgeGroupInterpolation* obj;
};

}
}
#endif
