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

#ifndef HMOD_EPISODE
#define HMOD_EPISODE

#include "Global.h"
#include "Host/WithinHost/Pathogenesis/State.h"
#include "Host/WithinHost/Infection/Infection.h"
#include "mon/Monitoring.h"
#include <ostream>

namespace OM {
namespace Host {
class Human;
}
namespace Clinical {

/** Summary of clinical events during a caseManagementMemory period, in one individual.
 *
 * Terminology:
 * An "event" is an instantaeneous alteration of state.
 * A "bout" is a single fever or other sickness (falling sick to recovery).
 * An "episode" is a clinical-view of sickness caused by a malaria infection.
 * There's no reason an "episode" can't span multiple infections and multiple
 * bouts of sickness and recovery (the most severe is reported). */
class Episode {
public:
    /**
     * State description flags. Note that values are carefully engineered to
     * work with Pathogenesis::State values.
     */
    enum State {
        /* Values here are written in hexadecimal: http://en.wikipedia.org/wiki/Hexadecimal
        * Many are designed to be "flags", so the value corresponds to a single bit:
        * http://en.wikipedia.org/wiki/Flag_byte
        * Max: 0x80000000
        * (note & | ^ are C++'s binary AND, OR and XOR operators). */
        None = WithinHost::Pathogenesis::None, ///< Not sick

        // Flags for current state/worst state to report:
        Sick = WithinHost::Pathogenesis::Sick,       ///< Sick (may or may not be from malaria)
        Malaria = WithinHost::Pathogenesis::Malaria, ///< Malaria sickness
        /// Used by ClinicalEventScheduler to indicate a second bout of malarial sickness within the same episode (roughly)
        SecondCase = 0x10,
        Severe = 0x8,                                        ///< Severe malaria case
        Coinfection = 0x4,                                   ///< Malaria with a coinfection
        Complicated = WithinHost::Pathogenesis::Complicated, ///< Flag used to indicate Severe and/or Coinfection

        //NEED_ANTIBIOTIC     = 0x40,         ///< Flag indicates a non-malaria fever requires (antibiotic) treatment

        MorbidityMask = 0x7F, ///< Mask coving all above states

        // Flags for outcome reporting:
        EventInHospital =
            0x400, ///< Indicates recovery/sequelae/death event occurred in hospital − only set on one of these events (ImmediateOutcomes only)
        DirectDeath = 0x1000, ///< Used for reporting death (from Complicated sickness)
        Sequelae = 0x2000,    ///< Reporting recovered with sequelae (from Complicated sickness)
        Recovery = 0x4000,    ///< Report that individual fully recovered
        EventFirstDay =
            0x8000, ///< Used in combination with DirectDeath to report death happens on first day (before treatment has effect)
        RunCmTree = 0x10000, ///< Flag to indicate that CM tree should be run now or after a delay
    };

    Episode(SimTime health_system_memory, bool health_system_memory_fix)
        : time(Sim::never()), health_system_memory_fix(health_system_memory_fix),
          health_system_memory(health_system_memory) {}

    ~Episode();

    /// Report anything pending, as on destruction
    void flush();

    /** Report an episode, its severity, and any outcomes it entails.
     *
     * @param human The human whose info is being reported
     * @param newState The severity (diagnosis) and outcome.
     */
    void update(const Host::Human& human, Episode::State new_state);

    /// Checkpointing
    void operator&(istream& stream);
    void operator&(ostream& stream); ///< ditto

    /// Time of event, potentially never
    SimTime time = Sim::never();
    /// Survey during which the event occured
    size_t survey_period{mon::NOT_USED};
    /// Age group of the individual when the episode's first bout occurred
    size_t age_group{0};
    /// Cohort membership
    uint32_t cohort_set{0};
    /// Descriptor of state, containing reporting info. Not all information will
    /// be reported (e.g. indirect deaths are reported independantly).
    Episode::State state{None};

    WithinHost::InfectionOrigin infection_type = WithinHost::InfectionOrigin::Indigenous;

private:
    /** Report a clinical episode.
     *
     * From _state, an episode is reported based on severity (Sick,
     * Malaria or Complicated), and any outcomes are reported: Recovery (in
     * hospital, i.e. with EventInHospital, only), Sequelae and DirectDeath
     * (both in and out of hospital). */
    void report();

    bool health_system_memory_fix = false;
    SimTime health_system_memory = Sim::never();
};

}
}
#endif
