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

#include "Host/WithinHost/Infection/EmpiricalInfection.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"

#include <sstream>
#include <fstream>
#include <string>
#include <cmath>

namespace OM::WithinHost {
using namespace ::OM::util;

// -----  parameters  -----

EmpiricalInfectionParams::EmpiricalInfectionParams(const util::CommandLineOptions& command_line)
    : maximum_permitted_amplification_per_cycle(1000.0),
      // alpha1 corresponds to 1 day before first patent, alpha2 2 days before first patent etc.
      alpha1(0.2647), alpha2(2.976), alpha3(0.9181), mu1(6.08e-04), mu2(0.624), mu3(0.3064), sigma0_res(0.9998),
      sigmat_res(0.002528),
      /* The following variables are assigned separately for each infection to enable optimisation of their values
       */
      inflation_mean(1.09635), inflation_variance(0.172029), extinction_level(0.0100976), overall_multiplier(0.697581) {
    sub_patent_limit = 10.0 / overall_multiplier;
    const string fname = command_line.lookup_resource("autoRegressionParameters.csv");
    fstream f_auto_regression_parameters(fname.c_str(), ios::in);
    if (!f_auto_regression_parameters.is_open())
        throw base_exception(string("file not found: ").append(fname), util::Error::FileIO);

    //read header of file (unused)
    string csv_line;
    getline(f_auto_regression_parameters, csv_line);
    if (csv_line != "day,mub1,sigb1,mub2,sigb2,mub3,sigb3")
        // this check is here to catch unexpected alterations
        throw TRACED_EXCEPTION_DEFAULT("autoRegressionParameters.csv does not have expected header line");

    // read every line from the stream
    while (getline(f_auto_regression_parameters, csv_line)) {

        std::istringstream csv_stream(csv_line);

        string csv_field1, csv_field2, csv_field3, csv_field4, csv_field5, csv_field6, csv_field7;

        // read every element from the line that is separated by commas
        getline(csv_stream, csv_field1, ',');
        getline(csv_stream, csv_field2, ',');
        getline(csv_stream, csv_field3, ',');
        getline(csv_stream, csv_field4, ',');
        getline(csv_stream, csv_field5, ',');
        getline(csv_stream, csv_field6, ',');
        getline(csv_stream, csv_field7, ',');

        istringstream csv_num1(csv_field1), csv_num2(csv_field2), csv_num3(csv_field3), csv_num4(csv_field4),
            csv_num5(csv_field5), csv_num6(csv_field6), csv_num7(csv_field7);

        int day = 0;
        csv_num1 >> day;
        if (day < 0 || day >= EmpiricalInfection::MaximumDurationInDays)
            throw TRACED_EXCEPTION_DEFAULT("EmpiricalInfection::init(): invalid day");
        csv_num2 >> mu_beta1[day];
        csv_num3 >> sigma_beta1[day];
        csv_num4 >> mu_beta2[day];
        csv_num5 >> sigma_beta2[day];
        csv_num6 >> mu_beta3[day];
        csv_num7 >> sigma_beta3[day];
    }
    f_auto_regression_parameters.close();
}

// -----  non-static class members (Construction and destruction, checkpointing)  -----

/* Initialises a new infection by assigning the densities for the last 3 prepatent days
*/
EmpiricalInfection::EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, LocalRng& rng,
                                       uint32_t prot_id, int origin, double growth_rate_multiplier)
    : CommonInfection(latent_p, prot_id, origin), patent_growth_rate_multiplier(growth_rate_multiplier),
      params(&params) {
    //sample the parasite densities for the last 3 prepatent days
    //note that the lag decreases with time
    lagged_log_densities[0] = sample_sub_patent_value(rng, params.alpha1, params.mu1, log(params.sub_patent_limit));
    lagged_log_densities[1] = sample_sub_patent_value(rng, params.alpha2, params.mu2, log(params.sub_patent_limit));
    lagged_log_densities[2] = sample_sub_patent_value(rng, params.alpha3, params.mu3, log(params.sub_patent_limit));
    //only the immediately preceding value is modified by the growth rate multiplier
    lagged_log_densities[0] += log(growth_rate_multiplier);
}

// -----  non-static class members (other functions)  -----

void EmpiricalInfection::set_patent_growth_rate_multiplier(double multiplier) {
    patent_growth_rate_multiplier = multiplier;
}

