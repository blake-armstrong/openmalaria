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

#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>

#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>

namespace OM::logging { // NOLINT(readability-identifier-naming)

namespace {

spdlog::level::level_enum to_spdlog(Level level) { return static_cast<spdlog::level::level_enum>(level); }

Level from_spdlog(spdlog::level::level_enum level) { return static_cast<Level>(level); }

class ConsoleSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::FILE* stream = msg.level >= spdlog::level::warn ? stderr : stdout;
        std::fwrite(formatted.data(), 1, formatted.size(), stream);
        std::fflush(stream);
    }

    void flush_() override {
        std::fflush(stdout);
        std::fflush(stderr);
    }
};

class CallbackSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    void add(Callback callback) {
        const std::scoped_lock lock(mutex_);
        callbacks.push_back(std::move(callback));
    }

    void clear() {
        const std::scoped_lock lock(mutex_);
        callbacks.clear();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        const std::string message(msg.payload.data(), msg.payload.size());
        for (const Callback& callback : callbacks)
            callback(from_spdlog(msg.level), message);
    }

    void flush_() override {}

private:
    std::vector<Callback> callbacks;
};

struct State {
    std::shared_ptr<ConsoleSink> console = std::make_shared<ConsoleSink>();
    std::shared_ptr<CallbackSink> callbacks = std::make_shared<CallbackSink>();
    spdlog::logger logger{"openmalaria", {console, callbacks}};

    State() {
        logger.set_formatter(std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "\n"));
        logger.set_level(to_spdlog(CompiledLevel));
    }
};

State& state() {
    static State instance;
    return instance;
}

}

bool enabled(Level level) { return state().logger.should_log(to_spdlog(level)); }

void write(Level level, std::string_view message) { state().logger.log(to_spdlog(level), message); }

void set_level(Level level) { state().logger.set_level(to_spdlog(level)); }

void set_console(bool on) { state().console->set_level(on ? spdlog::level::trace : spdlog::level::off); }

void register_log_callback(Callback callback) { state().callbacks->add(std::move(callback)); }

void clear_log_callbacks() { state().callbacks->clear(); }

}
