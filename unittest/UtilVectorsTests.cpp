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

#include "util/vectors.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

constexpr double RelTolerance = 1e-7;
constexpr double AbsTolerance = 1e-7;

TEST_CASE("expIDFT inverts logDFT", "[util][vectors]") {
    const std::vector<double> input{10, 8, 6, 6, 4, 3};
    std::vector<double> freq_domain(input.size() * 2 - 1);
    OM::util::vectors::logDFT(input, freq_domain);
    std::vector<double> result(input.size());
    OM::util::vectors::expIDFT(result, freq_domain, 0);

    REQUIRE(result.size() == input.size());
    for (std::size_t i = 0; i < result.size(); ++i) {
        CAPTURE(i);
        CHECK_THAT(result[i], WithinRel(input[i], RelTolerance) || WithinAbs(input[i], AbsTolerance));
    }
}

} // namespace
