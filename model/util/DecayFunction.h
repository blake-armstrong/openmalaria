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

#ifndef HMOD_DECAY_FUNCTION
#define HMOD_DECAY_FUNCTION

#include <limits>
#include <memory>

#include "Global.h"
#include "util/sampler.h"
#include "util/UnitParse.h"

namespace OM::util {

/** An interface for a few types of decay function (some of which may also be
 * suitible survival functions).
 *
 * Heterogeneity is implemented by passing a DecayFunction object to the eval
 * function; this should be sampled by the same DecayFunction.
 *****************************************************************************/
class DecayFunction {
public:
    DecayFunction(const Clock& clock, bool increasing, double initial_efficacy, double cv)
        : clock(&clock), increasing(increasing), initial_efficacy(initial_efficacy),
          het(std::move(LognormalSampler::fromMeanCV(1.0, cv))) {}

    DecayFunction(const DecayFunction& other)
        : clock(other.clock), increasing(other.increasing), initial_efficacy(other.initial_efficacy),
          het(other.het ? other.het->clone() : nullptr), het_factor(other.het_factor) {}
    DecayFunction& operator=(const DecayFunction&) = delete;
    DecayFunction(DecayFunction&&) = delete;
    DecayFunction& operator=(DecayFunction&&) = delete;

    virtual ~DecayFunction() = default;

    /** Return a new decay function, constructed from an XML element.
     * 
     * @param elt XML element specifying which function to use and parameters
     * @param eltName Name of XML element (for reasonable error reporting)
     */
    static unique_ptr<DecayFunction> make_object(const scnXml::DecayFunction& elt, const char* elt_name,
                                                 const Clock& clock, const CommandLineOptions& command_line);

    /** Sample a DecayFunction value (should be stored per individual).
     * 
     * Note that a DecayFunction is needed to call eval() even if heterogeneity
     * is not wanted. If sigma = 0 then the random number stream will not be
     * touched. */
    unique_ptr<DecayFunction> het_sample(LocalRng& rng) const { return het_sample(het->sample(rng)); }

    /** Generate a DecayFunction value from an existing sample. */
    std::unique_ptr<DecayFunction> het_sample(NormalSample sample) const { return het_sample(het->sample(sample)); }

    /** Generate a DecayFunction value from an existing sample. */
    unique_ptr<DecayFunction> het_sample(double factor) const {
        unique_ptr<DecayFunction> sample = make_het_sample(factor);
        sample->het_factor = factor;
        return sample;
    }

    /** Say you have a population of objects which each have two states:
     * decayed and not decayed. If you want to use a DecayFunction to model
     * the proportion of objects which have decayed, you need to work out per
     * object the age of decay. This function does that.
     * 
     * This is only valid where mu and sigma parameters are zero.
     * 
     * @returns Age at which an object should decay. */
    virtual SimTime sample_age_of_decay(LocalRng& rng) const = 0;

    inline double eval(double age_days) const {
        if (increasing)
            return 1.0 - compute(age_days) * initial_efficacy;
        else
            return compute(age_days) * initial_efficacy;
    }

    virtual double compute(double age_days) const = 0;

    /** Checkpoint a sampled copy, recreating it from this configured function. */
    void checkpoint_sample(const unique_ptr<DecayFunction>& sample, ostream& stream) const;
    void checkpoint_sample(unique_ptr<DecayFunction>& sample, istream& stream) const;

private:
    const Clock* clock;
    bool increasing;
    double initial_efficacy;
    std::unique_ptr<util::Sampler> het;

protected:
    SimTime round_to_ts_from_days(double days) const;

    virtual unique_ptr<DecayFunction> make_het_sample(double factor) const = 0;
    double het_factor = 0.0;
};

}

#endif
