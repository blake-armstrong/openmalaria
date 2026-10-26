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

#ifndef HMOD_HUMAN
#define HMOD_HUMAN
#include "Global.h"
#include "Transmission/PerHost.h"
#include "InfectionIncidenceModel.h"
#include "interventions/HumanComponents.h"
#include "util/checkpoint_containers.h"
#include <map>

namespace OM {
namespace Clinical {
class ClinicalModel;
class InfantMortality;
struct ClinicalParams;
}
namespace WithinHost {
class WHInterface;
struct WithinHostModel;
}
namespace Transmission {
class TransmissionModel;
}
namespace util {
class ModelOptionFlags;
}

namespace Host {
class NeonatalMortality;

struct HumanParams {
    const Transmission::PerHostParams* per_host = nullptr;
    const InfectionIncidenceParams* infection_incidence = nullptr;
    const util::ModelOptionFlags* options = nullptr;
    const WithinHost::WithinHostModel* within_host = nullptr;
    const Clinical::ClinicalParams* clinical = nullptr;
    const interventions::InterventionManager* interventions = nullptr;
};

/** Interface to all sub-models storing data per-human individual.
 *
 * Still contains some data, but most is now contained in sub-models. */
class Human {
public:
    /** Initialise all variables of a human datatype.
    * 
    * @param date_of_birth date of birth (usually start of next time step) */
    Human(SimTime date_of_birth, const HumanParams& params, util::MasterRng& rng);

    /// Allow move construction
    Human(Human&&) = default;
    Human& operator=(Human&&) = default;

    /// Disable copying
    Human(const Human&) = delete;
    Human& operator=(const Human&) = delete;

    /** Get human's age with respect to some time. */
    inline SimTime age(SimTime time) const;

    /** Return true if human is a member of the sub-population. */
    inline bool is_in_sub_pop(interventions::ComponentId id) const;

    inline uint32_t get_cohort_set() const;

    /** Return date of birth */
    SimTime get_dob() const;

    /** Return true if dead, false otherwise */
    bool is_dead() const;

    /** After this call, isDead() will be true */
    void kill();

    /** Add the human to an intervention component's sub-population for the given duration. 
     * A duration of zero implies no effect. */
    void add_to_cohort(interventions::ComponentId id, SimTime duration);

    /** Remove the human from an intervention component's sub-population */
    void remove_from_cohort(interventions::ComponentId id);

    /** Act on "remove from sub-population on first ..." events.
     * This is only for use during a human update. */
    void remove_first_event(interventions::sub_pop_remove::RemoveAtCode code);

    /** Remove host from expired cohorts */
    void update_cohort_set();

    /** Checkpoint (read) */
    void checkpoint(istream& stream);

    /** Checkpoint (write) */
    void checkpoint(ostream& stream);

    /** Contains per-species vector data (VectorModel only). */
    Transmission::PerHost per_host_transmission;

    /** The WithinHostModel models parasite density and immunity */
    unique_ptr<WithinHost::WHInterface> within_host_model;

    /** The InfectionIncidenceModel translates per-host EIR into new infections */
    unique_ptr<InfectionIncidenceModel> inf_incidence;

    /** The ClinicalModel encapsulates pathogenesis (sickness status),
    * case management (medicating drugs)
    * and clinical outcomes (morbidity, reporting). */
    unique_ptr<Clinical::ClinicalModel> clinical_model;

    util::LocalRng host_rng;         // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    util::LocalRng clinical_rng;     // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    util::LocalRng intervention_rng; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
    util::LocalRng monitoring_rng;   // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

    /** Vaccines */
    interventions::PerHumanVaccine vaccine;

    /** Made persistant to save a lookup each time step (significant performance improvement) */
    size_t monitoring_age_group = 0;

    /** The next continuous distribution in the series */
    uint32_t next_cts_dist = 0;

private:
    SimTime dob = Sim::never(); // date of birth; humans are always born at the end of a time step
    const interventions::InterventionManager* intervention_manager;

    /** Cache, updated when human is added to or removed from a sub-population */
    uint32_t cohort_set = 0;

    /** The state of he human. A human cannot be revived. */
    bool dead = false;

    /** This lists sub-populations of which the human is a member together with
    * expiry time.
    * 
    * Definition: a human is in sub-population p if subPopExp.contains(p) and,
    * for t=subPopExp[p], t > sim::now() (at the time of intervention
    * deployment) or t > sim::ts0() (equiv t >= sim::ts1()) during human update.
    * 
    * NOTE: this discrepancy is because intervention deployment effectively
    * happens at the end of a time step and we want a duration of 1 time step to
    * mean 1 intervention deployment (that where the human becomes a member) and
    * 1 human update (the next). */
    //TODO(optimisation): it might be better to instead store for each
    // ComponentId of interest the set of humans who are members
    std::map<interventions::ComponentId, SimTime> sub_pop_exp;
};

void summarize(Human& human, bool survey_only_new_ep);

void update(Human& human, Transmission::TransmissionModel& transmission, const NeonatalMortality& neonatal_mortality,
            Clinical::InfantMortality& infant_mortality, NewInfections& new_infections);

inline SimTime Human::age(SimTime time) const { return time - dob; }

inline bool Human::is_in_sub_pop(interventions::ComponentId id) const {
    auto it = sub_pop_exp.find(id);
    if (it == sub_pop_exp.end())
        return false; // no history of membership
    else
        return it->second >= Sim::now_or_ts0(); // added: has expired?
}

inline uint32_t Human::get_cohort_set() const { return cohort_set; }

inline SimTime Human::get_dob() const { return dob; }

inline bool Human::is_dead() const { return dead; }

inline void Human::kill() { dead = true; }

}
}
#endif
