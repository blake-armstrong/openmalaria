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

// Note: this file is included by exactly one source (Interventions.cpp)
// and contains definitions as well as declarations.

// The includes here are more for documentation than required.
#include "interventions/Interfaces.h"
#include "interventions/InterventionManager.h"
#include "util/ModelOptions.h"
#include "Clinical/ESCaseManagement.h"
#include "Clinical/ImmediateOutcomes.h"
#include "Host/Human.h"
#include "Transmission/TransmissionModel.h"
#include "Host/WithinHost/Diagnostic.h"
#include "PkPd/PkPdParams.h"
#include "mon/Monitoring.h"
#include "util/random.h"
#include <schema/healthSystem.h>
#include <schema/interventions.h>

#include <algorithm>

namespace OM::interventions {
using Host::Human;
using WithinHost::Diagnostic;

// ———  HumanIntervention  ———

bool component_cmp(const HumanInterventionComponent* a, const HumanInterventionComponent* b) {
    return a->component_type() < b->component_type();
}
HumanIntervention::HumanIntervention(const xsd::cxx::tree::sequence<scnXml::Component>& component_list,
                                     const InterventionManager& manager) {
    components.reserve(component_list.size());
    for (const auto& it : component_list) {
        const ComponentId id = manager.get_component_id(it.getId());
        const HumanInterventionComponent* component = &manager.get_component(id);
        components.push_back(component);
    }

    /** Sort components according to a standard order.
     * 
     * The point of this is to make results repeatable even when users change
     * the ordering of a list of intervention's components (since getting
     * repeatable results out of OpenMalaria is often a headache anyway, we
     * might as well at least remove this hurdle).
     * 
     * Note that when multiple interventions are deployed simultaneously, the
     * order of their deployments is still dependent on the order in the XML
     * file. */
    std::ranges::stable_sort(components, component_cmp);
}
HumanIntervention::HumanIntervention(const xsd::cxx::tree::sequence<scnXml::Component>& component_list,
                                     const xsd::cxx::tree::sequence<scnXml::Condition>& condition_list,
                                     const InterventionManager& manager) {
    components.reserve(component_list.size());
    for (const auto& it : component_list) {
        const ComponentId id = manager.get_component_id(it.getId());
        const HumanInterventionComponent* component = &manager.get_component(id);
        components.push_back(component);
    }

    /** Sort components according to a standard order.
     * 
     * The point of this is to make results repeatable even when users change
     * the ordering of a list of intervention's components (since getting
     * repeatable results out of OpenMalaria is often a headache anyway, we
     * might as well at least remove this hurdle).
     * 
     * Note that when multiple interventions are deployed simultaneously, the
     * order of their deployments is still dependent on the order in the XML
     * file. */
    std::ranges::stable_sort(components, component_cmp);

    for (const auto& it : condition_list) {
        const double min = it.getMinValue().present() ? it.getMinValue().get() : -numeric_limits<double>::infinity();
        const double max = it.getMaxValue().present() ? it.getMaxValue().get() : numeric_limits<double>::infinity();
        conditions.push_back(mon::setup_condition(it.getMeasure(), min, max, it.getInitialState()));
    }
}
HumanIntervention::HumanIntervention(const xsd::cxx::tree::sequence<scnXml::DTDeploy>& component_list,
                                     const InterventionManager& manager) {
    components.reserve(component_list.size());
    for (const auto& it : component_list) {
        const ComponentId id = manager.get_component_id(it.getComponent());
        const HumanInterventionComponent* component = &manager.get_component(id);
        components.push_back(component);
    }

    /** Sort components according to a standard order.
     * 
     * The point of this is to make results repeatable even when users change
     * the ordering of a list of intervention's components (since getting
     * repeatable results out of OpenMalaria is often a headache anyway, we
     * might as well at least remove this hurdle).
     * 
     * Note that when multiple interventions are deployed simultaneously, the
     * order of their deployments is still dependent on the order in the XML
     * file. */
    std::ranges::stable_sort(components, component_cmp);
}

void HumanIntervention::deploy(Human& human, mon::DeployMethod method, VaccineLimits vacc_limits) const {
    for (size_t cond : conditions) {
        // Abort deployment if any condition is false.
        if (!mon::check_condition(cond))
            return;
    }

    for (auto it : components) {
        const interventions::HumanInterventionComponent& component = *it;
        // we must report first, since it can change cohort and sub-population
        // which may affect what deployment does (at least in the case of reporting deployments)
        human.addToCohort(component.id(), component.duration());
        component.deploy(human, method, vacc_limits);
    }
}

void HumanIntervention::print_details(std::ostream& out) const {
    out << "human:";
    for (auto component : components) {
        out << '\t' << component->id().id;
    }
}

// ———  Utilities  ———

TriggeredDeployments::TriggeredDeployments(const scnXml::TriggeredDeployments& elt,
                                           const InterventionManager& manager) {
    lists.reserve(elt.getDeploy().size());
    for (const auto& it : elt.getDeploy()) {
        lists.emplace_back(it, manager);
    }
}
void TriggeredDeployments::deploy(Human& human, mon::DeployMethod method, VaccineLimits vacc_limits) const {
    for (const auto& list : lists) {
        list.deploy(human, method, vacc_limits);
    }
}

TriggeredDeployments::SubList::SubList(const scnXml::TriggeredDeployments::DeployType& elt,
                                       const InterventionManager& manager)
    : HumanIntervention(elt.getComponent(), manager), min_age(Sim::from_years_n(elt.getMinAge())),
      max_age(Sim::future()), coverage(elt.getP()) {
    if (elt.getMaxAge().present())
        max_age = Sim::from_years_n(elt.getMaxAge().get());

    if (min_age < Sim::zero() || max_age < min_age) {
        throw util::xml_scenario_error("triggered intervention must have 0 <= minAge <= maxAge");
    }

    if (coverage < 0.0 || coverage > 1.0) {
        throw util::xml_scenario_error("triggered intervention must have 0 <= coverage <= 1");
    }

    // Zero coverage: optimise
    if (coverage <= 0.0 || min_age >= max_age)
        components.clear();
}
void TriggeredDeployments::SubList::deploy(Host::Human& human, mon::DeployMethod method,
                                           VaccineLimits vacc_limits) const {
    // This may be used within a time step; in that case we use human age at
    // the beginning of the step since this gives the expected result with age
    // limits of 0 to 1 time step.
    //TODO: why does the above contradict what we do? Is this due to a date being shifted later?
    const SimTime age = human.age(Sim::now_or_ts1());
    if (age >= min_age && age < max_age) {
        if (coverage >= 1.0 || human.intervention_rng.bernoulli(coverage)) {
            HumanIntervention::deploy(human, method, vacc_limits);
        }
    }
}

// ———  Derivatives of HumanInterventionComponent  ———

class RecruitmentOnlyComponent : public HumanInterventionComponent {
public:
    RecruitmentOnlyComponent(ComponentId id) : HumanInterventionComponent(id) {}
    ~RecruitmentOnlyComponent() override = default;

