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

#ifndef HMOD_PER_HOST
#define HMOD_PER_HOST

#include "Global.h"
#include "interventions/Interfaces.h"
#include "util/AgeGroupInterpolation.h"
#include "util/DecayFunction.h"
#include "util/ModelOptions.h"
#include "util/checkpoint_containers.h"

#include "util/random.h"

#include <cassert>

namespace OM {
namespace Transmission {

using util::AgeGroupInterpolator;
using util::DecayFunction;
using util::LocalRng;

class HumanVectorInterventionComponent;

/** Stores vector model data applicable between a category of host and a
 * mosquito species: intervention descriptions and model parameters.
 *
 * Parameters are read from XML, and the availability rate is adjusted. */
class PerHostAnophParams {
public:
    PerHostAnophParams(const scnXml::Mosq& mosq, const util::ModelOptionFlags& options);

    PerHostAnophParams(PerHostAnophParams&&) noexcept = default;
    PerHostAnophParams& operator=(PerHostAnophParams&&) noexcept = default;

    PerHostAnophParams(const PerHostAnophParams&) = delete;
    PerHostAnophParams& operator=(const PerHostAnophParams&) = delete;

    /** @brief Probabilities of finding a host and surviving a feeding cycle
     * 
     * These parameters describe the mean and heterogeneity of α_i, P_B_i,
     * P_C_i and P_D_i across the human population. */
    //@{
    /** Availability rate (α_i) */
    double ento_availability_factor = 1.0;

    /** Availability heterogeneity factor
     * This multiplies the entoAvailabilityFactor. */
    unique_ptr<util::Sampler> ento_availability;

    /** Probability of mosquito successfully biting host (P_B_i) */
    util::BetaSampler prob_mosq_biting;

    /** Probability of mosquito escaping human and finding a resting site without
     * dying, after biting the human (P_C_i). */
    util::BetaSampler prob_mosq_find_rest_site;

    /** Probability of mosquito successfully resting after finding a resting site
     * (P_D_i). */
    util::BetaSampler prob_mosq_survival_resting;
    //@}
};

class PerHostParams {
public:
    PerHostParams() = default;
    PerHostParams(const scnXml::AgeGroupValues& availability_to_mosquitoes, const Clock& clock,
                  const util::CommandLineOptions& command_line);

    void add_species(const scnXml::Mosq& mosq, const util::ModelOptionFlags& options) {
        species.emplace_back(mosq, options);
    }

    /// Get the number of vector species
    size_t num_species() const { return species.size(); }

    /// Get parameters for the given vector species
    const PerHostAnophParams& get(size_t index) const { return species[index]; }

    /** entoAvailability is calculated externally, then set after other
     * parameters have been initialised.
     * 
     * This function doesn't need to exist, but helps make this fact obvious.
     * 
     * It should be called exactly once.
      */
    void set_ento_availability_factor(size_t index, double ento_availability) {
        species[index].ento_availability_factor = ento_availability;
    }

    void calc_availability_percentiles();

    double ento_availability_percentile(int p) const;

    double relative_availability_age(double age_years) const { return rel_avail_age.eval(age_years); }

    /** Get the age at which individuals are considered adults (i.e. where
     * availability to mosquitoes reaches its maximum). */
    SimTime adult_age(const Clock& clock) const { return clock.from_years_d(rel_avail_age.first_global_maximum()); }

private:
    AgeGroupInterpolator rel_avail_age;
    vector<PerHostAnophParams> species;
    vector<double> ento_availability_percentiles;
};

/**
 * A base class for interventions affecting human-vector interaction.
 * 
 * The constructor should initialise the data to represent an intervention
 * deployed at this time (sim::now()).
 * 
 * redeploy() should reset the intervention to a freshly deployed state. If
 * necessary, PerHost::deployComponent can be updated to make it create a new
 * instance instead of calling redeploy.
 */
class PerHostInterventionData {
public:
    virtual ~PerHostInterventionData() = default;

    /** Deploy an intervention. */
    virtual void redeploy(LocalRng& rng, const HumanVectorInterventionComponent& params) = 0;

