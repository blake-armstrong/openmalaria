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

#ifndef HMOD_WITHIN_HOST_MODEL
#define HMOD_WITHIN_HOST_MODEL

#include "Global.h"
#include "Host/WithinHost/CommonWithinHost.h"
#include "Host/WithinHost/DescriptiveWithinHost.h"
#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "Host/WithinHost/Treatments.h"
#include "Host/WithinHost/WHFalciparum.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/WHVivax.h"
#include "Parameters.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/random.h"

#include <cstdint>
#include <memory>

namespace scnXml {
class Scenario;
}

namespace OM {
namespace WithinHost {

struct WithinHostModel {
    enum class Kind : std::uint8_t { Descriptive, Common, Vivax };

    WithinHostModel() = default;
    WithinHostModel(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                    const util::CommandLineOptions& command_line, const GenotypeRegistry& genotypes,
                    const DiagnosticRegistry& diagnostics, const mon::MonitoringState& monitoring,
                    const scnXml::Scenario& scenario);

    std::unique_ptr<WHInterface> create(util::LocalRng& rng, double comorbidity_factor) const;

    Kind kind = Kind::Descriptive;
    FalciparumParams falciparum;
    CommonWithinHostParams common;
    DescriptiveWithinHostParams descriptive;
    VivaxParams vivax;
    Pathogenesis::PathogenesisParams pathogenesis;
    TreatmentRegistry treatments;
};

}
}

#endif
