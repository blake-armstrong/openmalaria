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

// Note: this file is included by exactly one source (InterventionManager.cpp)
// and contains definitions as well as declarations.

// The includes here are more for documentation than required.
#include "Global.h"
#include "util/errors.h"
#include "Clinical/ClinicalModel.h"
#include "Clinical/CMDecisionTree.h"
#include "Population.h"
#include "Transmission/TransmissionModel.h"
#include "Transmission/VectorModel.h"
#include "Transmission/PerHost.h"
#include "util/random.h"
#include "util/errors.h"
#include <schema/interventions.h>
#include <format>

#include <utility>

#include "util/SpeciesIndexChecker.h"
#include "Host/Human.h"

namespace OM::interventions {

using Host::Human;

struct ByDeployTime; // forward decl for friend

// ———  TimedDeployment and derivatives  ———

/** Interface for timed deployment of an intervention. */
class TimedDeployment {
public:
    /// Create, passing time of deployment
    explicit TimedDeployment(SimTime deploy_date) : date(deploy_date) {
        if (deploy_date < Sim::start_date()) {
            throw util::xml_scenario_error("timed intervention deployment: may not be negative");
        } else if (deploy_date >= Sim::end_date()) {
            cerr << "Warning: timed intervention deployment at date " << deploy_date;
            cerr << " happens after last survey" << endl;
        }
    }
    virtual ~TimedDeployment() = default;

    virtual void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) = 0;

    virtual void print_details(std::ostream& out) const = 0;

    // Read access required in this file; don't really need protection:
    SimTime date = Sim::never();
};

class DummyTimedDeployment : public TimedDeployment {
public:
    DummyTimedDeployment() : TimedDeployment(Sim::start_date() /* hack */) {
        // TimedDeployment's ctor checks that the deployment time-step is
        // within the intervention period. We want this time to be after the
        // last time-step, so set the time here after TimedDeployment's ctor
        // check has been done (hacky).
        date = Sim::future();
    }
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {}
    void print_details(std::ostream& out) const override { out << "Dummy"; }
};

class TimedChangeHSDeployment : public TimedDeployment {
public:
    TimedChangeHSDeployment(SimTime date, const scnXml::ChangeHS::TimedDeploymentType& hs,
                            Clinical::ClinicalParams& clinical, Clinical::DecisionTreeLibrary& decision_trees,
                            WithinHost::WithinHostModel& within_host, const InterventionManager& interventions,
                            const Clock& clock, const util::CommandLineOptions& command_line)
        : TimedDeployment(date), new_hs(hs._clone()), clinical(&clinical), decision_trees(&decision_trees),
          within_host(&within_host), interventions(&interventions), clock(&clock), command_line(&command_line) {}
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        clinical->set_health_system(*new_hs, *decision_trees, *within_host, *interventions, *clock, *command_line);
        new_hs.reset();
    }
    void print_details(std::ostream& out) const override { out << "ChangeHS"; }

private:
    unique_ptr<scnXml::HealthSystem> new_hs;
    Clinical::ClinicalParams* clinical;
    Clinical::DecisionTreeLibrary* decision_trees;
    WithinHost::WithinHostModel* within_host;
    const InterventionManager* interventions;
    const Clock* clock;
    const util::CommandLineOptions* command_line;
};

class TimedChangeEIRDeployment : public TimedDeployment {
public:
    TimedChangeEIRDeployment(SimTime date, const scnXml::ChangeEIR::TimedDeploymentType& nv)
        : TimedDeployment(date), new_eir(nv._clone()) {}
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        transmission.change_eir_intervention(*new_eir);
        new_eir.reset();
    }
    void print_details(std::ostream& out) const override { out << "ChangeEIR"; }

private:
    unique_ptr<scnXml::NonVector> new_eir;
};

class TimedUninfectVectorsDeployment : public TimedDeployment {
public:
    TimedUninfectVectorsDeployment(SimTime date) : TimedDeployment(date) {}
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        transmission.uninfect_vectors();
    }
    void print_details(std::ostream& out) const override { out << "UninfectVectors"; }
};

