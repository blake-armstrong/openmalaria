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

#ifndef H_OM_MON_MONITORING
#define H_OM_MON_MONITORING

#include "Global.h"
#include "mon/OutMeasures.h"
#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace OM {
namespace Clinical { // NOLINT(readability-identifier-naming)
class InfantMortality;
}
namespace Host {
class Human;
}
namespace interventions {
struct ComponentId;
}
namespace util {
class CommandLineOptions;
}
namespace mon {

inline constexpr size_t NotUsed = std::numeric_limits<size_t>::max();

enum class DeployMethod : std::uint8_t { Timed, Cts, Treat };

namespace internal {

struct Condition {
    bool value;
    Measure measure;
    double min, max;
};

struct SurveyDate {
    SimTime date = Sim::never();
    size_t num = NotUsed;

    bool is_reported() const { return num != NotUsed; }
};

struct MeasureStore {
    OutMeasure output;
    size_t n_ages = 1, n_cohorts = 1, n_species = 1, n_genotypes = 1, n_drugs = 1;
    std::variant<std::vector<int>, std::vector<double>> reports;

    size_t size() const { return n_ages * n_cohorts * n_species * n_genotypes * n_drugs; }
    size_t index(size_t a, size_t c, size_t sp, size_t g, size_t d) const;
};

} // namespace internal

struct MonitoringState {
    bool is_init = false;
    size_t survey_index = 0;
    size_t surv_num_event = NotUsed;
    size_t n_surveys = 0;
    size_t n_cohorts = 1;
    int report_imr = -1;
    std::vector<internal::Condition> conditions;
    std::vector<internal::SurveyDate> survey_dates;
    std::vector<SimTime> age_group_upper_bound;
    std::vector<uint32_t> cohort_sub_pop_numbers;
    std::map<interventions::ComponentId, uint32_t> cohort_sub_pop_ids;
    std::vector<internal::MeasureStore> stores;
    std::vector<std::vector<size_t>> stores_by_measure;
};

namespace internal {
void init_stores(const std::vector<OutMeasure>& enabled_measures, size_t n_species, size_t n_drugs);
} // namespace internal

} // namespace mon

namespace bridge {

inline mon::MonitoringState* current_monitoring = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline mon::MonitoringState& monitoring() {
    assert(current_monitoring != nullptr);
    return *current_monitoring;
}

}

class MonitoringBinding {
public:
    explicit MonitoringBinding(mon::MonitoringState& state) : previous(bridge::current_monitoring) {
        bridge::current_monitoring = &state;
    }
    MonitoringBinding(const MonitoringBinding&) = delete;
    MonitoringBinding& operator=(const MonitoringBinding&) = delete;
    MonitoringBinding(MonitoringBinding&&) = delete;
    MonitoringBinding& operator=(MonitoringBinding&&) = delete;
    ~MonitoringBinding() { bridge::current_monitoring = previous; }

private:
    mon::MonitoringState* previous;
};

namespace mon {

// ----- info API -----

inline size_t event_survey_number() { return bridge::monitoring().surv_num_event; }
inline size_t stat_survey_number() {
    const auto& r = bridge::monitoring();
    return r.is_init && r.survey_index < r.survey_dates.size() ? r.survey_dates[r.survey_index].num : NotUsed;
}
inline bool is_reported() { return !bridge::monitoring().is_init || stat_survey_number() != NotUsed; }
inline SimTime next_survey_date() {
    const auto& r = bridge::monitoring();
    return r.is_init && r.survey_index < r.survey_dates.size() ? r.survey_dates[r.survey_index].date : Sim::future();
}
size_t setup_condition(const std::string& measure_name, double min_value, double max_value, bool initial_state);
bool check_condition(size_t condition_key);
uint32_t update_cohort_set(uint32_t old, interventions::ComponentId sub_pop, bool is_member);

// ----- management API -----

void update_age_group(size_t& index, SimTime age);
void init_main_sim();
void conclude_survey();
void write_survey_data(const util::CommandLineOptions& command_line, const Clinical::InfantMortality& infant_mortality);

template <typename Stream> void checkpoint(Stream& stream);
extern template void checkpoint<std::ostream>(std::ostream& stream);
extern template void checkpoint<std::istream>(std::istream& stream);

// ----- direct recording API -----

void record_stat(Measure measure, double val, size_t species = 0, size_t genotype = 0);
void record_stat(Measure measure, const Host::Human& human, double val = 1.0, size_t species = 0, size_t genotype = 0,
                 size_t drug = 0);
void record_event(Measure measure, size_t survey, size_t age, uint32_t cohort);
void record_event(Measure measure, const Host::Human& human, double val = 1.0, int out_id = 0);
void record_deploy(Measure timed_measure, Measure cts_measure, const Host::Human& human, DeployMethod method,
                   double val = 1.0);
inline bool is_used(Measure measure) {
    assert(measure < MeasureCount);
    return !bridge::monitoring().stores_by_measure[measure].empty();
}

} // namespace mon
} // namespace OM

#endif
