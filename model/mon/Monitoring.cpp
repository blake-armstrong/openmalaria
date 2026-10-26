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

#include "mon/Monitoring.h"
#include "Host/WithinHost/Genotypes.h"
#include "Clinical/ClinicalModel.h"
#include "Host/Human.h"
#include "util/CommandLine.h"
#include "util/errors.h"
#include "schema/monitoring.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <gzstream/gzstream.h>
#include <iostream>
#include <numeric>
#include <type_traits>

namespace OM::mon {

using internal::Condition;

namespace {

uint32_t cohort_set_output_id(uint32_t cohort_set) {
    MonitoringState& runtime = bridge::monitoring();
    uint32_t out_num = 0;
    assert((cohort_set >> runtime.cohort_sub_pop_numbers.size()) == 0);
    for (uint32_t i = 0; i < runtime.cohort_sub_pop_numbers.size(); ++i) {
        if (cohort_set & (static_cast<uint32_t>(1) << i)) {
            out_num += runtime.cohort_sub_pop_numbers[i];
        }
    }
    return out_num;
}

template <typename T> void write_binary_value(ostream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

} // namespace

namespace internal {

size_t MeasureStore::index(size_t a, size_t c, size_t sp, size_t g, size_t d) const {
    assert(n_ages == 1 || a < n_ages);
    assert(n_cohorts == 1 || c < n_cohorts);
    assert(n_species == 1 || sp < n_species);
    assert(n_genotypes == 1 || g < n_genotypes);
    assert(n_drugs == 1 || d < n_drugs);
    return (d % n_drugs) +
           n_drugs * ((g % n_genotypes) +
                      n_genotypes * ((sp % n_species) + n_species * ((c % n_cohorts) + n_cohorts * (a % n_ages))));
}

namespace {

void add_store(const OutMeasure& om, size_t n_species, size_t n_drugs, bool force_no_categories) {
    MonitoringState& runtime = bridge::monitoring();
    assert(om.measure < MeasureCount);
    MeasureStore store;
    store.output = om;
    if (force_no_categories)
        store.output.out_id = -1;
    if (!force_no_categories && (om.dims & Dim::Age)) {
        assert(!runtime.age_group_upper_bound.empty());
    }
    store.n_ages = force_no_categories ? 1 : (om.dims & Dim::Age ? runtime.age_group_upper_bound.size() : 1);
    store.n_cohorts = force_no_categories ? 1 : (om.dims & Dim::Cohort ? runtime.n_cohorts : 1);
    store.n_species = force_no_categories ? 1 : (om.dims & Dim::Species ? n_species : 1);
    store.n_genotypes = force_no_categories ? 1 : (om.dims & Dim::Genotype ? WithinHost::Genotypes::count() : 1);
    store.n_drugs = force_no_categories ? 1 : (om.dims & Dim::Drug ? n_drugs : 1);
    const size_t report_count = store.size() * runtime.n_surveys;
    if (om.is_double)
        store.reports.emplace<vector<double>>(report_count, 0.0);
    else
        store.reports.emplace<vector<int>>(report_count, 0);
    runtime.stores_by_measure[om.measure].push_back(runtime.stores.size());
    runtime.stores.push_back(std::move(store));
}

} // namespace

void init_stores(const vector<OutMeasure>& enabled_measures, size_t n_species, size_t n_drugs) {
    MonitoringState& runtime = bridge::monitoring();
    runtime.stores.clear();
    runtime.stores_by_measure.assign(MeasureCount, {});
    for (const OutMeasure& om : enabled_measures)
        add_store(om, n_species, n_drugs, false);
}

void ensure_condition_store(const OutMeasure& om) {
    MonitoringState& runtime = bridge::monitoring();
    assert(om.measure < MeasureCount);
    if (runtime.stores_by_measure[om.measure].empty())
        add_store(om, 1, 1, true);
}

double survey_sum(Measure measure, size_t survey) {
    MonitoringState& runtime = bridge::monitoring();
    assert(measure < runtime.stores_by_measure.size());
    if (runtime.stores_by_measure[measure].empty())
        throw SWITCH_DEFAULT_EXCEPTION;
    const MeasureStore& store = runtime.stores[runtime.stores_by_measure[measure].front()];
    const size_t begin = survey * store.size();
    return std::visit(
        [&](const auto& reports) {
            return std::accumulate(reports.begin() + begin, reports.begin() + begin + store.size(), 0.0);
        },
        store.reports);
}

} // namespace internal

namespace {

void update_conditions() {
    MonitoringState& runtime = bridge::monitoring();
    const size_t survey = stat_survey_number();
    if (survey == NotUsed)
        return;
    for (Condition& cond : runtime.conditions) {
        const double val = internal::survey_sum(cond.measure, survey);
        cond.value = (val >= cond.min && val <= cond.max);
    }
}

void write_text_row(ostream& stream, int survey, int column, int measure, double value, bool is_double) {
    stream << survey << '\t' << column << '\t' << measure << '\t';
    if (is_double) {
        stream << value;
    } else {
        assert(std::trunc(value) == value);
        stream << static_cast<long long>(value);
    }
    stream << '\n';
}

size_t output_age_groups(const internal::MeasureStore& store) { return store.n_ages == 1 ? 1 : store.n_ages - 1; }

template <class Writer>
void write_measure(ostream& stream, Writer write_row, size_t survey, const internal::MeasureStore& store) {
    const OutMeasure& om = store.output;
    const size_t survey_start = survey * store.size();
    const size_t report_count = std::visit([](const auto& reports) { return reports.size(); }, store.reports);
    assert(report_count >= survey_start + store.size());
    const bool by_species = om.dims & Dim::Species;
    const bool by_drug = om.dims & Dim::Drug;
    const int age_group_add = om.dims & Dim::Age ? 1 : 0;

    if (by_species)
        assert(store.n_ages == 1 && store.n_cohorts == 1 && store.n_drugs == 1);
    if (by_drug)
        assert(store.n_species == 1 && store.n_genotypes == 1);
    for (size_t cohort_set = 0; cohort_set < store.n_cohorts; ++cohort_set) {
        for (size_t age_group = 0; age_group < output_age_groups(store); ++age_group) {
            for (size_t species = 0; species < store.n_species; ++species) {
                for (size_t genotype = 0; genotype < store.n_genotypes; ++genotype) {
                    for (size_t drug = 0; drug < store.n_drugs; ++drug) {
                        const int col2 = static_cast<int>(
                            by_species ? species + 1 + 1000000 * genotype
                                       : age_group + age_group_add +
                                             static_cast<size_t>(1000 * cohort_set_output_id(cohort_set)) +
                                             1000000 * (by_drug ? drug + 1 : genotype));
                        const size_t index = survey_start + store.index(age_group, cohort_set, species, genotype, drug);
                        const double value =
                            std::visit([index](const auto& reports) { return static_cast<double>(reports[index]); },
                                       store.reports);
                        write_row(stream, static_cast<int>(survey + 1), col2, om.out_id, value, om.is_double);
                    }
                }
            }
        }
    }
}

enum class ValueType : std::uint8_t { All, Integer, Double };

template <class Writer>
void write_rows(ostream& stream, const Clinical::InfantMortality& infant_mortality, Writer write_row,
                ValueType type = ValueType::All) {
    const MonitoringState& runtime = bridge::monitoring();
    for (size_t survey = 0; survey < runtime.n_surveys; ++survey) {
        for (const internal::MeasureStore& store : runtime.stores) {
            if (store.output.out_id < 0 || (type == ValueType::Integer && store.output.is_double) ||
                (type == ValueType::Double && !store.output.is_double))
                continue;
            write_measure(stream, write_row, survey, store);
        }
    }
    if (runtime.report_imr >= 0 && type != ValueType::Integer) {
        write_row(stream, 1, 1, runtime.report_imr, infant_mortality.all_cause(), true);
    }
}

uint64_t count_rows(bool is_double) {
    const MonitoringState& runtime = bridge::monitoring();
    uint64_t count = is_double && runtime.report_imr >= 0 ? 1 : 0;
    for (const internal::MeasureStore& store : runtime.stores) {
        if (store.output.out_id < 0 || store.output.is_double != is_double)
            continue;
        count += static_cast<uint64_t>(runtime.n_surveys) * output_age_groups(store) * store.n_cohorts *
                 store.n_species * store.n_genotypes * store.n_drugs;
    }
    return count;
}

void write_binary(ostream& stream, const Clinical::InfantMortality& infant_mortality) {
    static_assert(std::endian::native == std::endian::little, "output.bin uses little-endian values");
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559, "output.bin needs IEEE-754 doubles");
    static_assert(std::numeric_limits<int>::digits <= std::numeric_limits<int32_t>::digits,
                  "integer monitoring values must fit in int32_t");
    constexpr std::array<char, 8> Magic = {'O', 'M', 'O', 'U', 'T', 'B', 'I', 'N'};
    const uint32_t version = 2;
    const uint64_t integer_rows = count_rows(false);
    const uint64_t double_rows = count_rows(true);
    stream.write(Magic.data(), Magic.size());
    write_binary_value(stream, version);
    write_binary_value(stream, integer_rows);
    write_binary_value(stream, double_rows);

