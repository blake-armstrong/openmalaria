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

#ifndef HMOD_SIMPLE_DECAYING_VALUE
#define HMOD_SIMPLE_DECAYING_VALUE

#include "Global.h"
#include "util/DecayFunction.h"

namespace OM::util {

/** Parameters to for a decaying intervention whose effect can be described
 * by a single scalar output.
 * 
 * This class outputs a scalar which defaults to and decays to 0, but
 * immediately after deployment will be the set _initial value_.
 *****************************************************************************/
class SimpleDecayingValue {
public:
    /** Default construction: always return 0. */
    SimpleDecayingValue() : deploy_t(Sim::never()) {}

    /** Configure from an XML element. */
    inline void set(double initial_value, const scnXml::DecayFunction& elt, const char* name, const Clock& clock,
                    const CommandLineOptions& command_line) {
        decay = DecayFunction::make_object(elt, name, clock, command_line);
        initial = initial_value;
    }

    /** Configure from an XML element. */
    inline void set(double initial_value) { initial = initial_value; }

    /** Trigger a deployment, if decay function was set. */
    inline void deploy(LocalRng& rng, SimTime time) {
        if (decay.get() == nullptr)
            return; // cannot deploy
        deploy_t = time;
        het = decay->het_sample(rng);
    }

    /** Get the value (0 if before any deployment or after complete decay,
     * also 0 if no decay function or initial value was set,
     * otherwise between zero and the inital value). */
    inline double current_value(SimTime time) const {
        if (decay.get() == nullptr || het == nullptr)
            return 0.0; // decay wasn't set: the always return 0
        else
            return initial * het->eval(time - deploy_t);
    }

    /** Checkpointing: only checkpoint parameters which change after initial
     * set-up. */
    template <class S> void operator&(S& stream) {
        if (decay)
            decay->checkpoint_sample(het, stream);
        deploy_t & stream;
    }

private:
    /** Description of decay of effects on emergence. */
    unique_ptr<util::DecayFunction> decay;

    /** Initial value. Is initialised to 0. */
    double initial{0.0};

    /** Description of larviciding decay. */
    unique_ptr<util::DecayFunction> het;

    /** Time of larviciding deployment. */
    SimTime deploy_t = Sim::never();
};

}

#endif
