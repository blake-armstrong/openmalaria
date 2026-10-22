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

#ifndef HMOD_CM5_DAY_COMMON
#define HMOD_CM5_DAY_COMMON

#include "Host/WithinHost/Pathogenesis/State.h"
#include "Clinical/ClinicalModel.h"
#include "Host/Human.h"
#include "Host/WithinHost/WHInterface.h"
#include "mon/Monitoring.h"

#include <array>

namespace OM::Clinical {
using Host::Human;

/**
 * Common parts of 5-day case management models.
 */
class CM5DayCommon : public ClinicalModel {
public:
    /** Construct an instance (per-human).
     * 
     * @param tSF Treatment-seeking factor. Normally 1 but allows simple heterogeneity.
     */
    CM5DayCommon(const ClinicalParams& params, double tsf);

    bool is_existing_case() override {
        // If treated in the recent past:
        return Sim::now() > t_last_treatment && Sim::now() <= t_last_treatment + params().health_system_memory;
    }

protected:
    static constexpr std::array<mon::Measure, NumCaseTypes> TreatmentMeasures = {
        mon::measure("nTreatments1"), // first line official
        mon::measure("nTreatments2")  // second line official
    };

    void do_clinical_update(Human& human, double age_years, WithinHost::Pathogenesis::StatePair& pg) override;

    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

    /** Called when a non-severe/complicated malaria sickness occurs. */
    virtual void uncomplicated_event(Human& human, Episode::State pg_state) = 0;

    //! treatment seeking for heterogeneity
    double treatment_seeking_factor;

private:
    /** Called when a severe/complicated (with co-infection) malaria sickness occurs.
     *
     * Note: sets doomed = 4 if patient dies. */
    void severe_malaria(Human& human, Episode::State pg_state, double age_years, int& doomed);
};

}

#endif
