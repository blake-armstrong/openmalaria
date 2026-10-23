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

#include "Clinical/CMDecisionTree.h"
#include "Clinical/ClinicalModel.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Diagnostic.h"
#include "PkPd/PkPdParams.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/UnitParse.h"
#include "interventions/Interfaces.h"
#include "interventions/InterventionManager.h"

#include <limits>
#include <list>
#include <vector>
#include <map>

namespace OM::Clinical {

using WithinHost::Diagnostic;
using WithinHost::Diagnostics;
using namespace OM::util;
using std::map;
using std::vector;

// ———  special 'multiple' node  ———

/**
 * Branch out to multiple descendants.
 */
class CMDTMultiple : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTMultiple& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTMultiple*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (children.size() != p->children.size())
            return false;
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] != p->children[i])
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        CMDTOut result(false);
        for (auto it : children) {
            const CMDTOut r2 = it->exec(host_data);
            result.treated = result.treated || r2.treated;
            result.screened = result.screened || r2.screened;
        }
        return result;
    }

private:
    CMDTMultiple() = default;

    using ChildrenT = vector<const CMDecisionTree*>;
    ChildrenT children;
};

// ———  branching nodes  ———

/**
 * Should the patient recieve first or second line treatment?
 */
class CMDTCaseType : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTCaseType& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTCaseType*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (first_line != p->first_line)
            return false;
        if (second_line != p->second_line)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        // Uses of this in complicated cases should trigger an exception during initialisation.
        assert((host_data.pg_state & Episode::Sick) && !(host_data.pg_state & Episode::Complicated));

        if (host_data.pg_state & Episode::SecondCase)
            return second_line.exec(host_data);
        else
            return first_line.exec(host_data);
    }

private:
    CMDTCaseType(const CMDecisionTree& first_line, const CMDecisionTree& second_line)
        : first_line(first_line), second_line(second_line) {}

    const CMDecisionTree& first_line;
    const CMDecisionTree& second_line;
};

/**
 * Should the patient recieve a different treatment based on its infection type?
 */
class CMDTInfectionOrigin : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTInfectionOrigin& node,
                                        bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTInfectionOrigin*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (imported != p->imported)
            return false;
        if (introduced != p->introduced)
            return false;
        if (indigenous != p->indigenous)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        const WithinHost::InfectionOrigin infection_origin = host_data.within_host().infection_origin();
        if (infection_origin == WithinHost::InfectionOrigin::Imported)
            return imported.exec(host_data);
        else if (infection_origin == WithinHost::InfectionOrigin::Introduced)
            return introduced.exec(host_data);
        else
            return indigenous.exec(host_data);
    }

private:
    CMDTInfectionOrigin(const CMDecisionTree& imported, const CMDecisionTree& introduced,
                        const CMDecisionTree& indigenous)
        : imported(imported), introduced(introduced), indigenous(indigenous) {}

    const CMDecisionTree &imported, &introduced, &indigenous;
};

/**
 * Use a diagnostic, and call another decision tree based on the outcome.
 */
class CMDTDiagnostic : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTDiagnostic& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTDiagnostic*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (diagnostic != p->diagnostic)
            return false;
        if (positive != p->positive)
            return false;
        if (negative != p->negative)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        CMDTOut result;

        if (host_data.within_host().diagnostic_result(host_data.rng, diagnostic))
            result = positive.exec(host_data);
        else
            result = negative.exec(host_data);
        result.screened = true;

        return result;
    }

private:
    CMDTDiagnostic(const Diagnostic& diagnostic, const CMDecisionTree& positive, const CMDecisionTree& negative)
        : diagnostic(diagnostic), positive(positive), negative(negative) {}

    const Diagnostic& diagnostic;
    const CMDecisionTree& positive;
    const CMDecisionTree& negative;
};