void VaccineLimits::set(const scnXml::DeploymentBase& deploy) {
    if (deploy.getVaccMinPrevDoses().present()) {
        const int v = deploy.getVaccMinPrevDoses().get();
        if (v < 0)
            throw util::xml_scenario_error("vaccMinPrevDoses: min value is 0");
        min_prev_doses = v;
    }
    if (deploy.getVaccMaxCumDoses().present()) {
        const int v = deploy.getVaccMaxCumDoses().get();
        if (v < 0)
            throw util::xml_scenario_error("vaccMaxCumDoses: min value is 0");
        max_cum_doses = v;
    }
}

/// Base class for TimedHumanDeployment and ContinuousHumanDeployment
class HumanDeploymentBase {
protected:
    /**
     * @param deploy XML element describing deployment
     * @param intervention The intervention to deploy (list of components)
     * @param subPop Either ComponentId::wholePop() or a sub-population to which deployment is restricted
     * @param complement Whether to take the complement of the sub-population
     *  to which deployment will be restricted
     */
    HumanDeploymentBase(const scnXml::DeploymentBase& deploy, shared_ptr<const HumanIntervention> intervention,
                        ComponentId sub_pop, bool complement, const Transmission::PerHostParams& per_host_params)
        : coverage(deploy.getCoverage()), sub_pop(sub_pop), complement(complement),
          intervention(std::move(intervention)), host_params(&per_host_params) {
        if (!(coverage >= 0.0 && coverage <= 1.0)) {
            throw util::xml_scenario_error("intervention deployment coverage must be in range [0,1]");
        }
        // If coverageCorr or coverageVar is present, then the user is asking for the copula
        if (deploy.getCoverageCorr().present() || deploy.getCoverageVar().present()) {
            if (!(deploy.getCoverageCorr().present() && deploy.getCoverageVar().present()))
                throw std::runtime_error("[Gaussian copula]: both coverageCorr and coverageVar must be present");

            if (deploy.getCoverageVar().get() < 0)
                throw std::runtime_error("[Gaussian copula]: negative variance is not allowed");

            // if variance is 0, then no copula and fall back to probability = coverage
            if (deploy.getCoverageVar().get() > 0) {
                coverage_corr = deploy.getCoverageCorr().get();
                coverage_var = deploy.getCoverageVar().get();
                copula = true;
            }
        }

        vacc_limits.set(deploy);
    }

    inline void deploy_to_human(Host::Human& human, mon::DeployMethod method) const {
        intervention->deploy(human, method, vacc_limits);
    }

    bool copula = false;
    double coverage; // proportion coverage within group meeting above restrictions
    double coverage_corr = 0.0, coverage_var = 0.0;
    VaccineLimits vacc_limits;
    ComponentId sub_pop; // ComponentId::wholePop() if deployment is not restricted to a sub-population
    bool complement;
    shared_ptr<const HumanIntervention> intervention;

    const Transmission::PerHostParams& per_host_params() const { return *host_params; }

private:
    const Transmission::PerHostParams* host_params;
};

