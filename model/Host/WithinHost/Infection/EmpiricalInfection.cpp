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

namespace OM {
namespace WithinHost {
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
    string fname = command_line.lookup_resource("autoRegressionParameters.csv");
    fstream f_autoRegressionParameters(fname.c_str(), ios::in);
    if (!f_autoRegressionParameters.is_open())
        throw base_exception(string("file not found: ").append(fname), util::Error::FileIO);

    //read header of file (unused)
    string csvLine;
    getline(f_autoRegressionParameters, csvLine);
    if (csvLine != "day,mub1,sigb1,mub2,sigb2,mub3,sigb3")
        // this check is here to catch unexpected alterations
        throw TRACED_EXCEPTION_DEFAULT("autoRegressionParameters.csv does not have expected header line");

    // read every line from the stream
    while (getline(f_autoRegressionParameters, csvLine)) {

        std::istringstream csvStream(csvLine);

        string csvField1, csvField2, csvField3, csvField4, csvField5, csvField6, csvField7;

        // read every element from the line that is separated by commas
        getline(csvStream, csvField1, ',');
        getline(csvStream, csvField2, ',');
        getline(csvStream, csvField3, ',');
        getline(csvStream, csvField4, ',');
        getline(csvStream, csvField5, ',');
        getline(csvStream, csvField6, ',');
        getline(csvStream, csvField7, ',');

        istringstream csvNum1(csvField1), csvNum2(csvField2), csvNum3(csvField3), csvNum4(csvField4),
            csvNum5(csvField5), csvNum6(csvField6), csvNum7(csvField7);

        int day;
        csvNum1 >> day;
        if (day < 0 || day >= EmpiricalInfection::_maximumDurationInDays)
            throw TRACED_EXCEPTION_DEFAULT("EmpiricalInfection::init(): invalid day");
        csvNum2 >> mu_beta1[day];
        csvNum3 >> sigma_beta1[day];
        csvNum4 >> mu_beta2[day];
        csvNum5 >> sigma_beta2[day];
        csvNum6 >> mu_beta3[day];
        csvNum7 >> sigma_beta3[day];
    }
    f_autoRegressionParameters.close();
}

// -----  non-static class members (Construction and destruction, checkpointing)  -----

/* Initialises a new infection by assigning the densities for the last 3 prepatent days
*/
EmpiricalInfection::EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, LocalRng& rng,
                                       uint32_t protID, int origin, double growthRateMultiplier)
    : CommonInfection(latent_p, protID, origin), params(&params) {
    //sample the parasite densities for the last 3 prepatent days
    //note that the lag decreases with time
    _laggedLogDensities[0] = sampleSubPatentValue(rng, params.alpha1, params.mu1, log(params.sub_patent_limit));
    _laggedLogDensities[1] = sampleSubPatentValue(rng, params.alpha2, params.mu2, log(params.sub_patent_limit));
    _laggedLogDensities[2] = sampleSubPatentValue(rng, params.alpha3, params.mu3, log(params.sub_patent_limit));
    //only the immediately preceding value is modified by the growth rate multiplier
    _laggedLogDensities[0] += log(growthRateMultiplier);
    _patentGrowthRateMultiplier = growthRateMultiplier;
}

// -----  non-static class members (other functions)  -----

void EmpiricalInfection::setPatentGrowthRateMultiplier(double multiplier) { _patentGrowthRateMultiplier = multiplier; }

