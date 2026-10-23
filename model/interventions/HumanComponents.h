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

#ifndef HMOD_INTERVENTIONS_HUMAN_COMPONENTS
#define HMOD_INTERVENTIONS_HUMAN_COMPONENTS

#include "Global.h"
#include "interventions/Interfaces.h"
#include "util/sampler.h"
#include "util/DecayFunction.h"

namespace OM {
namespace Host {
class Human;
}
namespace interventions {
class VaccineComponent;
using util::LocalRng;

// ———  sub-population removal  ———

namespace sub_pop_remove {
/** Identifiers for types of sub-population removal option, followed by NUM
     * in last place (as a counter).
     * 
     * ON_FIRST_BOUT: remove humans from some sub-populations at the start of
     * their first clinical events following inclusion in the sub-population.
     * 
     * ON_FIRST_INFECTION: remove patent humans from some sub-population(s)
     * during each survey.
     * 
     * ON_FIRST_TREATMENT: remove humans from some sub-populations when
     * assigning each course of treatment.
     */
enum RemoveAtCode { OnFirstBout, OnFirstInfection, OnFirstTreatment, NUM };
}

// ———  vaccines  ———

namespace vaccines {
enum Types { PEV, BSV, TBV, NumVaccineTypes };
}

/** Per vaccine effect (type), per human details. */
class PerEffectPerHumanVaccine {
public:
    //Note: this constructor is only for checkpointing
    PerEffectPerHumanVaccine();

    /// Checkpointing
    template <class S> void operator&(S& stream) {
        component & stream;
        num_doses_administered & stream;
        time_last_deployment & stream;
        per_genotype_initial_efficacy & stream;
        checkpointHet(stream);
    }

private:
    PerEffectPerHumanVaccine(LocalRng& rng, const VaccineComponent& params);

    ComponentId component; // id of component (for finding parameters)
    // Type of vaccine
    /** Number of vaccine doses this individual has received.
     *
     * If an individual misses one EPI (continuous) vaccine dose, it's
     * intentional that they also miss following EPI doses (unless a timed mass
     * vaccination reintroduces them to the EPI schedule). */
    uint32_t num_doses_administered;
    /// Time of last vaccination with this vaccine type
    SimTime time_last_deployment = Sim::never();
    /// Efficacy at last deployment (undecayed)
    vector<double> per_genotype_initial_efficacy;
    unique_ptr<util::DecayFunction> het_sample;
    const VaccineComponent* params = nullptr;

    void checkpointHet(ostream& stream);
    void checkpointHet(istream& stream);

    friend class PerHumanVaccine;
};

/** Per-human vaccine code. */
class PerHumanVaccine {
public:
    PerHumanVaccine() = default;

    /** Get one minus the efficacy of the vaccine (1 for no effect, 0 for full effect). */
    double get_factor(vaccines::Types type, uint32_t genotype = 0) const;

    /** Vaccinate unless the passed VaccineLimits specify not to.
     * 
     * @returns true when the vaccine is administered */
    bool possibly_vaccinate(Host::Human& human, const VaccineComponent& params,
                            interventions::VaccineLimits vacc_limits);

#if 0
    /// Hack for R_0 experiment: make current human the infection source
    inline void specialR_0(){
        // At this point special TBV should have been given to everyone but the PEV to no-one
        effects[R_0_PEV].initialEfficacy = 1.0;
        effects[R_0_TBV].initialEfficacy = 0.0;
    }
#endif

    /// Checkpointing
    template <class S> void operator&(S& stream) { effects & stream; }

private:
    /// Details for each deployed vaccine for this human
    using EffectList = std::vector<PerEffectPerHumanVaccine>;
    EffectList effects;
};

}
}
#endif
