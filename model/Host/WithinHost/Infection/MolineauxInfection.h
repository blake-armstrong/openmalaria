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

#ifndef HMOD_MOLINEAUXINFECTION_H
#define HMOD_MOLINEAUXINFECTION_H

#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/ModelOptions.h"

#include <array>
#include <limits>

class MolineauxInfectionSuite;

namespace OM {
namespace WithinHost {

struct MolineauxInfectionParams;

/**
 * Implementation of a P. falciparum model by Molineaux et. al., published in:
 * 
 * L. MOLINEAUX, H. H. DIEBNER, M. EICHNER, W. E. COLLINS, G. M. JEFFERY and
 * K. DIETZ, 2001: Plasmodium falciparum parasitaemia described by a new
 * mathematical model. Parasitology, 122, pp 379-391
 * doi:10.1017/S0031182001007533
 */
class MolineauxInfection : public CommonInfection {
public:
    ///@brief Static class members
    //@{
    // Constants:
    // v: number of variants per clone (one infection = one new clone); 50 in paper
    //TODO(optimisation): v should have a significant effect on performance, but reducing by,
    // say, half, may not have a big effect on the model. Evaluate.
    static const size_t V = 50;
    // taus: used for the variantTranscending and variantSpecific array, 4 Molineaux time steps = 8 days
    static const size_t Taus = 4;
    //@}

    // Initialise. Samples several parameters.
    MolineauxInfection(const MolineauxInfectionParams& params, SimTime latent_p, LocalRng& rng, uint32_t prot_id,
                       int origin);
    // Load from a checkpoint:
    MolineauxInfection(const MolineauxInfectionParams& params, SimTime latent_p, istream& stream);
    virtual ~MolineauxInfection() {};

    virtual bool update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double body_mass);

protected:
    virtual void checkpoint(ostream& stream);

private:
    double get_variant_specific_summation(int i, double p_current);

    // Note: we also have inherited parameters:
    // m_startDate is used to give the age here
    // m_density is equivalent to Pc in paper
    // m_cumulativeExposureJ is cumulative parasite density (used by external immunity function)

    float mi[V];        // base multiplication factor per two-day cycle of variant i
    float sm_summation; // sum in eqn 7
    // index: we use ((bsAge/2) mod 4) for τ = t - δ_v respectively τ = t
    float lagged_pc[Taus]; // Pc(τ) for τ ∈ {t - δ_v, ..., t - 2}
    /* Pc_star, Pm_star: two host-specific critical densities.
     * These two values depend on the first local maximum or the difference
     * between the last positive day and the first positive day. */
    float pc_star, pm_star;

    struct Variant {
        // Initialise all variables to 0
        Variant();

        /// Checkpointing
        void operator&(ostream& stream);
        void operator&(istream& stream);

        float pi1, pi2;     // Pi(t+1), Pi(t+2): variant's i density (PRBC/μl blood)
        float si_summation; // sum in eqn 6
        // index: we use ((bsAge/2) mod 4) for τ = t - δ_v respectively τ = t
        float lagged_pi[Taus]; // Pi(τ) for τ ∈ {t - δ_v, ..., t - 2}
    };
    // variant-specific data; variants[i-1] corresponds to variant i in the paper
    vector<Variant> variants;

    const MolineauxInfectionParams* params;

    // allow unittest to access private vars
    friend class ::MolineauxInfectionSuite;
};

struct MolineauxInfectionParams {
    // depends on lognormal/gamma distribution for first_local_max
    bool first_local_maximum_gamma = false;
    // mean/shape and sd/scale of the first local maximum density for lognormal/gamma distribution
    double mean_shape_first_local_max = std::numeric_limits<double>::signaling_NaN();
    double sd_scale_first_local_max = std::numeric_limits<double>::signaling_NaN();

    // depends on between lognormal/gamma distribution for mean_duration diff_pos_days
    bool mean_duration_gamma = false;
    // mean/shape and sd/scale of the difference between last positive and first positive days for lognormal/gamma distribution
    double mean_shape_diff_pos_days = std::numeric_limits<double>::signaling_NaN();
    double sd_scale_diff_pos_days = std::numeric_limits<double>::signaling_NaN();

    // boolean choosing between gamma and lognormal distribution for equation 11
    bool multi_factor_gamma = false;

    // pairwise sample of case-specific P* parameters
    bool pairwise_p_star_sample = false;

    // q^(i+1) array
    // All the values of q^1... q^v are stored in this array.
    // This avoids the recalculation of those values every second time step. */
    std::array<double, MolineauxInfection::V> q_pow{};

    MolineauxInfectionParams() = default;
    MolineauxInfectionParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options);
};

}
}
#endif
