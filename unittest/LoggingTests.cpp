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

#include "util/logging.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

namespace {

using OM::logging::Level;

struct Captured {
    std::vector<std::pair<Level, std::string>> messages;

    Captured() {
        OM::logging::set_console(false);
        OM::logging::register_log_callback(
            [this](Level level, const std::string& message) { messages.emplace_back(level, message); });
    }
    Captured(const Captured&) = delete;
    Captured& operator=(const Captured&) = delete;
    Captured(Captured&&) = delete;
    Captured& operator=(Captured&&) = delete;
    ~Captured() {
        OM::logging::clear_log_callbacks();
        OM::logging::set_level(OM::logging::CompiledLevel);
        OM::logging::set_console(true);
    }
};

TEST_CASE("callbacks receive the level and the formatted message without a newline", "[util][logging]") {
    const Captured captured;
    OM::logging::info("Annual EIR for {}: {}", "gambiae", 12.5);
    OM::logging::warn("Warning: {}", "careful");
    OM::logging::error("plain text");
    const std::vector<std::pair<Level, std::string>> expected{
        {Level::Info, "Annual EIR for gambiae: 12.5"}, {Level::Warn, "Warning: careful"}, {Level::Error, "plain text"}};
    CHECK(captured.messages == expected);
}

TEST_CASE("set_level drops messages below it", "[util][logging]") {
    const Captured captured;
    OM::logging::set_level(Level::Warn);
    OM::logging::info("dropped");
    OM::logging::warn("kept");
    REQUIRE(captured.messages.size() == 1);
    CHECK(captured.messages[0].second == "kept");
}

TEST_CASE("debug messages reach the sinks only when compiled in", "[util][logging]") {
    const Captured captured;
    OM::logging::set_level(Level::Trace);
    OM::logging::debug("debug {}", 1);
    OM::logging::trace("trace {}", 2);
    CHECK(captured.messages.size() == static_cast<size_t>(OM::logging::CompiledLevel <= Level::Debug) +
                                          static_cast<size_t>(OM::logging::CompiledLevel <= Level::Trace));
}

TEST_CASE("clear_log_callbacks stops delivery", "[util][logging]") {
    std::vector<std::string> seen;
    OM::logging::set_console(false);
    OM::logging::register_log_callback([&seen](Level, const std::string& message) { seen.push_back(message); });
    OM::logging::info("first");
    OM::logging::clear_log_callbacks();
    OM::logging::info("second");
    OM::logging::set_console(true);
    CHECK(seen == std::vector<std::string>{"first"});
}

}
