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

#ifndef HMOD_NEONATAL_MORTALITY
#define HMOD_NEONATAL_MORTALITY

#include "Global.h"
#include "util/random.h"
#include "Host/Human.h"

#include <vector>

namespace scnXml {
class Clinical;
}
namespace OM {
class Population;
namespace util {
class CommandLineOptions;
}
namespace WithinHost {
class Diagnostic;
class DiagnosticRegistry;
}
namespace Host {

using util::LocalRng;

class NeonatalMortality {
public:
    NeonatalMortality() = default;
    NeonatalMortality(const scnXml::Clinical& clinical, const WithinHost::DiagnosticRegistry& diagnostics,
                      const util::CommandLineOptions& command_line);

    void checkpoint(istream& stream);
    void checkpoint(ostream& stream);

    /** Called for each birth; returns true if infant dies due to mother's
   * infection. */
    bool event_neonatal_mortality(LocalRng& rng) const;

    /** Calculate risk of a neonatal mortality based on humans 20-25 years old. */
    void update(vector<Human>& population);

private:
    /// Probability for a newborn to die (indirect death) because the mother is infected.
    /// Depends on the prevalence of parasitaemia in mother at some previous t.
    double risk_from_maternal_infection = 0.0;
    /// Array of stored prevalences of mothers over last 5 months
    std::vector<double> prev_by_gestational_age;

    // The model is parameterised based on patency levels; the diagnostic
    // used for this may be important.
    const WithinHost::Diagnostic* neonatal_diagnostic = nullptr;
};

}
}
#endif
