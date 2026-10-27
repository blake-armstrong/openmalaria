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

#ifndef OM_INTERVENTIONS_IRS
#define OM_INTERVENTIONS_IRS

#include "util/DecayFunction.h"
#include "Transmission/PerHost.h"
#include "schema/interventions.h"

namespace OM::interventions {
using Transmission::PerHostInterventionData;
using util::DecayFunction;
using util::LocalRng;
using util::LognormalSampler;
using util::NormalSampler;

class IRSComponent : public Transmission::HumanVectorInterventionComponent {
public:
    IRSComponent(ComponentId id, const scnXml::IRSDescription& elt, const map<string, size_t>& species_name_map,
                 const Clock& clock, const util::CommandLineOptions& command_line);

    void deploy(Host::Human& human, mon::DeployMethod method, VaccineLimits) const override;

    component_types::Type component_type() const override;

    void print_details(std::ostream& out) const override;

    unique_ptr<PerHostInterventionData> make_human_part(LocalRng& rng) const override;
    unique_ptr<PerHostInterventionData> make_human_part(istream& stream, ComponentId) const override;

private:
    /** Per mosquito-species parameters for extended IRS model. */
    class IRSAnopheles {
    public:
        IRSAnopheles()
            : proportion_protected(numeric_limits<double>::signaling_NaN()),
              proportion_unprotected(numeric_limits<double>::signaling_NaN()) {}
        void init(const scnXml::IRSDescription::AnophelesParamsType& elt, double proportion_use,
                  double max_insecticide);

        /// Get deterrency. See ComponentParams::effect for a more detailed description.
        inline double relative_attractiveness(double insecticide_content) const {
            return rel_attractiveness.relative_attractiveness(insecticide_content);
        }
        /// Get survival effect on mosquitoes before feeding.
        /// See ComponentParams::effect for a more detailed description.
        inline double preprandial_survival_factor(double insecticide_content) const {
            return preprandial_killing_effect.survival_factor(insecticide_content);
        }
        /// Get survival effect on mosquitoes after they've eaten.
        /// See ComponentParams::effect for a more detailed description.
        inline double postprandial_survival_factor(double insecticide_content) const {
            return postprandial_killing_effect.survival_factor(insecticide_content);
        }
        /// Get fecundity effect on mosquitoes surviving feeding
        inline double fecundity_effect(double insecticide_content) const {
            return rel_fecundity_effect.survival_factor(insecticide_content);
        }

        /// Return x*proportionProtected + proportionUnprotected
        inline double by_protection(double x) const { return x * proportion_protected + proportion_unprotected; }

    private:
        class RelativeAttractiveness {
        public:
            RelativeAttractiveness();

            /** Set parameters.
            * 
            * It is checked that input parameters lie in a range such that
            * the relative availability is always in the range (0,1] — that is,
            * the deterrent can never be perfect, but can have zero effect. */
            void init(const scnXml::IRSDeterrency& elt);

            /** Calculate effect. Positive is interpreted as having a positive effect
            * (thus decreasing availability or survival) and negative as having a
            * negative effect. Effect is not bounded, though it tends to
            * zero as holeIndex becomes large and insecticideContent tends to zero,
            * and parameters should be defined such that it is always in the
            * range [0,1]. */
            double relative_attractiveness(double insecticide_content) const;

        protected:
            double l_pf; // log of insecticide factor
            double insecticide_scaling;
        };
        class SurvivalFactor {
        public:
            SurvivalFactor();

            /** Set parameters.
            * 
            * It is checked that parameters lie in a suitible range, giving a
            * survival factor between 0 and 1. */
            void init(const scnXml::IRSKillingEffect& elt, bool post_prandial, double max_insecticide);
            /// Initialise, such that factor returned is always 1 (i.e. no effect).
            void init1();

            /** Calculate additional survival factor imposed by IRS on pre-/post-
            * prandial killing. Should be bounded to [0,1] and tend to 1 as the
            * IRS ages. */
            double survival_factor(double insecticide_content) const;

        private:
            double bf, pf; // base and insecticide factors
            double insecticide_scaling;
            double inv_base_survival; // stored for performance only
        };
        double proportion_protected;
        double proportion_unprotected;
        RelativeAttractiveness rel_attractiveness;
        SurvivalFactor preprandial_killing_effect;
        SurvivalFactor postprandial_killing_effect;
        SurvivalFactor rel_fecundity_effect;

        friend class HumanIRS;
    };

    double sample_initial_insecticide(LocalRng& rng) const;

    NormalSampler initial_insecticide;
    double max_insecticide; // maximum initial insecticide
    unique_ptr<DecayFunction> insecticide_decay;
    vector<IRSAnopheles> species; // vector specific params

    friend class HumanIRS;
};

/** Extended IRS model by OB and original model.
 * 
 * Each instance describes the effects of indoor residual spraying.
 */
class HumanIRS : public PerHostInterventionData {
public:
    HumanIRS(LocalRng& rng, const IRSComponent& params);
    HumanIRS(istream& stream, ComponentId id, const IRSComponent& params);

    void redeploy(LocalRng& rng, const Transmission::HumanVectorInterventionComponent& params) override;

    /// Get remaining insecticide content based on initial amount and decay.
    inline double get_insecticide_content(const IRSComponent& params) const {
        const SimTime age = Sim::now_or_ts1() - deploy_time; // implies age 1 TS on first use
        const double effect_survival = insecticide_decay_het->eval(age);
        return initial_insecticide * effect_survival;
    }

    /// Call once per time step to update holes
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
    // this is sampled for each deployment: initial insecticide content doesn't
    // depend on handling by the recipient
    double initial_insecticide{}; // units: mg/m²

    // this parameter is sampled on first deployment, but never resampled for the same human:
    unique_ptr<DecayFunction> insecticide_decay_het;
    const IRSComponent* component;
};

}

#endif