    /** Per time step update. Used by ITNs to update hole decay. */
    virtual void update(Host::Human& human) = 0;

    /** Get effect of deterrencies of interventions, as an attractiveness multiplier.
     * 
     * @return a value describing effect on attractiveness. Must not be
     * negative. 0 means mosquitoes are fully deterred, 1 that the intervention
     * has no effect, 2 that the intervention attracts twice as many mosquitoes
     * as would otherwise come. */
    virtual double relative_attractiveness(size_t species) const = 0;
    /** Get the killing effect on mosquitoes before they've eaten as a survival
     * multiplier. */
    virtual double preprandial_survival_factor(size_t species) const = 0;
    /** Get the killing effect on mosquitoes after they've eaten as a survival
     * multiplier. */
    virtual double postprandial_survival_factor(size_t species) const = 0;
    /// Get the mosquito fecundity multiplier (1 for no effect).
    virtual double rel_fecundity(size_t species) const = 0;

    /// Index of effect describing the intervention
    inline interventions::ComponentId id() const { return component_id; }

    /// Return true if this component is deployed (i.e. currently active)
    inline bool is_deployed() const { return deploy_time != Sim::never(); }

    /// Checkpointing (write only)
    void operator&(ostream& stream) {
        component_id & stream; // must be first; read externally so that the correct
                               // makeHumanPart function can be found
        checkpoint(stream);
    }

protected:
    /// Set the component id
    explicit PerHostInterventionData(interventions::ComponentId id) : deploy_time(Sim::now()), component_id(id) {}

    /// Checkpointing: write
    virtual void checkpoint(ostream& stream) = 0;

    SimTime deploy_time = Sim::never();      // time of deployment or sim::never()
    interventions::ComponentId component_id; // component id; don't change
};

/** A base class for human vector intervention parameters. */
class HumanVectorInterventionComponent : public interventions::HumanInterventionComponent {
public:
    ~HumanVectorInterventionComponent() override = default;

    /** Create a new object to store human-specific details of deployment. */
    virtual unique_ptr<PerHostInterventionData> make_human_part(LocalRng&) const = 0;
    virtual unique_ptr<PerHostInterventionData> make_human_part(istream& stream,
                                                                interventions::ComponentId id) const = 0;

protected:
    explicit HumanVectorInterventionComponent(interventions::ComponentId id) : HumanInterventionComponent(id) {}
};

/** Contains TransmissionModel parameters which need to be stored per host.
 *
 * Currently many members are public and directly accessed. */
class PerHost {
public:
    ///@brief Initialisation / checkpionting
    //@{
    PerHost();
    void initialise(const PerHostParams& per_host_params, LocalRng& rng, double availability_factor);
    //@}

    /// Call once per time step. Updates net holes.
    void update(Host::Human& human);

    /// Deploy some intervention component
    void deploy_component(LocalRng& rng, const HumanVectorInterventionComponent& params);
    //@}

    /** Calculates the adjustment for body size in exposure to mosquitoes,
     * relative to an average adult.
     * 
     * The bites are assumed proportional to average surface area for hosts of
     * the given age. Linear interpolation is used to calculate this from the
     * input array of surface areas. 
     * 
     * @param ageYears Age of host
     * @return the ratio of bites received by the host to the average for an adult 
     *
     * This is the age factor of availiability; mean output should be
     * mean population availability (that is, 1.0/invMeanPopAvail).
     * 
     * Also has a switch to put individuals entirely outside transmission. */
    inline double relative_availability_age(double age_years) const {
        return outside_transmission ? 0.0 : params->relative_availability_age(age_years);
    }

    /** Get the availability of this host to mosquitoes relative to an average
     * adult (including heterogeneity and age effects).
     *
     * Used to drive a simulation from an input EIR.
     * Is relativeAvailabilityHet()*relativeAvailabilityAge(ageYears).
     * 
     * Mean output is less than 1.0 (roughly 1.0/invMeanPopAvail).
     */
    inline double relative_availability_het_age(double age_years) const {
        return relative_availability_het * relative_availability_age(age_years);
    }

