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

#include "Clinical/ImmediateOutcomes.h"

#include <cmath>
#include "Host/WithinHost/WithinHostModel.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "util/random.h"

namespace OM::Clinical {
using namespace ::OM::util;

// ———  static, utility functions  ———

double get_health_system_acr_by_name(const scnXml::TreatmentDetails& td, const string& drug) {
    double val = NAN;
    if (drug == "CQ")
        val = td.getCQ().present() ? td.getCQ().get().getValue() : 0;
    else if (drug == "SP")
        val = td.getSP().present() ? td.getSP().get().getValue() : 0;
    else if (drug == "AQ")
        val = td.getAQ().present() ? td.getAQ().get().getValue() : 0;
    else if (drug == "SPAQ")
        val = td.getSPAQ().present() ? td.getSPAQ().get().getValue() : 0;
    else if (drug == "ACT")
        val = td.getACT().present() ? td.getACT().get().getValue() : 0;
    else if (drug == "QN")
        val = td.getQN().present() ? td.getQN().get().getValue() : 0;
    else if (drug == "selfTreatment")
        val = td.getSelfTreatment().getValue();
    else {
        throw util::xml_scenario_error("healthSystem.drugRegimen: firstLine / secondLine / inpatient has bad value");
    }

    if (!(val >= 0.0 && val <= 1.0)) {
        throw util::xml_scenario_error(string("healthSystem initialACR/compliance/nonCompliersEffective: ")
                                           .append(drug)
                                           .append(" value must be in range [0,1]"));
    }
    return val;
}

// note: "CQOptional" is the same type as for other drugs
const scnXml::TreatmentActions::CQOptional&
get_health_system_treatment_optional(const scnXml::TreatmentActions& treat_acts, const string& drug) {
    if (drug == "CQ")
        return treat_acts.getCQ();
    if (drug == "SP")
        return treat_acts.getSP();
    if (drug == "AQ")
        return treat_acts.getAQ();
    if (drug == "SPAQ")
        return treat_acts.getSPAQ();
    if (drug == "ACT")
        return treat_acts.getACT();
    if (drug == "QN")
        return treat_acts.getQN();
    throw util::xml_scenario_error("healthSystem.drugRegimen: firstLine / secondLine / inpatient has bad value");
}
WithinHost::TreatmentId get_health_system_treatment_by_name(const scnXml::TreatmentActions& treat_acts,
                                                            const string& drug,
                                                            WithinHost::WithinHostModel& within_host,
                                                            const interventions::InterventionManager& interventions,
                                                            const Clock& clock,
                                                            const util::CommandLineOptions& command_line) {
    const scnXml::TreatmentActions::CQOptional& elt = get_health_system_treatment_optional(treat_acts, drug);
    if (!elt.present())
        throw util::xml_scenario_error(string("healthSystem.treatmentActions: description required for ").append(drug));
    return within_host.treatments.add(elt.get(), interventions, clock, command_line);
}

// ———  static, init  ———

void ClinicalParams::set_immediate_outcomes(const scnXml::HSImmediateOutcomes& hs_description,
                                            WithinHost::WithinHostModel& within_host,
                                            const interventions::InterventionManager& interventions, const Clock& clock,
                                            const util::CommandLineOptions& command_line) {
    const string &first_line = hs_description.getDrugRegimen().getFirstLine(),
                 &second_line = hs_description.getDrugRegimen().getSecondLine(),
                 &inpatient = hs_description.getDrugRegimen().getInpatient();

    // ———  calculate probability of getting treatment  ———

    const double access_uc_official1 = hs_description.getPSeekOfficialCareUncomplicated1().getValue();
    const double access_uc_official2 = hs_description.getPSeekOfficialCareUncomplicated2().getValue();
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

    // ———  calculate probability of clearing parasites  ———

    const double compliance_first_line = get_health_system_acr_by_name(hs_description.getCompliance(), first_line);
    const double compliance_second_line = get_health_system_acr_by_name(hs_description.getCompliance(), second_line);

    const double cure_rate_first_line = get_health_system_acr_by_name(hs_description.getInitialACR(), first_line);
    const double cure_rate_second_line = get_health_system_acr_by_name(hs_description.getInitialACR(), second_line);

    const double non_compliers_effective_first_line =
        get_health_system_acr_by_name(hs_description.getNonCompliersEffective(), first_line);
    const double non_compliers_effective_second_line =
        get_health_system_acr_by_name(hs_description.getNonCompliersEffective(), second_line);

    const double compliance_self_treatment = hs_description.getCompliance().getSelfTreatment().getValue();
    const double cure_rate_self_treatment = hs_description.getInitialACR().getSelfTreatment().getValue();
    if (!(compliance_self_treatment >= 0.0 && compliance_self_treatment <= 1.0 && cure_rate_self_treatment >= 0.0 &&
          cure_rate_self_treatment <= 1.0)) {
        throw util::xml_scenario_error(
            "healthSystem initialACR/compliance/nonCompliersEffective: pSelfTreatment must be in range [0,1]");
    }

    immediate_outcomes.cure_rate_uc_official[FirstLine] =
        compliance_first_line * cure_rate_first_line + (1 - compliance_first_line) * non_compliers_effective_first_line;
    immediate_outcomes.cure_rate_uc_official[SecondLine] =
        compliance_second_line * cure_rate_second_line +
        (1 - compliance_second_line) * non_compliers_effective_second_line;
    immediate_outcomes.cure_rate_uc_self_treat[FirstLine] =
        compliance_self_treatment * cure_rate_self_treatment +
        (1 - compliance_self_treatment) * non_compliers_effective_first_line;
    immediate_outcomes.cure_rate_uc_self_treat[SecondLine] = 0.0; // as with access, this is a historical asymmetry

    five_day.cure_rate_severe = get_health_system_acr_by_name(hs_description.getInitialACR(), inpatient);

    immediate_outcomes.treatment_uc[FirstLine] = get_health_system_treatment_by_name(
        hs_description.getTreatmentActions(), first_line, within_host, interventions, clock, command_line);

    if (second_line == first_line)
        immediate_outcomes.treatment_uc[SecondLine] = immediate_outcomes.treatment_uc[FirstLine];
    else
        immediate_outcomes.treatment_uc[SecondLine] = get_health_system_treatment_by_name(
            hs_description.getTreatmentActions(), second_line, within_host, interventions, clock, command_line);

    if (inpatient == first_line)
        five_day.treatment_severe = immediate_outcomes.treatment_uc[FirstLine];
    else if (inpatient == second_line)
        five_day.treatment_severe = immediate_outcomes.treatment_uc[SecondLine];
    else
        five_day.treatment_severe = get_health_system_treatment_by_name(
            hs_description.getTreatmentActions(), inpatient, within_host, interventions, clock, command_line);

    immediate_outcomes.use_diagnostic_uc = hs_description.getUseDiagnosticUC();

    if (vivax_simple_model) {
        within_host.vivax.set_health_system(
            hs_description.getLiverStageDrug().present() ? &hs_description.getLiverStageDrug().get() : nullptr);
    } else if (hs_description.getLiverStageDrug().present()) {
        throw util::xml_scenario_error("health-system's liverStageDrug element only supported by vivax");
    }
}

// ———  per-human, update  ———

void ImmediateOutcomes::uncomplicated_event(Human& human, Episode::State pg_state) {
    const ImmediateOutcomesParams& io = params().immediate_outcomes;
    // If last treatment prescribed was in recent memory, consider second line.
    CaseType regimen = FirstLine;
    if (t_last_treatment + params().health_system_memory > params().clock().ts0()) {
        pg_state = Episode::State(pg_state | Episode::SecondCase);
        regimen = SecondLine;
    }

    latest_report.update(human, Episode::State(pg_state));

    const double x = human.clinical_rng.uniform_01();
    if (x < params().five_day.access_uc_any[regimen] * treatment_seeking_factor) {
        // UC1: official care OR self treatment
        // UC2: official care only

        if (io.use_diagnostic_uc) {
            mon::record_event(mon::measure("nTreatDiagnostics"), human);
            auto diag = params().diagnostics->monitoring_diagnostic();
            if (!human.within_host_model->diagnostic_result(human.clinical_rng, diag))
                return; // negative outcome: no treatment
        }

        t_last_treatment = params().clock().ts0();
        mon::record_event(TreatmentMeasures[regimen], human);

        const double p = (x < params().five_day.access_uc_self_treat[regimen] * treatment_seeking_factor)
                             ? io.cure_rate_uc_self_treat[regimen]
                             : io.cure_rate_uc_official[regimen];
        if (human.clinical_rng.bernoulli(p)) {
            // Could report Episode::Recovery to latestReport,
            // but we don't report out-of-hospital recoveries anyway.
            human.within_host_model->treatment(human, io.treatment_uc[regimen]);
        } else {
            // No change in parasitological status: treated outside of hospital
        }

        human.within_host_model->optional_pq_treatment(human);
    } else {
        // No change in parasitological status: non-treated
    }
}

}
