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

#ifndef HMOD_WITHIN_HOST_DESCRIPTIVE
#define HMOD_WITHIN_HOST_DESCRIPTIVE

#include "Host/WithinHost/WHFalciparum.h"
#include "Host/WithinHost/Infection/DescriptiveInfection.h"

#include <list>
#include <memory>

using namespace std;

namespace OM::WithinHost {

struct DescriptiveWithinHostParams {
    bool report_patent_infected = false;
};

/** Within-host model class for the original (descriptive) infection model.
 *
 * Note: this implementation has a few bugs with (hopefully) small effect
 * conditionally fixed (see MAX_DENS_CORRECTION and INNATE_MAX_DENS).
 * This allows reproduction of old results and is the main reason it cannot be
 * integrated with the CommonWithinHost class. */
class DescriptiveWithinHostModel : public WHFalciparum {
public:
    /// Create a new WHM
    DescriptiveWithinHostModel(const FalciparumParams& params, const DescriptiveWithinHostParams& descriptive_params,
                               LocalRng& rng, double comorbidity_factor);

    void import_infection(LocalRng& rng) override;

    /// load an infection from a checkpoint
    virtual void load_infection(istream& stream);
    void clear_immunity() override;

    void update(Host::Human& human, LocalRng& rng, int& n_new_infs_i, int& n_new_infs_l,
                vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double age_in_years) override;

    InfectionOrigin infection_origin() const override;

    bool summarize(Host::Human& human) const override;

protected:
    void clear_infections(Treatments::Stages stage) override;

    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

    // Doesn't do anything in this model:
    void treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d) override;

private:
    /** The list of all infections this human has.
     * 
     * Since infection models and within host models are very much intertwined,
     * the idea is that each WithinHostModel has its own list of infections. */
    std::list<std::unique_ptr<DescriptiveInfection>> infections;

    bool opt_vaccine_genotype = false;

    const DescriptiveWithinHostParams* descriptive_params;
};

}

#endif