class CMDTUncomplicated : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTUncomplicated& node,
                                        bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTUncomplicated*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (memory != p->memory)
            return false;
        if (positive != p->positive)
            return false;
        if (negative != p->negative)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        const Clinical::Episode& latest = host_data.human.clinicalModel->get_latest_report();

        if (((host_data.pg_state & Episode::Sick) && !(host_data.pg_state & Episode::Complicated)) ||
            (host_data.pg_state & Episode::Malaria)) {
            if (latest.time + memory >= Sim::now_or_ts0())
                return positive.exec(host_data);
            else
                return negative.exec(host_data);
        } else
            return negative.exec(host_data);
    }

private:
    CMDTUncomplicated(const SimTime memory, SimTime health_system_memory, const CMDecisionTree& positive,
                      const CMDecisionTree& negative)
        : memory(memory), positive(positive), negative(negative) {
        if (memory > health_system_memory)
            throw util::xml_scenario_error("<uncomplicated> memory parameter must be less than or equal to the "
                                           "healthsystem memory (hsmemory parameter)");
    }

    const SimTime memory;
    const CMDecisionTree& positive;
    const CMDecisionTree& negative;
};

class CMDTSevere : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTSevere& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTSevere*>(&that);
        if (p == nullptr)
            return false; // different type of node
        // if( diagnostic != p->diagnostic ) return false;
        if (positive != p->positive)
            return false;
        if (negative != p->negative)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        CMDTOut result;

        if (host_data.pg_state & Episode::Complicated)
            result = positive.exec(host_data);
        else
            result = negative.exec(host_data);

        // result.screened = true;
        return result;
    }

private:
    CMDTSevere(const CMDecisionTree& positive, const CMDecisionTree& negative)
        : positive(positive), negative(negative) {}

    const CMDecisionTree& positive;
    const CMDecisionTree& negative;
};

/**
 * Choose a branch randomly.
 * 
 * TODO(optimisation): can we re-order the trees such that this can be abstracted out or
 * used only once?
 */
class CMDTRandom : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTRandom& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTRandom*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (branches.size() != p->branches.size())
            return false;
        for (auto it1 = branches.begin(), it2 = p->branches.begin(); it1 != branches.end(); ++it1, ++it2) {
            if (it1->first != it2->first)
                return false;
            if (it1->second != it2->second)
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        auto it = branches.upper_bound(host_data.rng.uniform_01());
        assert(it != branches.end());
        return it->second->exec(host_data);
    }

private:
    CMDTRandom() = default;

    // keys are cumulative probabilities; last entry should equal 1
    using BranchesT = map<double, const CMDecisionTree*>;
    BranchesT branches;
};

/**
 * Choose what to do based on the patient's age.
 */
class CMDTAge : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTAge& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTAge*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (branches.size() != p->branches.size())
            return false;
        for (auto it1 = branches.begin(), it2 = p->branches.begin(); it1 != branches.end(); ++it1, ++it2) {
            if (it1->first != it2->first)
                return false;
            if (it1->second != it2->second)
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        // age is that of human at start of time step (i.e. may be as low as 0)
        auto it = branches.upper_bound(host_data.age_years);
        if (it == branches.end())
            throw TRACED_EXCEPTION("bad age-based decision tree switch", util::Error::PkPd);
        return it->second->exec(host_data);
    }

private:
    CMDTAge() = default;

    // keys are upper bounds of age categories
    using BranchesT = map<double, const CMDecisionTree*>;
    BranchesT branches;
};

// ———  action nodes  ———

/** Do nothing. **/
class CMDTNoTreatment : public CMDecisionTree {
protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTNoTreatment*>(&that);
        return p != nullptr; // same type: is equivalent
    }

    CMDTOut exec(CMHostData host_data) const override { return CMDTOut(false); }
};

