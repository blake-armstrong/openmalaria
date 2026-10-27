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

#include "Clinical/ClinicalModel.h"

#include <algorithm>

#include "Clinical/EventScheduler.h"
#include "Clinical/ImmediateOutcomes.h"
#include "Clinical/DecisionTree5Day.h"
#include "Host/NeonatalMortality.h"
#include "mon/Monitoring.h"
#include "util/ModelOptions.h"
#include "util/CommandLine.h"
#include "util/errors.h"
#include "util/UnitParse.h"
#include "schema/scenario.h"

#include <memory>

namespace OM::Clinical {

// -----  simulation-wide parameters  -----

ClinicalParams::ClinicalParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                               const util::CommandLineOptions& command_line,
                               const WithinHost::DiagnosticRegistry& diagnostics, const scnXml::Scenario& scenario,
                               const Clock& clock)
    : diagnostics(&diagnostics) {
    const scnXml::Clinical& clinical = scenario.getModel().getClinical();
    try {
        indirect_mort_bugfix = options.option(util::IndirectMortalityFix);
        //NOTE: if changing XSD, this should not have a default unit:
        health_system_memory =
            UnitParse::readShortDuration(clinical.getHealthSystemMemory(), UnitParse::STEPS, clock, command_line);
        odds_ratio_threshold = exp(parameters[Parameter::LogOddsRatioCfCommunity]);
        non_malaria_infant_mortality = parameters[Parameter::NonMalariaInfantMortality];
    } catch (const util::format_error& e) {
        throw util::xml_scenario_error(string("model/clinical/healthSystemMemory: ").append(e.message()));
    }

    if (options.option(util::ClinicalEventScheduler)) {
        kind = Kind::EventScheduler;
        event_scheduler = EventSchedulerParams(parameters, options, clinical, clock, command_line);
    } else {
        if (scenario.getHealthSystem().getImmediateOutcomes().present()) {
            kind = Kind::ImmediateOutcomes;

            if (command_line.option(util::CommandLine::DeprecationWarnings)) {
                cerr << "Deprecation warning: healthSystem: use of ImmediateOutcomes can be replaced by the more "
                        "flexible DecisionTree5Day (optional)"
                     << endl; // NOLINT(performance-avoid-endl)
            }
        }
        // else: decision tree 5 day

        five_day.cfr_pf_use_hospital = options.option(util::CfrPfUseHospital);
    }

    health_system_memory_fix = options.option(util::HealthSystemMemoryFix);
    vivax_simple_model = options.option(util::VivaxSimpleModel);
}

void ClinicalParams::set_health_system(const scnXml::HealthSystem& health_system, DecisionTreeLibrary& decision_trees,
                                       WithinHost::WithinHostModel& within_host,
                                       const interventions::InterventionManager& interventions, const Clock& clock,
                                       const util::CommandLineOptions& command_line) {
    case_fatality_rate.set(health_system.getCFR(), "CFR", clock, command_line);
    p_sequelae_inpatient.set(health_system.getPSequelaeInpatient(), "pSequelaeInpatient", clock, command_line);
    if (kind == Kind::EventScheduler) {
        if (!health_system.getEventScheduler().present()) {
            throw util::xml_scenario_error("Expected EventScheduler "
                                           "section in healthSystem data (initial or intervention)");
        }
        event_scheduler.set_health_system(health_system.getEventScheduler().get(), case_fatality_rate, decision_trees,
                                          clock, command_line);
    } else if (kind == Kind::ImmediateOutcomes) {
        if (!health_system.getImmediateOutcomes().present()) {
            throw util::xml_scenario_error("Expected ImmediateOutcomes "
                                           "section in healthSystem data (initial or intervention)");
        }
        set_immediate_outcomes(health_system.getImmediateOutcomes().get(), within_host, interventions, clock,
                               command_line);
    } else {
        if (!health_system.getDecisionTree5Day().present()) {
            throw util::xml_scenario_error("Expected DecisionTree5Day "
                                           "section in healthSystem data (initial or intervention)");
        }
        set_decision_tree_5day(health_system.getDecisionTree5Day().get(), decision_trees, within_host, interventions,
                               clock, command_line);
    }
}

unique_ptr<ClinicalModel> ClinicalParams::create(double treatment_seeking_factor,
                                                 mon::MonitoringState& monitoring) const {
    if (kind == Kind::EventScheduler) {
        return std::make_unique<ClinicalEventScheduler>(*this, treatment_seeking_factor, monitoring);
    } else if (kind == Kind::ImmediateOutcomes) {
        return std::make_unique<ImmediateOutcomes>(*this, treatment_seeking_factor, monitoring);
    } else {
        return std::make_unique<DecisionTree5Day>(*this, treatment_seeking_factor, monitoring);
    }
}

