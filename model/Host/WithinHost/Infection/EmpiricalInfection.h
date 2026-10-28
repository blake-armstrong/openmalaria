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

#ifndef HMOD_EMPIRICAL_INFECTION
#define HMOD_EMPIRICAL_INFECTION

#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/CommandLine.h"

#include <array>

namespace OM::WithinHost {

struct EmpiricalInfectionParams;

class EmpiricalInfection : public CommonInfection {
public:
    static const int MaximumDurationInDays = 418;

    /// @brief Construction and destruction
    //@{
    /// For checkpointing (don't use for anything else)
    EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime latent_p, istream& stream);
    /// Per instance initialisation; create new inf.
    EmpiricalInfection(const EmpiricalInfectionParams& params, SimTime start_date, SimTime latent_p, LocalRng& rng,
                       uint32_t prot_id, int origin, double growth_rate_multiplier);
    /** Destructor
   * 
   * Note: this destructor does nothing in order to allow shallow copying to
   * the population list. */
    ~EmpiricalInfection() override = default;
    //@}

    /// Set patent growth rate multiplier.
    /// This was used for independant parameterization.
    void set_patent_growth_rate_multiplier(double multiplier);

    bool update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double) override;

protected:
    void checkpoint(ostream& stream) override;

private:
    double get_inflated_density(LocalRng& rng, double non_inflated_density);
    double sigma_noise(size_t age_days);
    double sample_sub_patent_value(LocalRng& rng, double alpha, double mu, double upper_bound);

    std::array<double, 3> lagged_log_densities{};
    double patent_growth_rate_multiplier{};

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
    std::array<double, EmpiricalInfection::MaximumDurationInDays> mu_beta1{};
    std::array<double, EmpiricalInfection::MaximumDurationInDays> sigma_beta1{};
    std::array<double, EmpiricalInfection::MaximumDurationInDays> mu_beta2{};
    std::array<double, EmpiricalInfection::MaximumDurationInDays> sigma_beta2{};
    std::array<double, EmpiricalInfection::MaximumDurationInDays> mu_beta3{};
    std::array<double, EmpiricalInfection::MaximumDurationInDays> sigma_beta3{};
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

#endif