/// Timed deployment of human-specific interventions
class TimedHumanDeployment : public TimedDeployment, protected HumanDeploymentBase {
public:
    /** 
     * @param date Time of deployment
     * @param mass XML element specifying the age range and compliance
     * (proportion of eligible individuals who receive the intervention).
     * @param intervention The HumanIntervention to deploy.
     * @param subPop Either ComponentId::wholePop() or a sub-population to which deployment is restricted
     */
    TimedHumanDeployment(SimTime date, const scnXml::MassDeployment& mass,
                         shared_ptr<const HumanIntervention> intervention, ComponentId sub_pop, bool complement,
                         const Transmission::PerHostParams& per_host_params)
        : TimedDeployment(date),
          HumanDeploymentBase(mass, std::move(intervention), sub_pop, complement, per_host_params),
          min_age(Sim::from_years_n(mass.getMinAge())), max_age(Sim::future()) {
        if (mass.getMaxAge().present())
            max_age = Sim::from_years_n(mass.getMaxAge().get());

        if (min_age < Sim::zero() || max_age < min_age)
            throw util::xml_scenario_error(
                "timed intervention must have 0 <= minAvailability <= maxAvailability <= 100");

        if (mass.getMaxAvailability().present())
            max_availability = mass.getMaxAvailability().get();

        if (mass.getMinAvailability().present())
            min_availability = mass.getMinAvailability().get();

        if (min_availability < Sim::zero() || max_availability < min_availability)
            throw util::xml_scenario_error(
                "timed intervention must have 0 <= minAvailability <= maxAvailability <= 100");
    }

    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        for (Human& human : population) {
            const SimTime age = human.age(Sim::now());
            if (age >= min_age && age < max_age) {
                double availability = 0.0;
                for (size_t i = 0; i < per_host_params().num_species(); ++i)
                    availability += human.per_host_transmission.anoph_ento_availability[i];

                double probability = coverage;

                if (copula && coverage > 0) {
                    try {
                        // Beta distribution for intervention (Beta distributed)
                        const double beta_mean = coverage;    // Assuming this value is given
                        const double beta_var = coverage_var; // Given as well
                        const double alpha = ((1.0 - beta_mean) / beta_var - 1.0 / beta_mean) * (beta_mean * beta_mean);
                        const double beta = alpha * (1.0 / beta_mean - 1.0);

                        if (per_host_params().num_species() > 1)
                            throw std::runtime_error("only supports one mosquito species");

                        const double ux = per_host_params().get(0).ento_availability->cdf(
                            human.per_host_transmission.anoph_ento_availability_raw[0]);
                        if (ux < 1) {
                            const double xx = gsl_cdf_ugaussian_Pinv(ux); // Unit interval to Normal

                            // Apply the Gaussian copula transformation for correlation
                            const ComponentId cid = sub_pop;

                            /** human.intervention_rng.gauss(0.0, 1.0) is calculated once per component and per human. Re-deployment of
                             * the same component on the same human must use the same gaussian sample. Therefore we store
                             * this value in the human the first time it is calculated. */
                            double g = 0.0;
                            const auto it = human.per_host_transmission.copula_gaussian_samples.find(cid);
                            if (it != human.per_host_transmission.copula_gaussian_samples.end())
                                g = it->second;
                            else {
                                g = human.intervention_rng.gauss(0.0, 1.0);
                                human.per_host_transmission.copula_gaussian_samples[cid] = g;
                            }
                            const double yy = coverage_corr * xx + g * sqrt(1 - coverage_corr * coverage_corr);

                            // Transform back to unit interval
                            const double uy = gsl_cdf_ugaussian_P(yy);

                            if (alpha < 0 || beta < 0)
                                throw std::runtime_error("resulting alpha and beta parameters must be positive");

                            // Get the final probability from Beta distribution
                            probability = gsl_cdf_beta_Pinv(uy, alpha, beta);
                        } else // Gamma with CV=0
                            probability = human.intervention_rng.beta(alpha, beta);
                    } catch (const std::exception& e) {
                        throw std::runtime_error(
                            std::format("[Gaussian copula]: computing correlated intervention probability using "
                                        "Gaussian copula and Beta distribution: {}. Possible reason: coverage might be "
                                        "too high and/or population size might be too small.",
                                        e.what()));
                    }
                }

                if (availability >= per_host_params().ento_availability_percentile(min_availability) &&
                    availability <= per_host_params().ento_availability_percentile(max_availability)) {
                    if (sub_pop == ComponentId::whole_pop() || (human.is_in_sub_pop(sub_pop) != complement)) {
                        if (human.intervention_rng.bernoulli(probability)) {
                            deploy_to_human(human, mon::DeployMethod::Timed);
                        }
                    }
                }
            }
        }
    }

    void print_details(std::ostream& out) const override {
        out << "Human:" << endl;
        out << "\tage: " << Sim::in_years(min_age) << "y\tmax age: " << Sim::in_years(max_age) << "y\tsubpop: ";
        if (sub_pop == ComponentId::whole_pop())
            out << "whole";
        else
            out << sub_pop.id;
        out << "\tcomplement: " << complement << "\tcoverage: " << coverage << "\t";
        intervention->print_details(out);
    }

protected:
    // restrictions on deployment
    SimTime min_age, max_age;
    SimTime min_availability{0}, max_availability{100};
};

