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

#include "Host/WithinHost/Infection/PennyInfection.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>

namespace OM::WithinHost {

using namespace OM::util;

/** @brief The static variables (double)

Parameter names match names in Penny et al (2011)

Innate immunity related
 * beta_N := minimum value for R_N^x (value 0.5198)
 * psi_N := minimum value for R_N^y  (value 0.0946)
 * kappa_N := slope of innate immune responses R_N^x and R_N^y (value 2.9506)
 * sigma_epsilon := standard deviation associated with random effects (logNormal) (value 1.4217) 

Clonal immunity related
 * beta_C := minimum value for R_C^x  (value 0.1872)
 * psi_C := minimum value for R_C^y  (value 0.2224)
 * kappa_C := slope of clonal immune responses (value 1.9535) 
 * delta_C := delay to clonal antibody response (days) (value 7.2038 round to 7)  
 * rho_C := decay of clonal antibodies (value 0.1292) 
 * exp_negRho_C := exp of negative rho_C (value exp(-0.1292))

Variant-specific immunity related
 * beta_V := minimum value for R_V^x (value 0.0427)  
 * kappa_V := slope of variant specific immune responses R_V^x (value 4.1529)  
 * delta_V := delay to variant specific antibody response in R_V^x (days) (value 6.3572 round to 6)  
 * rho_V := decay of variant specific antibodies in R_V^x (value 2.5482)  
 * exp_negRho_V := exp of negative rho_V (value exp(-2.5482))
 * lambda_V := arrival times for new dominant variants (days) (value 4.2119) 

Parameters to assign infection dependent parameters
 * mu_Y := mean of log-Normal to assign Y(0) (log(per microL)) (value 3.9700)
 * sigma_Y := standard-deviation of log-Normal to assign Y(0) (value 1.3436)
 * mu_X := mean of log-Normal to assign X(0) (log(per microL)) (value 1.9969)
 * sigma_X := standard-deviation of log-Normal to assign X(0) (value 0.7424)
 * mu_TN := mean of log-Normal to assign T_N (log(per microL)) (value 7.5872)
 * sigma_TN  := standard-deviation of log-Normal to assign T_N (value 2.8977)
 * mu_TC := mean of log-Normal to assign T_C (log(per microL)) (value 5.5573)  
 * sigma_TC  := standard-deviation of log-Normal to assign T_C (value 0.4068) 
 * mu_TV := mean of log-Normal to assign T_V (log(per microL)) (value 6.12898)  
 * sigma_TV  := standard-deviation of log-Normal to assign T_V (value 1.3768) 
 
 Other infection parameters
 * m_rep := replication per cycle (value 16)
 * Omega := critical density when infection ends (per microL) (value 2.5times 10^-4) 
 
 */
//@{
/* Innate immunity related */
const double BetaN = 0.5198;
const double PsiN = 0.0946;
const double KappaN = 2.9506;
const double SigmaEpsilon = 1.4217;

/* Clonal immunity related */
const double BetaC = 0.1872;
const double PsiC = 0.2224;
const double KappaC = 1.9535;
const double RhoC = 0.1292;
const double ExpNegRhoC = exp(-RhoC);

/* Variant-specific immunity related */
const double BetaV = 0.0427;
const double KappaV = 4.1529;
const double RhoV = 2.5482;
const double ExpNegRhoV = exp(-RhoV);
const double LambdaV = 4.2119;
const double ProbLambdaV = 1.0 / LambdaV;

/* Parameters to assign infection dependent parameters */
const double MuY = 3.9700;
const double SigmaY = 1.3436;
const double AY = 8.7305;
const double BY = 0.4547;

const double MuX = 1.9969;
const double SigmaX = 0.7424;
const double AX = 7.2350;
const double BX = 0.2760;

const double MuTn = 7.5872;
const double SigmaTn = 2.8977;
const double ATn = 6.8558;
const double BTn = 1.1067;

const double MuTc = 5.5573;
const double SigmaTc = 0.4068;
const double ATc = 186.6233;
const double BTc = 0.0297;

const double MuTv = 6.12898;
const double SigmaTv = 1.3768;
const double ATv = 19.8167;
const double BTv = 0.3093;

/* Other infection parameters */
const double MRep = 16.0;
const double Omega = 0.00025;
//TODO ending of infection is density per microL of ADULTS, we need to relate weight to BV

//@}

PennyInfectionParams::PennyInfectionParams(const util::ModelOptionFlags& options)
    : immune_threshold_gamma(options.option(util::ImmuneThresholdGamma)),
      update_density_gamma(options.option(util::UpdateDensityGamma)) {}

PennyInfection::PennyInfection(const PennyInfectionParams& params, SimTime latent_p, LocalRng& rng, uint32_t prot_id,
                               int origin)
    : CommonInfection(latent_p, prot_id, origin), params(&params) {
    // assign infection dependent immune thresholds
    if (params.immune_threshold_gamma) /* using gamma distribution */ {
        do {
            threshold_n = exp(rng.gamma(ATn, BTn));
            threshold_c = exp(rng.gamma(ATc, BTc));
            threshold_v = exp(rng.gamma(ATv, BTv));
        } while (threshold_n <= threshold_c || threshold_n <= threshold_v);
    } else /* using lognormal distribution */ {
        do {
            threshold_n = exp(rng.gauss(MuTn, SigmaTn));
            threshold_c = exp(rng.gauss(MuTc, SigmaTc));
            threshold_v = exp(rng.gauss(MuTv, SigmaTv));
        } while (threshold_n <= threshold_c || threshold_n <= threshold_v);
    }

    for (double& cir_densitie : cir_densities) {
        cir_densitie = 0.0;
    }
    for (double& seq_densitie : seq_densities) {
        seq_densitie = 0.0;
    }
}

bool PennyInfection::update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double) {
    int age_days = bs_age; // lazy
    if (bs_age == Sim::zero()) {
        // assign initial densities (Y circulating, X sequestered)
        size_t today = mod_nn(age_days, DeltaC);

        if (params->update_density_gamma) {
            cir_densities[today] = exp(rng.gamma(AY, BY));
        } else {
            cir_densities[today] = exp(rng.gauss(MuY, SigmaY));
        }

        density = cir_densities[today];
        today = mod_nn(age_days, DeltaV);

        if (params->update_density_gamma) {
            seq_densities[today] = exp(rng.gamma(AX, BX));
        } else {
            seq_densities[today] = exp(rng.gauss(MuX, SigmaX));
        }
    } else /*not first day*/
    {
        // save yesterday's density (since getVariantSpecificSummation may reset it to zero)
        size_t yesterday_v = mod_nn(age_days - 1, DeltaV);
        double seq_density_yesterday = seq_densities[yesterday_v];

        // The immune responses are represented by the variables
        // R_Nx, RNy (probability that a parasite escapes control by clonal immune response)
        // R_Cx, RCy (probability that a parasite escapes control by clonal immune response)
        // R_Vx,    (probability that a parasite escapes control by clonal immune response)

        // innate immunity
        size_t yesterday_c = mod_nn(age_days - 1, DeltaC);
        double base_n = cir_densities[yesterday_c] / threshold_n;
        double base_npow = pow(base_n, KappaN);
        double r_nx = (1.0 - BetaN) / (1.0 + base_npow) + BetaN;
        double r_ny = (1.0 - PsiN) / (1.0 + base_npow) + PsiN;

        // clonal immunity
        double base_c = get_clonal_summation(age_days) / threshold_c;
        double base_cpow = pow(base_c, KappaC);
        double r_cx = (1.0 - BetaC) / (1.0 + base_cpow) + BetaC;
        double r_cy = (1.0 - PsiC) / (1.0 + base_cpow) + PsiC;

        // variant specific immunity
        double base_v = get_variant_specific_summation(rng, age_days) / threshold_v;
        double r_vx = (1.0 - BetaV) / (1.0 + pow(base_v, KappaV)) + BetaV;

        // cirDensity: circulating density of circulating at t
        // seqDensity: sequestered density of circulating at t
        // new cirDensity  = (seqDensity(t-1)) * replication * the probability that
        //  seq parasites escape control by immune response.
        // new seqDensity  = (cirDensity(t-1)) * the probability that
        //  circ parasites escape control by immune response.
        double cir_density_new = seq_density_yesterday * MRep * r_vx * r_cx * r_nx;
        double seq_density_new = cir_densities[yesterday_c] * r_cy * r_ny;

        // end infection if density less than Omega (per microL)
        // add random biologicaleffect to circulating
        if (cir_density_new < Omega) {
            cir_density_new = 0.0;
        } else {

            if (params->update_density_gamma) {
                double a_cir_dens = pow(log(cir_density_new), 2) / pow(SigmaEpsilon, 2);
                double b_cir_dens = pow(SigmaEpsilon, 2) / log(cir_density_new);
                cir_density_new = exp(rng.gamma(a_cir_dens, b_cir_dens)) * survival_factor;
            } else {
                cir_density_new = exp(rng.gauss(log(cir_density_new), SigmaEpsilon)) * survival_factor;
            }
            // please don't simplify this, we want more chance at ending infection
            if (cir_density_new < Omega) {
                cir_density_new = 0.0;
            }
        }
        seq_density_new *= survival_factor;
        if (seq_density_new < Omega) {
            if (cir_density_new == 0.0) {
                // infection is extinct
                return true;
            }
            seq_density_new = 0.0;
        }

        size_t today_c = mod_nn(age_days, DeltaC);
        cir_densities[today_c] = cir_density_new;
        density = cir_densities[today_c];

        size_t today_v = mod_nn(age_days, DeltaV);
        seq_densities[today_v] = seq_density_new;
    }

    // used for immunity across infections
    cum_exposure_j += density;

    // if we haven't already exited this funciton, the infection is not extinct (so return false)
    return false;
}