class CMDTReport : public CMDecisionTree {
public:
    CMDTReport(const scnXml::DecisionTree::ReportSequence& seq) {
        out_ids.reserve(seq.size());
        for (const scnXml::DTReport& report_elt : seq) {
            out_ids.push_back(report_elt.getOutputNumber());
        }
        assert(out_ids.size() > 0); // CMDTTreatPKPD should not be used in this case
    }

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTReport*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (out_ids.size() != p->out_ids.size())
            return false;
        for (auto it1 = out_ids.begin(), it2 = p->out_ids.begin(); it1 != out_ids.end(); ++it1, ++it2) {
            if (*it1 != *it2)
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        for (const int out_id : out_ids) {
            mon::record_event(mon::measure("nCMDTReport"), host_data.human, 1, out_id);
        }
        return CMDTOut(false);
    }

private:
    vector<int> out_ids;
};

/** Report treament without affecting parasites. **/
class CMDTTreatFailure : public CMDecisionTree {
protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTTreatFailure*>(&that);
        return p != nullptr; // same type: is equivalent
    }

    CMDTOut exec(CMHostData host_data) const override { return CMDTOut(true /*report treatment*/); }
};

/**
 * Deliver one or more treatments via the PK/PD model.
 */
class CMDTTreatPKPD : public CMDecisionTree {
public:
    CMDTTreatPKPD(const pk_pd::CourseRegistry& courses, const scnXml::DecisionTree::TreatPKPDSequence& seq) {
        treatments.reserve(seq.size());
        for (const scnXml::DTTreatPKPD& treat_elt : seq) {
            treatments.emplace_back(courses, treat_elt.getSchedule(), treat_elt.getDosage(), treat_elt.getDelay_h());
        }
        assert(treatments.size() > 0); // CMDTTreatPKPD should not be used in this case
    }

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTTreatPKPD*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (treatments.size() != p->treatments.size())
            return false;
        for (auto it1 = treatments.begin(), it2 = p->treatments.begin(); it1 != treatments.end(); ++it1, ++it2) {
            if (*it1 != *it2)
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        for (const TreatInfo& treatment : treatments) {
            host_data.within_host().treat_pk_pd(treatment.schedule, treatment.dosage, host_data.age_years, 0.0);
        }
        return CMDTOut(true);
    }

private:
    struct TreatInfo {
        TreatInfo(const pk_pd::CourseRegistry& courses, const string& s, const string& d, double h)
            : schedule(courses.find_schedule(s)), dosage(courses.find_dosages(d)), delay_h(h) {}
        inline bool operator!=(const TreatInfo& that) const {
            return schedule != that.schedule || dosage != that.dosage || delay_h != that.delay_h;
        }
        size_t schedule; // index of the schedule
        size_t dosage;   // name of the dosage table
        double delay_h;  // delay in hours
    };
    vector<TreatInfo> treatments;
};

/**
 * Deliver one or more simple treatments
 */
class CMDTTreatSimple : public CMDecisionTree {
public:
    CMDTTreatSimple(const scnXml::DecisionTree::TreatSimpleSequence& elt)
        : time_liver(Sim::zero()), time_blood(Sim::zero()) {
        for (const auto& treat_elt : elt) {
            //NOTE: this code is currently identical to that in SimpleTreatComponent
            try {
                SimTime dur_l = UnitParse::readShortDuration(treat_elt.getDurationLiver(), UnitParse::NONE),
                        dur_b = UnitParse::readShortDuration(treat_elt.getDurationBlood(), UnitParse::NONE);
                const SimTime neg1 = -Sim::one_ts();
                if (dur_l < neg1 || dur_b < neg1) {
                    throw util::xml_scenario_error(
                        "treatSimple: cannot have durationBlood or durationLiver less than -1");
                }
                time_liver.push_back(dur_l);
                time_blood.push_back(dur_b);
            } catch (const util::format_error& e) {
                throw util::xml_scenario_error(string("treatSimple: ").append(e.message()));
            }
        }
    }

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTTreatSimple*>(&that);
        if (p == nullptr)
            return false; // different type of node
        return time_liver == p->time_liver && time_blood == p->time_blood;
    }

    CMDTOut exec(CMHostData host_data) const override {
        bool bs_treatment = false;
        for (size_t i = 0; i < time_liver.size(); i++) {
            bs_treatment = host_data.within_host().treat_simple(host_data.human, time_liver[i], time_blood[i]);
        }
        return CMDTOut(bs_treatment);
    }

