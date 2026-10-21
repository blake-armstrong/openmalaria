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

#ifndef Hmod_CommonWithinHost
#define Hmod_CommonWithinHost

#include "Global.h"
#include "Host/WithinHost/WHFalciparum.h"
#include "Host/WithinHost/Infection/CommonInfection.h"
#include "PkPd/LSTMModel.h"
#include "util/AgeGroupInterpolation.h"

#include <limits>

using namespace std;

namespace OM {
namespace WithinHost {

struct CommonWithinHostParams {
    util::AgeGroupInterpolator mass_by_age;
    double het_mass_mult_std_dev = std::numeric_limits<double>::signaling_NaN();
    double min_het_mass_mult = std::numeric_limits<double>::signaling_NaN();
    bool report_infected_or_patent_infected = false;

    CommonWithinHostParams() = default;
    explicit CommonWithinHostParams(const scnXml::Scenario& scenario);
};

/** Common within-host model functionality.
 *
 * This is not used by the old Descriptive within-host
 * models, but encapsulates nearly all the within-host (non-infection) code
 * required by the Dummy and Empirical within-host models.
 */
class CommonWithinHost : public WHFalciparum {
public:
    CommonWithinHost(const FalciparumParams& params, const CommonWithinHostParams& common_params, LocalRng& rng,
                     double comorbidity_factor);
    virtual ~CommonWithinHost();

    virtual void importInfection(LocalRng& rng);

    virtual void treatPkPd(size_t schedule, size_t dosage, double age, double delay_d);
    virtual void clearImmunity();

    virtual void update(Host::Human& human, LocalRng& rng, int& nNewInfs_i, int& nNewInfs_l,
                        vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double ageInYears);

    virtual void addProphylacticEffects(const vector<double>& pClearanceByTime);

    /** \brief Factory functions to create infections.
     *
     * These allow creation of the correct type of infection in a generic manner.
     * 
     * The first variant is for creating a new infection, the second for loading
     * one from a checkpoint. */
    //@{
    static CommonInfection* (*createInfection)(LocalRng& rng, uint32_t protID, int origin);
    static CommonInfection* (*checkpointedInfection)(istream& stream);
    //@}

    virtual InfectionOrigin getInfectionOrigin() const;

    virtual bool summarize(Host::Human& human) const;

protected:
    virtual void clearInfections(Treatments::Stages stage);

    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);

private:
    /// Multiplies the mean mass (for this age) as a heterogeneity factor.
    double hetMassMultiplier;

    /// Encapsulates drug code for each human
    pk_pd::LSTMModel pkpdModel;

    /** The list of all infections this human has.
     *
     * Since infection models and within host models are very much intertwined,
     * the idea is that each WithinHostModel has its own list of infections. */
    //TODO: better to template class over infection type than use dynamic type?
    std::list<CommonInfection*> infections;

    bool opt_vaccine_genotype = false;

    const CommonWithinHostParams* common_params;
};

}
}
#endif