/// Timed deployment of human-specific interventions in cumulative mode
class TimedCumulativeHumanDeployment : public TimedHumanDeployment {
public:
    /** 
     * @param date Time of deployment
     * @param mass XML element specifying the age range and compliance
     * (proportion of eligible individuals who receive the intervention).
     * @param intervention The HumanIntervention to deploy.
     * @param subPop Either ComponentId::wholePop() or a sub-population to which deployment is restricted
     * @param cumCuvId Id of component to test coverage for
     */
    TimedCumulativeHumanDeployment(SimTime date, const scnXml::MassDeployment& mass,
                                   shared_ptr<const HumanIntervention> intervention, ComponentId sub_pop,
                                   bool complement, ComponentId cum_cuv_id,
                                   const Transmission::PerHostParams& per_host_params)
        : TimedHumanDeployment(date, mass, std::move(intervention), sub_pop, complement, per_host_params),
          cum_cov_ind(cum_cuv_id) {}

    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        // Cumulative case: bring target group's coverage up to target coverage
        vector<Host::Human*> unprotected;
        size_t total = 0; // number of humans within age bound and optionally subPop
        for (Host::Human& human : population) {
            const SimTime age = human.age(Sim::now());
            if (age >= min_age && age < max_age) {
                double availability = 0.0;
                for (size_t i = 0; i < per_host_params().num_species(); ++i)
                    availability += human.per_host_transmission.anoph_ento_availability[i];
                if (availability >= per_host_params().ento_availability_percentile(min_availability) &&
                    availability <= per_host_params().ento_availability_percentile(max_availability)) {
                    if (sub_pop == ComponentId::whole_pop() || (human.is_in_sub_pop(sub_pop) != complement)) {
                        total += 1;
                        if (!human.is_in_sub_pop(cum_cov_ind))
                            unprotected.push_back(&human);
                    }
                }
            }
        }

        if (total == 0)
            return; // no humans to deploy to; avoid divide by zero
        const double prop_protected = static_cast<double>(total - unprotected.size()) / static_cast<double>(total);
        if (prop_protected < coverage) {
            // Proportion propProtected are already covered, so need to
            // additionally cover the proportion (coverage - propProtected),
            // selected from the list unprotected.
            const double additional_coverage = (coverage - prop_protected) / (1.0 - prop_protected);
            cerr << "cum deployment: prop protected " << prop_protected << "; additionalCoverage "
                 << additional_coverage << "; total " << total << endl;
            for (Human* human : unprotected) {
                if (human->intervention_rng.uniform_01() < additional_coverage) {
                    deploy_to_human(*human, mon::DeployMethod::Timed);
                }
            }
        }
    }

protected:
    ComponentId cum_cov_ind;
};

class TimedVectorDeployment : public TimedDeployment {
public:
    TimedVectorDeployment(SimTime date, size_t instance) : TimedDeployment(date), inst(instance) {}
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model)
            vector_model->deploy_vector_pop_interv(inst);
    }
    void print_details(std::ostream& out) const override { out << "Vector"; }

private:
    size_t inst;
};

class TimedTrapDeployment : public TimedDeployment {
public:
    TimedTrapDeployment(SimTime date, size_t instance, double ratio, SimTime lifespan)
        : TimedDeployment(date), inst(instance), ratio(ratio), lifespan(lifespan) {}
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            const double number = static_cast<double>(population.size()) * ratio;
            vector_model->deploy_vector_trap(inst, number, lifespan);
        }
    }
    void print_details(std::ostream& out) const override {
        out << "VectorTrap:" << endl;
        out << "\tratio: " << ratio << "\tlifespan: " << lifespan;
    }

private:
    size_t inst;
    double ratio;
    SimTime lifespan = Sim::never();
};

