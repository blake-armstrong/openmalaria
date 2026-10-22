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

#ifndef Hmod_ClinicalEventSchduler
#define Hmod_ClinicalEventSchduler

#include "Global.h"
#include "Clinical/ClinicalModel.h"
#include "Clinical/ESCaseManagement.h"

#include <list>

namespace OM {
namespace Clinical {

/** Tracks clinical status (sickness), triggers case management for new events,
 * medicates treatment, determines patient recovery, death and sequelae.
 *
 * Note: there are several variables that only need to be used during a
 * bout. It's possible that memory usage could be reduced by storing them
 * externally in a temporary object during episodes (but unlikely worth doing).
 */
class ClinicalEventScheduler : public ClinicalModel {
public:
    ClinicalEventScheduler(const ClinicalParams& params, double tSF);

    virtual bool isExistingCase();

protected:
    virtual void doClinicalUpdate(Human& human, double ageYears, WithinHost::Pathogenesis::StatePair& pg);

    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);

private:
    // Note on memory usage: Pathogenesis::State is and enum (an int), so we
    // have a vtable followed by 3 ints, a double and a list. Alignment probably
    // wastes some space.
    /// Current state of sickness
    Episode::State pgState;

    /** Set to when a bout should start. If sim::ts0() equals this, a bout
     * is started (UC & severe behaviour different).
     *
     * Note: medications are not delayed by this. */
    SimTime caseStartTime = Sim::never();

    /** The individual recovers when sim::ts0() >= timeOfRecovery,
     * assuming they didn't die. */
    SimTime timeOfRecovery = Sim::never();

    /// Time at which last treatment was recieved (for second-case considerations).
    SimTime timeLastTreatment = Sim::never();

    /// Total parasite density at last time step (used during a bout).
    double previousDensity;
};

}
}
#endif
