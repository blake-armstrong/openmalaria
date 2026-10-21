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

#ifndef Hmod_EmpiricalInfection
#define Hmod_EmpiricalInfection

#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/CommandLine.h"

#include <array>

namespace OM {
namespace WithinHost {

struct EmpiricalInfectionParams;

class EmpiricalInfection : public CommonInfection {
public:
    static const int _maximumDurationInDays = 418;

    /// @brief Construction and destruction
    //@{
    /// For checkpointing (don't use for anything else)
    EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, istream& stream);
    /// Per instance initialisation; create new inf.
    EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, LocalRng& rng, uint32_t protID,
                       int origin, double growthRateMultiplier);
    /** Destructor
   * 
   * Note: this destructor does nothing in order to allow shallow copying to
   * the population list. */
    virtual ~EmpiricalInfection() {}
    //@}

    /// Set patent growth rate multiplier.
    /// This was used for independant parameterization.
    void setPatentGrowthRateMultiplier(double multiplier);

    virtual bool updateDensity(LocalRng& rng, double survivalFactor, SimTime bsAge, double);

protected:
    virtual void checkpoint(ostream& stream);

private:
    double getInflatedDensity(LocalRng& rng, double nonInflatedDensity);
    double sigma_noise(int ageDays);
    double samplePatentValue(LocalRng& rng, double mu, double sigma, double lowerBound);
    double sampleSubPatentValue(LocalRng& rng, double mu, double sigma, double upperBound);

    double _laggedLogDensities[3];
    double _patentGrowthRateMultiplier;

    const EmpiricalInfectionParams* params;
};

/// Set by the constructor and some reset by override_inflation_factors
struct EmpiricalInfectionParams {
    double maximum_permitted_amplification_per_cycle = 0.0;
    double sub_patent_limit = 0.0;
    double alpha1 = 0.0;
    double alpha2 = 0.0;
    double alpha3 = 0.0;
    double mu1 = 0.0;
    double mu2 = 0.0;
    double mu3 = 0.0;
    double sigma0_res = 0.0;
    double sigmat_res = 0.0;
    std::array<double, EmpiricalInfection::_maximumDurationInDays> mu_beta1{};
    std::array<double, EmpiricalInfection::_maximumDurationInDays> sigma_beta1{};
    std::array<double, EmpiricalInfection::_maximumDurationInDays> mu_beta2{};
    std::array<double, EmpiricalInfection::_maximumDurationInDays> sigma_beta2{};
    std::array<double, EmpiricalInfection::_maximumDurationInDays> mu_beta3{};
    std::array<double, EmpiricalInfection::_maximumDurationInDays> sigma_beta3{};
    double inflation_mean = 0.0;
    double inflation_variance = 0.0;
    double extinction_level = 0.0;
    double overall_multiplier = 0.0;

    EmpiricalInfectionParams() = default;
    explicit EmpiricalInfectionParams(const util::CommandLineOptions& command_line);

    /// only for parameterisation?
    void override_inflation_factors(double inflation_mean, double inflation_variance, double extinction_level,
                                    double overall_multiplier);
};

}
}
#endif
