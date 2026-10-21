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

#include "Host/WithinHost/Infection/CommonInfectionModel.h"

#include "Host/WithinHost/Infection/DummyInfection.h"
#include "util/errors.h"

namespace OM::WithinHost {

std::unique_ptr<CommonInfection> CommonInfectionModel::create(SimTime latent_p, util::LocalRng& rng, uint32_t genotype,
                                                              int origin) const {
    switch (kind) {
    case Kind::Dummy:
        return std::make_unique<DummyInfection>(latent_p, rng, genotype, origin);
    case Kind::Empirical:
        return std::make_unique<EmpiricalInfection>(empirical, latent_p, rng, genotype, origin, 1);
    case Kind::Molineaux:
        return std::make_unique<MolineauxInfection>(molineaux, latent_p, rng, genotype, origin);
    case Kind::Penny:
        return std::make_unique<PennyInfection>(penny, latent_p, rng, genotype, origin);
    }
    throw TRACED_EXCEPTION_DEFAULT("unknown infection model kind");
}

std::unique_ptr<CommonInfection> CommonInfectionModel::load(SimTime latent_p, std::istream& stream) const {
    switch (kind) {
    case Kind::Dummy:
        return std::make_unique<DummyInfection>(latent_p, stream);
    case Kind::Empirical:
        return std::make_unique<EmpiricalInfection>(empirical, latent_p, stream);
    case Kind::Molineaux:
        return std::make_unique<MolineauxInfection>(molineaux, latent_p, stream);
    case Kind::Penny:
        return std::make_unique<PennyInfection>(penny, latent_p, stream);
    }
    throw TRACED_EXCEPTION_DEFAULT("unknown infection model kind");
}

}
