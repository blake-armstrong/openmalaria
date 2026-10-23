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

#include "mon/init.h"

#include "Host/WithinHost/Diagnostic.h"
#include "interventions/InterventionManager.h"
#include "mon/Monitoring.h"
#include "schema/monitoring.h"
#include "schema/scenario.h"
#include "util/CommandLine.h"
#include "util/UnitParse.h"
#include "util/errors.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <iostream>
#include <map>

namespace OM {
namespace mon {

using internal::SurveyDate;

namespace {

constexpr uint32_t MaxCohortNumber = uint32_t{1} << 21;

void init_age_groups(const scnXml::Monitoring& monitoring) {
    MonitoringState& runtime = bridge::monitoring();
    const scnXml::MonAgeGroup::GroupSequence& groups = monitoring.getAgeGroup().getGroup();
    if (!(monitoring.getAgeGroup().getLowerbound() <= 0.0)) {
        throw util::xml_scenario_error("Expected survey age-group lowerbound of 0");
    }

    runtime.age_group_upper_bound.resize(groups.size() + 1);
    for (size_t i = 0; i < groups.size(); ++i) {
        runtime.age_group_upper_bound[i] = Sim::from_years_d(groups[i].getUpperbound());
    }
    runtime.age_group_upper_bound[groups.size()] = Sim::future();
}

} // namespace

void init_reporting(const scnXml::Scenario& scenario) {
    MonitoringState& runtime = bridge::monitoring();
    runtime.report_imr = -1;
    const scnXml::MonitoringOptions& opts_elt = scenario.getMonitoring().getSurveyOptions();
    std::vector<OutMeasure> reported_measures;
    reported_measures.reserve(opts_elt.getOption().size());

    std::set<int> out_ids;
    for (const scnXml::MonitoringOption& opt_elt : opts_elt.getOption()) {
        if (!opt_elt.getValue())
            continue;

        OutMeasure om = find_out_measure(opt_elt.getName());
        if (om.measure == InvalidMeasure) {
            if (is_obsolete_measure(opt_elt.getName())) {
                throw util::xml_scenario_error("obsolete survey option: " + std::string(opt_elt.getName()));
            }
            throw util::xml_scenario_error("unrecognised survey option: " + std::string(opt_elt.getName()));
        }
        if ((om.measure == measure("sumlogDens") || om.measure == measure("logDensByGenotype")) &&
            WithinHost::Diagnostics::monitoring_diagnostic().allows_false_positives()) {
            throw util::xml_scenario_error("measure " + std::string(opt_elt.getName()) +
                                           " may not be used when monitoring diagnostic sensitivity < 1");
        }

        auto apply_category = [&](Dim flag, const auto& option, const char* label) {
            if (!option.present())
                return;
            if (option.get() && !(om.dims & flag)) {
                throw util::xml_scenario_error("measure " + std::string(opt_elt.getName()) +
                                               " does not support categorisation by " + label);
            }
            if (!option.get())
                om.dims &= ~flag;
        };
        apply_category(Dim::Age, opt_elt.getByAge(), "age group");
        apply_category(Dim::Cohort, opt_elt.getByCohort(), "cohort");
        apply_category(Dim::Species, opt_elt.getBySpecies(), "species");
        apply_category(Dim::Genotype, opt_elt.getByGenotype(), "genotype");
        apply_category(Dim::Drug, opt_elt.getByDrugType(), "drug type");

        if (opt_elt.getOutputNumber().present())
            om.out_id = opt_elt.getOutputNumber().get();
        if (om.out_id < 0) {
            throw util::xml_scenario_error("monitoring output number must not be negative");
        }
        if (!out_ids.insert(om.out_id).second) {
            throw util::xml_scenario_error("monitoring output number " + std::to_string(om.out_id) +
                                           " used more than once");
        }
        if (om.measure == AllCauseImr) {
            runtime.report_imr = om.out_id;
            continue;
        }
        reported_measures.push_back(om);
    }
    std::sort(reported_measures.begin(), reported_measures.end(),
              [](const OutMeasure& lhs, const OutMeasure& rhs) { return lhs.out_id < rhs.out_id; });
    const size_t n_species = scenario.getEntomology().getVector().present()
                                 ? scenario.getEntomology().getVector().get().getAnopheles().size()
                                 : 1;
    const size_t n_drugs =
        scenario.getPharmacology().present() ? scenario.getPharmacology().get().getDrugs().getDrug().size() : 1;
    internal::init_stores(reported_measures, n_species, n_drugs);
}

SimTime read_survey_dates(const scnXml::Monitoring& monitoring) {
    MonitoringState& runtime = bridge::monitoring();
    const scnXml::Surveys::SurveyTimeSequence& survs = monitoring.getSurveys().getSurveyTime();
    std::map<SimTime, bool> surveys;

    auto add_survey = [&surveys](SimTime date, bool reporting) {
        if (reporting)
            surveys[date] = true;
        else
            surveys.insert(std::make_pair(date, false));
    };

    for (size_t i = 0; i < survs.size(); ++i) {
        const scnXml::SurveyTime& surv = survs[i];
        try {
            std::string s = surv;
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
                    s.end());
            SimTime cur = UnitParse::readDate(s, UnitParse::STEPS);
            const bool reporting = surv.getReported();
            if (surv.getRepeatStep().present() != surv.getRepeatEnd().present()) {
                throw util::xml_scenario_error("surveyTime: use of repeatStep or repeatEnd without other");
            }
            if (surv.getRepeatStep().present()) {
                const SimTime step = UnitParse::readDuration(surv.getRepeatStep().get(), UnitParse::NONE);
                if (step < Sim::one_ts()) {
                    throw util::xml_scenario_error("surveyTime: repeatStep must be >= 1");
                }
                const SimTime end = UnitParse::readDate(surv.getRepeatEnd().get(), UnitParse::NONE);
                while (cur < end) {
                    add_survey(cur, reporting);
                    cur = cur + step;
                }
            } else {
                add_survey(cur, reporting);
            }
        } catch (const util::format_error& e) {
            throw util::xml_scenario_error(std::string("surveyTime: ").append(e.message()));
        }
    }

