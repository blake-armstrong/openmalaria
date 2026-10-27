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

#ifndef OM_INTERVENTIONS_INTERVENTIONS
#define OM_INTERVENTIONS_INTERVENTIONS

#include "Global.h"
#include "interventions/Interfaces.h"
#include "interventions/HumanComponents.h"
#include "Host/ImportedInfections.h"
#include "Transmission/TransmissionModel.h"
#include "schema/interventions.h"

#include <array>

namespace OM {
namespace Host {
class Human;
}
namespace Clinical {
class ClinicalParams;
class DecisionTreeLibrary;
}
namespace WithinHost {
class DiagnosticRegistry;
class GenotypeRegistry;
struct WithinHostModel;
}
namespace Transmission {
class PerHostParams;
}

namespace interventions {

class ContinuousHumanDeployment;
class TimedDeployment;

/** Management of interventions deployed on a per-time-step basis. */
class InterventionManager {
public:
    InterventionManager();
    InterventionManager(const InterventionManager&) = delete;
    InterventionManager& operator=(const InterventionManager&) = delete;
    InterventionManager(InterventionManager&&) = delete;
    InterventionManager& operator=(InterventionManager&&) = delete;
    ~InterventionManager();

    /** Read XML descriptions. */
    void init(const scnXml::Interventions& interv_elt, Transmission::TransmissionModel& transmission,
              const Transmission::PerHostParams& per_host_params, const WithinHost::DiagnosticRegistry& diagnostics,
              const WithinHost::GenotypeRegistry& genotypes, WithinHost::WithinHostModel& within_host,
              Clinical::ClinicalParams& clinical, Clinical::DecisionTreeLibrary& decision_trees,
              const util::ModelOptionFlags& options, const util::CommandLineOptions& command_line,
              mon::MonitoringState& monitoring);

    /// Checkpointing
    template <class S> void checkpoint(S& stream) {
        using namespace OM::util::checkpoint;
        // most members are only set from XML,
        // nextTimed varies but is re-set by loadFromCheckpoint
        imported_infections & stream;
    }

    /** Call after loading a checkpoint, passing the intervention-period time.
     * 
     * Serves to replace health-system and EIR where changeHS/changeEIR
     * interventions have been used. */
    void load_from_checkpoint(vector<Host::Human>& population, Transmission::TransmissionModel& transmission);

    /** @brief Deploy interventions
     *
     * Timed interventions are deployed for this time step.
     * 
     * Continuous interventions are deployed as humans reach the target ages.
     * Unlike with vaccines, missing one schedule doesn't preclude the next. */
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission);

    /** Get a constant reference to a component class with a certain index.
     * 
     * @throws util::base_exception if the index is out-of-range */
    const HumanInterventionComponent& get_component(ComponentId id) const {
        if (id.id >= human_components.size())
            throw util::base_exception("invalid component id");
        return *human_components[id.id];
    }

    /** Get a numeric ComponentId from the textual identifier used in the XML.
     * 
     * If textId is unknown, an xml_scenario_error is thrown. */
    ComponentId get_component_id(const std::string& text_id) const;

    /** The sub-populations for which the given removal option is enabled. */
    const vector<ComponentId>& remove_at_ids(sub_pop_remove::RemoveAtCode code) const { return remove_at_lists[code]; }

private:
    // Map of textual identifiers to numeric identifiers for components
    std::map<std::string, ComponentId> identifier_map;
    // All human intervention components, indexed by a number. This list is used
    // during initialisation and thereafter only for memory management.
    vector<unique_ptr<HumanInterventionComponent>> human_components;
    // For each RemoveAtCode (excluding NUM), a list of all sub-population
    // identifiers for which the option is enabled.
    std::array<vector<ComponentId>, sub_pop_remove::NUM> remove_at_lists;
    // Continuous interventions, sorted by deployment age (weakly increasing)
    vector<ContinuousHumanDeployment> continuous;
    // List of all timed interventions. Should be sorted (time weakly increasing).
    vector<unique_ptr<TimedDeployment>> timed;
    uint32_t next_timed = 0; // not chcekpointed (see loadFromCheckpoint)
    bool verbose = false;

    // imported infections are not really interventions, and handled by a separate class
    // (but are grouped here for convenience and due toassociation in schema)
    OM::Host::ImportedInfections imported_infections;
};

}

}
#endif
