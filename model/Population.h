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

#ifndef HMOD_POPULATION
#define HMOD_POPULATION

#include "Global.h"
#include "PopulationAgeStructure.h"
#include "Host/Human.h"

#include <vector>

namespace OM {
namespace mon {
class Continuous;
}
namespace WithinHost {
class DiagnosticRegistry;
}

/** The simulated human population. */
class Population {
public:
    Population(size_t size, const AgeStructure& age_structure, Host::HumanParams human_params, util::MasterRng& rng);

    /** Create the initial population of Humans */
    void create_initial_humans();

    /** Remove dead humans, outmigrate others and introduce babies to
     * while keeping the population size and demography distirbution unchanged. */
    void update();

    /** Return the size of the human population */
    inline size_t get_size() const;

    /** Return the number of recent births */
    inline int get_recent_births() const;

    /** Reset the number of recent births to 0 */
    inline void reset_recent_births();

    const interventions::InterventionManager& interventions() const { return *human_params.interventions; }

    const Clock& clock() const { return *human_params.clock; }

    /** Checkpoint (read) */
    void checkpoint(istream& stream);

    /** Checkpoint (write) */
    void checkpoint(ostream& stream);

    /** The simulated human hosts */
    vector<Host::Human> humans;

private:
    const AgeStructure* age_structure;
    Host::HumanParams human_params;
    util::MasterRng* rng;

    /** Size of the human population */
    size_t size = 0;

    /** Births count since last continuous output */
    int recent_births = 0;
};

void register_continuous_population_callbacks(mon::Continuous& continuous,
                                              const WithinHost::DiagnosticRegistry& diagnostics);

/// Delegate to print the number of hosts
void cts_hosts(Population& population, ostream& stream);
/// Delegate to print cumulative numbers of hosts under various age limits
void cts_host_demography(Population& population, ostream& stream);
/// Delegate to print the number of births since last count
void cts_recent_births(Population& population, ostream& stream);
/// Delegate to print the number of patent hosts
void cts_patent_hosts(Population& population, const WithinHost::DiagnosticRegistry& diagnostics, ostream& stream);
/// Delegate to print immunity's cumulativeh parameter
void cts_immunity_h(Population& population, ostream& stream);
/// Delegate to print immunity's cumulativeY parameter (mean across population)
void cts_immunity_y(Population& population, ostream& stream);
/// Delegate to print immunity's cumulativeY parameter (median across population)
void cts_median_immunity_y(Population& population, ostream& stream);
/// Delegate to print the mean age-based availability reduction of each human relative to an adult
void cts_mean_age_avail_effect(Population& population, ostream& stream);
void cts_itn_coverage(Population& population, ostream& stream);
void cts_irs_coverage(Population& population, ostream& stream);
void cts_gvi_coverage(Population& population, ostream& stream);

inline size_t Population::get_size() const { return size; }

inline int Population::get_recent_births() const { return recent_births; }

inline void Population::reset_recent_births() { recent_births = 0; }

}
#endif