    runtime.survey_dates.clear();
    runtime.survey_dates.reserve(surveys.size());
    runtime.n_surveys = 0;
    for (const auto& [date, reporting] : surveys) {
        const size_t num = reporting ? runtime.n_surveys++ : NotUsed;
        runtime.survey_dates.push_back({date, num});
    }
    if (runtime.survey_dates.empty()) {
        throw util::xml_scenario_error("Scenario defines no surveys; at least one is required.");
    }
    if (!runtime.survey_dates.back().is_reported()) {
        std::cerr
            << "Warning: the last survey is unreported. Having surveys beyond the last reported survey is pointless."
            << std::endl;
    }

    if (util::CommandLine::option(util::CommandLine::PrintSurveyTimes)) {
        std::cout << "Survey\tsteps\tdate\n";
        for (const SurveyDate& survey_date : runtime.survey_dates) {
            if (!survey_date.is_reported())
                continue;
            std::cout << (survey_date.num + 1) << '\t' << Sim::in_steps(survey_date.date - Sim::start_date()) << '\t'
                      << survey_date.date << std::endl;
        }
    }

    runtime.n_cohorts = 1;
    if (monitoring.getCohorts().present()) {
        runtime.n_cohorts = static_cast<uint32_t>(1) << monitoring.getCohorts().get().getSubPop().size();
    }
    init_age_groups(monitoring);
    return runtime.survey_dates.back().date;
}

void init_cohorts(const scnXml::Monitoring& monitoring) {
    MonitoringState& runtime = bridge::monitoring();
    runtime.cohort_sub_pop_numbers.clear();
    runtime.cohort_sub_pop_ids.clear();
    if (!monitoring.getCohorts().present())
        return;

    const scnXml::Cohorts mon_cohorts = monitoring.getCohorts().get();
    uint32_t next_id = 0;
    for (auto it = mon_cohorts.getSubPop().begin(), end = mon_cohorts.getSubPop().end(); it != end; ++it) {
        const interventions::ComponentId comp_id = bridge::intervention_manager().get_component_id(it->getId());
        const bool inserted = runtime.cohort_sub_pop_ids.insert(std::make_pair(comp_id, next_id)).second;
        if (!inserted) {
            throw util::xml_scenario_error(std::string("cohort specification uses sub-population \"")
                                               .append(it->getId())
                                               .append("\" more than once"));
        }
        const auto number = it->getNumber();
        if (number < 1 || number > MaxCohortNumber || !std::has_single_bit(static_cast<uint32_t>(number))) {
            throw util::xml_scenario_error(std::string("cohort specification assigns sub-population \"")
                                               .append(it->getId())
                                               .append("\" a number which is not a power of 2 between 1 and ")
                                               .append(std::to_string(MaxCohortNumber)));
        }
        runtime.cohort_sub_pop_numbers.push_back(number);
        next_id += 1;
    }
}

} // namespace mon
} // namespace OM
