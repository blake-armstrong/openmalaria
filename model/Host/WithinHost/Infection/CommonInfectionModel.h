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

#ifndef HMOD_COMMON_INFECTION_MODEL
#define HMOD_COMMON_INFECTION_MODEL

#include "Global.h"
#include "Host/WithinHost/Infection/CommonInfection.h"
#include "Host/WithinHost/Infection/EmpiricalInfection.h"
#include "Host/WithinHost/Infection/MolineauxInfection.h"
#include "Host/WithinHost/Infection/PennyInfection.h"
#include "util/random.h"

#include <cstdint>
#include <istream>
#include <memory>

namespace OM::WithinHost {

struct CommonInfectionModel {
    enum class Kind : std::uint8_t { Dummy, Empirical, Molineaux, Penny };

    std::unique_ptr<CommonInfection> create(SimTime start_date, SimTime latent_p, util::LocalRng& rng,
                                            uint32_t genotype, int origin) const;
    std::unique_ptr<CommonInfection> load(SimTime latent_p, std::istream& stream) const;

    Kind kind = Kind::Dummy;
    EmpiricalInfectionParams empirical;
    MolineauxInfectionParams molineaux;
    PennyInfectionParams penny;
};

}

#endif
