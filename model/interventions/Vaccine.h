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

#ifndef HMOD_INTERVENTIONS_VACCINE
#define HMOD_INTERVENTIONS_VACCINE

#include "interventions/HumanComponents.h"
#include "schema/interventions.h"

namespace OM {
namespace Host {
class Human;
}
namespace interventions {
using util::LocalRng;

/** Vaccine intervention parameters.
 *
 * Used to represent PEV, BSV and TBV vaccines.
 * Each that has a descriptor is applied
 * simultaneously by a continuous or timed intervention strategy (no
 * way to determine which are used).
 *
 * All parameters (inc. non-static) are only set by initParameters(). */
class VaccineComponent : public HumanInterventionComponent {
public:
    VaccineComponent(ComponentId id, const scnXml::VaccineDescription& seq, vaccines::Types type,
                     bool report_deployments);

    void deploy(Host::Human& human, mon::Deploy::Method method, VaccineLimits vacc_limits) const;

    virtual component_types::Type component_type() const;

    virtual void print_details(std::ostream& out) const;

private:
    /** Get the initial efficacy of the vaccine.
     *
     * @param numPrevDoses The number of prior vaccinations of the individual. */
    double get_initial_efficacy(LocalRng& rng, size_t num_prev_doses, uint32_t genotype) const;

    /// Vaccine component type
    vaccines::Types type;

    /// Function representing decay of effect
    unique_ptr<util::DecayFunction> decay_func;

    /* Vaccine type specific parameters
     * Initial mean efficacy, definition depends on vaccine type */
    vector<vector<double>> per_genotype_initial_mean_efficacy;

    // Distribution of efficacies among individuals, parameter to sample from beta dist.
    double efficacy_b;

    // VACCINE_GENOTYPE option
    bool opt_vaccine_genotype = false;

    //TODO(monitoring):
    /** Until the monitoring system is updated, only one type of vaccination
     * delivery can be reported. This is whichever is first configured. */
    bool report_deployments;

    friend class PerHumanVaccine;
    friend class PerEffectPerHumanVaccine;
};

}
}
#endif