private:
    vector<SimTime> time_liver, time_blood;
};

/**
 * Deploy one or more interventions.
 */
/*NOTE: we use interventions::HumanIntervention to read and sort the list of
 * interventions to be deployed, though we don't use pointers it finds. */
class CMDTDeploy : public CMDecisionTree, interventions::HumanIntervention {
public:
    CMDTDeploy(const scnXml::DecisionTree::DeploySequence& seq)
        : HumanIntervention(seq, bridge::intervention_manager()) {}

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTDeploy*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (components.size() != p->components.size())
            return false;
        for (auto it1 = components.begin(), it2 = p->components.begin(); it1 != components.end(); ++it1, ++it2) {
            // here we can compare pointers since components are always de-duplicated
            if (*it1 != *it2)
                return false;
        }
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        deploy(host_data.human, mon::deploy::TREAT, interventions::VaccineLimits(/*default initialise: no limits*/));

        //NOTE: it's not intuitively obvious what value should be returned here
        // in the case of intervention deployment. This at least means that
        // repeat seekers get second-line treatment.
        return CMDTOut(false);
    }
};

class CMDTCohort : public CMDecisionTree {
public:
    static const CMDecisionTree& create(DecisionTreeLibrary& library, const ::scnXml::DTCohort& node, bool is_uc);

protected:
    bool operator==(const CMDecisionTree& that) const override {
        if (this == &that)
            return true; // short cut: same object thus equivalent
        const auto* p = dynamic_cast<const CMDTCohort*>(&that);
        if (p == nullptr)
            return false; // different type of node
        if (!(component == p->component))
            return false;
        if (positive != p->positive)
            return false;
        if (negative != p->negative)
            return false;
        return true; // no tests failed; must be the same
    }

    CMDTOut exec(CMHostData host_data) const override {
        CMDTOut result;

        // Rely on the health system memory to not count the same episode twice
        if (host_data.human.isInSubPop(component))
            result = positive.exec(host_data);
        else
            result = negative.exec(host_data);

        // result.screened = true;
        return result;
    }

private:
    CMDTCohort(const std::string& component, const CMDecisionTree& positive, const CMDecisionTree& negative)
        : component(bridge::intervention_manager().get_component_id(component)), positive(positive),
          negative(negative) {}

    interventions::ComponentId component;
    const CMDecisionTree& positive;
    const CMDecisionTree& negative;
};

// ———  library  ———

// Saves a decision to the library, making it const.
// Also optimises away duplicates
const CMDecisionTree& DecisionTreeLibrary::save(unique_ptr<CMDecisionTree> decision) {
    // We search the library for a duplicate, and delete this one if there is
    // a duplicate. Note that this is not implemented efficiently, but a little
    // wasted time at start up is hardly a concern.
    for (auto& d : decisions) {
        if (*d == *decision) {
            return *d;
        }
    }

    // No match: add to the library.
    decisions.push_back(std::move(decision));
    return *decisions.back();
}

