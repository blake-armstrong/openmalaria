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

#ifndef HMOD_COMMON_INFECTION
#define HMOD_COMMON_INFECTION

#include "Host/WithinHost/Infection/Infection.h"
#include "util/random.h"

namespace OM::WithinHost {

using util::LocalRng;

/** Represent infections used by CommonWithinHost.
 * 
 * All these use a 1-day time step, however CommonWithinHost handles the
 * conversion when the main simulation uses a 5 day time step by updating
 * infections and the PK-PD model multiple times.
 * 
 * Note therefore that sim::ts0(), ts1(), etc. may not always be accurate since
 * it is only updated once per main time step. */
class CommonInfection : public Infection {
public:
    /// @brief Construction and destruction
    //@{
    /// For checkpointing (don't use for anything else)
    CommonInfection(SimTime latent_p, istream& stream) : Infection(latent_p, stream) {}
    /// Per instance initialisation; create new inf.
    CommonInfection(SimTime latent_p, uint32_t genotype, int origin) : Infection(latent_p, genotype, origin) {}
    ~CommonInfection() override;
    //@}

    /** Update: calculate new density. Call this once per day.
     * 
     * @param survivalFactor Density multiplier to introduce drug & vaccine effects
     * @param now The simulation time. Use this instead of sim::ts1().
     * @param body_mass Body mass of host in kg
     * @returns True when the infection goes extinct. */
    inline bool update(LocalRng& rng, double survival_factor, SimTime now, double body_mass) {
        SimTime bs_age = now - start_date - latent_period(); // age of post-latent-period blood stage
        if (bs_age < Sim::zero())
            return false; // latent period (liver stage) — don't do anything
        else
            return update_density(rng, survival_factor, bs_age, body_mass);
    }

    map<size_t, double> kn; // IC50^slope per drug type, if sampled

protected:
    /** Update: calculate new density.
     *
     * @param survivalFactor Density multiplier to introduce drug & vaccine
     *   effects
     * @param bsAge Age of the patent blood-stage infection (sim::zero() on
     *  first day). Note that liver and pre-patent blood stages occur before
     *  this, but this function is not called during those stages.
     * @param body_mass Body mass of host in kg
     * @returns True when the infection goes extinct. */
    virtual bool update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double body_mass) = 0;

    void checkpoint(ostream& stream) override;
};

}

#endif