// NOTE: TS never provided a lot of documentation for this code.
const int EI_MAX_SAMPLES = 10;
bool EmpiricalInfection::updateDensity(LocalRng& rng, double survivalFactor, SimTime bsAge, double) {
//to avoid the formula for the linear predictor being excessively long we introduce L for the lagged densities
#define L _laggedLogDensities

    if (bsAge >= _maximumDurationInDays || !(L[0] > -999999.9)) // Note: second test is extremely unlikely to fail
        return true;                                            // cut-off point

    // constraints to ensure the density is defined and not exploding
    double upperLimitoflogDensity =
        log(params->maximum_permitted_amplification_per_cycle * exp(L[1]) / params->inflation_mean);
    double amplificationPerCycle;
    double localDensity; // density before scaling by overall_multiplier
    size_t ageDays = bsAge;
    for (int tries0 = 0; tries0 < EI_MAX_SAMPLES; ++tries0) {
        double logDensity;
        for (int tries1 = 0; tries1 < EI_MAX_SAMPLES; ++tries1) {
            double b_1 = rng.gauss(params->mu_beta1[ageDays], params->sigma_beta1[ageDays]);
            double b_2 = rng.gauss(params->mu_beta2[ageDays], params->sigma_beta2[ageDays]);
            double b_3 = rng.gauss(params->mu_beta3[ageDays], params->sigma_beta3[ageDays]);
            double expectedlogDensity =
                b_1 * (L[0] + L[1] + L[2]) / 3 + b_2 * (L[2] - L[0]) / 2 + b_3 * (L[2] + L[0] - 2 * L[1]) / 4;

            //include sampling error
            logDensity = rng.gauss(expectedlogDensity, sigma_noise(ageDays));
            //include drug and immunity effects via growthRateMultiplier
            logDensity += log(_patentGrowthRateMultiplier);

            if (logDensity <= upperLimitoflogDensity) //got an acceptable density, we're done
                break;                                // most of the time this should happen first try
        }
        if (!(logDensity <= upperLimitoflogDensity)) // in case all the above attempts fail, cap logDensity
            logDensity = upperLimitoflogDensity;

        localDensity = getInflatedDensity(rng, logDensity);

        localDensity *= survivalFactor; // Apply drug and vaccine effects

        // Infections that get killed before they become patent:
        if ((ageDays == 0) && (localDensity < params->sub_patent_limit)) {
            localDensity = 0.0;
        }

        amplificationPerCycle = localDensity / exp(L[1]);
        if (localDensity >= 0.0 && amplificationPerCycle <= params->maximum_permitted_amplification_per_cycle)
            break; // We're done. Hopefully usually with the first try.
    }
    if (!(localDensity >= 0.0 &&
          amplificationPerCycle <= params->maximum_permitted_amplification_per_cycle)) // in case the above tries fail
        localDensity = params->maximum_permitted_amplification_per_cycle * exp(L[1]);

    _laggedLogDensities[2] = _laggedLogDensities[1];
    _laggedLogDensities[1] = _laggedLogDensities[0];
    _laggedLogDensities[0] = log(localDensity);

    m_density = localDensity;
    m_cumulativeExposureJ += m_density;

    // Note: here use a positive test for survival, since if m_density became an NaN tests against it will return false:
    if (m_density > params->extinction_level)
        return false; // Still parasites; infection didn't go extinct
    else
        return true; // parasites are extinct; infection will be removed from model
#undef L
}

double EmpiricalInfection::sampleSubPatentValue(LocalRng& rng, double alpha, double mu, double upperBound) {
    double beta = alpha * (1.0 - mu) / mu;
    double nonInflatedValue = upperBound + log(rng.beta(alpha, beta));
    double inflatedValue;
    int tries = 0;
    do {
        inflatedValue = getInflatedDensity(rng, nonInflatedValue);
        tries++;
    } while ((inflatedValue > upperBound) && tries < 10);
    if (inflatedValue > upperBound)
        inflatedValue = upperBound;
    return inflatedValue;
}

// NOTE: method is unused — why?
double EmpiricalInfection::samplePatentValue(LocalRng& rng, double mu, double sigma, double lowerBound) {
    double returnValue;
    do {
        double nonInflatedValue = rng.gauss(mu, sigma);
        returnValue = getInflatedDensity(rng, nonInflatedValue);
    } while (returnValue < lowerBound);
    return returnValue;
}

double EmpiricalInfection::sigma_noise(int ageDays) {
    return params->sigma0_res + params->sigmat_res * ((double)ageDays);
}

double EmpiricalInfection::getInflatedDensity(LocalRng& rng, double nonInflatedDensity) {
    double inflatedLogDensity =
        log(params->inflation_mean) + rng.gauss(nonInflatedDensity, sqrt(params->inflation_variance));
    return exp(inflatedLogDensity);
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
    _laggedLogDensities[0] & stream;
    _laggedLogDensities[1] & stream;
    _laggedLogDensities[2] & stream;
    _patentGrowthRateMultiplier & stream;
}
void EmpiricalInfection::checkpoint(ostream& stream) {
    CommonInfection::checkpoint(stream);
    _laggedLogDensities[0] & stream;
    _laggedLogDensities[1] & stream;
    _laggedLogDensities[2] & stream;
    _patentGrowthRateMultiplier & stream;
}

}
}
