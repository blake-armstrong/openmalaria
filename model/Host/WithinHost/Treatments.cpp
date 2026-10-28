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

#include "Host/WithinHost/Treatments.h"
#include "interventions/InterventionManager.h"
#include "util/errors.h"
#include "util/UnitParse.h"
#include "schema/healthSystem.h"

namespace OM::WithinHost {

// ———  TreatmentRegistry  ———

TreatmentId TreatmentRegistry::add(const scnXml::TreatmentOption& desc,
                                   const interventions::InterventionManager& interventions, const Clock& clock,
                                   const util::CommandLineOptions& command_line) {
    Treatments treatment(desc, interventions, clock, command_line);
    TreatmentId id(treatments.size());
    treatments.push_back(std::move(treatment));
    return id;
}

// ———   non-static  ———

Treatments::Stages stage_from_string(const std::string& str) {
    if (str == "liver")
        return Treatments::LIVER;
    if (str == "blood")
        return Treatments::BLOOD;
    if (str == "both")
        return Treatments::BOTH;
    throw util::format_error(std::string("stage must be liver, blood or both, not ").append(str));
}
Treatments::Treatments(const scnXml::TreatmentOption& elt, const interventions::InterventionManager& interventions,
                       const Clock& clock, const util::CommandLineOptions& command_line)
    : TriggeredDeployments(elt, interventions), time_liver(Sim::zero()), time_blood(Sim::zero()) {
    for (const auto& it : elt.getClearInfections()) {
        try {
            //NOTE: if changing XSD, this should not be called "timesteps" or have a default unit
            const SimTime len =
                unit_parse::read_short_duration(it.getTimesteps(), unit_parse::STEPS, clock, command_line);
            if (len < -Sim::one_ts() || len == Sim::zero()) {
                throw util::format_error("timesteps must be ≥ 1 or have special value -1");
            }
            const Stages stage = stage_from_string(it.getStage());
            if (stage & LIVER) {
                if (time_liver != Sim::zero()) // existing treatment configuration
                    throw util::format_error("multiple specification of liver stage effect");
                time_liver = len;
            }
            if (stage & BLOOD) {
                if (time_blood != Sim::zero()) // existing treatment configuration
                    throw util::format_error("multiple specification of blood stage effect");
                time_blood = len;
            }
        } catch (const util::format_error& e) {
            throw util::xml_scenario_error(string(".../clearInfections/timesteps: ").append(e.message()));
        }
    }
}

}