double ClinicalModel::get_community_cfr(double case_fatality_ratio) const {
    const double x = case_fatality_ratio * params().odds_ratio_threshold;
    return x / (1 - case_fatality_ratio + x);
}

// -----  non-static construction, destruction and checkpointing  -----

ClinicalModel::ClinicalModel(const ClinicalParams& params, mon::MonitoringState& monitoring)
    : latest_report(monitoring, params.health_system_memory, params.health_system_memory_fix),
      clinical_params(&params) {}

// -----  other non-static methods  -----

bool ClinicalModel::is_dead(SimTime age) {
    if (age >= Sim::max_human_age()) // too old (reached age limit)
        doomed = DoomedTooOld;
    return doomed > NotDoomed; // killed by some means
}

void ClinicalModel::update(Human& human, double age_years, bool new_born,
                           const Host::NeonatalMortality& neonatal_mortality) {
    if (doomed < NotDoomed) // Countdown to indirect mortality
        doomed -= Sim::one_ts();

    //indirect death: if this human's about to die, don't worry about further episodes:
    if (doomed <= DoomedExpired) { //clinical bout 6 intervals before
        mon::record_event(mon::measure("nIndDeaths"), human);
        doomed = DoomedIndirect;
        return;
    }
    if (new_born /* i.e. first update since birth */) {
        // Chance of neonatal mortality:
        if (neonatal_mortality.event_neonatal_mortality(human.clinical_rng)) {
            mon::record_event(mon::measure("nIndDeaths"), human);
            doomed = DoomedNeonatal;
            return;
        }
    }

    const bool is_doomed = doomed != NotDoomed;
    WithinHost::Pathogenesis::StatePair pg = human.within_host_model->determine_morbidity(human, age_years, is_doomed);
    latest_state = static_cast<Episode::State>(pg.state);

    do_clinical_update(human, age_years, pg);
}

void ClinicalModel::update_infant_deaths(SimTime age, InfantMortality& infant_mortality) {
    // update array for the infant death rates
    if (age < Sim::one_year()) {
        const size_t index = age / Sim::one_ts();

        // Testing doomed == DOOMED_NEXT_TS gives very slightly different results than
        // testing doomed == DOOMED_INDIRECT (due to above if(..))
        const bool is_doomed = doomed == DoomedComplicated || doomed == DoomedNextTs || doomed == DoomedNeonatal;
        infant_mortality.report_risk(index, is_doomed);
    }
}

void ClinicalModel::checkpoint(istream& stream) {
    latest_report & stream;
    doomed & stream;
}
void ClinicalModel::checkpoint(ostream& stream) {
    latest_report & stream;
    doomed & stream;
}

// ——— infant mortality ———

InfantMortality::InfantMortality(double non_malaria_mortality)
    : infant_deaths(Sim::steps_per_year()), infant_intervals_at_risk(Sim::steps_per_year()),
      non_malaria_mortality(non_malaria_mortality) {}

void InfantMortality::pre_main_sim_init() {
    std::ranges::fill(infant_deaths, 0);
    std::ranges::fill(infant_intervals_at_risk, 0);
}

void InfantMortality::checkpoint(istream& stream) {
    infant_deaths & stream;
    infant_intervals_at_risk & stream;
}
void InfantMortality::checkpoint(ostream& stream) {
    infant_deaths & stream;
    infant_intervals_at_risk & stream;
}

void InfantMortality::report_risk(size_t index, bool is_doomed) {
    infant_intervals_at_risk[index] += 1; // baseline
    if (is_doomed)
        infant_deaths[index] += 1; // deaths
}

double InfantMortality::all_cause() const {
    double infant_prop_surviving = 1.0; // use to calculate proportion surviving
    for (size_t i = 0; i < Sim::steps_per_year(); i += 1) {
        // multiply by proportion of infants surviving at each interval
        infant_prop_surviving *= static_cast<double>(infant_intervals_at_risk[i] - infant_deaths[i]) /
                                 static_cast<double>(infant_intervals_at_risk[i]);
    }
    // Child deaths due to malaria (per 1000), plus non-malaria child deaths. Deaths per 1000 births is the return unit.
    return (1.0 - infant_prop_surviving) * 1000.0 + non_malaria_mortality;
}

}