    /// Reports to monitoring, nothing else
    void deploy(Host::Human& human, mon::DeployMethod method, VaccineLimits) const override {
        mon::record_deploy(mon::measure("nMassRecruitOnly"), mon::measure("nCtsRecruitOnly"), human, method);
    }

    component_types::Type component_type() const override { return component_types::RecruitOnly; }

    void print_details(std::ostream& out) const override { out << id().id << "\tRecruit only"; }
};

class ScreenComponent : public HumanInterventionComponent {
public:
    ScreenComponent(ComponentId id, const scnXml::Screen& elt, const InterventionManager& manager,
                    const WithinHost::DiagnosticRegistry& diagnostics)
        : HumanInterventionComponent(id), diagnostic(diagnostics.get(elt.getDiagnostic())),
          positive(elt.getPositive(), manager), negative(elt.getNegative(), manager) {}

    void deploy(Human& human, mon::DeployMethod method, VaccineLimits vacc_limits) const override {
        mon::record_deploy(mon::measure("nMassScreenings"), mon::measure("nCtsScreenings"), human, method);
        if (human.withinHostModel->diagnostic_result(human.intervention_rng, diagnostic)) {
            positive.deploy(human, method, vacc_limits);
        } else {
            negative.deploy(human, method, vacc_limits);
        }
    }

    component_types::Type component_type() const override { return component_types::SCREEN; }

