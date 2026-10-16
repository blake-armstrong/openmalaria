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

#ifndef HMOD_LSTM_DRUG_CONVERSION
#define HMOD_LSTM_DRUG_CONVERSION

#include "Global.h"
#include "PkPd/Drug/LSTMDrug.h"
#include "PkPd/Drug/LSTMDrugType.h"

using namespace std;

namespace OM {

namespace pk_pd {
struct DoseParams;
}

namespace pk_pd {

struct ParamsConvFactor;

/** A class holding pkpd drug use info.
 * 
 * Conversion model: two 1-compartment models with conversion.
 *
 * Each human has an instance for each type of drug present in their blood. */
class LSTMDrugConversion : public LSTMDrug {
public:
    /** Create a new instance. */
    LSTMDrugConversion(const LSTMDrugType& parent, const LSTMDrugType& metabolite, LocalRng& rng);

    size_t get_index() const override;
    double get_concentration(size_t index) const override;

    double calculate_drug_factor(LocalRng& rng, WithinHost::CommonInfection* inf, double body_mass) const override;
    void update_concentration(double body_mass) override;
    double get_metabolite_concentration() const;
    double get_parent_concentration() const;

protected:
    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

    double calculate_factor(const ParamsConvFactor& p, double duration) const;

    //TODO: do we need to link these?
    const LSTMDrugType &parent_type, &metabolite_type;

    /// Amount of parent drug in gut, parent drug in circulation and metabolite
    // in circulation; units: mg. In the paper, these are labelled A, B, C.
    //TODO: move metabolite data to a separate class object? Or not?
    double qty_g{0.0}, qty_p{0.0}, qty_m{0.0};

    /// @brief Sampled elimination and conversion rate constants
    //@{
    /// Absorbtion rate constant (-x)
    double nka;
    /// Elimination rate constant for parent (-y * body_mass ^ m_exponent)
    double nk_p_sample;
    /// Conversion rate constant (-z * body_mass ^ m_exponent)
    double nconv_sample;
    /// Elimination rate constant for metabolite (-k * body_mass ^ m_exponent)
    double nk_m_sample;
    /// Volume of distribution of metabolite (parent's Vd uses vol_dist)
    double vol_dist_metabolite;
    /// Last body mass (kg)
    double last_bm{};
    //@}

private:
    void set_conversion_parameters(ParamsConvFactor& p, double body_mass) const;
    void set_killing_parameters(LocalRng& rng, ParamsConvFactor& p, WithinHost::CommonInfection* inf) const;
};

}
}
#endif
