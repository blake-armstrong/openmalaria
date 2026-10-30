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

#include "util/errors.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace {

constexpr std::string_view Prefix = "Sorry, this feature hasn't been implemented by OpenMalaria yet: ";

}

TEST_CASE("an unimplemented_exception's message is the prefix and its own text", "[errors][F99]") {
    const OM::util::unimplemented_exception longer("a long description of a missing feature");
    const OM::util::unimplemented_exception shorter("short");
    CHECK(std::string(longer.message()) == std::string(Prefix) + "a long description of a missing feature");
    CHECK(std::string(shorter.message()) == std::string(Prefix) + "short");
}

TEST_CASE("each unimplemented_exception keeps its message after another's is read", "[errors][F93]") {
    const OM::util::unimplemented_exception first("first");
    const OM::util::unimplemented_exception second("second");
    const char* first_message = first.message();
    CHECK(std::string(second.message()) == std::string(Prefix) + "second");
    CHECK(std::string(first_message) == std::string(Prefix) + "first");
}