class TimedNonHumanHostsDeployment : public TimedDeployment {
public:
    TimedNonHumanHostsDeployment(SimTime date, size_t instance, const string& interv_name,
                                 const scnXml::Description2::AnophelesSequence& list,
                                 const scnXml::DecayFunction& decay, Transmission::TransmissionModel& transmission,
                                 const Clock& clock, const util::CommandLineOptions& command_line)
        : TimedDeployment(date), instance(instance), interv_name(interv_name) {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            util::SpeciesIndexChecker checker(interv_name, vector_model->species_index);
            for (const scnXml::NonHumanHostsSpeciesIntervention& anoph : list) {
                const string& mosq_name = anoph.getMosquito();
                const size_t i = checker.getIndex(mosq_name);

                Transmission::Anopheles::AnophelesModel* anoph_model = vector_model->species[i].get();

                if (anoph_model->reduce_nhh_availability[interv_name].size() <= instance)
                    anoph_model->reduce_nhh_availability[interv_name].resize(instance + 1);
                if (anoph_model->reduce_p_b_i[interv_name].size() <= instance)
                    anoph_model->reduce_p_b_i[interv_name].resize(instance + 1);
                if (anoph_model->reduce_p_c_i[interv_name].size() <= instance)
                    anoph_model->reduce_p_c_i[interv_name].resize(instance + 1);
                if (anoph_model->reduce_p_d_i[interv_name].size() <= instance)
                    anoph_model->reduce_p_d_i[interv_name].resize(instance + 1);
                if (anoph_model->reduce_fecundity[interv_name].size() <= instance)
                    anoph_model->reduce_fecundity[interv_name].resize(instance + 1);

                if (anoph.getAvailabilityReduction().present()) {
                    const scnXml::AvailabilityReduction& decay_func = anoph.getAvailabilityReduction().get();
                    if (decay_func.getInitial() > 1.0)
                        throw util::xml_scenario_error(
                            "availabilityReduction intervention: initial effect must be <= 1");
                    anoph_model->reduce_nhh_availability[interv_name][instance].set(
                        decay_func.getInitial(), decay, "availabilityReduction", clock, command_line);
                }
                if (anoph.getPreprandialKillingEffect().present()) {
                    const scnXml::PreprandialKillingEffect& decay_func = anoph.getPreprandialKillingEffect().get();
                    if (decay_func.getInitial() < 0 || decay_func.getInitial() > 1)
                        throw util::xml_scenario_error(
                            "PreprandialKillingEffect intervention: initial effect must be between 0 and 1");
                    anoph_model->reduce_p_b_i[interv_name][instance].set(decay_func.getInitial(), decay, "reduceP_B_I",
                                                                         clock, command_line);
                }
                if (anoph.getPostprandialKillingEffect().present()) {
                    const scnXml::PostprandialKillingEffect& decay_func = anoph.getPostprandialKillingEffect().get();
                    if (decay_func.getInitial() < 0 || decay_func.getInitial() > 1)
                        throw util::xml_scenario_error(
                            "PostprandialKillingEffect intervention: initial effect must be between 0 and 1");
                    anoph_model->reduce_p_c_i[interv_name][instance].set(decay_func.getInitial(), decay, "reduceP_C_I",
                                                                         clock, command_line);
                }
                if (anoph.getRestingKillingEffect().present()) {
                    const scnXml::RestingKillingEffect& decay_func = anoph.getRestingKillingEffect().get();
                    if (decay_func.getInitial() < 0 || decay_func.getInitial() > 1)
                        throw util::xml_scenario_error(
                            "RestingKillingEffect intervention: initial effect must be be between 0 and 1");
                    anoph_model->reduce_p_d_i[interv_name][instance].set(decay_func.getInitial(), decay, "reduceP_D_I",
                                                                         clock, command_line);
                }
                if (anoph.getFecundityReduction().present()) {
                    const scnXml::FecundityReduction& decay_func = anoph.getFecundityReduction().get();
                    if (decay_func.getInitial() < 0 || decay_func.getInitial() > 1)
                        throw util::xml_scenario_error(
                            "FecundityReduction intervention: initial effect must be be between 0 and 1");
                    anoph_model->reduce_fecundity[interv_name][instance].set(decay_func.getInitial(), decay,
                                                                             "reduceFecundity", clock, command_line);
                }
            }
            checker.checkNoneMissed();
        }
    }
    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            for (size_t i = 0; i < vector_model->species_index.size(); ++i) {
                Transmission::Anopheles::AnophelesModel* anoph_model = vector_model->species[i].get();

                if (anoph_model->nhh_instances.count(interv_name) == 0)
                    throw util::xml_scenario_error("non human hosts type " + interv_name +
                                                   " not deployed during non human hosts intervention deployment");

                anoph_model->reduce_nhh_availability[interv_name][instance].deploy(vector_model->rng, Sim::now());
                anoph_model->reduce_p_b_i[interv_name][instance].deploy(vector_model->rng, Sim::now());
                anoph_model->reduce_p_c_i[interv_name][instance].deploy(vector_model->rng, Sim::now());
                anoph_model->reduce_p_d_i[interv_name][instance].deploy(vector_model->rng, Sim::now());
                anoph_model->reduce_fecundity[interv_name][instance].deploy(vector_model->rng, Sim::now());
            }
        }
    }
    void print_details(std::ostream& out) const override { out << "NonHumanHosts(" << interv_name << ")"; }

