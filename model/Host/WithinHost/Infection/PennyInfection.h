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

/*This model refers to the paper:
 *PENNY et al  (2011). The potential effects of blood stage vaccines on the within-host dynamics of Plasmodium Falciparum */

#ifndef HMOD_PENNYINFECTION_H
#define HMOD_PENNYINFECTION_H

#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/ModelOptions.h"

class PennyInfectionSuite;

namespace OM {
namespace WithinHost {

struct PennyInfectionParams {
    bool immune_threshold_gamma = false;
    bool update_density_gamma = false;

    PennyInfectionParams() = default;
    explicit PennyInfectionParams(const util::ModelOptionFlags& options);
};

class PennyInfection : public CommonInfection {
public:
    /// Constructor
    PennyInfection(const PennyInfectionParams& params, SimTime latent_p, LocalRng& rng, uint32_t prot_id, int origin);
    /// Resume from a checkpoint
    PennyInfection(const PennyInfectionParams& params, SimTime latent_p, istream& stream);
    /// Destructor
    virtual ~PennyInfection() {};

    virtual bool update_density(LocalRng& rng, double survival_factor, SimTime bs_age, double);

    /** Get the density of sequestered parasites. */
    inline double seq_density(int age_days) {
        size_t today_v = mod_nn(age_days, DeltaV);
        return seq_densities[today_v];
    }

protected:
    virtual void checkpoint(ostream& stream);

private:
    // function to obtain the summation component of variant specific immunity
    double get_variant_specific_summation(LocalRng& rng, int age_days);
    // function to obtain the summation component of clonal immunity
    double get_clonal_summation(int age_days);

    // delta_C := delay to clonal antibody response (days) (value 7.2038 round to 7)
    static const int DeltaC = 7;
    // delta_V := delay to variant specific antibody response in R_V^x (days) (value 6.3572 round to 6)
    static const int DeltaV = 6;

    // Circulating densities, 1 to delta_C days ago.
    // index (age mod delta_C) corresponds to delta_C days ago.
    double cir_densities[DeltaC];
    // as above, but length delta_V
    double seq_densities[DeltaV];

    // threshold_N is critical threshold for innate immunity (for sigmoidal immune function)
    double threshold_n;
    // threshold_V is critical threshold for variant specific immunity (for sigmoidal immune function)
    double threshold_v;
    // threshold_C is critical threshold for clonal immunity (for sigmoidal immune function)
    double threshold_c;

    // tracked summation of densities with decay for variant specific immunity (sigmoidal function)
    double variant_specific_summation;
    // tracked summation of densities with decay for clonal immunity (sigmoidal function)
    double clonal_summation;

    const PennyInfectionParams* params;

    // allow unittest to access private vars
    friend class ::PennyInfectionSuite;
};

}
}
#endif
