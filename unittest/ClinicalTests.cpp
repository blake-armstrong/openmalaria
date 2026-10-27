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
#include "Clinical/ClinicalParams.h"
#include "Clinical/DecisionTree5Day.h"
#include "Clinical/EventScheduler.h"
#include "Clinical/ImmediateOutcomes.h"
#include "schema/healthSystem.h"
#include "util/AgeGroupInterpolation.h"
#include "util/CommandLine.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>
#include <vector>

namespace {

using OM::Clinical::ClinicalParams;
using OM::Clinical::Episode;

constexpr std::byte Poison{0xA5};

template <class Model> Episode::State latest_state_in_poisoned_storage(const ClinicalParams& params) {
    OM::mon::MonitoringState monitoring;
    alignas(Model) std::array<std::byte, sizeof(Model)> storage{};
    storage.fill(Poison);
    auto* model = ::new (storage.data()) Model(params, 1.0, monitoring);
    const Episode::State state = model->get_latest_state();
    std::destroy_at(model);
    return state;
}

TEST_CASE("a new clinical model's latest state is None, whatever its memory held", "[clinical][F60]") {
    const ClinicalParams params;
    CHECK(latest_state_in_poisoned_storage<OM::Clinical::DecisionTree5Day>(params) == Episode::None);
    CHECK(latest_state_in_poisoned_storage<OM::Clinical::ImmediateOutcomes>(params) == Episode::None);
    CHECK(latest_state_in_poisoned_storage<OM::Clinical::ClinicalEventScheduler>(params) == Episode::None);
}

TEST_CASE("each kind of clinical model from ClinicalParams::create starts in state None", "[clinical][F60]") {
    using Kind = ClinicalParams::Kind;
    ClinicalParams params;
    OM::mon::MonitoringState monitoring;
    for (const Kind kind : {Kind::DecisionTree5Day, Kind::ImmediateOutcomes, Kind::EventScheduler}) {
        CAPTURE(static_cast<int>(kind));
        params.kind = kind;
        CHECK(params.create(1.0, monitoring)->get_latest_state() == Episode::None);
    }
}

scnXml::HSEventScheduler event_scheduler(std::initializer_list<double> daily_pr_imm_ucts) {
    scnXml::ClinicalOutcomes outcomes(3, 3, 6, 6);
    for (const double p : daily_pr_imm_ucts)
        outcomes.getDailyPrImmUCTS().push_back(p);
    return {scnXml::DecisionTree(), scnXml::DecisionTree(), outcomes};
}

OM::util::AgeGroupInterpolator case_fatality_rate() {
    scnXml::AgeGroupValues values;
    values.setInterpolation("none");
    values.getGroup().push_back(scnXml::Group(0.1, 0.0));
    OM::util::AgeGroupInterpolator cfr;
    cfr.set(values, "CFR");
    return cfr;
}

TEST_CASE("each health system replaces the event scheduler's dailyPrImmUCTS", "[clinical][F59]") {
    const OM::util::CommandLineOptions command_line;
    const OM::CommandLineBinding command_line_binding{command_line};
    OM::util::AgeGroupInterpolator cfr = case_fatality_rate();
    OM::Clinical::EventSchedulerParams params;

    params.set_parameters(event_scheduler({1.0}), cfr);
    CHECK(params.cum_daily_pr_imm_ucts == std::vector<double>{1.0});

    params.set_parameters(event_scheduler({0.2, 0.8}), cfr);
    CHECK(params.cum_daily_pr_imm_ucts == std::vector<double>{0.2, 1.0});

    params.set_parameters(event_scheduler({1.0}), cfr);
    CHECK(params.cum_daily_pr_imm_ucts == std::vector<double>{1.0});
}

} // namespace