private:
    size_t instance;
    string interv_name;
};

constexpr const char* VecModeErr =
    "vector interventions can only be used in dynamic transmission mode (mode=\"dynamic\")";

class TimedAddNonHumanHostsDeployment : public TimedDeployment {
public:
    TimedAddNonHumanHostsDeployment(SimTime date, const string& interv_name, SimTime lifespan,
                                    const scnXml::Description3::AnophelesSequence& list,
                                    Transmission::TransmissionModel& transmission,
                                    const Transmission::PerHostParams& per_host_params)
        : TimedDeployment(date), interv_name(interv_name), lifespan(lifespan), per_host_params(&per_host_params) {
        Transmission::VectorModel const* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            util::SpeciesIndexChecker checker(interv_name, vector_model->species_index);
            for (const scnXml::NonHumanHostsVectorSpecies& anoph : list) {
                const string& mosq_name = anoph.getMosquito();
                checker.getIndex(mosq_name);

                NhhParamsInterv nhh{};
                nhh.mosq_relative_availability_human = anoph.getMosqRelativeAvailabilityHuman().getValue();
                nhh.mosq_prob_biting = anoph.getMosqProbBiting().getValue();
                nhh.mosq_prob_finding_rest_site = anoph.getMosqProbFindRestSite().getValue();
                nhh.mosq_prob_resting = anoph.getMosqProbResting().getValue();
                nhh.host_fecundity_factor = anoph.getHostFecundityFactor().getValue();
                nhh_params[mosq_name] = nhh;
            }
            checker.checkNoneMissed();
        }
    }

    void deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) override {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            const auto pop_size = static_cast<double>(population.size());
            if (vector_model->intervention_mode != Transmission::SimulationMode::DynamicEir) {
                throw util::xml_scenario_error(VecModeErr);
            }
            for (size_t i = 0; i < vector_model->species_index.size(); ++i) {
                Transmission::Anopheles::AnophelesModel* anoph_model = vector_model->species[i].get();

                if (anoph_model->nhh_instances.count(interv_name) != 0)
                    throw util::xml_scenario_error("non human hosts type " + interv_name +
                                                   " already deployed during non human hosts deployment");

                const NhhParamsInterv& p = nhh_params[anoph_model->mosq.name];
                const double adult_avail = per_host_params->get(i).ento_availability->mean();
                const double avail_i = per_host_params->get(i).ento_availability_factor * pop_size * adult_avail *
                                       p.mosq_relative_availability_human;
                Transmission::Anopheles::Nhh nhh;

                nhh.avail_i = avail_i;
                nhh.p_b_i = p.mosq_prob_biting;
                nhh.p_c_i = p.mosq_prob_finding_rest_site;
                nhh.p_d_i = p.mosq_prob_resting;
                nhh.rel_fecundity = p.host_fecundity_factor;
                nhh.expiry = Sim::now() + lifespan;

                // add the nhh to the active nhh instances
                anoph_model->nhh_instances[interv_name] = nhh;
            }
        }
    }
    void print_details(std::ostream& out) const override {
        out << "AddNonHumanHosts(" << interv_name << "):" << endl;
        out << "\tlifespan: " << lifespan;
    }

private:
    struct NhhParamsInterv {
        double mosq_relative_availability_human;
        double mosq_prob_biting;
        double mosq_prob_finding_rest_site;
        double mosq_prob_resting;
        double host_fecundity_factor;
    };
    string interv_name;
    SimTime lifespan = Sim::never();
    std::map<string, NhhParamsInterv> nhh_params;
    const Transmission::PerHostParams* per_host_params;
};

