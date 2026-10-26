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

#ifndef HMOD_CM_DECISION_TREE
#define HMOD_CM_DECISION_TREE

#include "Host/Human.h"
#include "Host/WithinHost/Pathogenesis/State.h"
#include "Clinical/Episode.h"
#include <schema/healthSystem.h>

#include <memory>
#include <vector>

namespace OM {
namespace WithinHost {
class WHInterface;
class DiagnosticRegistry;
}
namespace pk_pd {
struct PkPdParams;
}
namespace Clinical {
using WithinHost::WHInterface;

/** All data which needs to be passed to the decision tree evaluators. */
struct CMHostData {
    /** Initialise, from a human h drawing from stream r, with age age_years,
     * and pathogenesis state pg_state. Note that pg_state is only needed for
     * uncomplicated trees (see DecisionTreeLibrary::create()). */
    CMHostData(Host::Human& h, util::LocalRng& r, double age_years, Episode::State pg_state)
        : human(h), age_years(age_years), pg_state(pg_state), rng(r) {}
    Host::Human& human;
    double age_years; // only cached to save recalculating
    Episode::State pg_state;
    util::LocalRng& rng; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    inline WHInterface& within_host() { return *human.withinHostModel; }
};
/** All output data from the decision tree. */
struct CMDTOut {
    CMDTOut() : treated(false), screened(false) {}
    explicit CMDTOut(bool t) : treated(t), screened(false) {}
    CMDTOut(bool t, bool s) : treated(t), screened(s) {}
    bool treated;  // true iff some blood-stage treatment was administered
    bool screened; // true iff some diagnostic was used
};

/**
 * Decision tree node abstraction.
 * 
 * Sub-classes represent either a decision node (first/second line case, a
 * diagnostic with positive/negative outcome, a random decision) or an action.
 *****************************************************************************/
class CMDecisionTree {
public:
    virtual ~CMDecisionTree() = default;

    /** Test for equivalence in two decision trees. Nodes are equivalent if
     * they have the same type, same deployments and treatments, and their
     * sub-nodes are equivalent. */
    virtual bool operator==(const CMDecisionTree& that) const = 0;
    inline bool operator!=(const CMDecisionTree& that) const { return !(*this == that); }

    /** Execute the decision tree.
     * 
     * Reporting: use of diagnostics is reported. Treatment is not, but the
     * output may be used to determine whether any treatment took place. */
    virtual CMDTOut exec(CMHostData host_data) const = 0;
};

class DecisionTreeLibrary {
public:
    DecisionTreeLibrary() = default;
    DecisionTreeLibrary(const WithinHost::DiagnosticRegistry& diagnostics, const pk_pd::PkPdParams& pk_pd,
                        SimTime health_system_memory, const interventions::InterventionManager& interventions)
        : diagnostic_registry(&diagnostics), pk_pd_params(&pk_pd), hs_memory(health_system_memory),
          manager(&interventions) {}

    /** Create a user-configured decision from an XML node.
     * 
     * @param node XML element describing the tree
     * @param isUC If isUC is false and a "case type" decision is created, an
     *  xml_scenario_error exception is thrown. CMHostData::pgState is only
     *  used by the "case type" decision, so if isUC is false when creating the
     *  tree, pgState does not need to be set when executing the tree. */
    const CMDecisionTree& create(const ::scnXml::DecisionTree& node, bool is_uc);

    const CMDecisionTree& save(std::unique_ptr<CMDecisionTree> decision);

    const WithinHost::DiagnosticRegistry& diagnostics() const { return *diagnostic_registry; }
    const pk_pd::PkPdParams& pk_pd() const { return *pk_pd_params; }
    SimTime health_system_memory() const { return hs_memory; }
    const interventions::InterventionManager& intervention_manager() const { return *manager; }

private:
    const WithinHost::DiagnosticRegistry* diagnostic_registry = nullptr;
    const pk_pd::PkPdParams* pk_pd_params = nullptr;
    SimTime hs_memory = Sim::never();
    const interventions::InterventionManager* manager = nullptr;
    std::vector<std::unique_ptr<CMDecisionTree>> decisions;
};

}

}
#endif