// NOTE: TS never provided a lot of documentation for this code.
const int EiMaxSamples = 10;
bool EmpiricalInfection::update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double) {
    //to avoid the formula for the linear predictor being excessively long we introduce lag for the lagged densities
    const std::array<double, 3>& lag = lagged_log_densities;

    if (bs_age >= MaximumDurationInDays || !(lag[0] > -999999.9)) // Note: second test is extremely unlikely to fail
        return true;                                              // cut-off point

    // constraints to ensure the density is defined and not exploding
    const double upper_limit_of_log_density =
        log(params->maximum_permitted_amplification_per_cycle * exp(lag[1]) / params->inflation_mean);
    double amplification_per_cycle = NAN;
    double local_density = NAN; // density before scaling by overall_multiplier
    const size_t age_days = bs_age;
    for (int tries0 = 0; tries0 < EiMaxSamples; ++tries0) {
        double log_density = NAN;
        for (int tries1 = 0; tries1 < EiMaxSamples; ++tries1) {
            const double b_1 = rng.gauss(params->mu_beta1[age_days], params->sigma_beta1[age_days]);
            const double b_2 = rng.gauss(params->mu_beta2[age_days], params->sigma_beta2[age_days]);
            const double b_3 = rng.gauss(params->mu_beta3[age_days], params->sigma_beta3[age_days]);
            const double expected_log_density = b_1 * (lag[0] + lag[1] + lag[2]) / 3 + b_2 * (lag[2] - lag[0]) / 2 +
                                                b_3 * (lag[2] + lag[0] - 2 * lag[1]) / 4;

            //include sampling error
            log_density = rng.gauss(expected_log_density, sigma_noise(age_days));
            //include drug and immunity effects via growthRateMultiplier
            log_density += log(patent_growth_rate_multiplier);

            if (log_density <= upper_limit_of_log_density) //got an acceptable density, we're done
                break;                                     // most of the time this should happen first try
        }
        if (!(log_density <= upper_limit_of_log_density)) // in case all the above attempts fail, cap logDensity
            log_density = upper_limit_of_log_density;

        local_density = get_inflated_density(rng, log_density);

        local_density *= survival_factor; // Apply drug and vaccine effects

        // Infections that get killed before they become patent:
        if ((age_days == 0) && (local_density < params->sub_patent_limit)) {
            local_density = 0.0;
        }

        amplification_per_cycle = local_density / exp(lag[1]);
        if (local_density >= 0.0 && amplification_per_cycle <= params->maximum_permitted_amplification_per_cycle)
            break; // We're done. Hopefully usually with the first try.
    }
    if (!(local_density >= 0.0 &&
          amplification_per_cycle <= params->maximum_permitted_amplification_per_cycle)) // in case the above tries fail
        local_density = params->maximum_permitted_amplification_per_cycle * exp(lag[1]);

    lagged_log_densities[2] = lagged_log_densities[1];
    lagged_log_densities[1] = lagged_log_densities[0];
    lagged_log_densities[0] = log(local_density);

    density = local_density;
    cum_exposure_j += density;

    // Note: here use a positive test for survival, since if m_density became an NaN tests against it will return false:
    if (density > params->extinction_level)
        return false; // Still parasites; infection didn't go extinct
    else
        return true; // parasites are extinct; infection will be removed from model
}

double EmpiricalInfection::sample_sub_patent_value(LocalRng& rng, double alpha, double mu, double upper_bound) {
    const double beta = alpha * (1.0 - mu) / mu;
    const double non_inflated_value = upper_bound + log(rng.beta(alpha, beta));
    double inflated_value = NAN;
    int tries = 0;
    do {
        inflated_value = get_inflated_density(rng, non_inflated_value);
        tries++;
    } while ((inflated_value > upper_bound) && tries < 10);
    if (inflated_value > upper_bound)
        inflated_value = upper_bound;
    return inflated_value;
}

double EmpiricalInfection::sigma_noise(size_t age_days) {
    return params->sigma0_res + params->sigmat_res * static_cast<double>(age_days);
}

double EmpiricalInfection::get_inflated_density(LocalRng& rng, double non_inflated_density) {
    const double inflated_log_density =
        log(params->inflation_mean) + rng.gauss(non_inflated_density, sqrt(params->inflation_variance));
    return exp(inflated_log_density);
}

void EmpiricalInfectionParams::override_inflation_factors(double inflation_mean, double inflation_variance,
                                                          double extinction_level, double overall_multiplier) {
    this->inflation_variance = inflation_variance;
    this->inflation_mean = inflation_mean;
    this->extinction_level = extinction_level;
    this->overall_multiplier = overall_multiplier;
    sub_patent_limit = 10.0 / overall_multiplier;
}

// -----  checkpointing  -----

EmpiricalInfection::EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, istream& stream)
    : CommonInfection(latent_p, stream), params(&params) {
    lagged_log_densities[0] & stream;
    lagged_log_densities[1] & stream;
    lagged_log_densities[2] & stream;
    patent_growth_rate_multiplier & stream;
}
void EmpiricalInfection::checkpoint(ostream& stream) {
    CommonInfection::checkpoint(stream);
    lagged_log_densities[0] & stream;
    lagged_log_densities[1] & stream;
    lagged_log_densities[2] & stream;
    patent_growth_rate_multiplier & stream;
}

}