// ———  ContinuousHumanDeployment  ———

/** Interface for continuous deployment of an intervention. */
class ContinuousHumanDeployment : protected HumanDeploymentBase {
public:
    /// Create, passing deployment age
    ContinuousHumanDeployment(SimTime begin, SimTime end, const ::scnXml::ContinuousDeployment& elt,
                              shared_ptr<const HumanIntervention> intervention, ComponentId sub_pop, bool complement,
                              const Transmission::PerHostParams& per_host_params)
        : HumanDeploymentBase(elt, std::move(intervention), sub_pop, complement, per_host_params), begin(begin),
          end(end), deploy_age(Sim::from_years_n(elt.getTargetAgeYrs())) {
        if (begin < Sim::start_date() || end < begin) {
            throw util::xml_scenario_error("continuous intervention must have startDate <= begin <= end");
        }
        if (deploy_age <= Sim::zero()) {
            throw util::xml_scenario_error(
                std::format("continuous intervention with target age {:g} years corresponds to time step {}; must be "
                            "at least 1.",
                            elt.getTargetAgeYrs(), Sim::in_steps(deploy_age)));
        }
        if (deploy_age > Sim::max_human_age()) {
            ostringstream msg;
            msg << "continuous intervention must have target age no greater than ";
            msg << Sim::in_years(Sim::max_human_age());
            throw util::xml_scenario_error(msg.str());
        }
        if (elt.getMaxAvailability().present())
            max_availability = elt.getMaxAvailability().get();

        if (elt.getMinAvailability().present())
            min_availability = elt.getMinAvailability().get();

        if (min_availability < Sim::zero() || max_availability < min_availability)
            throw util::xml_scenario_error(
                "timed intervention must have 0 <= minAvailability <= maxAvailability <= 100");
    }

    /** Apply filters and potentially deploy.
     * 
     * @returns false iff this deployment (and thus all later ones in the
     *  ordered list) happens in the future. */
    bool filter_and_deploy(Host::Human& human) const {
        const SimTime age = human.age(Sim::now());
        if (deploy_age > age) {
            // stop processing continuous deployments for this
            // human for now because remaining ones happen in the future
            return false;
        } else if (deploy_age == age) {
            double availability = 0.0;
            for (size_t i = 0; i < per_host_params().num_species(); ++i)
                availability += human.per_host_transmission.anoph_ento_availability_raw[i];

            if (availability >= per_host_params().ento_availability_percentile(min_availability) &&
                availability <= per_host_params().ento_availability_percentile(max_availability)) {
                auto now = Sim::interv_date();
                if (begin <= now && now < end &&
                    (sub_pop == ComponentId::whole_pop() || (human.is_in_sub_pop(sub_pop) != complement)) &&
                    human.intervention_rng.uniform_01() < coverage) // RNG call should be last test
                {
                    deploy_to_human(human, mon::DeployMethod::Cts);
                }
            }
        } //else: for some reason, a deployment age was missed; ignore it
        return true;
    }

    inline void print_details(std::ostream& out) const {
        out << "Human:" << endl;
        out << "\tbegin: " << begin << "\tend: ";
        if (end == Sim::future())
            out << "(none)";
        else
            out << end;
        out << "\tdeployAge: " << Sim::in_years(deploy_age) << "y\tsubpop: ";
        if (sub_pop == ComponentId::whole_pop())
            out << "whole";
        else
            out << sub_pop.id;
        out << "\tcomplement: " << complement << "\tcoverage: " << coverage << "\t";
        intervention->print_details(out);
    }

protected:
    SimTime begin, end; // first time step active and first time step no-longer active
    SimTime deploy_age = Sim::never();
    SimTime min_availability{0}, max_availability{100};

    friend ByDeployTime;
};

struct ByDeployTime {
    bool operator()(const unique_ptr<TimedDeployment>& a, const unique_ptr<TimedDeployment>& b) const {
        return a->date < b->date;
    }

    bool operator()(const ContinuousHumanDeployment& a, const ContinuousHumanDeployment& b) const {
        return a.deploy_age < b.deploy_age;
    }
};

}
