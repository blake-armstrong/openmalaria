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
#ifndef HMOD_PK_PD_PARAMS
#define HMOD_PK_PD_PARAMS

#include "PkPd/Drug/LSTMDrugType.h"
#include "PkPd/LSTMMedicate.h"

namespace scnXml {
class Scenario;
}

namespace OM {
namespace WithinHost {
class GenotypeRegistry;
}
namespace util {
class CommandLineOptions;
}
namespace pk_pd {

/** PK/PD data of a simulation: drug types and treatment courses. Empty when
 * the scenario has no pharmacology element. */
struct PkPdParams {
    PkPdParams() = default;
    PkPdParams(const scnXml::Scenario& scenario, const WithinHost::GenotypeRegistry& genotypes,
               const util::CommandLineOptions& command_line);

    DrugRegistry drugs;
    CourseRegistry courses;
};

}
}

#endif