const CMDecisionTree& DecisionTreeLibrary::create(const scnXml::DecisionTree& node, bool is_uc) {
    if (node.getMultiple().present())
        return CMDTMultiple::create(*this, node.getMultiple().get(), is_uc);
    // branching nodes
    if (node.getCaseType().present())
        return CMDTCaseType::create(*this, node.getCaseType().get(), is_uc);
    if (node.getInfectionOrigin().present())
        return CMDTInfectionOrigin::create(*this, node.getInfectionOrigin().get(), is_uc);
    if (node.getDiagnostic().present())
        return CMDTDiagnostic::create(*this, node.getDiagnostic().get(), is_uc);
    if (node.getUncomplicated().present())
        return CMDTUncomplicated::create(*this, node.getUncomplicated().get(), is_uc);
    if (node.getSevere().present())
        return CMDTSevere::create(*this, node.getSevere().get(), is_uc);
    if (node.getRandom().present())
        return CMDTRandom::create(*this, node.getRandom().get(), is_uc);
    if (node.getAge().present())
        return CMDTAge::create(*this, node.getAge().get(), is_uc);
    if (node.getCohort().present())
        return CMDTCohort::create(*this, node.getCohort().get(), is_uc);

    // action nodes
    if (node.getNoTreatment().present())
        return save(unique_ptr<CMDecisionTree>(new CMDTNoTreatment()));
    if (node.getReport().size())
        return save(unique_ptr<CMDecisionTree>(new CMDTReport(node.getReport())));
    if (node.getTreatFailure().present())
        return save(unique_ptr<CMDecisionTree>(new CMDTTreatFailure()));
    if (node.getTreatPKPD().size())
        return save(unique_ptr<CMDecisionTree>(new CMDTTreatPKPD(pk_pd().courses, node.getTreatPKPD())));
    if (node.getTreatSimple().size())
        return save(unique_ptr<CMDecisionTree>(new CMDTTreatSimple(node.getTreatSimple())));
    if (node.getDeploy().size())
        return save(unique_ptr<CMDecisionTree>(new CMDTDeploy(node.getDeploy())));
    throw xml_scenario_error("unterminated decision tree");
}

const CMDecisionTree& CMDTMultiple::create(DecisionTreeLibrary& library, const scnXml::DTMultiple& node, bool is_uc) {
    unique_ptr<CMDTMultiple> self(new CMDTMultiple());
    for (const scnXml::DTCaseType& sn : node.getCaseType()) {
        self->children.push_back(&CMDTCaseType::create(library, sn, is_uc));
    }
    for (const scnXml::DTInfectionOrigin& sn : node.getInfectionOrigin()) {
        self->children.push_back(&CMDTInfectionOrigin::create(library, sn, is_uc));
    }
    for (const scnXml::DTDiagnostic& sn : node.getDiagnostic()) {
        self->children.push_back(&CMDTDiagnostic::create(library, sn, is_uc));
    }
    for (const scnXml::DTUncomplicated& sn : node.getUncomplicated()) {
        self->children.push_back(&CMDTUncomplicated::create(library, sn, is_uc));
    }
    for (const scnXml::DTSevere& sn : node.getSevere()) {
        self->children.push_back(&CMDTSevere::create(library, sn, is_uc));
    }
    for (const scnXml::DTRandom& sn : node.getRandom()) {
        self->children.push_back(&CMDTRandom::create(library, sn, is_uc));
    }
    for (const scnXml::DTAge& sn : node.getAge()) {
        self->children.push_back(&CMDTAge::create(library, sn, is_uc));
    }
    for (const scnXml::DTCohort& sn : node.getCohort()) {
        self->children.push_back(&CMDTCohort::create(library, sn, is_uc));
    }
    if (node.getTreatPKPD().size()) {
        self->children.push_back(
            &library.save(unique_ptr<CMDecisionTree>(new CMDTTreatPKPD(library.pk_pd().courses, node.getTreatPKPD()))));
    }
    if (node.getTreatSimple().size()) {
        self->children.push_back(&library.save(unique_ptr<CMDecisionTree>(new CMDTTreatSimple(node.getTreatSimple()))));
    }
    if (node.getReport().size()) {
        self->children.push_back(&library.save(unique_ptr<CMDecisionTree>(new CMDTReport(node.getReport()))));
    }
    if (node.getDeploy().size()) {
        self->children.push_back(&library.save(unique_ptr<CMDecisionTree>(new CMDTDeploy(node.getDeploy()))));
    }
    return library.save(std::move(self));
}

const CMDecisionTree& CMDTCaseType::create(DecisionTreeLibrary& library, const scnXml::DTCaseType& node, bool is_uc) {
    if (!is_uc) {
        throw util::xml_scenario_error("decision tree: caseType can only be used for uncomplicated cases");
    }
    return library.save(unique_ptr<CMDecisionTree>(
        new CMDTCaseType(library.create(node.getFirstLine(), is_uc), library.create(node.getSecondLine(), is_uc))));
}

