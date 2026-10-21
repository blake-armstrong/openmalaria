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

#ifndef HS_DIAGNOSTIC
#define HS_DIAGNOSTIC

#include "Global.h"
#include "util/random.h"

#include <cassert>
#include <map>
#include <string>

namespace scnXml {
class Diagnostics;
class Diagnostic;
class Scenario;
}

namespace OM {
class Parameters;
namespace util {
class CommandLineOptions;
class ModelOptionFlags;
}
namespace WithinHost {

using util::LocalRng;

class Diagnostic {
public:
    /** Use the test.
     * 
     * @param dens Current parasite density in parasites per µL
     * @param densHRP2 Equivalent density for diagnostics dependent on HRP2
     * @returns True if outcome is positive. */
    bool is_positive(LocalRng& rng, double dens, double dens_hrp2) const;

    inline bool operator!=(const Diagnostic& that) const {
        return specificity != that.specificity || dens_lim != that.dens_lim;
    }

    /// True if false positives are possible
    bool allows_false_positives() const;

private:
    /** Construct from XML parameters. */
    Diagnostic(const Parameters& parameters, const util::ModelOptionFlags& options, const scnXml::Diagnostic& elt);
    /** Construct as deterministic. */
    explicit Diagnostic(double min_dens);

    // switch: either not-a-number indicating a deterministic test, or specificity
    double specificity;
    // depending on model, this is either the minimum detectible density
    // or the density at which test has half a chance of a positive outcome
    double dens_lim;

    // if true, the diagnostic is dependent on the HRP2 antigen
    bool uses_hrp2;

    friend class DiagnosticRegistry;
};

/** Library of parameterised Diagnostic objects. */
class DiagnosticRegistry {
public:
    DiagnosticRegistry() = default;
    /** Initialise from input data. */
    DiagnosticRegistry(const Parameters& parameters, const util::ModelOptionFlags& options,
                       const util::CommandLineOptions& command_line, const scnXml::Scenario& scenario);
    DiagnosticRegistry(const DiagnosticRegistry&) = delete;
    DiagnosticRegistry& operator=(const DiagnosticRegistry&) = delete;
    DiagnosticRegistry(DiagnosticRegistry&&) = default;
    DiagnosticRegistry& operator=(DiagnosticRegistry&&) = default;
    ~DiagnosticRegistry() = default;

    /** Look up a diagnostic by name and get a reference to it.
     * 
     * @throws util::xml_scenario_error on failure
     */
    const Diagnostic& get(const std::string& name) const;

    const Diagnostic& monitoring_diagnostic() const {
        assert(monitoring != nullptr);
        return *monitoring;
    }

private:
    /** Make a new diagnostic with deterministic density and return a reference. */
    const Diagnostic& make_deterministic(double min_dens);

    std::map<std::string, Diagnostic> diagnostic_set;
    const Diagnostic* monitoring = nullptr;
};

}

namespace bridge {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline const WithinHost::DiagnosticRegistry* current_diagnostics = nullptr;

inline const WithinHost::DiagnosticRegistry& diagnostics() {
    assert(current_diagnostics != nullptr);
    return *current_diagnostics;
}

}

class DiagnosticsBinding {
public:
    explicit DiagnosticsBinding(const WithinHost::DiagnosticRegistry& diagnostics)
        : previous(bridge::current_diagnostics) {
        bridge::current_diagnostics = &diagnostics;
    }
    DiagnosticsBinding(const DiagnosticsBinding&) = delete;
    DiagnosticsBinding& operator=(const DiagnosticsBinding&) = delete;
    DiagnosticsBinding(DiagnosticsBinding&&) = delete;
    DiagnosticsBinding& operator=(DiagnosticsBinding&&) = delete;
    ~DiagnosticsBinding() { bridge::current_diagnostics = previous; }

private:
    const WithinHost::DiagnosticRegistry* previous;
};

namespace WithinHost {

/** Static members to do with Diagnostic: library of parameterised Diagnostic
 * objects. */
class Diagnostics {
public:
    /** Look up a diagnostic by name and get a reference to it.
     * 
     * @throws util::xml_scenario_error on failure
     */
    static const Diagnostic& get(const std::string& name) { return bridge::diagnostics().get(name); }

    /// Static access functions

    static inline const Diagnostic& monitoring_diagnostic() { return bridge::diagnostics().monitoring_diagnostic(); }
};

}
}
#endif
