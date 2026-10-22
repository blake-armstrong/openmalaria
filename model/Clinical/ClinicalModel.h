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

#ifndef HMOD_CLINICAL_MODEL
#define HMOD_CLINICAL_MODEL

#include "Clinical/ClinicalParams.h"
#include "Host/Human.h"
#include "Episode.h"
#include <cassert>
#include <memory>
#include <vector>

namespace OM {
namespace Host {
class NeonatalMortality;
}
namespace Clinical {
using Host::Human;

class InfantMortality;

/** The clinical model models the effects of sickness dependant on malarial
 * parasite densities and administers anti-malaria treatments via the drug
 * model (or in a simpler case, directly clearing infections).
 * 
 * So far, sickness types include uncomplicated and severe malaria cases and
 * non-malaria sickness.
 * 
 * Patient outcomes include full recovery, recovery with sequelae and death.
 * 
 * Reporting includes patient outcome and potentially drug usage and use of
 * RDTs (Rapid Diagnostic Tests) for costing purposes. */
class ClinicalModel {
public:
    /// Destructor
    virtual ~ClinicalModel();

    /** Returns true if the human has been killed by some means.
     * 
     * Also kills the human if he/she reaches the simulation age limit.
     */
    bool is_dead(SimTime age);

    /// Returns true if the human has died.
    inline bool is_dead() const { return doomed > NotDoomed; }

    /** Run main part of the model: determine the sickness status and any
     * treatment for the human.
     * 
     * @param ageYears Age of human.
     * @param newBorn True if human age is one time step old */
    void update(Human& human, double age_years, bool new_born, const Host::NeonatalMortality& neonatal_mortality);

    /** For infants, updates the infantIntervalsAtRisk and potentially
     * infantDeaths arrays. */
    void update_infant_deaths(SimTime age, InfantMortality& infant_mortality);

    /** Special option to allow reports not to be delivered for existing cases
     * (within health-system-memory and not new cases). */
    virtual bool is_existing_case() = 0;

    inline static SimTime hs_memory() { return bridge::clinical_params().health_system_memory; }

    /// Force all pending summaries to be reported. Should only be called when
    /// class is about to be destroyed anyway to avoid affecting output.
    inline void flush_reports() { latest_report.flush(); }

    inline const Episode& get_latest_report() const { return latest_report; }

    inline const Episode::State& get_latest_state() const { return latest_state; }

    /// Checkpointing
    template <class S> void operator&(S& stream) { checkpoint(stream); }

    /** Time of the last treatment (sim::never() if never treated). */
    SimTime t_last_treatment = Sim::never();

protected:
    /// Constructor.
    explicit ClinicalModel(const ClinicalParams& params);

    const ClinicalParams& params() const { return *clinical_params; }

    /** Update for clinical model - new pathogenesis status, treatment, etc.
     *
     * @param withinHostModel WithinHostModel of human.
     * @param hostTransmission per-host transmission data of human.
     * @param ageYears Age of human.
     * @param ageGroup Survey age group of human. */
    virtual void do_clinical_update(Human& human, double age_years, WithinHost::Pathogenesis::StatePair& pg) = 0;

    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);

    /** Last episode; report to survey pending a new episode or human's death. */
    Episode latest_report;
    Episode::State latest_state;

    /** @brief Positive values of _doomed variable (codes). */
    enum {
        DoomedExpired = -35,   // codes less than or equal to this mean "dead now"
        DoomedNextTs = -30,    // will expire on next time step
        DoomedStartTimer = -1, // set on start of doomed timer
        NotDoomed =
            0, // all codes greater than this mean "already dead"; codes less than this mean a count-down to death has started
        DoomedTooOld = 1,      ///< died because reached age limit
        DoomedComplicated = 4, ///< died from severe malaria or malaria with a coinfection
        DoomedNeonatal = 6,    ///< died due to mother's malaria infection
        DoomedIndirect = 7,    ///< died indirectly from malaria (after a delay)
    };

    /** Can indicate that the individual is dead or about to die.
     *
     * If doomed < 0, the individual is doomed to die.
     * 
     * If doomed > 0, the individual is dead, and will be removed from the
     * population at the beginning of the next time step. NOTE:
     * updateInfantDeaths() counts deaths after the fact, thus cannot remove immediately. */
    int doomed;

    /// Calculate the case fatality "rate" in the community as a function of
    /// that in hospitals.
    double get_community_cfr(double case_fatality_ratio) const;

private:
    const ClinicalParams* clinical_params;
};

/// Infant mortality reporting
class InfantMortality {
public:
    InfantMortality() = default;
    explicit InfantMortality(double non_malaria_mortality);

    void pre_main_sim_init();

    void checkpoint(istream& stream);
    void checkpoint(ostream& stream);

    void report_risk(size_t index, bool is_doomed);

    /// Calculate infant mortality as deaths/1000 livebirths for the whole main-
    /// simulation period (not as deaths/1000 years-at-risk per survey).
    ///
    /// This mimicks field data on all-cause mortality in infants.
    /// Uses the kaplan-meier method because the demography was set up to provide
    /// a stable age-distribution but unfortunately does not accurately describe
    /// death rates. The kaplan-meier estimate is the product of the proportion of
    /// infants survivng at each interval.
    double all_cause() const;

private:
    // Infant death summaries (checkpointed).
    std::vector<int> infant_deaths;
    std::vector<int> infant_intervals_at_risk;

    /// Non-malaria mortality in under 1year olds.
    double non_malaria_mortality = 0.0;
};

}

namespace bridge {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline const Clinical::InfantMortality* current_infant_mortality = nullptr;

inline const Clinical::InfantMortality& infant_mortality() {
    assert(current_infant_mortality != nullptr);
    return *current_infant_mortality;
}

}

class InfantMortalityBinding {
public:
    explicit InfantMortalityBinding(const Clinical::InfantMortality& infant_mortality)
        : previous(bridge::current_infant_mortality) {
        bridge::current_infant_mortality = &infant_mortality;
    }
    InfantMortalityBinding(const InfantMortalityBinding&) = delete;
    InfantMortalityBinding& operator=(const InfantMortalityBinding&) = delete;
    InfantMortalityBinding(InfantMortalityBinding&&) = delete;
    InfantMortalityBinding& operator=(InfantMortalityBinding&&) = delete;
    ~InfantMortalityBinding() { bridge::current_infant_mortality = previous; }

private:
    const Clinical::InfantMortality* previous;
};

}
#endif
