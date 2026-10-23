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

#include "Global.h"
#include "Transmission/TransmissionModel.h"
#include "Transmission/Anopheles/AnophelesModel.h"
#include "Transmission/PerHost.h"
#include "Population.h"
#include "Host/WithinHost/Genotypes.h"
#include "util/vectors.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "util/StreamValidator.h"

#include <cmath>
#include <utility>

#include <cstdio>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_roots.h>

struct QuadraticParams {
    double ah;          // The availability rate of hosts h. Time-1
    double an;          // The availability rate of hosts n. Time-1
    double mu_v_interv; // Mosquito mortality rate while host-seeking. Time-1
    double theta_d;     // Proportion of the night that the mosquito spends host-seeking. Time.
    double p_at_wanted; // PAt is the daily probability of a mosquito feeding on an ATSB
};

double quadratic_f(double x, void* params) {
    const auto* p = static_cast<const QuadraticParams*>(params);

    const double ah = p->ah;
    const double an = p->an;
    const double mu_v_interv = p->mu_v_interv;
    const double theta_d = p->theta_d;
    const double p_at_wanted = p->p_at_wanted;

    const double tmp = (x + ah + an + mu_v_interv);

    return (1.0 - exp(-tmp * theta_d)) * x / tmp - p_at_wanted;
}

double quadratic_df(double x, void* params) {
    const auto* p = static_cast<const QuadraticParams*>(params);

    const double ah = p->ah;
    const double an = p->an;
    const double mu_v_interv = p->mu_v_interv;
    const double theta_d = p->theta_d;

    const double tmp = (x + ah + an + mu_v_interv);

    const double a = (1.0 - exp(-tmp * theta_d)) * x / (tmp * tmp);
    const double b = (1.0 - exp(-tmp * theta_d)) / tmp;
    const double c = exp(-tmp * theta_d) * x * theta_d / tmp;

    return -a + b + c;
}

void quadratic_fdf(double x, void* params, double* y, double* dy) {
    const auto* p = static_cast<const QuadraticParams*>(params);

    const double ah = p->ah;
    const double an = p->an;
    const double mu_v_interv = p->mu_v_interv;
    const double theta_d = p->theta_d;
    const double p_at_wanted = p->p_at_wanted;

    const double tmp = (x + ah + an + mu_v_interv);

    const double a = (1.0 - exp(-tmp)) * x / (tmp * tmp);
    const double b = (1.0 - exp(-tmp)) / tmp;
    const double c = exp(-tmp * theta_d) * x * theta_d / tmp;

    *y = (1.0 - exp(-tmp * theta_d)) * x / tmp - p_at_wanted;
    *dy = -a + b + c;
}

#define USE_DERIVATIVE

#ifdef USE_DERIVATIVE
using GslRootSolverType = gsl_root_fdfsolver_type;
using GslRootSolver = gsl_root_fdfsolver;
auto gsl_root_solver_alloc = gsl_root_fdfsolver_alloc;
auto gsl_root_solver_free = gsl_root_fdfsolver_free;
auto gsl_root_solver_name = gsl_root_fdfsolver_name;
auto gsl_root_solver_iterate = gsl_root_fdfsolver_iterate;
auto gsl_root_solver_root = gsl_root_fdfsolver_root;
// auto gsl_root_solver_method = gsl_root_fdfsolver_newton;
auto gsl_root_solver_method = gsl_root_fdfsolver_secant;
// auto gsl_root_solver_method = gsl_root_fdfsolver_steffenson;
#else
using GslRootSolverType = gsl_root_fsolver_type;
using GslRootSolver = gsl_root_fsolver;
auto gsl_root_solver_alloc = gsl_root_fsolver_alloc;
auto gsl_root_solver_free = gsl_root_fsolver_free;
auto gsl_root_solver_name = gsl_root_fsolver_name;
auto gsl_root_solver_iterate = gsl_root_fsolver_iterate;
auto gsl_root_solver_root = gsl_root_fsolver_root;
// auto gsl_root_solver_method = gsl_root_fsolver_brent;
auto gsl_root_solver_method = gsl_root_fsolver_bisection;
// auto gsl_root_solver_method = gsl_root_fsolver_falsepos;
#endif

