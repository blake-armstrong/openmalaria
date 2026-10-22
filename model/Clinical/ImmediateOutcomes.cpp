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
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/WHVivax.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "util/random.h"

namespace OM {
namespace Clinical {
using namespace ::OM::util;

// ———  static, utility functions  ———

double getHealthSystemACRByName(const scnXml::TreatmentDetails& td, const string& drug) {
    double val;
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
const scnXml::TreatmentActions::CQOptional& getHealthSystemTreatmentOptional(const scnXml::TreatmentActions& treatActs,
                                                                             const string& drug) {
    if (drug == "CQ")
        return treatActs.getCQ();
    if (drug == "SP")
        return treatActs.getSP();
    if (drug == "AQ")
        return treatActs.getAQ();
    if (drug == "SPAQ")
        return treatActs.getSPAQ();
    if (drug == "ACT")
        return treatActs.getACT();
    if (drug == "QN")
        return treatActs.getQN();
    throw util::xml_scenario_error("healthSystem.drugRegimen: firstLine / secondLine / inpatient has bad value");
}
WithinHost::TreatmentId getHealthSystemTreatmentByName(const scnXml::TreatmentActions& treatActs, const string& drug) {
    const scnXml::TreatmentActions::CQOptional& elt = getHealthSystemTreatmentOptional(treatActs, drug);
    if (!elt.present())
        throw util::xml_scenario_error(string("healthSystem.treatmentActions: description required for ").append(drug));
    return WithinHost::WHInterface::add_treatment(elt.get());
}

// ———  static, init  ———

void ClinicalParams::set_immediate_outcomes(const scnXml::HSImmediateOutcomes& hsDescription) {
    const string &firstLine = hsDescription.getDrugRegimen().getFirstLine(),
                 &secondLine = hsDescription.getDrugRegimen().getSecondLine(),
                 &inpatient = hsDescription.getDrugRegimen().getInpatient();

    // ———  calculate probability of getting treatment  ———

    double accessUCOfficial1 = hsDescription.getPSeekOfficialCareUncomplicated1().getValue();
    double accessUCOfficial2 = hsDescription.getPSeekOfficialCareUncomplicated2().getValue();
    // Note: this asymmetry is historical, and probably matters little:
    five_day.access_uc_self_treat[FirstLine] = hsDescription.getPSelfTreatUncomplicated().getValue();
    five_day.access_uc_self_treat[SecondLine] = 0.0;
    five_day.access_uc_any[FirstLine] = accessUCOfficial1 + five_day.access_uc_self_treat[FirstLine];
    five_day.access_uc_any[SecondLine] = accessUCOfficial1 + five_day.access_uc_self_treat[SecondLine];
    five_day.access_severe = hsDescription.getPSeekOfficialCareSevere().getValue();

    if (accessUCOfficial1 < 0.0 || five_day.access_uc_self_treat[FirstLine] < 0.0 ||
        five_day.access_uc_any[FirstLine] > 1.0 || accessUCOfficial2 < 0.0 ||
        five_day.access_uc_self_treat[SecondLine] < 0.0 || five_day.access_uc_any[SecondLine] > 1.0 ||
        five_day.access_severe < 0.0 || five_day.access_severe > 1.0) {
        throw util::xml_scenario_error("healthSystem: "
                                       "pSeekOfficialCareUncomplicated1 and pSelfTreatUncomplicated must be"
                                       " at least 0 and their sum must be at most 1, and"
                                       "pSeekOfficialCareUncomplicated2 and pSeekOfficialCareSevere must "
                                       "be in range [0,1]");
    }

    // ———  calculate probability of clearing parasites  ———

    const double complianceFirstLine = getHealthSystemACRByName(hsDescription.getCompliance(), firstLine);
    const double complianceSecondLine = getHealthSystemACRByName(hsDescription.getCompliance(), secondLine);

    const double cureRateFirstLine = getHealthSystemACRByName(hsDescription.getInitialACR(), firstLine);
    const double cureRateSecondLine = getHealthSystemACRByName(hsDescription.getInitialACR(), secondLine);

    const double nonCompliersEffectiveFirstLine =
        getHealthSystemACRByName(hsDescription.getNonCompliersEffective(), firstLine);
    const double nonCompliersEffectiveSecondLine =
        getHealthSystemACRByName(hsDescription.getNonCompliersEffective(), secondLine);

    const double complianceSelfTreatment = hsDescription.getCompliance().getSelfTreatment().getValue();
    const double cureRateSelfTreatment = hsDescription.getInitialACR().getSelfTreatment().getValue();
    if (!(complianceSelfTreatment >= 0.0 && complianceSelfTreatment <= 1.0 && cureRateSelfTreatment >= 0.0 &&
          cureRateSelfTreatment <= 1.0)) {
        throw util::xml_scenario_error(
            "healthSystem initialACR/compliance/nonCompliersEffective: pSelfTreatment must be in range [0,1]");
    }

    immediate_outcomes.cure_rate_uc_official[FirstLine] =
        complianceFirstLine * cureRateFirstLine + (1 - complianceFirstLine) * nonCompliersEffectiveFirstLine;
    immediate_outcomes.cure_rate_uc_official[SecondLine] =
        complianceSecondLine * cureRateSecondLine + (1 - complianceSecondLine) * nonCompliersEffectiveSecondLine;
    immediate_outcomes.cure_rate_uc_self_treat[FirstLine] =
        complianceSelfTreatment * cureRateSelfTreatment +
        (1 - complianceSelfTreatment) * nonCompliersEffectiveFirstLine;
    immediate_outcomes.cure_rate_uc_self_treat[SecondLine] = 0.0; // as with access, this is a historical asymmetry

    five_day.cure_rate_severe = getHealthSystemACRByName(hsDescription.getInitialACR(), inpatient);

    immediate_outcomes.treatment_uc[FirstLine] =
        getHealthSystemTreatmentByName(hsDescription.getTreatmentActions(), firstLine);

    if (secondLine == firstLine)
        immediate_outcomes.treatment_uc[SecondLine] = immediate_outcomes.treatment_uc[FirstLine];
    else
        immediate_outcomes.treatment_uc[SecondLine] =
            getHealthSystemTreatmentByName(hsDescription.getTreatmentActions(), secondLine);

    if (inpatient == firstLine)
        five_day.treatment_severe = immediate_outcomes.treatment_uc[FirstLine];
    else if (inpatient == secondLine)
        five_day.treatment_severe = immediate_outcomes.treatment_uc[SecondLine];
    else
        five_day.treatment_severe = getHealthSystemTreatmentByName(hsDescription.getTreatmentActions(), inpatient);

    immediate_outcomes.use_diagnostic_uc = hsDescription.getUseDiagnosticUC();

    if (vivax_simple_model) {
        WithinHost::WHVivax::set_hs_parameters(
            hsDescription.getLiverStageDrug().present() ? &hsDescription.getLiverStageDrug().get() : 0);
    } else if (hsDescription.getLiverStageDrug().present()) {
        throw util::xml_scenario_error("health-system's liverStageDrug element only supported by vivax");
    }
}

// ———  per-human, update  ———

void ImmediateOutcomes::uncomplicatedEvent(Human& human, Episode::State pgState) {
    const ImmediateOutcomesParams& io = params().immediate_outcomes;
    // If last treatment prescribed was in recent memory, consider second line.
    CaseType regimen = FirstLine;
    if (m_tLastTreatment + params().health_system_memory > Sim::ts0()) {
        pgState = Episode::State(pgState | Episode::SECOND_CASE);
        regimen = SecondLine;
    }

    latestReport.update(human, Episode::State(pgState));

    double x = human.clinical_rng.uniform_01();
    if (x < params().five_day.access_uc_any[regimen] * m_treatmentSeekingFactor) {
        // UC1: official care OR self treatment
        // UC2: official care only

        if (io.use_diagnostic_uc) {
            mon::recordEvent(mon::measure("nTreatDiagnostics"), human);
            auto diag = WithinHost::Diagnostics::monitoring_diagnostic();
            if (!human.withinHostModel->diagnostic_result(human.clinical_rng, diag))
                return; // negative outcome: no treatment
        }

        m_tLastTreatment = Sim::ts0();
        mon::recordEvent(measures[regimen], human);

        double p = (x < params().five_day.access_uc_self_treat[regimen] * m_treatmentSeekingFactor)
                       ? io.cure_rate_uc_self_treat[regimen]
                       : io.cure_rate_uc_official[regimen];
        if (human.clinical_rng.bernoulli(p)) {
            // Could report Episode::RECOVERY to latestReport,
            // but we don't report out-of-hospital recoveries anyway.
            human.withinHostModel->treatment(human, io.treatment_uc[regimen]);
        } else {
            // No change in parasitological status: treated outside of hospital
        }

        human.withinHostModel->optional_pq_treatment(human);
    } else {
        // No change in parasitological status: non-treated
    }
}

}
}
