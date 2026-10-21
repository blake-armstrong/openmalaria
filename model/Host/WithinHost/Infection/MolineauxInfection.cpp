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

#include "Host/WithinHost/Infection/MolineauxInfection.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/checkpoint_containers.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>

namespace OM {
namespace WithinHost {

using namespace OM::util;

// ———  hard-coded model constants  ———

// sigma, rho: decay parameters, per day, of the acquired variant-specific and variant-transcending immune responses
const double Sigma = 0.02;
const double SigmaDecay = exp(-2.0 * Sigma);
// const double rho = 0.0;      // NOTE: rho is not used; if setting this, also change the code below (search rho)
// beta: Minimum value of the probability that a parasite escapes control by the acquired and variant-transcending immune response
const double Beta = 0.01;
// s: the fraction of parasites switching among variants per two-day cycle
const double S = 0.02;
// q: Parameter of the geometric distribution of switching probabilities
const double Q = 0.3;

// mu_m, sigma_m: Mean and standard deviation to use for the normal distribution setting the variant specific multiplication factor.
const double MuM = 16.0;
const double SigmaM = 10.4;
// constants for sampling multiplication factors from a Gamma distribution instead of normal (above)
const double ShapeM = 2.4;
const double ScaleM = 6.8;

// k_c,k_m: constants allowing calculation of Pc_star and Pm_star from host-specific data
const double KC = 0.2;
const double KM = 0.04;
// Pv_star: critical density of a variant, common to all variants
const double InvPvStar = 1.0 / 30.0;
// kappa_c, kappa_m, kappa_v: Stiffness parameters for saturation of immune responses
const int KappaC = 3;
const int KappaM = 1;
const int KappaV = 3;
// C: Maximum daily antigenic stimulus, per mul, of the acquired variant-transcending immune response
const double C = 1.0;

// Blood volume per kg can be found, for example, here:
// http://hypertextbook.com/facts/1998/LanNaLee.shtml
// Futher searches give values between around 65 and 85 ml/kg.
const double BloodVolPerKg = 7e4; // μl/kg; (equals 70 ml/kg)
// PRBC = parasited red blood cell
const double InitialDens = 0.1;  // initial parasite density in PRBC/μl
const double ElimParasites = 50; // minimum number PRBC to avoid elimination of infection and variant

/* Case-specific parameters.
 * 
 * For each of the 35 Malaria Therapy patients, this is:
 *  0) the duration of the infection as last day with positive density minus first positive
 *  1) the parasite density in parasites/microlitre at the first local maximum
 * 
 * Indexing is 2*patient + param where 0 <= patient < 35 and 0 <= param < 2.
 */
const double CaseSpecificData[] = {
    216, 18600,  // G131
    198, 13080,  // G140
    206, 45720,  // G141
    366, 23760,  // G142
    230, 60840,  // G143
    172, 6000,   // G146
    100, 2340,   // G147
    236, 31440,  // G159
    236, 453600, // G161
    120, 4240,   // G173
    176, 195840, // G174
    178, 60120,  // G178
    36,  8720,   // G184
    44,  8000,   // G193
    242, 395280, // G200
    70,  28320,  // G210
    292, 200160, // G212
    248, 59320,  // G217
    98,  66480,  // G23
    176, 61200,  // G230
    234, 169920, // G240
    226, 46800,  // G264
    270, 19260,  // G265
    278, 86040,  // G279
    212, 110160, // G290
    264, 43200,  // G385
    364, 133920, // G407
    184, 222480, // G408
    160, 21420,  // G414
    220, 74160,  // G416
    132, 210960, // G423
    176, 89280,  // G439
    208, 105840, // G445
    330, 21600,  // G457
    404, 156240  // G48
};

// ———  parameters  ———

MolineauxInfectionParams::MolineauxInfectionParams(const Parameters& parameters, const util::ModelOptionFlags& options)
    : multi_factor_gamma(options.option(util::ParasiteReplicationGamma)),
      pairwise_p_star_sample(options.option(util::MolineauxPairwiseSample)) {
    if (!pairwise_p_star_sample) {
        mean_shape_first_local_max = parameters[Parameter::MeanLocalMaxDensity];
        sd_scale_first_local_max = parameters[Parameter::SdLocalMaxDensity];

        mean_shape_diff_pos_days = parameters[Parameter::MeanDiffPosDays];
        sd_scale_diff_pos_days = parameters[Parameter::SdDiffPosDays];

        // with gamma distribution shape and scale parameters has to be recalculated
        if (options.option(util::FirstLocalMaximumGamma)) {
            //NOTE: variant does not appear to work well (according to unit test stats)
            first_local_maximum_gamma = true;
            mean_shape_first_local_max = pow(mean_shape_first_local_max, 2) / pow(sd_scale_first_local_max, 2);
            sd_scale_first_local_max = pow(sd_scale_first_local_max, 2) / mean_shape_first_local_max;
        } else {
            first_local_maximum_gamma = false;
        }

        // with gamma distribution shape and scale parameters has to be recalculated
        if (options.option(util::MeanDurationGamma)) {
            //NOTE: variant does not appear to work well (according to unit test stats)
            mean_duration_gamma = true;
            mean_shape_diff_pos_days = pow(mean_shape_diff_pos_days, 2) / pow(sd_scale_diff_pos_days, 2);
            sd_scale_diff_pos_days = pow(sd_scale_diff_pos_days, 2) / mean_shape_diff_pos_days;
        } else {
            mean_duration_gamma = false;
        }
    }

    for (size_t i = 0; i < MolineauxInfection::V; i++) {
        q_pow[i] = pow(Q, static_cast<double>(i + 1));
    }
}

// ———  MolineauxInfection: initialisation  ———

MolineauxInfection::MolineauxInfection(const MolineauxInfectionParams& params, SimTime latent_p, LocalRng& rng,
                                       uint32_t genotype, int origin)
    : CommonInfection(latent_p, genotype, origin), params(&params) {
    for (size_t i = 0; i < V; i++) {
        // Molineaux paper, equation 11
        if (params.multi_factor_gamma) {
            do {
                mi[i] = static_cast<float>(rng.gamma(ShapeM, ScaleM));
            } while (mi[i] < 1.0);
        } else {
            do {
                mi[i] = static_cast<float>(rng.gauss(MuM, SigmaM));
            } while (mi[i] < 1.0);
        }
    }

    for (size_t tau = 0; tau < Taus; tau++) {
        lagged_pc[tau] = 0.0;
    }

    sm_summation = 0.0;

    if (params.pairwise_p_star_sample) {
        int patient = rng.uniform(35);
        pc_star = static_cast<float>(KC * CaseSpecificData[2 * patient + 1]);
        pm_star = static_cast<float>(KM * CaseSpecificData[2 * patient + 0]);
    } else {
        // Sampling of the first local maxima:
        if (params.first_local_maximum_gamma) {
            pc_star = static_cast<float>(
                KC * pow(10.0, rng.gamma(params.mean_shape_first_local_max, params.sd_scale_first_local_max)));
        } else {
            pc_star = static_cast<float>(
                KC * pow(10.0, rng.gauss(params.mean_shape_first_local_max, params.sd_scale_first_local_max)));
        }

        // Sampling of duration:
        if (params.mean_duration_gamma) {
            pm_star = static_cast<float>(
                KM * pow(10.0, rng.gamma(params.mean_shape_diff_pos_days, params.sd_scale_diff_pos_days)));
        } else {
            pm_star = static_cast<float>(
                KM * pow(10.0, rng.gauss(params.mean_shape_diff_pos_days, params.sd_scale_diff_pos_days)));
        }
    }
}

MolineauxInfection::Variant::Variant() : pi1(0.0), pi2(0.0), si_summation(0.0) {
    for (size_t tau = 0; tau < Taus; tau++) {
        lagged_pi[tau] = 0.0;
    }
}

// ———  MolineauxInfection: density updates  ———

bool MolineauxInfection::update_density(LocalRng&, double survival_factor, SimTime age_bs, double body_mass) {
    // bsAge : age of blood stage; 0 implies initial density (0.1; time t=0 in
    // paper, t=1 in MP's Matlab code), age 2 days is after first update step
    // survivalFactor : probabilty of merozoites surviving drugs, inter-infection immunity and vaccines

    double blood_volume = BloodVolPerKg * body_mass;
    double elim_dens = ElimParasites / blood_volume; // 50 / 5e6 = 5e-5

    // ———  1. Update m_density (Pc), Pi and related  ———
    double pi[V] = {0.0f};

    if (age_bs == Sim::zero()) {
        // The first variant starts with a pre-set density (regardless of blood
        // volume; this is an assumption by DH; paper assumes fixed volume)
        variants.resize(1);
        pi[0] = InitialDens;
        density = InitialDens;
    } else {
        double sum = 0.0;
        for (size_t i = 0; i < variants.size(); i++) {
            double new_p = survival_factor * variants[i].pi1;
            pi[i] = new_p;
            variants[i].pi1 = static_cast<float>(survival_factor * variants[i].pi2);
            sum += new_p;
        }
        density = sum;
    }

    // Integration with inter-infection immunity model
    cum_exposure_j += density;

    if (density <= elim_dens) {
        return true; // infection goes extinct
    }

    // Every other step (if bsAge is even) we calculate new densities (the
    // Molineaux model uses a 2-day step; we have simply added interpolation
    // for the interleaving densities). In other cases we are finished now.
    if (mod_nn(int(age_bs), 2) != 0) {
        return false; // end of update, not extinct
    }

    // ———  2-4: Calculate infection-specific immune responses  ———
    // Sc (probability that a parasite escapes control by innate and variant-transcending immune response)
    // Sm (                        "                      acquired and variant-transcending immune response)
    // S[i] (                      "                      acquired and variant-specific immune response)

    // ———  2. innate immunity (equation 5)  ———
    static_assert(KappaC == 3, "kappa_c == 3"); // allows us to optimise pow(..., kappa_c) to multiplication
    const double base = density / pc_star;
    const double sc = 1.0 / (1.0 + base * base * base);

    // ———  3. variant-transcending immune response (equations 7, 8)  ———
    // 3.a) Update the sum in (7), based on previous result
    const size_t tau = mod_nn(age_bs / 2, Taus); // 8 days ago has same index as today
    // NOTE: rho == 0 so we can optimise this code:
    //Sm_summation = Sm_summation * exp(-2.0*rho) + laggedPc[index];
    sm_summation = sm_summation + lagged_pc[tau];

    // 3.b) Update laggedPc (equation 8, but only stored for the last tau steps)
    //We could use min here, but it seems that min has problems with static const double C
    lagged_pc[tau] = static_cast<float>(density < C ? density : C);

    // 3.c) calculate S_m(t) (equation 7)
    static_assert(KappaM == 1, "kappa_m == 1"); // allows us to optimise out pow(..., kappa_m):
    //double Sm = (1.0 - beta) / (1.0 + pow(Sm_summation / Pm_star, kappa_m)) + beta
    const double sm = (1.0 - Beta) / (1.0 + sm_summation / pm_star) + Beta;

    // ———  4. variant-specific immune response (equation 6)  ———
    double si[V];           // calculate value for each variant
    double sum_qj_sj = 0.0; // simultaneously calculation summataion in equation 4

    for (size_t i = 0; i < V; i++) {
        if (i < variants.size()) {
            // 4.a) Update the sum in (6) based on the last step's value
            //note: sigma_decay = exp(-2*sigma)
            variants[i].si_summation =
                static_cast<float>(variants[i].si_summation * SigmaDecay + variants[i].lagged_pi[tau]);
            // 4.b) update history of density (P_i(t))
            variants[i].lagged_pi[tau] = static_cast<float>(pi[i]);

            // 4.c) calculate S_i(t) (equation 6)
            static_assert(KappaV == 3, "kappa_v == 3"); // again, optimise pow to multiplication
            const double base = variants[i].si_summation * InvPvStar;
            si[i] = 1.0 / (1.0 + base * base * base); // eqn 6, given κ_v = 3
        } else {
            si[i] = 1.0; // eqn 6 for the case when P_i(τ) = 0 for τ ≤ t - δ_m
        }
        sum_qj_sj += params->q_pow[i] * si[i]; // sum in eqn 4
    }

    // ———  5. Variant densities, equations 1, 2 and 4  ———
    for (size_t i = 0; i < V; i++) {
        // 4.a) Calculate p_i, variant selection probability (eqn 4)
        double p_i = 0.0;
        if (si[i] >= 0.1) {
            //note: q_pow[i] = pow(q, i+1)
            p_i = params->q_pow[i] * si[i] / sum_qj_sj;
        }

        // 4.b) calculate P_i'(t+2) [eqn 1] then P_i(t+2) [eqn 2]
        // This is the growth rate after taking immune effect into account:
        double growth_factor = mi[i] * si[i] * sc * sm; // part of eqn 1
        if (i < variants.size()) {
            // Pi_prime: the variant's density at time t+2 (eqn 1)
            double pi_prime = ((1.0 - S) * pi[i] + S * p_i * density) * growth_factor;

            if (pi_prime < elim_dens)
                pi_prime = 0.0; // eqn 2

            variants[i].pi1 = static_cast<float>(sqrt(pi[i] * pi_prime));
            variants[i].pi2 = static_cast<float>(pi_prime);
        } else {
            // In this case P_i(τ) = 0 for all τ ≤ t, and we haven't allocated storage.

            // Pi_prime: the variant's density at time t+2 (eqn 1 in paper)
            double pi_prime = (S * p_i * density) * growth_factor;

            // Molineaux paper equation 2
            if (pi_prime >= elim_dens) { // [if not, P_i(t+2) = 0]
                // express a new variant at time t+2:
                variants.resize(i + 1); // allocate (potentially for multiple variants)
                variants[i].pi2 = static_cast<float>(pi_prime);
            }
        }
    }

    return false; // end of update, not extinct
}

// ———  MolineauxInfection: checkpointing  ———

MolineauxInfection::MolineauxInfection(const MolineauxInfectionParams& params, SimTime latent_p, istream& stream)
    : CommonInfection(latent_p, stream), params(&params) {
    sm_summation & stream;
    for (size_t i = 0; i < V; i++) {
        mi[i] & stream;
    }
    variants & stream;
    for (size_t j = 0; j < Taus; j++) {
        lagged_pc[j] & stream;
    }
    pc_star & stream;
    pm_star & stream;
}

void MolineauxInfection::checkpoint(ostream& stream) {
    CommonInfection::checkpoint(stream);

    sm_summation & stream;
    for (size_t i = 0; i < V; i++) {
        mi[i] & stream;
    }
    variants & stream;
    for (size_t j = 0; j < Taus; j++) {
        lagged_pc[j] & stream;
    }
    pc_star & stream;
    pm_star & stream;
}

void MolineauxInfection::Variant::operator&(istream& stream) {
    bool non_zero;
    non_zero & stream;
    if (non_zero) {
        pi1 & stream;
        pi2 & stream;
        si_summation & stream;
        for (size_t i = 0; i < Taus; ++i) {
            lagged_pi[i] & stream;
        }
    }
    // else: all members are zero-initialised by the constructor, so don't do anything
}

void MolineauxInfection::Variant::operator&(ostream& stream) {
    bool non_zero = pi1 != 0.0 || pi2 != 0.0 || si_summation != 0.0;

    non_zero & stream;
    if (non_zero) {
        pi1 & stream;
        pi2 & stream;
        si_summation & stream;
        for (size_t i = 0; i < Taus; ++i) {
            lagged_pi[i] & stream;
        }
    }
}

}
}
