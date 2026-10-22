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

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <new>

namespace {

using OM::Clinical::ClinicalParams;
using OM::Clinical::Episode;

constexpr std::byte Poison{0xA5};

template <class Model> Episode::State latest_state_in_poisoned_storage(const ClinicalParams& params) {
    alignas(Model) std::array<std::byte, sizeof(Model)> storage{};
    storage.fill(Poison);
    auto* model = ::new (storage.data()) Model(params, 1.0);
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
    for (const Kind kind : {Kind::DecisionTree5Day, Kind::ImmediateOutcomes, Kind::EventScheduler}) {
        CAPTURE(static_cast<int>(kind));
        params.kind = kind;
        CHECK(params.create(1.0)->get_latest_state() == Episode::None);
    }
}

} // namespace
