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

#include "Host/NeonatalMortality.h"
#include "Population.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Diagnostic.h"
#include "util/random.h"
#include "util/CommandLine.h"
#include "schema/healthSystem.h"

#include <cmath>

namespace OM::Host {
using namespace OM::util;

//Goodman estimated for neonatal mortality due to malaria in pregnancy
const double GEst = 0.011;
//Critical value of Prev20-25 for neonatal mortality
const double CritPrev2025 = 0.25;
//Critical value for estimating prevalence in primigravidae
const double CritPrevPrim = 0.19;
//Proportion of births with primigravid mothers
const double PBirthPrim = 0.3;

// optimised constants:
const double Y = PBirthPrim * GEst;
const double Z = -1.0 / CritPrev2025;

NeonatalMortality::NeonatalMortality(const scnXml::Clinical& clinical,
                                     const WithinHost::DiagnosticRegistry& diagnostics,
                                     const util::CommandLineOptions& command_line) {
    const SimTime five_months = Sim::from_days(5 * 30);
    prev_by_gestational_age.assign(Sim::in_steps(five_months), 0.0);

    if (clinical.getNeonatalMortality().present()) {
        neonatal_diagnostic = &diagnostics.get(clinical.getNeonatalMortality().get().getDiagnostic());
    } else {
        //NOTE: this is a compatibility option for older scenarios
        if (command_line.option(CommandLine::DeprecationWarnings)) {
            cerr << "Deprecation warning: specification of the diagnostic "
                    "used by the Neonatal Mortality model is recommended "
                    "(model/clinical/neonatalMortality)"
                 << endl; // NOLINT(performance-avoid-endl)
        }
        neonatal_diagnostic = &diagnostics.monitoring_diagnostic();
    }
}

void NeonatalMortality::checkpoint(istream& stream) {
    risk_from_maternal_infection & stream;
    prev_by_gestational_age & stream;
}
void NeonatalMortality::checkpoint(ostream& stream) {
    risk_from_maternal_infection & stream;
    prev_by_gestational_age & stream;
}

bool NeonatalMortality::event_neonatal_mortality(LocalRng& rng) const {
    return rng.uniform_01() <= risk_from_maternal_infection;
}

void NeonatalMortality::update(vector<Host::Human>& population) {
    // ———  find potential mothers and their prevalence  ———
    // For individuals in the age range 20-25, we sum:
    int n_counter = 0; // total number
    int p_counter = 0; // number with patent infections, needed for prev in 20-25y
    // Lower and upper bounds for potential mothers (as in model description)
    const SimTime age_lb = Sim::from_years_i(20);
    const SimTime age_ub = Sim::from_years_i(25);

    for (Human& human : population) {
        // diagnosticDefault() gives patency after the last time step's
        // update, so it's appropriate to use age at the beginning of this step.
        const SimTime age = human.age(Sim::ts0());

        // Note: since we're using a linked list, we have to iterate until we reach
        // the individuals we're interested in. Due to population structure, it's
        // probably quickest to start iterating from the oldest.
        if (age >= age_ub)
            continue;
        if (age < age_lb)
            break; // Not interested in younger individuals.

        n_counter++;
        if (human.withinHostModel->diagnostic_result(human.clinical_rng, *neonatal_diagnostic)) {
            p_counter++;
        }
    }

    // ———  calculate risk of neonatal mortality  ———
    //default value for prev2025, for use when there are no 20-25 year olds
    double prev2025 = 0.25;
    if (n_counter > 0)
        prev2025 = static_cast<double>(p_counter) / n_counter;

    double max_prev = prev2025;
    //update the vector containing the prevalence by gestational age
    const size_t index = Sim::modulo_steps(Sim::ts0(), static_cast<int>(prev_by_gestational_age.size()));
    prev_by_gestational_age[index] = prev2025;
    for (const double prev : prev_by_gestational_age) {
        if (prev > max_prev) {
            max_prev = prev;
        }
    }

    // equation (2) p 75 AJTMH 75 suppl 2
    const double prev_pg = max_prev / (CritPrevPrim + max_prev);
    // equation (1) p 75 AJTMH 75 suppl 2 including 30% multiplier
    risk_from_maternal_infection = Y * (1.0 - exp(prev_pg * Z));
}

}