    void print_details(std::ostream& out) const override { out << id().id << "\tScreen"; }

private:
    const Diagnostic& diagnostic;
    HumanIntervention positive, negative;
};

/// Simple treatment: no PK/PD, just remove parasites
class TreatSimpleComponent : public HumanInterventionComponent {
public:
    TreatSimpleComponent(ComponentId id, const scnXml::DTTreatSimple& elt) : HumanInterventionComponent(id) {
        //NOTE: this code is currently identical to that in CMDTTreatSimple
        try {
            SimTime dur_l = UnitParse::readShortDuration(elt.getDurationLiver(), UnitParse::NONE),
                    dur_b = UnitParse::readShortDuration(elt.getDurationBlood(), UnitParse::NONE);
            const SimTime neg1 = -Sim::one_ts();
            if (dur_l < neg1 || dur_b < neg1) {
                throw util::xml_scenario_error("treatSimple: cannot have durationBlood or durationLiver less than -1");
            }
            if (util::ModelOptions::option(util::VivaxSimpleModel)) {
                if (dur_l != Sim::zero() || dur_b != neg1)
                    throw util::unimplemented_exception(
                        "vivax model only supports timestepsLiver=0, timestepsBlood=-1");
                // Actually, the model ignores these parameters; we just don't want somebody thinking it doesn't.
            }
            time_liver = dur_l;
            time_blood = dur_b;
        } catch (const util::format_error& e) {
            throw util::xml_scenario_error(string("treatSimple: ").append(e.message()));
        }
    }

    void deploy(Human& human, mon::DeployMethod method, VaccineLimits) const override {
        mon::record_deploy(mon::measure("nMDAs"), mon::measure("nCtsMDA"), human, method);
        human.withinHostModel->treat_simple(human, time_liver, time_blood);
    }

    component_types::Type component_type() const override { return component_types::TreatSimple; }

    void print_details(std::ostream& out) const override { out << id().id << "\ttreatSimple"; }

private:
    SimTime time_liver, time_blood;
};

//NOTE: this is similar to CMDTTreatPKPD, but supporting only a single "treatment"
class TreatPKPDComponent : public HumanInterventionComponent {
public:
    TreatPKPDComponent(ComponentId id, const scnXml::DTTreatPKPD& elt, const pk_pd::PkPdParams& pk_pd)
        : HumanInterventionComponent(id), schedule(pk_pd.courses.find_schedule(elt.getSchedule())),
          dosage(pk_pd.courses.find_dosages(elt.getDosage())), delay_d(elt.getDelay_h() / 24.0 /* convert to days */) {}

    void deploy(Human& human, mon::DeployMethod method, VaccineLimits) const override {
        mon::record_deploy(mon::measure("nMDAs"), mon::measure("nCtsMDA"), human, method);
        const double age = Sim::in_years(human.age(Sim::now_or_ts1()));
        human.withinHostModel->treat_pk_pd(schedule, dosage, age, delay_d);
    }

    component_types::Type component_type() const override { return component_types::TreatPkpd; }

    void print_details(std::ostream& out) const override { out << id().id << "\ttreatPKPD"; }

private:
    size_t schedule; // index of the schedule
    size_t dosage;   // name of the dosage table
    double delay_d;  // delay in days
};

class DecisionTreeComponent : public HumanInterventionComponent {
public:
    DecisionTreeComponent(ComponentId id, const scnXml::DecisionTree& elt,
                          Clinical::DecisionTreeLibrary& decision_trees)
        : HumanInterventionComponent(id), tree(decision_trees.create(elt, false)) {}

    void deploy(Human& human, mon::DeployMethod method, VaccineLimits) const override {
        const Clinical::CMDTOut out =
            tree.exec(Clinical::CMHostData(human, human.intervention_rng, Sim::in_years(human.age(Sim::now_or_ts1())),
                                           human.clinicalModel->get_latest_state()));
        if (out.treated) {
            mon::record_deploy(mon::measure("nMDAs"), mon::measure("nCtsMDA"), human, method);
        }
        if (out.screened) {
            mon::record_deploy(mon::measure("nMassScreenings"), mon::measure("nCtsScreenings"), human, method);
        }
    }

    component_types::Type component_type() const override { return component_types::CmDt; }

    void print_details(std::ostream& out) const override { out << id().id << "\tDecision tree"; }

private:
    const Clinical::CMDecisionTree& tree;
};

class ClearImmunityComponent : public HumanInterventionComponent {
public:
    ClearImmunityComponent(ComponentId id) : HumanInterventionComponent(id) {}

    void deploy(Human& human, mon::DeployMethod method, VaccineLimits) const override {
        human.withinHostModel->clear_immunity();
        //NOTE: not reported
    }

    component_types::Type component_type() const override { return component_types::ClearImmunity; }

    void print_details(std::ostream& out) const override { out << id().id << "\tclear immunity"; }
};

}