    auto write_id_column = [&](auto select) {
        for (const ValueType type : {ValueType::Integer, ValueType::Double}) {
            write_rows(
                stream, infant_mortality,
                [&](ostream& output, int survey, int column, int measure, double, bool) {
                    write_binary_value<int32_t>(output, select(survey, column, measure));
                },
                type);
        }
    };
    write_id_column([](int survey, int, int) { return survey; });
    write_id_column([](int, int column, int) { return column; });
    write_id_column([](int, int, int measure) { return measure; });
    write_rows(
        stream, infant_mortality,
        [](ostream& output, int, int, int, double value, bool) {
            assert(std::trunc(value) == value);
            write_binary_value<int32_t>(output, static_cast<int32_t>(value));
        },
        ValueType::Integer);
    write_rows(
        stream, infant_mortality,
        [](ostream& output, int, int, int, double value, bool) { write_binary_value<double>(output, value); },
        ValueType::Double);
}

void write_text(ostream& stream, const Clinical::InfantMortality& infant_mortality) {
    write_rows(stream, infant_mortality, write_text_row);
}

template <class Stream, class Writer> void write_output(const string& filename, ios::openmode mode, Writer writer) {
    Stream stream(filename.c_str(), mode);
    if (!stream.rdbuf()->is_open()) {
        throw util::base_exception("Unable to open monitoring output file \"" + filename + "\"", util::Error::FileIO);
    }
    writer(stream);
    stream.close();
    if (!stream) {
        throw util::base_exception("Unable to write monitoring output file \"" + filename + "\"", util::Error::FileIO);
    }
}

void update_survey_numbers() {
    MonitoringState& runtime = bridge::monitoring();
    runtime.surv_num_event = NotUsed;
    for (size_t i = runtime.survey_index; i < runtime.survey_dates.size(); ++i) {
        runtime.surv_num_event = runtime.survey_dates[i].num;
        if (runtime.surv_num_event != NotUsed)
            break;
    }
}

} // namespace
size_t setup_condition(const string& measure_name, double min_value, double max_value, bool initial_state) {
    MonitoringState& runtime = bridge::monitoring();
    const OutMeasure om = find_out_measure(measure_name);
    if (om.measure == InvalidMeasure) {
        throw util::xml_scenario_error("unrecognised measure: " + measure_name);
    }

    if (!is_valid_condition(om.measure)) {
        throw util::xml_scenario_error("cannot use measure " + measure_name + " as condition of deployment");
    }
    internal::ensure_condition_store(om);

    runtime.conditions.push_back({.value = initial_state, .measure = om.measure, .min = min_value, .max = max_value});
    return runtime.conditions.size() - 1;
}

bool check_condition(size_t condition_key) {
    MonitoringState& runtime = bridge::monitoring();
    assert(condition_key < runtime.conditions.size());
    return runtime.conditions[condition_key].value;
}

static void record_value(Measure measure, size_t survey, size_t age, uint32_t cohort, size_t species, size_t genotype,
                         size_t drug, double val, int out_id = 0) {
    if (survey == NotUsed)
        return;
    MonitoringState& runtime = bridge::monitoring();
    assert(measure < runtime.stores_by_measure.size());
    for (const size_t idx : runtime.stores_by_measure[measure]) {
        internal::MeasureStore& store = runtime.stores[idx];
        if (out_id != 0 && store.output.out_id != out_id)
            continue;
        const size_t index = survey * store.size() + store.index(age, cohort, species, genotype, drug);
        std::visit(
            [index, val](auto& reports) {
                using Value = typename std::decay_t<decltype(reports)>::value_type;
                assert(index < reports.size());
                if constexpr (std::is_integral_v<Value>)
                    assert(std::trunc(val) == val);
                reports[index] += static_cast<Value>(val);
            },
            store.reports);
    }
}

void record_stat(Measure measure, double val, size_t species, size_t genotype) {
    record_value(measure, stat_survey_number(), 0, 0, species, genotype, 0, val);
}

void record_stat(Measure measure, const Host::Human& human, double val, size_t species, size_t genotype, size_t drug) {
    record_value(measure, stat_survey_number(), human.monitoring_age_group, human.get_cohort_set(), species, genotype,
                 drug, val);
}

void record_event(Measure measure, size_t survey, size_t age, uint32_t cohort) {
    record_value(measure, survey, age, cohort, 0, 0, 0, 1.0);
}

void record_event(Measure measure, const Host::Human& human, double val, int out_id) {
    record_value(measure, event_survey_number(), human.monitoring_age_group, human.get_cohort_set(), 0, 0, 0, val,
                 out_id);
}

void record_deploy(Measure timed_measure, Measure cts_measure, const Host::Human& human, DeployMethod method,
                   double val) {
    assert(method == DeployMethod::Timed || method == DeployMethod::Cts || method == DeployMethod::Treat);
    const Measure selected = method == DeployMethod::Timed ? timed_measure
                             : method == DeployMethod::Cts ? cts_measure
                                                           : ::OM::mon::measure("nTreatDeployments");
    record_event(selected, human, val);
}

template <typename Stream> void checkpoint(Stream& stream) {
    MonitoringState& runtime = bridge::monitoring();
    runtime.is_init & stream;
    runtime.survey_index & stream;
    runtime.surv_num_event & stream;
    for (internal::MeasureStore& store : runtime.stores) {
        std::visit(
            [&stream](auto& reports) {
                const size_t expected_size = reports.size();
                size_t stored_size = expected_size;
                stored_size & stream;
                if (stored_size != expected_size) {
                    throw util::checkpoint_error("Monitoring report size mismatch: checkpoint has " +
                                                 std::to_string(stored_size) + ", expected " +
                                                 std::to_string(expected_size));
                }
                for (auto& report : reports)
                    report & stream;
            },
            store.reports);
    }
}
template void checkpoint<ostream>(ostream& stream);
template void checkpoint<istream>(istream& stream);
// ———  surveys  ———

void init_main_sim() {
    MonitoringState& runtime = bridge::monitoring();
    runtime.survey_index = 0;
    runtime.is_init = true;
    update_survey_numbers();
}

void conclude_survey() {
    MonitoringState& runtime = bridge::monitoring();
    update_conditions();
    runtime.survey_index += 1;
    update_survey_numbers();
}

void write_survey_data(const util::CommandLineOptions& command_line,
                       const Clinical::InfantMortality& infant_mortality) {
    const string& filename = command_line.output_name();
    const bool binary = command_line.output_format() == util::CommandLine::OutputFormat::Bin;
    const auto write = binary ? write_binary : write_text;
    const auto writer = [&](ostream& stream) { write(stream, infant_mortality); };
    const ios::openmode mode = ios::out | ios::binary;

    if (command_line.option(util::CommandLine::CompressOutput)) {
        write_output<ogzstream>(filename, mode, writer);
    } else {
        write_output<ofstream>(filename, mode, writer);
    }
}

void update_age_group(size_t& index, SimTime age) {
    MonitoringState& runtime = bridge::monitoring();
    while (age >= runtime.age_group_upper_bound[index]) {
        ++index;
    }
}

uint32_t update_cohort_set(uint32_t old, interventions::ComponentId sub_pop, bool is_member) {
    MonitoringState& runtime = bridge::monitoring();
    auto it = runtime.cohort_sub_pop_ids.find(sub_pop);
    if (it == runtime.cohort_sub_pop_ids.end())
        return old;
    const uint32_t sub_pop_id = static_cast<uint32_t>(1) << it->second;
    return (old & ~sub_pop_id) | (is_member ? sub_pop_id : 0);
}

} // namespace OM::mon
