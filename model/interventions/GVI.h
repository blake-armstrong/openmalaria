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

#ifndef OM_INTERVENTIONS_GVI
#define OM_INTERVENTIONS_GVI

#include "Transmission/PerHost.h"
#include "interventions/Interfaces.h"
#include "util/DecayFunction.h"
#include "util/sampler.h"
#include "schema/interventions.h"

namespace OM::interventions {
using Transmission::PerHostInterventionData;
using util::DecayFunction;
using util::LocalRng;
using util::LognormalSampler;

/** Constant parameters for generic vector intervention model. */
class GVIComponent : public Transmission::HumanVectorInterventionComponent {
public:
    /** Initialise parameters
     * 
     * @param elt Component description from XML
     * @param species_name_map Map of species names to indices.
     */
    GVIComponent(ComponentId, const scnXml::GVIDescription& elt, const map<string, size_t>& species_name_map);

    void deploy(Host::Human& human, mon::Deploy::Method method, VaccineLimits) const override;

    component_types::Type component_type() const override;

    void print_details(std::ostream& out) const override;

    unique_ptr<PerHostInterventionData> make_human_part(LocalRng& rng) const override;
    unique_ptr<PerHostInterventionData> make_human_part(istream& stream, ComponentId) const override;

private:
    /** Per mosquito-species parameters for generic vector intervention model. */
    class GVIAnopheles {
    public:
        GVIAnopheles()
            : proportion_protected(numeric_limits<double>::signaling_NaN()),
              proportion_unprotected(numeric_limits<double>::signaling_NaN()),
              deterrency(numeric_limits<double>::signaling_NaN()),
              preprandial_killing(numeric_limits<double>::signaling_NaN()),
              postprandial_killing(numeric_limits<double>::signaling_NaN()),
              fecundity_reduction(numeric_limits<double>::signaling_NaN()) {}
        void init(const scnXml::GVIDescription::AnophelesParamsType& elt, double proportion_use);

        /// Return x*proportionProtected + proportionUnprotected
        inline double by_protection(double x) const { return x * proportion_protected + proportion_unprotected; }

        double proportion_protected;
        double proportion_unprotected;
        double deterrency;
        double preprandial_killing;
        double postprandial_killing;
        double fecundity_reduction;

        friend class HumanGVI;
    };

    unique_ptr<DecayFunction> decay;
    vector<GVIAnopheles> species; // vector specific params

    friend class HumanGVI;
};

/** Low-level (generic) vector intervention model. Has three effects:
 * deterrency, pre-prandial killing and post-prandial killing.
 * 
 * This is the per-host (but not per vector) part.
 */
class HumanGVI : public PerHostInterventionData {
public:
    HumanGVI(LocalRng& rng, const GVIComponent& params);
    HumanGVI(istream& stream, ComponentId id, const GVIComponent& params);

    void redeploy(LocalRng& rng, const Transmission::HumanVectorInterventionComponent&) override;

    /** This is the survival factor of the effect. */
    inline double get_effect_survival(const GVIComponent& params) const {
        SimTime age = Sim::now_or_ts1() - deploy_time; // implies age 1 TS on first use
        return decay_het->eval(age);
    }

    void update(Host::Human& human) override;

    /// Get deterrency. See ComponentParams::effect for a more detailed description.
    double relative_attractiveness(size_t species_index) const override;
    /// Get killing effect on mosquitoes before they've eaten.
    /// See ComponentParams::effect for a more detailed description.
    double preprandial_survival_factor(size_t species_index) const override;
    /// Get killing effect on mosquitoes after they've eaten.
    /// See ComponentParams::effect for a more detailed description.
    double postprandial_survival_factor(size_t species_index) const override;
    /// Get the mosquito fecundity multiplier (1 for no effect).
    double rel_fecundity(size_t species_index) const override;

protected:
    void checkpoint(ostream& stream) override;

private:
    // this parameter is sampled on first deployment, but never resampled for the same human:
    unique_ptr<DecayFunction> decay_het;
    const GVIComponent* component;
};

}

#endif