namespace OM::Transmission::Anopheles {
using namespace OM::util;

// -----  Initialisation of model, done before human warmup  ------

void AnophelesModel::initialise(size_t species, MosquitoParams mosq_params, const PerHostParams& per_host,
                                const WithinHost::GenotypeRegistry& genotype_registry) {
    mosq = std::move(mosq_params);
    per_host_params = &per_host;
    genotypes = &genotype_registry;

    n_v_length = (mosq.eip_duration + mosq.rest_duration);

    // -----  allocate memory  -----
    // Set up fArray and ftauArray. Each step, all elements not set here are
    // calculated, even if they aren't directly used in the end;
    // however all calculated values are used in calculating the next value.
    f_array.resize((mosq.eip_duration - mosq.rest_duration + Sim::one_day()));
    f_array[Sim::zero()] = 1.0;
    ftau_array.resize(mosq.eip_duration);
    for (int i = 0; i < mosq.rest_duration; i += 1) {
        ftau_array[i] = 0.0;
    }
    ftau_array[mosq.rest_duration] = 1.0;
    uninfected_v.resize(n_v_length);
    uninfected_v[Sim::zero()] = numeric_limits<double>::quiet_NaN(); // index not used
}

void AnophelesModel::init_availability(size_t species, const vector<NhhParams>& nhhs, int population_size,
                                       PerHostParams& per_host) {
    // A: Host seeking
    // Proportion of host-seeking parous mosquitoes (those who have laid eggs)
    // which laid eggs that day:
    const double a0 = mosq.laid_eggs_same_day_proportion;
    // Probability that the mosquito survives the feeding cycle.
    // Note: Pf = M, the parous rate (prop mosqs which have laid eggs):
    const double pf = mosq.survival_feeding_cycle_probability;
    const double human_blood_index = mosq.human_blood_index; // χ (chi)
    // Cycle probabilities, when biting a human:
    // B: Host encountered
    const double p_b1 = mosq.prob_biting;
    // C: Fed
    const double p_c1 = mosq.prob_find_rest_site;
    // D: Resting
    const double p_d1 = mosq.prob_resting;
    // E: Laying eggs (ovipositing)
    const double p_e1 = mosq.prob_ovipositing;

    // -----  Calculate P_A, P_A1, P_A_n  -----
    // P_A is prob that a mosq is still host seeking after 1 day. It is also the
    // proportion of parous mosquitoes who have waited at least 1 day since
    // laying, thus 1 - P_A = A0.
    const double init_p_a = 1.0 - a0;
    // This is multiplied by the probability of encountering some type of host
    // on a given night is the total availability of this type of host (N_i * α_i).
    // Note: P_Ai = α_i * N_i / availFactor
    const double avail_factor = -log(init_p_a) / (mosq.seeking_duration * (1.0 - init_p_a));
    // Probability that a mosquito encounters a human on a given night
    double p_a1 = numeric_limits<double>::quiet_NaN();
    // Probability that a mosquito encounters any non human host on a given night
    // (confusingly labelled P_Ah in paper)
    double p_ah = numeric_limits<double>::quiet_NaN();

    if (nhhs.empty()) {
        // Number of non-human hosts: χ=1
        p_a1 = a0 * pf / (p_b1 * p_c1 * p_d1 * p_e1);
        p_ah = 0.0;
    } else {
        // Have non-human hosts: χ<1

        // let v = χ * P_D_1 * P_E_1; note that this is the average for humans
        const double v = human_blood_index * p_d1 * p_e1;
        const double chi1 = 1.0 - human_blood_index; // 1 - χ

        double sum_xi = 0.0;  // sum rel. avail across NNHs; should be 1
        double sum_u = 0.0;   // sum u across NNHs where u = xi * P_B * P_C
        double sum_uvw = 0.0; // sum u*(v+w) across NNHs where w = (1-χ)*P_D*P_E

        for (auto& nhh : nhhs) {
            // availability population of hosts of this type relative to other non-human hosts:
            const double xi_i = nhh.mosq_relative_ento_availability;
            // cycle probabilities, when biting this type of host:
            const double p_b_i = nhh.mosq_prob_biting;
            const double p_c_i = nhh.mosq_prob_find_rest_site;
            const double p_d_i = nhh.mosq_prob_resting;
            sum_xi += xi_i;
            const double u_i = xi_i * p_b_i * p_c_i;
            sum_u += u_i;
            const double w_i = chi1 * p_d_i * p_e1; // note: we now assume P_E_i = P_E1
            sum_uvw += u_i * (v + w_i);
        }

        if (!(sum_xi > 0.9999 && sum_xi < 1.0001)) {
            throw xml_scenario_error("The sum of the relative entomological availability (ξ_i) "
                                     "across non-human hosts must be 1!");
        }

        // equations (14), (15) of paper:
        p_a1 = (a0 * pf * human_blood_index * sum_u) / (p_b1 * p_c1 * sum_uvw);
        p_ah = (a0 * pf * chi1) / sum_uvw;
    }

    // -----  Calculate availability rate of hosts (α_i) and non-human population data  -----
    per_host.set_ento_availability_factor(species, (p_a1 / population_size) * avail_factor);

    init_avail = (p_a1 / population_size) * avail_factor;

    nhh_avail = 0.0;
    nhh_sigma_df = 0.0;
    nhh_sigma_dff = 0.0;
    for (auto& nhh : nhhs) {
        // availability population of hosts of this type relative to other non-human hosts:
        const double xi_i = nhh.mosq_relative_ento_availability;
        // cycle probabilities, when biting this type of host:
        const double p_b_i = nhh.mosq_prob_biting;
        const double p_c_i = nhh.mosq_prob_find_rest_site;
        const double p_d_i = nhh.mosq_prob_resting;
        const double rel_fecundity = nhh.host_fecundity_factor;
        const double p_ahi = p_ah * xi_i;            // probability of encountering this type of NNH on a given night
        const double avail_i = p_ahi * avail_factor; // N_i * α_i

        nhh_avail += avail_i;                              // N * α (nhh_avail is not used anymore)
        const double df = avail_i * p_b_i * p_c_i * p_d_i; // term in P_df series
        nhh_sigma_df += df;
        nhh_sigma_dff += df * rel_fecundity;
        // Note: we would do the same for P_dif except that it's multiplied by
        // infectiousness of host to mosquito which is zero.

        const string name = nhh.name;

        Nhh nhhi;
        nhhi.avail_i = avail_i;
        nhhi.p_b_i = p_b_i;
        nhhi.p_c_i = p_c_i;
        nhhi.p_d_i = p_d_i;
        nhhi.rel_fecundity = rel_fecundity;
        nhhi.expiry = Sim::future();

        nhh_instances[name] = nhhi;
    }

    // ———  set mosqSeekingDeathRate  ———
    // Note: sum_k( P_Ak ) = P_A1 + P_Ah
    const double mu1 = (1.0 - (init_p_a + p_a1 + p_ah)) / (1.0 - init_p_a);
    const double mu2 = -log(init_p_a) / mosq.seeking_duration;
    mosq.seeking_death_rate = mu1 * mu2;

    if (mosq.seeking_death_rate <= 0)
        throw xml_scenario_error("Anopheles::init mosq.seekingDeathRate is <= 0. \
The probability of a mosquito surviving the entire feeding cycle \
should be less than the product of the probabilities of a \
mosquito surviving biting, finding a resting site, resting and ovipositing.");
}

/** Called by initialise function to init variables directly related to EIR
 *
 * @param anoph Data from XML
 * @param initialisationEIR In/out parameter: TransmissionModel::initialisationEIR
 * @param EIPDuration parameter from MosqTransmission (used for an estimation)
 */
void AnophelesModel::init_eir(const vector<double>& init_eir365, vector<double> fs_coeffic_init,
                              double eir_rotate_angle_init, double prop_infectious, double prop_infected) {
    fs_coeffic = std::move(fs_coeffic_init);
    eir_rotate_angle = eir_rotate_angle_init;
    species_eir = init_eir365;

    partial_init_eir.resize(Sim::steps_per_year(), 0.0);

    // Add to the TransmissionModel's EIR, used for the initalization phase.
    // Note: sum stays the same, units changes to per-time-step.
    for (SimTime i = Sim::zero(); i < Sim::one_year(); i = i + Sim::one_day()) {
        partial_init_eir[mod_nn(Sim::in_steps(i), Sim::steps_per_year())] += init_eir365[i];
    }

    if (util::CommandLine::option(util::CommandLine::PrintAnnualEir))
        cout << "Annual EIR for " << mosq.name << ": " << vectors::sum(species_eir) << endl;

    // Set other data used for mosqEmergeRate calculation:
    init_nv_from_sv = 1.0 / prop_infectious;
    init_ov_from_sv = init_nv_from_sv * prop_infected;
}

// -----  Initialisation of model which is done after creating initial humans  -----

void AnophelesModel::init2(int n_humans, double mean_pop_avail, double sum_avail, double sigma_f, double sigma_df,
                           double sigma_dff) {
    // -----  Calculate P_A, P_Ai, P_df based on pop age structure  -----

    // ν_A: rate at which mosquitoes find hosts or die (i.e. leave host-seeking state)
    const double leave_rate = sum_avail + nhh_avail + mosq.seeking_death_rate;
    sigma_df += nhh_sigma_df;
    sigma_dff += nhh_sigma_dff;

    // Probability of a mosquito not finding a host this day:
    const double ts_p_a = exp(-leave_rate * mosq.seeking_duration);
    const double avail_divisor = (1.0 - ts_p_a) / leave_rate; // α_d

    // Input per-species EIR is the mean EIR experienced by a human adult.
    // We use sumPFindBite below to get required S_v.
    const double sum_p_find_bite = sigma_f * avail_divisor;
    const double ts_p_df = sigma_df * avail_divisor * mosq.prob_mosq_survival_ovipositing;
    const double ts_p_dff = sigma_dff * avail_divisor * mosq.prob_mosq_survival_ovipositing;

    const double ts_p_amu = (1 - ts_p_a) * mosq.seeking_death_rate / (mosq.seeking_death_rate + sum_avail + nhh_avail);
    const double ts_p_a1 = (1 - ts_p_a) * sum_avail / (mosq.seeking_death_rate + sum_avail + nhh_avail);
    const double ts_p_ah = (1 - ts_p_a) * nhh_avail / (mosq.seeking_death_rate + sum_avail + nhh_avail);
    // for( auto it = nhhInstances.begin(); it != nhhInstances.end(); ++it){
    //     initialP_Ah += (1-initialP_A) * it->second.avail_i / (mosqSeekingDeathRate + sum_avail + nhh_avail);
    // }

    // -----  Calculate required S_v based on desired EIR  -----
    init_nv0_from_sv = init_nv_from_sv * (1.0 - ts_p_a - ts_p_df);
    init_sv_from_eir = n_humans * mean_pop_avail / sum_p_find_bite;

    // We estimate forcedS_v from EIR
    forced_s_v = species_eir;
    vectors::scale(forced_s_v, init_sv_from_eir);

    n_v.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    o_v_i.resize(n_v_length * genotypes->count(), 0.0);
    o_v_l.resize(n_v_length * genotypes->count(), numeric_limits<double>::quiet_NaN());
    s_v_i.resize(n_v_length * genotypes->count(), 0.0);
    s_v_l.resize(n_v_length * genotypes->count(), numeric_limits<double>::quiet_NaN());
    p_a.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    p_df.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    p_dif_i.resize(n_v_length * genotypes->count(), 0.0);
    p_dif_l.resize(n_v_length * genotypes->count(), 0.0);
    p_dff.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    p_amu.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    p_a1.resize(n_v_length, numeric_limits<double>::quiet_NaN());
    p_ah.resize(n_v_length, numeric_limits<double>::quiet_NaN());

    // Initialize per-day variables; S_v, N_v and O_v are only estimated
    assert(n_v_length <= static_cast<int>(forced_s_v.size()));
    for (int t = 0; t < n_v_length; t++) {
        p_a[t] = ts_p_a;
        p_amu[t] = ts_p_amu;
        p_a1[t] = ts_p_a1;
        p_ah[t] = ts_p_ah;
        p_df[t] = ts_p_df;
        p_dff[t] = ts_p_dff;
        n_v[t] = forced_s_v[t] * init_nv_from_sv;
        for (size_t genotype = 0; genotype < genotypes->count(); ++genotype) {
            s_v_l[t * genotypes->count() + genotype] = forced_s_v[t] * genotypes->initial_freq(genotype);
            o_v_l[t * genotypes->count() + genotype] = s_v_l[t * genotypes->count() + genotype] * init_ov_from_sv;
        }
    }

    // Crude estimate of mosqEmergeRate: (1 - P_A(t) - P_df(t)) / (T * ρ_S) * S_T(t)
    mosq_emerge_rate = forced_s_v;
    vectors::scale(mosq_emerge_rate, init_nv0_from_sv);

    // All set up to drive simulation from forcedS_v
}

bool AnophelesModel::init_iterate() { return true; }
//@}

void AnophelesModel::init_vector_interv(const scnXml::VectorSpeciesIntervention& elt, size_t instance) {
    if (emergence_reduction.size() <= instance)
        emergence_reduction.resize(instance + 1);

    if (elt.getEmergenceReduction().present()) {
        const scnXml::EmergenceReduction& elt2 = elt.getEmergenceReduction().get();
        if (elt2.getInitial() > 1.0)
            throw util::xml_scenario_error("emergenceReduction intervention: initial effect must be ≤ 1");
        emergence_reduction[instance].set(elt2.getInitial(), elt2.getDecay(), "emergenceReduction");
    }
    if (prob_additional_death_sugar_feeding_intervs.size() <= instance)
        prob_additional_death_sugar_feeding_intervs.resize(instance + 1);
    if (seeking_death_rate_intervs.size() <= instance)
        seeking_death_rate_intervs.resize(instance + 1);
    if (prob_death_ovipositing_intervs.size() <= instance)
        prob_death_ovipositing_intervs.resize(instance + 1);

    if (elt.getProbAdditionalDeathSugarFeeding().present()) {
        const scnXml::ProbAdditionalDeathSugarFeeding& elt2 = elt.getProbAdditionalDeathSugarFeeding().get();
        if (elt2.getInitial() < -1.0)
            throw util::xml_scenario_error("probAdditionalDeathSugarFeeding intervention: initial effect must be ≥ -1");
        prob_additional_death_sugar_feeding_intervs[instance].set(elt2.getInitial(), elt2.getDecay(),
                                                                  "seekingDeathRateIncrease");
    }
    if (elt.getSeekingDeathRateIncrease().present()) {
        const scnXml::SeekingDeathRateIncrease& elt2 = elt.getSeekingDeathRateIncrease().get();
        if (elt2.getInitial() < -1.0)
            throw util::xml_scenario_error("seekingDeathRateIncrease intervention: initial effect must be ≥ -1");
        seeking_death_rate_intervs[instance].set(elt2.getInitial(), elt2.getDecay(), "seekingDeathRateIncrease");
    }
    if (elt.getProbDeathOvipositing().present()) {
        const scnXml::ProbDeathOvipositing& elt2 = elt.getProbDeathOvipositing().get();
        if (elt2.getInitial() < 0.0 || elt2.getInitial() > 1.0)
            throw util::xml_scenario_error("probDeathOvipositing intrevention: initial effect must be in range [0,1]");
        prob_death_ovipositing_intervs[instance].set(elt2.getInitial(), elt2.getDecay(), "probDeathOvipositing");
    }
}
void AnophelesModel::init_vector_trap(const scnXml::Description1& desc, size_t instance) {
    assert(trap_params.size() == instance); // if triggered, this is a code error not XML
    TrapParams params;
    params.rel_avail = desc.getRelativeAvailability().getValue();
    params.avail_decay = DecayFunction::makeObject(desc.getDecayOfAvailability(), "decayOfAvailability");
    trap_params.push_back(std::move(params));
}

void AnophelesModel::deploy_vector_pop_interv(LocalRng& rng, size_t instance) {
    assert(instance < emergence_reduction.size());
    emergence_reduction[instance].deploy(rng, Sim::now());
    // do same as in above function (of EmergenceModel)
    assert(instance < seeking_death_rate_intervs.size() && instance < prob_death_ovipositing_intervs.size());
    prob_additional_death_sugar_feeding_intervs[instance].deploy(rng, Sim::now());
    seeking_death_rate_intervs[instance].deploy(rng, Sim::now());
    prob_death_ovipositing_intervs[instance].deploy(rng, Sim::now());
}
void AnophelesModel::deploy_vector_trap(LocalRng& rng, size_t species, size_t instance, double pop_size,
                                        SimTime lifespan) {
    assert(instance < trap_params.size());
    TrapData data;
    data.instance = instance;
    const PerHostAnophParams& anoph_params = per_host_params->get(species);
    const double adult_avail = anoph_params.ento_availability->mean() * anoph_params.ento_availability_factor;
    data.initial_avail = pop_size * adult_avail * trap_params[instance].rel_avail;
    data.avail_het = trap_params[instance].avail_decay->hetSample(rng);
    data.deploy_time = Sim::now();
    data.expiry = Sim::now() + lifespan;
    baited_traps.push_back(std::move(data));
}

// Every sim::oneTS() days:
void AnophelesModel::advance_period(double sum_avail, double sigma_df, vector<double>& sigma_dif_i,
                                    vector<double>& sigma_dif_l, double sigma_dff, bool is_dynamic) {
    /* Largely equations correspond to Nakul Chitnis's model in
      "A mathematic model for the dynamics of malaria in
      mosquitoes feeding on a heterogeneous host population" [MMDM]
    section 2, 3.5-3.6, plus extensions to a non-autonomous case from
      "Nonautonomous Difference Equations for Malaria Dynamics
                   in a Mosquito Population" [NDEMD]

    We calculate EIR over a time step (one or five days) as:
      sum_{for t over days in step} σ_i[t] * s_v[t]
      = sum_... (N_v[t] * P_Ai[t] * P_B_i[t])/(T*N_i[t]) * S_v[t]/N_v[t]
      = sum_... P_Ai[t] * P_B_i[t] * S_v[t]
    (since T == 1 and N_i[t] == 1 for all t).

      P_Ai[t] = (1 - P_A[t]) α_i[t] / sum_{h in hosts} α_h[t]
    (letting N_h[t] == 1 for all h,t). The only part of this varying per-host is
      α_i[t] = host.entoAvailability (index, human.getAgeInYears())
      Let availDivisor[t] = (1 - P_A[t]) / (sum_{h in hosts} α_h[t] + μ_vA).

    Note that although the model allows α_i and P_B_i to vary per-day, they only
    vary per time step of the main simulation. Hence:
      EIR = (sum_{t=...} S_v[t] * availDivisor[t]) * α_i * P_B_i

    Since S_v[t] * availDivisor[t] does not vary per individual, we calculate this
    per time step of the main simulation as partialEIR:
      partialEIR = (sum_{t=...} S_v[t] * availDivisor[t])

    Hence calculateEIR() only needs to do the following:
      EIR = partialEIR * α_i * P_B_i
    */

    // -----  Calculate P_A, P_Ai, P_df, P_dif based on human pop  -----

    // ν_A: rate at which mosquitoes find hosts or die (i.e. leave host-seeking state
    double leave_rate = mosq.seeking_death_rate;
    for (const util::SimpleDecayingValue& increase : seeking_death_rate_intervs) {
        leave_rate *= 1.0 + increase.current_value(Sim::ts0());
    }
    const double mu_v_interv = leave_rate;
    leave_rate += sum_avail;

    // NON-HUMAN HOSTS INTERVENTIONS
    // Check if some nhh must be removed
    for (auto it = nhh_instances.begin(); it != nhh_instances.end();) {
        if (Sim::ts0() >= it->second.expiry) {
            it = nhh_instances.erase(it);
            continue;
        }
        it++;
    }

    double modified_nhh_avail = 0.0;
    double modified_nhh_sigma_df = 0.0;
    double modified_nhh_sigma_dff = 0.0;

    map<string, Nhh> current_nhh = nhh_instances;

    for (auto& it : reduce_nhh_availability) {
        for (const auto& decay : it.second) {
            if (current_nhh.count(it.first) != 0) // Check that the non-human hosts still exist
                current_nhh[it.first].avail_i *= 1.0 - decay.current_value(Sim::ts0());
        }
    }

    for (auto& it : reduce_p_b_i) {
        for (const auto& decay : it.second) {
            if (current_nhh.count(it.first) != 0) // Check that the non-human hosts still exist
                current_nhh[it.first].p_b_i *= 1.0 - decay.current_value(Sim::ts0());
        }
    }

    for (auto& it : reduce_p_c_i) {
        for (const auto& decay : it.second) {
            if (current_nhh.count(it.first) != 0) // Check that the non-human hosts still exist
                current_nhh[it.first].p_c_i *= 1.0 - decay.current_value(Sim::ts0());
        }
    }

    for (auto& it : reduce_p_d_i) {
        for (const auto& decay : it.second) {
            if (current_nhh.count(it.first) != 0) // Check that the non-human hosts still exist
                current_nhh[it.first].p_d_i *= 1.0 - decay.current_value(Sim::ts0());
        }
    }

    for (auto& it : reduce_fecundity) {
        for (const auto& decay : it.second) {
            if (current_nhh.count(it.first) != 0) // Check that the non-human hosts still exist
                current_nhh[it.first].rel_fecundity *= 1.0 - decay.current_value(Sim::ts0());
        }
    }

    for (auto& it : current_nhh) {
        modified_nhh_avail += it.second.avail_i;
        const double df =
            it.second.avail_i * it.second.p_b_i * it.second.p_c_i * it.second.p_d_i; // term in P_df series
        modified_nhh_sigma_df += df;
        modified_nhh_sigma_dff += df * it.second.rel_fecundity;
    }

    leave_rate += modified_nhh_avail;
    sigma_df += modified_nhh_sigma_df;
    sigma_dff += modified_nhh_sigma_dff;
    // NON-HUMAN HOSTS INTERVENTIONS

    for (auto it = baited_traps.begin(); it != baited_traps.end();) {
        if (Sim::ts0() > it->expiry) {
            it = baited_traps.erase(it);
            continue;
        }
        const SimTime age = Sim::ts0() - it->deploy_time;
        const double decay_coeff = it->avail_het->eval(age);
        leave_rate += it->initial_avail * decay_coeff;
        // sigma_df doesn't change: mosquitoes do not survive traps
        it++;
    }

    // Calculate alpha_t for ATSB interventions with fixed target PA
    // =============================================================
    double p_death_seeking = 0.0;
    for (const util::SimpleDecayingValue& p_death : prob_additional_death_sugar_feeding_intervs)
        p_death_seeking += p_death.current_value(Sim::ts0());

    if (p_death_seeking > 1.0)
        throw xml_scenario_error(
            "VectorPop: the cumulative probability of death wile seeking is greater than 1 during the simulation");

    if (p_death_seeking > 0.0) {
        int status = 0;
        int iter = 0, max_iter = 20;

        double x0 = NAN, a_t = 0.5; // guess

        QuadraticParams params = {.ah = sum_avail,
                                  .an = modified_nhh_avail,
                                  .mu_v_interv = mu_v_interv,
                                  .theta_d = mosq.seeking_duration,
                                  .p_at_wanted = p_death_seeking};

        const GslRootSolverType* t = gsl_root_solver_method;
        GslRootSolver* s = gsl_root_solver_alloc(t);

#ifdef USE_DERIVATIVE
        gsl_function_fdf fdf;
        fdf.f = &quadratic_f;
        fdf.df = &quadratic_df;
        fdf.fdf = &quadratic_fdf;
        fdf.params = &params;

        gsl_root_fdfsolver_set(s, &fdf, a_t);
#else
        gsl_function f;
        f.function = &quadratic_f;
        f.params = &params;

        gsl_root_fsolver_set(s, &f, 0.0, 1.0);
#endif

        do {
            iter++;
            status = gsl_root_solver_iterate(s);
            x0 = a_t;
            a_t = gsl_root_solver_root(s);

            status = gsl_root_test_delta(a_t, x0, 0, 1e-4);
        } while (status == GSL_CONTINUE && iter < max_iter);

        gsl_root_solver_free(s);

        leave_rate += a_t;
    }
    // =============================================================

    // Probability of a mosquito not finding a host this day:
    const double ts_p_a = exp(-leave_rate * mosq.seeking_duration);
    const double avail_divisor = (1.0 - ts_p_a) / leave_rate; // α_d

    // alphaE (α_E) is α_d * P_E, where P_E may be adjusted by interventions
    double alpha_e = avail_divisor * mosq.prob_mosq_survival_ovipositing;
    for (const util::SimpleDecayingValue& p_death : prob_death_ovipositing_intervs) {
        alpha_e *= 1.0 - p_death.current_value(Sim::ts0());
    }
    const double ts_p_df = sigma_df * alpha_e;
    const double ts_p_dff = sigma_dff * alpha_e;

    // from now, sigma_dif becomes P_dif (but we can't simply rename):
    vectors::scale(sigma_dif_i, alpha_e);
    vectors::scale(sigma_dif_l, alpha_e);

    // Summed per day:
    partial_eir_i.assign(genotypes->count(), 0.0);
    partial_eir_l.assign(genotypes->count(), 0.0);

    reset_ts_stats();

    // Computing for output only
    const double ts_p_amu =
        (1 - ts_p_a) * mosq.seeking_death_rate / (mosq.seeking_death_rate + sum_avail + modified_nhh_avail);
    const double ts_p_a1 = (1 - ts_p_a) * sum_avail / (mosq.seeking_death_rate + sum_avail + modified_nhh_avail);
    const double ts_p_ah =
        (1 - ts_p_a) * modified_nhh_avail / (mosq.seeking_death_rate + sum_avail + modified_nhh_avail);

    // The code within the for loop needs to run per-day, wheras the main
    // simulation uses one or five day time steps.
    const SimTime next_ts = Sim::ts0() + Sim::one_ts();
    for (SimTime d0 = Sim::ts0(); d0 < next_ts; d0 = d0 + Sim::one_day()) {
        update(d0, ts_p_a, ts_p_amu, ts_p_a1, ts_p_ah, ts_p_df, sigma_dif_i, sigma_dif_l, ts_p_dff, is_dynamic,
               partial_eir_i, partial_eir_l, avail_divisor);
    }
}

void AnophelesModel::update(SimTime d0, double ts_p_a, double ts_p_amu, double ts_p_a1, double ts_p_ah, double ts_p_df,
                            const vector<double>& ts_p_dif_i, const vector<double>& ts_p_dif_l, double ts_p_dff,
                            bool is_dynamic, vector<double>& partial_eir_i, vector<double>& partial_eir_l,
                            double eir_factor) {
    double intervention_survival = 1.0;
    for (const auto& i : emergence_reduction)
        intervention_survival *= 1.0 - i.current_value(Sim::ts0());

    const int d1 = d0 + 1; //sim::oneDay(); // end of step

    // We add N_v_length so that we can use mod_nn() instead of mod().
    const int d1_mod = d1 + n_v_length;
    assert(d1_mod >= n_v_length);
    // Indecies for end time, start time, and mosqRestDuration days before end time:
    const int t1 = util::mod_nn(d1, n_v_length);
    const int t0 = util::mod_nn(d0, n_v_length);
    const int ttau = util::mod_nn(d1_mod - mosq.rest_duration, n_v_length);

    // These only need to be calculated once per time step, but should be
    // present in each of the previous N_v_length - 1 positions of arrays.
    p_a[t1] = ts_p_a;
    p_amu[t1] = ts_p_amu;
    p_a1[t1] = ts_p_a1;
    p_ah[t1] = ts_p_ah;
    p_df[t1] = ts_p_df;
    p_dff[t1] = ts_p_dff;

    const size_t n = genotypes->count();

    for (size_t i = 0; i < n; ++i) {
        p_dif_i[t1 * n + i] = ts_p_dif_i[i];
        p_dif_l[t1 * n + i] = ts_p_dif_l[i];
    }

    // BEGIN cache calculation: fArray, ftauArray, uninfected_v
    // Set up array with n in 1..θ_s−τ for f(d1Mod-n) (NDEMD eq. 1.6)
    for (int n = 1; n <= mosq.rest_duration; n++) {
        const int tn = util::mod_nn(d1_mod - n, n_v_length);
        f_array[n] = f_array[n - 1] * p_a[tn];
    }
    f_array[mosq.rest_duration] += p_df[ttau];

    const int f_a_end = mosq.eip_duration - mosq.rest_duration;
    for (int n = mosq.rest_duration + 1; n <= f_a_end; n++) {
        const int tn = util::mod_nn(d1_mod - n, n_v_length);
        f_array[n] = p_df[tn] * f_array[n - mosq.rest_duration] + p_a[tn] * f_array[n - 1];
    }

    // Set up array with n in 1..θ_s−1 for f_τ(d1Mod-n) (NDEMD eq. 1.7)
    const int f_prod_end = mosq.rest_duration * 2;
    for (int n = mosq.rest_duration + 1; n <= f_prod_end; n++) {
        const int tn = util::mod_nn(d1_mod - n, n_v_length);
        ftau_array[n] = ftau_array[n - 1] * p_a[tn];
    }
    ftau_array[f_prod_end] += p_df[util::mod_nn(d1_mod - f_prod_end, n_v_length)];

    for (int n = f_prod_end + 1; n < mosq.eip_duration; n++) {
        const int tn = util::mod_nn(d1_mod - n, n_v_length);
        ftau_array[n] = p_df[tn] * ftau_array[n - mosq.rest_duration] + p_a[tn] * ftau_array[n - 1];
    }

    for (int d = 1; d < n_v_length; d++) {
        const int t = util::mod_nn(d1_mod - d, n_v_length);
        double sum = n_v[t];
        for (size_t i = 0; i < n; ++i)
            sum -= (o_v_i[t * n + i] + o_v_l[t * n + i]); // .at(t, i);
        uninfected_v[d] = sum;
    }
    // END cache calculation: fArray, ftauArray, uninfected_v

    double total_s_v = 0.0;
    for (size_t g = 0; g < n; ++g) {
        // Num infected seeking mosquitoes is the new ones (those who were
        // uninfected tau days ago, started a feeding cycle then, survived and
        // got infected) + those who didn't find a host yesterday + those who
        // found a host tau days ago and survived a feeding cycle.
        // O_v.at(t1, g) = P_dif.at(ttau, g) * uninfected_v[mosq.restDuration] + P_A[t0] * O_v.at(t0, g) +
        //                        P_df[ttau] * O_v.at(ttau, g);
        o_v_i[t1 * n + g] = p_dif_i[ttau * n + g] * uninfected_v[mosq.rest_duration] + p_a[t0] * o_v_i[t0 * n + g] +
                            p_df[ttau] * o_v_i[ttau * n + g];
        o_v_l[t1 * n + g] = p_dif_l[ttau * n + g] * uninfected_v[mosq.rest_duration] + p_a[t0] * o_v_l[t0 * n + g] +
                            p_df[ttau] * o_v_l[ttau * n + g];

        // BEGIN S_v
        double sum_i = 0.0, sum_l = 0.0;
        const int ts = d1_mod - mosq.eip_duration;
        for (int l = 1; l < mosq.rest_duration; l++) {
            const int tsl = util::mod_nn(ts - l, n_v_length); // index d1Mod - theta_s - l
            sum_i += p_dif_i[tsl * n + g] * p_df[ttau] * (uninfected_v[mosq.eip_duration + l]) *
                     ftau_array[mosq.eip_duration + l - mosq.rest_duration];
            sum_l += p_dif_l[tsl * n + g] * p_df[ttau] * (uninfected_v[mosq.eip_duration + l]) *
                     ftau_array[mosq.eip_duration + l - mosq.rest_duration];
        }

        const int tsm = util::mod_nn(ts, n_v_length); // index d1Mod - theta_s
        s_v_i[t1 * n + g] =
            p_dif_i[tsm * n + g] * f_array[mosq.eip_duration - mosq.rest_duration] * (uninfected_v[mosq.eip_duration]) +
            sum_i + p_a[t0] * s_v_i[t0 * n + g] + p_df[ttau] * s_v_i[ttau * n + g];
        s_v_l[t1 * n + g] =
            p_dif_l[tsm * n + g] * f_array[mosq.eip_duration - mosq.rest_duration] * (uninfected_v[mosq.eip_duration]) +
            sum_l + p_a[t0] * s_v_l[t0 * n + g] + p_df[ttau] * s_v_l[ttau * n + g];

        if (is_dynamic) {
            // We cut-off transmission when no more than X mosquitos are infected to
            // allow true elimination in simulations. Unfortunately, it may cause problems with
            // trying to simulate extremely low transmission, such as an R_0 case.
            if (s_v_i[t1 * n + g] + s_v_l[t1 * n + g] <= mosq.min_infected_threshold) {
                s_v_l[t1 * n + g] = 0.0;
                s_v_i[t1 * n + g] = 0.0; // Removing this will break unit tests
            }
        }

        partial_eir_i[g] += s_v_i[t1 * n + g] * eir_factor;
        partial_eir_l[g] += s_v_l[t1 * n + g] * eir_factor;
        total_s_v += s_v_i[t1 * n + g] + s_v_l[t1 * n + g];
        // END S_v
    }

    // We use time at end of step (i.e. start + 1) in index:
    const int d5_year = util::mod_nn(d1, Sim::from_years_i(5));
    quinquennial_s_v[d5_year] = total_s_v;

    const double n_ovipositing = p_dff[ttau] * n_v[ttau]; // number ovipositing on this step
    const double new_adults = get_emergence_rate(d0, mosq_emerge_rate, n_ovipositing) * intervention_survival;
    util::streamValidate(new_adults);

    // num seeking mosquitos is: new adults + those which didn't find a host
    // yesterday + those who found a host tau days ago and survived cycle:
    n_v[t1] = new_adults + p_a[t0] * n_v[t0] + n_ovipositing;

    time_step_n_v0 += new_adults;
}

// -----  Summary and intervention functions  -----
void AnophelesModel::change_eir_intervention(const scnXml::NonVector& non_vector_data) {
    // assert(nonVectorData.getEIRDaily().present()); // XML loading code should enforce this
    const scnXml::NonVector::EIRDailySequence& daily = non_vector_data.getEIRDaily();

    // const scnXml::DailyValues &seasM = seasonality.getDailyValues().get();
    // const scnXml::DailyValues::ValueSequence &daily = seasM.getValue();

    // The minimum EIR allowed in the array. The product of the average EIR and a constant.
    const double min_eir = 0.01 * average_eir(non_vector_data);

    if (daily.size() < static_cast<size_t>(Sim::one_year()))
        throw util::xml_scenario_error(
            "entomology.anopheles.seasonality.dailyValues insufficient daily data for a year");

    vector<int> n_days(Sim::in_steps(Sim::from_days(daily.size() - 1)) + 1, 0);

    intervention_eir.assign(n_days.size(), 0.0);

    const auto required_days = static_cast<size_t>(Sim::end_date() - Sim::start_date()) + 1;

    if (daily.size() < required_days)
        throw util::xml_scenario_error("Insufficient intervention phase EIR values provided");

    for (SimTime mpcday = Sim::zero(), end_day = Sim::from_days(daily.size()); mpcday < end_day;
         mpcday = mpcday + Sim::one_day()) {
        const double eir_daily = std::max(static_cast<double>(daily[mpcday]), min_eir);

        // istep is the time period to which the day is assigned.
        const size_t istep = Sim::in_steps(mpcday);
        n_days[istep]++;
        intervention_eir[istep] += eir_daily;
    }
    // divide by number of records assigned to each interval (usually one per day)
    for (size_t i = 0; i < intervention_eir.size(); ++i) {
        intervention_eir[i] *= Sim::one_ts() / static_cast<double>(n_days[i]);
    }
}

void AnophelesModel::uninfect_vectors() {
    for (size_t i = 0; i < n_v_length * genotypes->count(); i++) {
        o_v_i[i] = 0.0;
        o_v_l[i] = 0.0;
        s_v_i[i] = 0.0;
        s_v_l[i] = 0.0;
        p_dif_i[i] = 0.0;
        p_dif_l[i] = 0.0;
    }
}

double sum1(const std::vector<double>& arr, int end, int n_v_length) {
    double val = 0.0;
    // Last time step ended at sim::now(). Values are stored per day, and for
    // the last time step values at sim::now() and four previos were set.
    for (int d1 = end - Sim::one_ts(); d1 < end; d1++) {
        val += arr[util::mod_nn(d1, n_v_length)];
    }
    return val / Sim::one_ts();
}

double sum2(const std::vector<double>& arr, int end, int n_v_length, size_t n_genotypes) {
    double val = 0.0;
    // Last time step ended at sim::now(). Values are stored per day, and for
    // the last time step values at sim::now() and four previos were set.
    for (int d1 = end - Sim::one_ts(); d1 < end; d1++) {
        const int i1 = util::mod_nn(d1, n_v_length);
        for (size_t g = 0; g < n_genotypes; ++g) {
            val += arr[i1 * n_genotypes + g]; //.at(i1, g);
        }
    }
    return val / Sim::one_ts();
}

double sum3(const std::vector<double>& arr, size_t g, int end, int n_v_length, size_t n_genotypes) {
    double val = 0.0;
    // Last time step ended at sim::now(). Values are stored per day, and for
    // the last time step values at sim::now() and four previos were set.
    for (int d1 = end - Sim::one_ts(); d1 < end; d1++) {
        val += arr[util::mod_nn(d1, n_v_length) * n_genotypes + g]; // .at(mod_nn(d1, N_v_length), g);
    }
    return val / Sim::one_ts();
}

double AnophelesModel::get_last_vec_stat(VecStat vs) const {
    // Last time step ended at sim::now(). Values are stored per day, and for
    // the last time step values at sim::now() and four previos were set.
    // One plus last, plus (0 mod N_v_length) to avoid negatives:
    const int end = Sim::now() + 1 + n_v_length;
    switch (vs) {
    case PA:
        return sum1(p_a, end, n_v_length);
    case PDF:
        return sum1(p_df, end, n_v_length);
    case PDIF:
        return sum2(p_dif_i, end, n_v_length, genotypes->count()) + sum2(p_dif_l, end, n_v_length, genotypes->count());
    case NV:
        return sum1(n_v, end, n_v_length);
    case OV:
        return sum2(o_v_i, end, n_v_length, genotypes->count()) + sum2(o_v_l, end, n_v_length, genotypes->count());
    case SV:
        return sum2(s_v_i, end, n_v_length, genotypes->count()) + sum2(s_v_l, end, n_v_length, genotypes->count());
    case PAmu:
        return sum1(p_amu, end, n_v_length);
    case PA1:
        return sum1(p_a1, end, n_v_length);
    case PAh:
        return sum1(p_ah, end, n_v_length);
    default:
        throw SWITCH_DEFAULT_EXCEPTION;
    }
}

void AnophelesModel::summarize(size_t species) const {
    // Last time step ended at sim::now(). Values are stored per day, and for
    // the last time step values at sim::now() and four previos were set.
    // One plus last, plus (0 mod N_v_length) to avoid negatives:
    const int end = Sim::now() + 1 + n_v_length;
    mon::record_stat(mon::measure("Vector_Nv0"), get_last_n_v0(), species);
    mon::record_stat(mon::measure("Vector_Nv"), sum1(n_v, end, n_v_length), species);
    for (size_t g = 0; g < genotypes->count(); ++g) {
        mon::record_stat(mon::measure("Vector_Ov"),
                         sum3(o_v_i, g, end, n_v_length, genotypes->count()) +
                             sum3(o_v_l, g, end, n_v_length, genotypes->count()),
                         species, g);
        mon::record_stat(mon::measure("Vector_Sv"),
                         sum3(s_v_i, g, end, n_v_length, genotypes->count()) +
                             sum3(s_v_l, g, end, n_v_length, genotypes->count()),
                         species, g);
    }
}

} // namespace OM::Transmission::Anopheles
