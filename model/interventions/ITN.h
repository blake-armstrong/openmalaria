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

#ifndef OM_INTERVENTIONS_ITN
#define OM_INTERVENTIONS_ITN

#include "util/DecayFunction.h"
#include "Transmission/PerHost.h"
#include "util/sampler.h"
#include "schema/interventions.h"

namespace OM {
namespace interventions {
using Transmission::PerHostInterventionData;
using util::DecayFunction;
using util::LocalRng;
using util::LognormalSampler;
using util::NormalSampler;

// —————  utility classes (internal use only)  —————

namespace factors {
class SurvivalFactor {
public:
    /// Set parameters.
    ///
    /// It is checked that parameters lie in a suitible range, giving a
    /// survival factor between 0 and 1.
    ///
    /// @param elt  Element from XML
    /// @param maxInsecticide
    /// @param eltName  Element name (used in error messages)
    /// @param isDeterrent True when used to describe a deterrency effect, false when used to
    ///     describe a killing effect.
    void init(const scnXml::ITNKillingEffect& elt, double max_insecticide, const char* elt_name,
              bool attractiveness_constraints);

    /// Variant to set paramters for the logit model.
    ///
    /// @param elt  Element from XML
    /// @param maxHoleIndex Parameter from parent element in XML
    /// @param isDeterrent True when used to describe a deterrency effect, false when used to
    ///     describe a killing effect.
    void init_logit(const scnXml::ITNEffectLogit& elt, double max_hole_index, bool is_deterrent);

    /// Initialise the model to always return no effect (factor 1).
    void init1();

    /** Part of survival factor, used by new ITN deterrency model. */
    double rel_p_att(double hole_index, double insecticide_content) const;
    /** Calculate additional survival factor imposed by nets on pre-/post-
        * prandial killing. Should be bounded to [0,1] and tend to 1 as the
        * net ages. */
    double survival_factor(double hole_index, double insecticide_content) const;

private:
    union {
        struct {
            double bf, hf, pf, IF; // base, hole, insecticide and interaction factors
            double hole_scaling, insecticide_scaling;
            double inv_base_survival; // stored for performance only; ≥1
        } a;
        struct {
            double bf, hf, pf, IF;
            double h_max;
            double inv_base_effect; // 1 over base effect
        } b;
    };
    bool use_logit_eqns;
};

class RelativeAttractiveness {
    enum DeterrencyModel { NoModel, SingleStage, TwoStage, TwoStageLogit };

public:
    RelativeAttractiveness() : model(0) {}

    /** Set parameters for single-stage deterrency model.
         * 
         * It is checked that input parameters lie in a range such that
         * the relative availability is always in the range (0,1] — that is,
         * the deterrent can never be perfect, but can have zero effect. */
    void init_single_stage(const scnXml::ITNDeterrency& elt, double max_insecticide);

    /** Set parameters for two-stage deterrency model.
         * 
         * It is checked that input parameters lie in a range such that
         * the relative availability is always in the range (0,1] — that is,
         * the deterrent can never be perfect, but can have zero effect. */
    void init_two_stage(const scnXml::TwoStageDeterrency& elt, double max_insecticide, double hole_index_max,
                        bool hole_index_max_present = false);

    /** Calculate effect. Range of output is any value ≥ 0.
         * 
         * 0 implies a fully effective deterrent, 0.5 a 50% effective
         * deterrent, 1 has no effect, >1 attracts extra mosquitoes. */
    double relative_attractiveness(double hole_index, double insecticide_content) const;

private:
    union {
        struct {
            double l_hf, l_pf, l_if; // logs of hole, insecticide and interaction factors
            double hole_scaling, insecticide_scaling;
        } a;
        struct {
            double l_pf_entering; // log of insecticide factor
            double insecticide_scaling_entering;
            double pad;
            SurvivalFactor p_attacking;
        } b;
        struct {
            double ent_base_factor;
            double ent_insecticide_factor;
            double p_ent0_inv;
            SurvivalFactor p_attacking;
        } c;
    };
    int model;
};
}

// —————  main, public classes  —————

class ITNComponent : public Transmission::HumanVectorInterventionComponent {
public:
    ITNComponent(ComponentId id, const scnXml::ITNDescription& elt, const map<string, size_t>& species_name_map);

    virtual void deploy(Host::Human& human, mon::Deploy::Method method, VaccineLimits) const;

    virtual component_types::Type component_type() const;

    virtual void print_details(std::ostream& out) const;

