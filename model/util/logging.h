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

#ifndef HMOD_UTIL_LOGGING
#define HMOD_UTIL_LOGGING

#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#ifndef SPDLOG_ACTIVE_LEVEL
#error "SPDLOG_ACTIVE_LEVEL is set by model/CMakeLists.txt"
#endif

namespace OM::logging { // NOLINT(readability-identifier-naming)

enum class Level : std::uint8_t { Trace, Debug, Info, Warn, Error };

inline constexpr Level CompiledLevel = static_cast<Level>(SPDLOG_ACTIVE_LEVEL);

using Callback = std::function<void(Level, const std::string&)>;

bool enabled(Level level);
void write(Level level, std::string_view message);

void set_level(Level level);
void set_console(bool on);
void register_log_callback(Callback callback);
void clear_log_callbacks();

template <Level L, typename... Args> void emit(std::format_string<Args...> format, Args&&... args) {
    if constexpr (CompiledLevel <= L) {
        if (enabled(L))
            write(L, std::format(format, std::forward<Args>(args)...));
    }
}

template <typename... Args> void trace(std::format_string<Args...> format, Args&&... args) {
    emit<Level::Trace>(format, std::forward<Args>(args)...);
}

template <typename... Args> void debug(std::format_string<Args...> format, Args&&... args) {
    emit<Level::Debug>(format, std::forward<Args>(args)...);
}

template <typename... Args> void info(std::format_string<Args...> format, Args&&... args) {
    emit<Level::Info>(format, std::forward<Args>(args)...);
}

template <typename... Args> void warn(std::format_string<Args...> format, Args&&... args) {
    emit<Level::Warn>(format, std::forward<Args>(args)...);
}

template <typename... Args> void error(std::format_string<Args...> format, Args&&... args) {
    emit<Level::Error>(format, std::forward<Args>(args)...);
}

}

#endif
