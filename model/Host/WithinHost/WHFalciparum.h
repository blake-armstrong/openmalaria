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

#ifndef HMOD_WITHIN_HOST_IMMUNITY
#define HMOD_WITHIN_HOST_IMMUNITY

#include "Global.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Treatments.h"
#include "util/ModelOptions.h"
#include "util/vectors.h"

#include <list>
#include <memory>

using namespace std;

class InfectionImmunitySuite;

namespace OM::WithinHost {
class DiagnosticRegistry;
class GenotypeRegistry;
namespace Pathogenesis {
class PathogenesisModel;
struct PathogenesisParams;
}

struct FalciparumParams {
    //Standard dev innate immunity for densities
    double sigma_i = 0.0;

    // contribution of parasite densities to acquired immunity in the presence of fever
    double imm_penalty_22 = 0.0;

    // Remaining immunity against asexual parasites(after each time step, each of 2 components y and h)
    // This variable decays the effectors m_cumulative_h and m_cumulative_Y exponentially.
    double imm_effector_remain = 0.0;

    // Remaining immunity against asexual parasites(after time step, each of 2 components y and h)
    // This variable decays the effectors m_cumulative_h and m_cumulative_Y in a way that their
    // effects on densities (1-Dh and 1-Dy) decay exponentially.
    double asex_imm_remain = 0.0;

    /// Critical value for immunity trigger (cumulative densities)
    double inv_cumulative_y_star = 0.0;
    /// Critical value for immunity trigger (cumulative inoculations)
    double inv_cumulative_h_star = 0.0;

    /// Maternal protection at birth
    double alpha_m = 0.0;

    /// More or less (up to 0.693) inverse quantity of alphaMStar (AJTM p. 9 eq. 12),
    /// decay rate of maternal protection in years^(-1).
    double decay_m = 0.0;

    /// Length of m_y_lag array. Wouldn't have to be dynamic if Global::interval was known at compile-time.
    int y_lag_len = 0;

    /// True if any by-genotype reporting is enabled
    bool report_infections_by_genotype = false;

    bool vaccine_genotype = false;

    SimTime latent_p = Sim::never();

    const GenotypeRegistry* genotypes = nullptr;
    const DiagnosticRegistry* diagnostics = nullptr;

    FalciparumParams() = default;
    FalciparumParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                     const GenotypeRegistry& genotypes, const DiagnosticRegistry& diagnostics);
};

/**
 * Immunity code and base class for all current P. falciparum models.
 */
class WHFalciparum : public WHInterface {
public:
    /// @brief Constructors, destructors and checkpointing functions
    //@{
    WHFalciparum(const FalciparumParams& params, const Pathogenesis::PathogenesisParams& pathogenesis,
                 const TreatmentRegistry& treatments, LocalRng& rng, double comorbidity_factor);
    ~WHFalciparum() override;
    //@}

    double prob_transmission_to_mosquito(vector<double>& prob_trans_genotype_i,
                                         vector<double>& prob_trans_genotype_l) const override;

    // No PQ treatment for falciparum in current models:
    void optional_pq_treatment(Host::Human& human) override {}

    inline double get_total_density() const override { return total_density; }

    bool diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const override;
    void treatment(Host::Human& human, TreatmentId treat_id) override;
    bool treat_simple(Host::Human& human, SimTime time_liver, SimTime time_blood) override;

    Pathogenesis::StatePair determine_morbidity(Host::Human& human, double age_years, bool is_doomed) override;

    inline double get_cumulative_h() const override { return cumulative_h; }
    inline double get_cumulative_y() const override { return cumulative_y; }

protected:
    /** Clear infections of the appropriate stages.
     * 
     * @param stage Which stages of the infection are affected
     */
    virtual void clear_infections(Treatments::Stages stage) = 0;

    ///@brief Immunity model parameters
    //@{
    /** Updates for the immunity model − assumes m_cumulative_h and m_cumulative_Y
     * have already been incremented.
     *
     * Applies decay of immunity against asexual blood stages, if present. */
    void update_immune_status();

    /** @returns A multiplier describing the proportion of parasites surviving
     * immunity effects this time step.
     * 
     * Note that in the Descriptive model this multiplies log(density), but the
     * new density has no effect on future densities, wheras the Empirical model
     * multiplies the actual density (which then affects density on the following
     * time step). */
    double immunity_survival_factor(double age_in_years, double cumulative_exposure_j);

    //!innate ability to control parasite densities
    double innate_imm_surv_fact;

    /** Number of infections received since birth. */
    double cumulative_h{0.0};
    /// Cumulative parasite density since birth (units: days * units of density)
    double cumulative_y{0.0};
    /// m_cumulative_Y from previous time step
    double cumulative_y_lag{0.0};
    //@}

    /// Total asexual blood stage density (sum of density of infections).
    double total_density{0.0};
    /// Density from the point of view of diagnostics dependent on HRP2
    double hrp2_density{0.0};

    /** Maximum parasite density of any infection during the previous interval.
     *
     * With 5-day time steps, this is not just the maximum density of any infection
     * at the end of the time step, but something designed to emulate the maximum
     * of 5 daily samples. */
    double time_step_max_density{0.0};

    /** Total asexual blood stage density over last 20 days (uses samples from
    * 10, 15 and 20 days ago).
    *
    * m_y_lag[sim::ts0().moduloSteps(y_lag_len)] corresponds to the density
    * from the previous time step (once updateInfection has been called). */
    std::vector<double> y_lag_imported, y_lag_local;

    /// The PathogenesisModel introduces illness dependant on parasite density
    unique_ptr<Pathogenesis::PathogenesisModel> pathogenesis_model;

    /// End of step on which treatment expires = start of first step after expiry
    SimTime treat_expiry_liver{0}, treat_expiry_blood{0};

    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

    const FalciparumParams& params() const { return *falciparum_params; }
    const GenotypeRegistry& genotypes() const { return *params().genotypes; }
    const DiagnosticRegistry& diagnostics() const { return *params().diagnostics; }

private:
    const FalciparumParams* falciparum_params;
    const TreatmentRegistry* treatments;

    friend class ::InfectionImmunitySuite;
};

}

#endif
