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
#include "util/ModelOptions.h"
#include "util/random.h"
#include "util/errors.h"

namespace OM {
namespace Clinical {
using namespace ::OM::util;

// ———  static, init  ———

void ClinicalParams::set_decision_tree_5day(const scnXml::HSDT5Day& hs_description) {
    double access_uc_official1 = hs_description.getPSeekOfficialCareUncomplicated1().getValue();
    double access_uc_official2 = hs_description.getPSeekOfficialCareUncomplicated2().getValue();
    // Note: this asymmetry is historical, and probably matters little:
    five_day.access_uc_self_treat[FirstLine] = hs_description.getPSelfTreatUncomplicated().getValue();
    five_day.access_uc_self_treat[SecondLine] = 0.0;
    five_day.access_uc_any[FirstLine] = access_uc_official1 + five_day.access_uc_self_treat[FirstLine];
    five_day.access_uc_any[SecondLine] = access_uc_official1 + five_day.access_uc_self_treat[SecondLine];
    five_day.access_severe = hs_description.getPSeekOfficialCareSevere().getValue();

    if (access_uc_official1 < 0.0 || five_day.access_uc_self_treat[FirstLine] < 0.0 ||
        five_day.access_uc_any[FirstLine] > 1.0 || access_uc_official2 < 0.0 ||
        five_day.access_uc_self_treat[SecondLine] < 0.0 || five_day.access_uc_any[SecondLine] > 1.0 ||
        five_day.access_severe < 0.0 || five_day.access_severe > 1.0) {
        throw util::xml_scenario_error("healthSystem: "
                                       "pSeekOfficialCareUncomplicated1 and pSelfTreatUncomplicated must be"
                                       " at least 0 and their sum must be at most 1, and"
                                       "pSeekOfficialCareUncomplicated2 and pSeekOfficialCareSevere must "
                                       "be in range [0,1]");
    }

    decision_tree_5day.tree_uc_official = &CMDecisionTree::create(hs_description.getTreeUCOfficial(), true);
    decision_tree_5day.tree_uc_self_treat = &CMDecisionTree::create(hs_description.getTreeUCSelfTreat(), true);

    five_day.cure_rate_severe = hs_description.getCureRateSevere().getValue();
    five_day.treatment_severe = WHInterface::add_treatment(hs_description.getTreatmentSevere());

    if (vivax_simple_model) {
        WithinHost::WHVivax::set_hs_parameters(
            hs_description.getLiverStageDrug().present() ? &hs_description.getLiverStageDrug().get() : 0);
    } else if (hs_description.getLiverStageDrug().present()) {
        throw util::xml_scenario_error("health-system's liverStageDrug element only supported by vivax");
    }
}

// ———  per-human, update  ———

void DecisionTree5Day::uncomplicated_event(Human& human, Episode::State pg_state) {
    // If last treatment prescribed was in recent memory, consider second line.
    CaseType regimen = FirstLine;
    if (t_last_treatment + params().health_system_memory > Sim::ts0()) {
        pg_state = Episode::State(pg_state | Episode::SecondCase);
        regimen = SecondLine;
    }

    latest_report.update(human, Episode::State(pg_state));

    double x = human.clinical_rng.uniform_01();
    if (x < params().five_day.access_uc_any[regimen] * treatment_seeking_factor) {
        CMHostData host_data(human, human.clinical_rng, Sim::in_years(human.age(Sim::ts0())), pg_state);

        // Run tree (which may deploy treatment)
        CMDTOut output = (x < params().five_day.access_uc_self_treat[regimen] * treatment_seeking_factor)
                             ? params().decision_tree_5day.tree_uc_self_treat->exec(host_data)
                             : params().decision_tree_5day.tree_uc_official->exec(host_data);

        if (output.treated) { // if any treatment or intervention deployed
            t_last_treatment = Sim::ts0();
            mon::recordEvent(measures[regimen], human);
        }
        if (output.screened) {
            mon::recordEvent(mon::measure("nTreatDiagnostics"), human);
        }

        human.withinHostModel->optional_pq_treatment(human);
    } else {
        // No care sought
    }
}

}
}
