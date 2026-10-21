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

#include "Host/WithinHost/Infection/DummyInfection.h"
#include "util/ModelOptions.h"

#include <algorithm>
#include <sstream>
#include <cstring>

namespace OM::WithinHost {

DummyInfection::DummyInfection(SimTime latent_p, LocalRng& rng, uint32_t prot_id, int origin)
    : CommonInfection(latent_p, prot_id, origin) {
    density = 16; // increased by DH to avoid zeros in initialKappa
}

bool DummyInfection::update_density(LocalRng& rng, double survival_factor, SimTime, double) {
    const double growth_rate = 8.0;
    const double parasite_threshold = 1;

    density = (mod_nn(static_cast<int>(density * growth_rate), 20000)) * survival_factor;
    cum_exposure_j += density;

    if (density < parasite_threshold) {
        return true;
    } else {
        return false;
    }
}

DummyInfection::DummyInfection(SimTime latent_p, istream& stream) : CommonInfection(latent_p, stream) {}

}
