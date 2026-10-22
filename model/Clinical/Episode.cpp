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

#include "Clinical/Episode.h"
#include "Host/Human.h"
#include "Host/WithinHost/WHInterface.h"

namespace OM {
namespace Clinical {

Episode::~Episode() { report(); }

void Episode::flush() {
    report();
    time = Sim::never();
}

void Episode::update(const Host::Human& human, Episode::State new_state) {
    bool to_report = false;

    if (health_system_memory_fix && (time + health_system_memory <= Sim::ts0()))
        to_report = true;
    else if (!health_system_memory_fix && (time + health_system_memory < Sim::ts0()))
        to_report = true;

    if (to_report) {
        infection_type = human.withinHostModel->infection_origin();

        report();

        time = Sim::ts0();
        survey_period = mon::eventSurveyNumber();
        age_group = human.monitoringAgeGroup;
        cohort_set = human.getCohortSet();
        state = new_state;
    } else {
        state = Episode::State(state | new_state);
    }
}

void Episode::report() {
    if (time < Sim::zero()) // Nothing to report
        return;

    // Reports malarial/non-malarial UC fever dependent on cause, not diagnosis.
    if (state & Episode::Malaria) {
        // Malarial fevers: report bout
        if (state & Episode::Complicated) {
            mon::recordEvent(mon::measure("nSevere"), survey_period, age_group, cohort_set);
            if (state & Episode::Severe) {
                mon::recordEvent(mon::measure("nSevereWithoutComorbidities"), survey_period, age_group, cohort_set);
            }
        } else { // UC or UC2
            mon::recordEvent(mon::measure("nUncomp"), survey_period, age_group, cohort_set);
            if (infection_type == WithinHost::InfectionOrigin::Indigenous)
                mon::recordEvent(mon::measure("nUncomp_Indigenous"), survey_period, age_group, cohort_set);
            else if (infection_type == WithinHost::InfectionOrigin::Introduced)
                mon::recordEvent(mon::measure("nUncomp_Introduced"), survey_period, age_group, cohort_set);
            else
                mon::recordEvent(mon::measure("nUncomp_Imported"), survey_period, age_group, cohort_set);
        }

        // Report outcomes of malarial fevers
        if (state & Episode::EventInHospital) {
            if (state & Episode::DirectDeath) {
                mon::recordEvent(mon::measure("nDirDeaths"), survey_period, age_group, cohort_set);
                mon::recordEvent(mon::measure("nHospitalDeaths"), survey_period, age_group, cohort_set);
                if (state & Episode::EventFirstDay) {
                    mon::recordEvent(mon::measure("Clinical_FirstDayDeaths"), survey_period, age_group, cohort_set);
                    mon::recordEvent(mon::measure("Clinical_HospitalFirstDayDeaths"), survey_period, age_group,
                                     cohort_set);
                }
            } else if (state & Episode::Sequelae) {
                mon::recordEvent(mon::measure("nSeq"), survey_period, age_group, cohort_set);
                mon::recordEvent(mon::measure("nHospitalSeqs"), survey_period, age_group, cohort_set);
            } else if (state & Episode::Recovery) {
                mon::recordEvent(mon::measure("nHospitalRecovs"), survey_period, age_group, cohort_set);
            }
        } else {
            if (state & Episode::DirectDeath) {
                mon::recordEvent(mon::measure("nDirDeaths"), survey_period, age_group, cohort_set);
                if (state & Episode::EventFirstDay) {
                    mon::recordEvent(mon::measure("Clinical_FirstDayDeaths"), survey_period, age_group, cohort_set);
                }
            } else if (state & Episode::Sequelae) {
                mon::recordEvent(mon::measure("nSeq"), survey_period, age_group, cohort_set);
            }
            // Don't care about out-of-hospital recoveries
        }
    } else if (state & Episode::Sick) {
        // Report non-malarial fever and outcomes
        mon::recordEvent(mon::measure("nNMFever"), survey_period, age_group, cohort_set);

        if (state & Episode::DirectDeath) {
            mon::recordEvent(mon::measure("nNmfDeaths"), survey_period, age_group, cohort_set);
        }
    }
}

void Episode::operator&(istream& stream) {
    time & stream;
    if (time > Sim::zero()) {
        survey_period & stream;
        age_group & stream;
        cohort_set & stream;
        int s;
        s & stream;
        state = Episode::State(s);
    }
}
void Episode::operator&(ostream& stream) {
    time & stream;
    if (time >= Sim::zero()) {
        survey_period & stream;
        age_group & stream;
        cohort_set & stream;
        state & stream;
    }
}

}
}
