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

#ifndef HMOD_CLINICAL_PARAMS
#define HMOD_CLINICAL_PARAMS

#include "Global.h"
#include "Host/WithinHost/WHInterface.h"
#include "Parameters.h"
#include "util/AgeGroupInterpolation.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace scnXml {
class Scenario;
class Clinical;
class HealthSystem;
class HSImmediateOutcomes;
class HSDT5Day;
class HSEventScheduler;
}

namespace OM {
namespace WithinHost {
class DiagnosticRegistry;
struct WithinHostModel;
}
namespace interventions {
class InterventionManager;
}
namespace mon {
struct MonitoringState;
}
namespace Clinical { // NOLINT(readability-identifier-naming)

class ClinicalModel;
class CMDecisionTree;
class DecisionTreeLibrary;

enum CaseType : std::uint8_t { FirstLine, SecondLine, NumCaseTypes }; // NOLINT(cppcoreguidelines-use-enum-class)

struct CM5DayParams {
    bool cfr_pf_use_hospital = false;
    std::array<double, NumCaseTypes> access_uc_any{};
    std::array<double, NumCaseTypes> access_uc_self_treat{};
    double access_severe = 0.0;
    double cure_rate_severe = 0.0;
    WithinHost::TreatmentId treatment_severe;
};

struct ImmediateOutcomesParams {
    std::array<double, NumCaseTypes> cure_rate_uc_official{};
    std::array<double, NumCaseTypes> cure_rate_uc_self_treat{};
    std::array<WithinHost::TreatmentId, NumCaseTypes> treatment_uc;
    bool use_diagnostic_uc = false;
};

struct DecisionTree5DayParams {
    const CMDecisionTree* tree_uc_official = nullptr;
    const CMDecisionTree* tree_uc_self_treat = nullptr;
};

struct EventSchedulerParams {
    EventSchedulerParams() = default;
    EventSchedulerParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                         const scnXml::Clinical& clinical);

    void set_health_system(const scnXml::HSEventScheduler& es_data, util::AgeGroupInterpolator& case_fatality_rate,
                           DecisionTreeLibrary& decision_trees);
    void set_parameters(const scnXml::HSEventScheduler& es_data, util::AgeGroupInterpolator& case_fatality_rate);

    bool non_malaria_fevers = false;

    const CMDecisionTree* uncomplicated_tree = nullptr;
    const CMDecisionTree* complicated_tree = nullptr;

    /// Maximum number of time steps (including first of case) an individual
    /// will remember they are sick before resetting.
    SimTime max_uc_seeking_memory = Sim::never();
    /// Length of an uncomplicated case
    SimTime uncomplicated_case_duration = Sim::never();
    /// Length of a complicated case
    SimTime complicated_case_duration = Sim::never();
    /// Time-span for which individual is at risk of death in complicated case
    /// minus length of complicated case (must be <= 0)
    SimTime extra_days_at_risk = Sim::never();
    /// First value is probability immediate treatment, second is first +
    /// probability 1-day delay to treatment seeking, etc. Last value must be 1.
    /// Index units are days.
    std::vector<double> cum_daily_pr_imm_ucts;

    /// Parameter of S(t) for t > 0
    double neg_v = 0.0;
    /// Parameter
    double alpha = 0.0;

    /** Base log odds of treatment of non-malarial fevers in absense of a
     * malaria diagnostic and irrespective of whether treatment is needed.
     *
     * In our model, this is logit(P₀), not β₀. */
    double log_odds_ab_base = std::numeric_limits<double>::signaling_NaN();
    /** Added to log odds treatment when a malaria diagnostic indicates no
     * parasites. Symbol in model: β₁. */
    double log_odds_ab_neg_test = std::numeric_limits<double>::signaling_NaN();
    /** Added to log odds treatment when a malaria diagnostic indicates
     * parasites. Symbol in model: β₂. */
    double log_odds_ab_pos_test = std::numeric_limits<double>::signaling_NaN();
    /** Added to log odds treatment when NMF is categorized as an illness
     * potentially leading to death (Pathogenesis::NEED_ANTIBIOTIC).
     * Symbol in model: β₃. */
    double log_odds_ab_need = std::numeric_limits<double>::signaling_NaN();
    /** Added to log odds treatment when given by an informal provider.
     * Symbol in model: β₄. */
    double log_odds_ab_informal = std::numeric_limits<double>::signaling_NaN();
    /** One minus the efficacy of antibiotic/NMF treatment (i.e. a multiplier
     * for fatality-rate given that the case is treated). */
    double one_minus_efficacy_ab = std::numeric_limits<double>::signaling_NaN();
    /** Case fatality rate of non-malaria fevers requiring treatment given that
     * the case is not treated. */
    util::AgeGroupInterpolator severe_nmf_mortality;

    /// Probability that an NMF needs antibiotic treatment and could lead to death.
    util::AgeGroupInterpolator nmf_need_antibiotic;
    /** Probability that a malarial fever classified as uncomplicated requires
     * antibiotic treatment (and death could occur from non-malarial causes).
     * Unclear whether using this at the same time as comorbidity parameters
     * makes any sense. */
    util::AgeGroupInterpolator mf_need_antibiotic;
};

struct ClinicalParams {
    enum class Kind : std::uint8_t { DecisionTree5Day, ImmediateOutcomes, EventScheduler };

    ClinicalParams() = default;
    ClinicalParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                   const util::CommandLineOptions& command_line, const WithinHost::DiagnosticRegistry& diagnostics,
                   const scnXml::Scenario& scenario);

    void set_health_system(const scnXml::HealthSystem& health_system, DecisionTreeLibrary& decision_trees,
                           WithinHost::WithinHostModel& within_host,
                           const interventions::InterventionManager& interventions);

    std::unique_ptr<ClinicalModel> create(double treatment_seeking_factor, mon::MonitoringState& monitoring) const;

    Kind kind = Kind::DecisionTree5Day;
    bool indirect_mort_bugfix = false;
    bool health_system_memory_fix = false;
    bool vivax_simple_model = false;

    const WithinHost::DiagnosticRegistry* diagnostics = nullptr;

    /// The maximum age of a sickness bout, for another bout to be considered
    /// part of the same episode.
    SimTime health_system_memory = Sim::never();

    //log odds ratio of case-fatality in community compared to hospital
    double odds_ratio_threshold = 0.0;
    /// Non-malaria mortality in under 1year olds.
    double non_malaria_infant_mortality = 0.0;

    /// Age-specific hospital case fatality "rates"
    util::AgeGroupInterpolator case_fatality_rate;
    /// Age-specific in-hospital rates of sequelae given a severe malaria bout
    /// Note: out-patients have currently have the same probabilities of sequelae
    util::AgeGroupInterpolator p_sequelae_inpatient;

    CM5DayParams five_day;
    ImmediateOutcomesParams immediate_outcomes;
    DecisionTree5DayParams decision_tree_5day;
    EventSchedulerParams event_scheduler;

private:
    void set_immediate_outcomes(const scnXml::HSImmediateOutcomes& hs_description,
                                WithinHost::WithinHostModel& within_host,
                                const interventions::InterventionManager& interventions);
    void set_decision_tree_5day(const scnXml::HSDT5Day& hs_description, DecisionTreeLibrary& decision_trees,
                                WithinHost::WithinHostModel& within_host,
                                const interventions::InterventionManager& interventions);
};

}

}

#endif