const CMDecisionTree& CMDTInfectionOrigin::create(DecisionTreeLibrary& library, const scnXml::DTInfectionOrigin& node,
                                                  bool is_uc) {
    return library.save(unique_ptr<CMDecisionTree>(
        new CMDTInfectionOrigin(library.create(node.getImported(), is_uc), library.create(node.getIntroduced(), is_uc),
                                library.create(node.getIndigenous(), is_uc))));
}

const CMDecisionTree& CMDTDiagnostic::create(DecisionTreeLibrary& library, const scnXml::DTDiagnostic& node,
                                             bool is_uc) {
    return library.save(unique_ptr<CMDecisionTree>(new CMDTDiagnostic(library.diagnostics().get(node.getDiagnostic()),
                                                                      library.create(node.getPositive(), is_uc),
                                                                      library.create(node.getNegative(), is_uc))));
}

const CMDecisionTree& CMDTUncomplicated::create(DecisionTreeLibrary& library, const scnXml::DTUncomplicated& node,
                                                bool is_uc) {
    return library.save(unique_ptr<CMDecisionTree>(new CMDTUncomplicated(
        UnitParse::readShortDuration(node.getMemory(), UnitParse::STEPS), library.health_system_memory(),
        library.create(node.getPositive(), is_uc), library.create(node.getNegative(), is_uc))));
}

const CMDecisionTree& CMDTSevere::create(DecisionTreeLibrary& library, const scnXml::DTSevere& node, bool is_uc) {
    return library.save(unique_ptr<CMDecisionTree>(
        new CMDTSevere(library.create(node.getPositive(), is_uc), library.create(node.getNegative(), is_uc))));
}

const CMDecisionTree& CMDTCohort::create(DecisionTreeLibrary& library, const scnXml::DTCohort& node, bool is_uc) {
    return library.save(unique_ptr<CMDecisionTree>(new CMDTCohort(
        node.getComponent(), library.create(node.getPositive(), is_uc), library.create(node.getNegative(), is_uc))));
}

const CMDecisionTree& CMDTRandom::create(DecisionTreeLibrary& library, const scnXml::DTRandom& node, bool is_uc) {
    unique_ptr<CMDTRandom> result(new CMDTRandom());

    double cum_p = 0.0;
    for (const scnXml::Outcome& outcome : node.getOutcome()) {
        cum_p += outcome.getP();
        result->branches.insert(make_pair(cum_p, &library.create(outcome, is_uc)));
    }

    // Test cum_p is approx. 1.0 in case the input tree is wrong. We require no
    // less than one to make sure generated random numbers are not greater than
    // the last option.
    if (cum_p < 1.0 || cum_p > 1.001) {
        throw util::xml_scenario_error("decision tree (random node): expected probability sum to be 1.0 but found " +
                                       to_string(cum_p));
    }

    return library.save(std::move(result));
}

const CMDecisionTree& CMDTAge::create(DecisionTreeLibrary& library, const scnXml::DTAge& node, bool is_uc) {
    unique_ptr<CMDTAge> result(new CMDTAge());

    double last_age = numeric_limits<double>::quiet_NaN();
    const CMDecisionTree* last_node = nullptr;
    for (const scnXml::Age& age : node.getAge()) {
        if (last_age != last_age) {
            if (age.getLb() != 0.0)
                throw util::xml_scenario_error("decision tree age switch must have first lower bound equal 0");
        } else {
            if (age.getLb() <= last_age) {
                throw util::xml_scenario_error("decision tree age switch must list age groups in increasing order");
            }
            const double lb = age.getLb();
            result->branches.insert(make_pair(lb, last_node));
        }
        last_node = &library.create(age, is_uc);
        last_age = age.getLb();
    }
    const double no_lb = numeric_limits<double>::infinity();
    result->branches.insert(make_pair(no_lb, last_node));

    return library.save(std::move(result));
}

}
