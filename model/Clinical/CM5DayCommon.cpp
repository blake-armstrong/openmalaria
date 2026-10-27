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

#include "Clinical/DecisionTree5Day.h"
#include "Clinical/CMDecisionTree.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/WHVivax.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "util/random.h"

#include <array>

namespace OM::Clinical {
using namespace ::OM::util;

// ———  per-human, construction and destruction  ———

CM5DayCommon::CM5DayCommon(const ClinicalParams& params, double tsf, mon::MonitoringState& monitoring)
    : ClinicalModel(params, monitoring), treatment_seeking_factor(tsf) {}

// ———  per-human, update  ———

void CM5DayCommon::do_clinical_update(Human& human, double age_years, WithinHost::Pathogenesis::StatePair& pg) {
    const auto pg_state = static_cast<Episode::State>(pg.state);

    if (pg_state & Episode::Malaria) {
        if (pg_state & Episode::Complicated) {
            severe_malaria(human, pg_state, age_years, doomed);
        } else if (params().indirect_mort_bugfix || !pg.indirect_mortality) {
            // NOTE: the "not indirect mortality" bit is a historical accident.
            // Validity is debatable, but there's no point changing now.
            // (This does affect tests.)
            uncomplicated_event(human, pg_state);
        }
    } else if (pg_state & Episode::Sick) { // sick but not from malaria
        uncomplicated_event(human, pg_state);
    }

    if (pg.indirect_mortality && doomed == NotDoomed)
        doomed = -Sim::one_ts();

    if (t_last_treatment == Sim::ts0())
        human.remove_first_event(interventions::sub_pop_remove::OnFirstTreatment);

    if (pg_state & Episode::Sick)
        human.remove_first_event(interventions::sub_pop_remove::OnFirstBout);
}

void CM5DayCommon::severe_malaria(Human& human, Episode::State pg_state, double age_years, int& doomed) {
    const CM5DayParams& five_day = params().five_day;
    // Probability of getting treatment (only part which is case managment):
    const double p2 = five_day.access_severe * treatment_seeking_factor;
    // Probability of getting cured after getting treatment:
    const double p3 = five_day.cure_rate_severe;
    // p4 is the hospital case-fatality rate from Tanzania
    const double p4 = params().case_fatality_rate.eval(age_years);
    // p5a here is the community threshold case-fatality rate
    const double p5a = get_community_cfr(p4);
    // p5b here is the in-hospital treatment-failure case-fatality rate
    // In our code it was originally the community CFR, but in the published model description it should be the hospital CFR
    const double p5b = (five_day.cfr_pf_use_hospital) ? p4 : p5a;
    // p6 is P(seq) for treated patients
    const double p6 = params().p_sequelae_inpatient.eval(age_years);
    // p7 is P(seq) when parasites aren't cleared
    // note: if p7 were not equal p6, we might have to do something similar to p5b
    const double p7 = p6;

    std::array<double, 9> q{};
    // Community deaths
    q[0] = (1 - p2) * p5a;
    // Community sequelae
    q[1] = q[0] + (1 - p2) * (1 - p5a) * p7;
    // Community survival
    q[2] = q[1] + (1 - p2) * (1 - p5a) * (1 - p7);
    // In-hospital parasitological failure deaths
    q[3] = q[2] + p2 * (1 - p3) * p5b;
    // In-hospital parasitological failure sequelae
    q[4] = q[3] + p2 * (1 - p3) * (1 - p5b) * p7;
    // In-hospital parasitological failure survivors
    q[5] = q[4] + p2 * (1 - p3) * (1 - p5b) * (1 - p7);
    // In-hospital parasitological success deaths
    q[6] = q[5] + p2 * p3 * p4;
    // In-hospital parasitological success sequelae
    q[7] = q[6] + p2 * p3 * (1 - p4) * p6;
    // In-hospital parasitological success survival
    q[8] = q[7] + p2 * p3 * (1 - p4) * (1 - p6);

    // Expectation of death is:
    // double exDeath = (q[6] - q[5]) + (q[3] - q[2]) + q[0];
    // expanded, separating in-hospital and without (q[0]):
    const double ex_hospital_death = p2 * (p3 * p4 + (1 - p3) * p5b);
    const double ex_death = ex_hospital_death + (1 - p2) * p5a;
    mon::record_stat(mon::measure("expectedHospitalDeaths"), human, ex_hospital_death);
    mon::record_stat(mon::measure("expectedDirectDeaths"), human, ex_death);

    // Expectation of sequelae is:
    // double exSeq = (q[7] - q[6]) + (q[4] - q[3]) + (q[1] - q[0]);
    // expanded and simplified, noting that p7 == p6 :
    const double ex_seq = (p2 * (p3 * (1 - p4) + (1 - p3) * (1 - p5b)) + (1 - p2) * (1 - p5a)) * p6;
    mon::record_stat(mon::measure("expectedSequelae"), human, ex_seq);

    const double prandom = human.clinical_rng.uniform_01();

    //NOTE: we do not model diagnostics in this case
    if (prandom >= q[2]) { // treated in hospital
        t_last_treatment = Sim::ts0();
        mon::record_event(mon::measure("nTreatments3"), human);
        const auto state_treated = Episode::State(pg_state | Episode::EventInHospital);

        if (prandom >= q[5]) { // treatment successful at clearing parasites
            // this actually means successful treatment in this case:
            human.within_host_model->treatment(human, five_day.treatment_severe);

            if (prandom < q[6]) { // death (despite success in clearing parasites)
                latest_report.update(human, Episode::State(state_treated | Episode::DirectDeath));
                doomed = DoomedComplicated;
            } else if (prandom < q[7]) { // Patient recovers, but with sequelae (don't report full recovery)
                latest_report.update(human, Episode::State(state_treated | Episode::Sequelae));
            } else /*prandom < q[8]*/ { // patient recovers fully
                latest_report.update(human, Episode::State(state_treated | Episode::Recovery));
            }
        } else /*prandom < q[5]*/ { // Treated but parasites not cleared (in hospital)
            // No change in parasitological status among in-hospital patients

            if (prandom < q[3]) { // death
                latest_report.update(human, Episode::State(state_treated | Episode::DirectDeath));
                doomed = DoomedComplicated;
            } else if (prandom < q[4]) { // sequelae without parasite clearance
                latest_report.update(human, Episode::State(state_treated | Episode::Sequelae));
            } else /*prandom < q[5]*/ { // full recovery from episode
                latest_report.update(human, pg_state);
            }
        }
    } else /*prandom < q[2]*/ {
        // Not treated, thus no change in parasitological status

        if (prandom < q[0]) { // outcome 0: death in community
            latest_report.update(human, Episode::State(pg_state | Episode::DirectDeath));
            doomed = DoomedComplicated;
        } else if (prandom < q[1]) { // outcome 1: sequelae in community
            latest_report.update(human, Episode::State(pg_state | Episode::Sequelae));
        } else /*prandom < q[2]*/ { // outcome 2: full recovery from episode in community
            latest_report.update(human, pg_state);
        }
    }
}

// ———  per-human, intervention & checkpointing  ———

void CM5DayCommon::checkpoint(istream& stream) {
    ClinicalModel::checkpoint(stream);
    t_last_treatment & stream;
    treatment_seeking_factor & stream;
}
void CM5DayCommon::checkpoint(ostream& stream) {
    ClinicalModel::checkpoint(stream);
    t_last_treatment & stream;
    treatment_seeking_factor & stream;
}

}
