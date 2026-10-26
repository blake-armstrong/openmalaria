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

namespace OM::Clinical {

Episode::~Episode() { report(); }

void Episode::flush() {
    report();
    time = Sim::never();
}

void Episode::update(const Host::Human& human, Episode::State new_state) {
    const bool to_report =
        health_system_memory_fix ? time + health_system_memory <= Sim::ts0() : time + health_system_memory < Sim::ts0();

    if (to_report) {
        infection_type = human.within_host_model->infection_origin();

        report();

        time = Sim::ts0();
        survey_period = mon::event_survey_number();
        age_group = human.monitoring_age_group;
        cohort_set = human.get_cohort_set();
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
            mon::record_event(mon::measure("nSevere"), survey_period, age_group, cohort_set);
            if (state & Episode::Severe) {
                mon::record_event(mon::measure("nSevereWithoutComorbidities"), survey_period, age_group, cohort_set);
            }
        } else { // UC or UC2
            mon::record_event(mon::measure("nUncomp"), survey_period, age_group, cohort_set);
            if (infection_type == WithinHost::InfectionOrigin::Indigenous)
                mon::record_event(mon::measure("nUncomp_Indigenous"), survey_period, age_group, cohort_set);
            else if (infection_type == WithinHost::InfectionOrigin::Introduced)
                mon::record_event(mon::measure("nUncomp_Introduced"), survey_period, age_group, cohort_set);
            else
                mon::record_event(mon::measure("nUncomp_Imported"), survey_period, age_group, cohort_set);
        }

        // Report outcomes of malarial fevers
        if (state & Episode::EventInHospital) {
            if (state & Episode::DirectDeath) {
                mon::record_event(mon::measure("nDirDeaths"), survey_period, age_group, cohort_set);
                mon::record_event(mon::measure("nHospitalDeaths"), survey_period, age_group, cohort_set);
                if (state & Episode::EventFirstDay) {
                    mon::record_event(mon::measure("Clinical_FirstDayDeaths"), survey_period, age_group, cohort_set);
                    mon::record_event(mon::measure("Clinical_HospitalFirstDayDeaths"), survey_period, age_group,
                                      cohort_set);
                }
            } else if (state & Episode::Sequelae) {
                mon::record_event(mon::measure("nSeq"), survey_period, age_group, cohort_set);
                mon::record_event(mon::measure("nHospitalSeqs"), survey_period, age_group, cohort_set);
            } else if (state & Episode::Recovery) {
                mon::record_event(mon::measure("nHospitalRecovs"), survey_period, age_group, cohort_set);
            }
        } else {
            if (state & Episode::DirectDeath) {
                mon::record_event(mon::measure("nDirDeaths"), survey_period, age_group, cohort_set);
                if (state & Episode::EventFirstDay) {
                    mon::record_event(mon::measure("Clinical_FirstDayDeaths"), survey_period, age_group, cohort_set);
                }
            } else if (state & Episode::Sequelae) {
                mon::record_event(mon::measure("nSeq"), survey_period, age_group, cohort_set);
            }
            // Don't care about out-of-hospital recoveries
        }
    } else if (state & Episode::Sick) {
        // Report non-malarial fever and outcomes
        mon::record_event(mon::measure("nNMFever"), survey_period, age_group, cohort_set);

        if (state & Episode::DirectDeath) {
            mon::record_event(mon::measure("nNmfDeaths"), survey_period, age_group, cohort_set);
        }
    }
}

void Episode::operator&(istream& stream) {
    time & stream;
    if (time > Sim::zero()) {
        survey_period & stream;
        age_group & stream;
        cohort_set & stream;
        int s = 0;
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
