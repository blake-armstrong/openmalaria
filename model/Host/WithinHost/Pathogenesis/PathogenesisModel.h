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

#ifndef HMOD_PATHOGENESIS_MODEL
#define HMOD_PATHOGENESIS_MODEL

#include "Global.h"
#include "Parameters.h"
#include "Host/WithinHost/Pathogenesis/State.h"
#include "util/AgeGroupInterpolation.h"
#include "util/ModelOptions.h"
#include "util/random.h"

#include <cstdint>
#include <limits>
#include <memory>

namespace scnXml {
class HSESNMF;
class Clinical;
}

namespace OM {
namespace Host {
class Human;
}

namespace WithinHost::Pathogenesis {

using util::LocalRng;

class PathogenesisModel;

struct MuellerParams {
    double rate_multiplier_31 = 0.0;
    double density_exponent_32 = 0.0;

    MuellerParams() = default;
    MuellerParams(const OM::Parameters& parameters, const Clock& clock);
};

struct PyrogenParams {
    //Pyrogenic threshold at birth (Y*0)
    double init_pyro_thres = 0.0;
    // Derived parameters without good names:
    double b = std::numeric_limits<double>::signaling_NaN();
    // Ystar2: critical value in determining increase in pyrogenic threshold
    double ystar2_13 = 0.0;
    double a = std::numeric_limits<double>::signaling_NaN();
    //Ystar1: critical value of parasite density in determing increase in pyrog t
    double ystar1_26 = 0.0;

    PyrogenParams() = default;
    PyrogenParams(const OM::Parameters& parameters, const Clock& clock);
};

struct PathogenesisParams {
    enum class Kind : std::uint8_t { Pyrogen, Mueller, Predetermined };

    PathogenesisParams() = default;
    PathogenesisParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                       const scnXml::Clinical& clinical, bool nmf_only, const Clock& clock,
                       const util::CommandLineOptions& command_line);

    std::unique_ptr<PathogenesisModel> create(double comorbidity_factor) const;

    /** For Vivax: determine the chance of a NMF and sample, returning either
     * None or StateNmf. */
    Pathogenesis::State sample_nmf(LocalRng& rng, double age_years) const;

    Kind kind = Kind::Pyrogen;
    /// Comorbidity prevalence at birth as a risk factor for indirect mortality
    double indir_risk_co_factor = 0.0;
    /// sevMal: critical density for severe malaria bout (Y*B1)
    double severe_mal_threshold = 0.0;
    /// Comorbidity prevalence at birth as a risk factor for severe
    double comorb_intercept = 0.0;
    /// One over critical age for co-morbidity (for both severe and indirect)
    double inv_crit_age_comorb = 0.0;
    /// Rate of Non-Malaria Fever incidence by age. Non-seasonal.
    util::AgeGroupInterpolator nmf_incidence;
    MuellerParams mueller;
    PyrogenParams pyrogen;
};

/*! PathogenesisModel abstract base class.
 *
 * Previously named MorbidityModel and PresentationModel. */
class PathogenesisModel {
public:
    virtual ~PathogenesisModel() = default;

    /** Determines the health of the individual based on his/her parasitemia.
     *
     * May introduce severe or uncomplicated cases of malaria, as well as non-
     * malaria fevers. */
    StatePair determine_state(Host::Human& human, double age_years, double time_step_max_density, double end_density,
                              bool is_doomed);

    /** Summarize PathogenesisModel details
     *
     * Only PyrogenPathogenesis implements this; other models don't have anything
     * to add to the summary. */
    virtual void summarize(const Host::Human& human) {}

    /// Checkpointing
    template <class S> void operator&(S& stream) { checkpoint(stream); }

protected:
    /** Create a PathogenesisModel. */
    PathogenesisModel(const PathogenesisParams& params, double comorbidity_factor);

    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);

protected:
    virtual double get_p_episode(double time_step_max_density, double total_density) = 0;

    const PathogenesisParams& params() const { return *pathogenesis_params; }

    //! comorbidity factor for heterogeneity
    double comorbidity_factor;

private:
    const PathogenesisParams* pathogenesis_params;
};

}

}
#endif