    virtual unique_ptr<PerHostInterventionData> make_human_part(LocalRng& rng) const;
    virtual unique_ptr<PerHostInterventionData> make_human_part(istream& stream, ComponentId id) const;

private:
    /** Per mosquito-species parameters for extended ITN model. */
    class ITNAnopheles {
    public:
        ITNAnopheles()
            : proportion_protected(numeric_limits<double>::signaling_NaN()),
              proportion_unprotected(numeric_limits<double>::signaling_NaN()) {}
        void init(const scnXml::ITNDescription::AnophelesParamsType& elt, double proportion_use,
                  double max_insecticide);

        /// Get deterrency. See ComponentParams::effect for a more detailed description.
        /// Range: ≥0 where 0=fullly deter, 1=no effect, >1 = attract
        inline double relative_attractiveness(double hole_index, double insecticide_content) const {
            return by_protection(rel_attractiveness.relative_attractiveness(hole_index, insecticide_content));
        }
        /// Get killing effect on mosquitoes before feeding.
        /// See ComponentParams::effect for a more detailed description.
        inline double preprandial_survival_factor(double hole_index, double insecticide_content) const {
            return by_protection(preprandial_killing_effect.survival_factor(hole_index, insecticide_content));
        }
        /// Get killing effect on mosquitoes after they've eaten.
        /// See ComponentParams::effect for a more detailed description.
        inline double postprandial_survival_factor(double hole_index, double insecticide_content) const {
            return by_protection(postprandial_killing_effect.survival_factor(hole_index, insecticide_content));
        }
        /// Get mosquito fecundity multiplier
        inline double rel_fecundity(double hole_index, double insecticide_content) const {
            return by_protection(rel_fecundity_effect.survival_factor(hole_index, insecticide_content));
        }

        /// Return x*proportionProtected + proportionUnprotected
        inline double by_protection(double x) const { return x * proportion_protected + proportion_unprotected; }

    private:
        double proportion_protected;
        double proportion_unprotected;
        factors::RelativeAttractiveness rel_attractiveness;
        factors::SurvivalFactor preprandial_killing_effect;
        factors::SurvivalFactor postprandial_killing_effect;
        factors::SurvivalFactor rel_fecundity_effect;

        friend class HumanITN;
    };

    NormalSampler initial_insecticide;
    unique_ptr<LognormalSampler> hole_rate; // holes per step
    unique_ptr<LognormalSampler> rip_rate;  // rips per hole per step
    double max_insecticide;                 // maximum initial insecticide
    double rip_factor;                      // factor expressing how significant rips are in comparison to holes
    unique_ptr<DecayFunction> insecticide_decay;
    unique_ptr<DecayFunction> attrition_of_nets;
    vector<ITNAnopheles> species; // vector specific params

    friend class HumanITN;
};

/** Extended ITN model by OB.
 * 
 * Each instance describes a hypothetical net (or no net).
 */
class HumanITN : public PerHostInterventionData {
public:
    HumanITN(LocalRng& rng, const ITNComponent& params);
    HumanITN(istream& stream, ComponentId id, const ITNComponent& params);

    virtual void redeploy(LocalRng& rng, const Transmission::HumanVectorInterventionComponent& params);

    inline double get_hole_index() const { return hole_index; }
    inline double get_insecticide_content(const ITNComponent& params) const {
        SimTime age = Sim::now_or_ts1() - deploy_time; // implies age 1 TS on first use
        double effect_survival = insecticide_decay_het->eval(age);
        return initial_insecticide * effect_survival;
    }

    /// Call once per time step to update holes
    virtual void update(Host::Human& human);

    /// Get deterrency. See ComponentParams::effect for a more detailed description.
    virtual double relative_attractiveness(size_t species_index) const;
    /// Get killing effect on mosquitoes before they've eaten.
    /// See ComponentParams::effect for a more detailed description.
    virtual double preprandial_survival_factor(size_t species_index) const;
    /// Get killing effect on mosquitoes after they've eaten.
    /// See ComponentParams::effect for a more detailed description.
    virtual double postprandial_survival_factor(size_t species_index) const;
    /// Get the mosquito fecundity multiplier (1 for no effect).
    virtual double rel_fecundity(size_t species_index) const;

protected:
    virtual void checkpoint(ostream& stream);

private:
    // these parameters express the current state of the net:
    SimTime disposal_time = Sim::never(); // time at which net will be disposed of (if it's not already been replaced)
    int n_holes;                          // total number of holes
    double hole_index;                    // a measure of both the number and size of holes
    double initial_insecticide;           // units: mg/m²

    // these parameters are sampled from log-normal per net, but thereafter constant:
    double hole_rate; // rate at which new holes are created (holes/time-step)
    double rip_rate;  // rate at which holes are enlarged (rips/hole/time-step)
    unique_ptr<DecayFunction> insecticide_decay_het;
    const ITNComponent* component;
};

}
}

#endif