double PennyInfection::get_variant_specific_summation(LocalRng& rng, int age_days) {
    // check if new dominant variant has arrived
    bool new_var_dominant = rng.bernoulli(ProbLambdaV);
    //draw from bernouli distrb, prob 1/lambda_V
    if (new_var_dominant) {
        variant_specific_summation = 0;
        for (double& seq_densitie : seq_densities) {
            seq_densitie = 0.0;
        }
    }
    //The effective exposure is computed by adding in the delta_V-day lagged parasite density
    //and decaying the previous value for the effective exposure with decay parameter rho_V
    size_t index = mod_nn(age_days, DeltaV);
    variant_specific_summation = (variant_specific_summation * ExpNegRhoV) + seq_densities[index];

    return variant_specific_summation;
}

double PennyInfection::get_clonal_summation(int age_days) {
    //The effective exposure is computed by adding in the delta_C-day lagged parasite density
    //and decaying the previous value for the effective exposure with decay parameter rho_C
    size_t index = mod_nn(age_days, DeltaC);
    clonal_summation = (clonal_summation * ExpNegRhoC) + cir_densities[index];

    return clonal_summation;
}

PennyInfection::PennyInfection(const PennyInfectionParams& params, SimTime latent_p, istream& stream)
    : CommonInfection(latent_p, stream), params(&params) {
    for (double& cir_densitie : cir_densities) {
        cir_densitie & stream;
    }
    for (double& seq_densitie : seq_densities) {
        seq_densitie & stream;
    }
    threshold_n & stream;
    threshold_v & stream;
    threshold_c & stream;
    variant_specific_summation & stream;
    clonal_summation & stream;
}

void PennyInfection::checkpoint(ostream& stream) {
    CommonInfection::checkpoint(stream);
    for (double cir_densitie : cir_densities) {
        cir_densitie & stream;
    }
    for (double seq_densitie : seq_densities) {
        seq_densitie & stream;
    }
    threshold_n & stream;
    threshold_v & stream;
    threshold_c & stream;
    variant_specific_summation & stream;
    clonal_summation & stream;
}

}