    /** Availability rate of human to mosquitoes (α_i). Equals 
     * entoAvailabilityHetVecItv()*getRelativeAvailability().
     *
     * To be clear, this includes effects from HetVecItc (het, interv, availability
     * rate) as well as age (avail. relative to an adult). It does not divide
     * by the average availability of the population, which was incorrectly done
     * in the past. */
    inline double ento_availability_full(size_t species, double age_years) const {
        return ento_availability_het_vec_itv(species) * relative_availability_age(age_years);
    }
    //@}

    /** Availability of host to mosquitoes (α_i) excluding age factor
     * 
     * (Includes heterogeneity, intervention, and human-to-vector availability
     * rate factors.)
     * 
     * Assume mean is human-to-vector availability rate factor. */
    double ento_availability_het_vec_itv(size_t species) const;

    ///@brief Get effects of interventions pre/post biting
    //@{
    /** Probability of a mosquito succesfully biting a host (P_B_i). */
    double prob_mosq_biting(size_t species) const;
    /** Probability of a mosquito succesfully finding a resting
     * place after biting and then resting (P_C_i * P_D_i). */
    double prob_mosq_resting(size_t species) const;
    /** Multiplicative factor for the number of fertile eggs laid by mosquitoes
     * after feeding on this host. Should be 1 normally, less than 1 to reduce
     * fertility, greater than 1 to increase. */
    double rel_mosq_fecundity(size_t species) const;
    //@}

    ///@brief Convenience wrappers around several functions
    //@{
    /// entoAvailabilityHetVecItv * probMosqBiting
    inline double avail_bite(size_t species) const {
        return ento_availability_het_vec_itv(species) * prob_mosq_biting(species);
    }
    //@}

    ///@brief Miscellaneous
    //@{
    /** Get whether the user has any active deployments of interventions of
     * the given type, where type is one of those interventions deriving the
     * PerHostInterventionData class (in other cases this will always return
     * false). */
    bool has_active_interv(interventions::component_types::Type type,
                           const interventions::InterventionManager& interventions) const;

    /// Checkpointing
    void operator&(ostream& stream) {
        checkpoint_fields(stream);
        checkpoint_intervs(stream);
    }
    void checkpoint(istream& stream, const interventions::InterventionManager& interventions) {
        checkpoint_fields(stream);
        checkpoint_intervs(stream, interventions);
    }
    //@}

    // Determines whether human is outside transmission
    bool outside_transmission{false};

    // Heterogeneity factor in availability; this is already multiplied into the
    // entoAvailability param stored in HostMosquitoInteraction.
    double relative_availability_het;

    /** Species availability rate of human to mosquitoes */
    vector<double> anoph_ento_availability_raw;

    /** Species availability rate of human to mosquitoes, including heterogeneity factor
     * and base rate, but excluding age and intervention factors. */
    vector<double> anoph_ento_availability;

    /** Species probability of mosquito successfully biting host (P_B_i) in the absense of
     * interventions. */
    vector<double> anoph_prob_mosq_biting;

    /** Species probability of mosquito escaping human and finding a resting site, then
     * resting without dying, after biting the human (P_C_i * P_D_i) in the
     * absense of interventions. */
    vector<double> anoph_prob_mosq_resting;

    /** @brief gaussian sample used in the deployment of intervention 
     * components using the gaussian copula transformation. The factors are 
     * component specific and are sampled once per human during the first 
     * deployment attempt. */
    std::map<interventions::ComponentId, double> copula_gaussian_samples;

private:
    template <class S> void checkpoint_fields(S& stream) {
        anoph_ento_availability_raw & stream;
        anoph_ento_availability & stream;
        anoph_prob_mosq_biting & stream;
        anoph_prob_mosq_resting & stream;
        relative_availability_het & stream;
        outside_transmission & stream;
        copula_gaussian_samples & stream;
    }
    void checkpoint_intervs(ostream& stream);
    void checkpoint_intervs(istream& stream, const interventions::InterventionManager& interventions);

    vector<unique_ptr<PerHostInterventionData>> active_components;

    const PerHostParams* params = nullptr;
};

}

}
#endif
