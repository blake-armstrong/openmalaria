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

#ifndef HMOD_WH_PATH_SUBMODELS
#define HMOD_WH_PATH_SUBMODELS

#include "Global.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"

using namespace std;

namespace OM {
namespace WithinHost {
namespace Pathogenesis {

/// Müller presentation model.
class MuellerPathogenesis : public PathogenesisModel {
public:
    MuellerPathogenesis(const PathogenesisParams& params, double comorbidity_factor)
        : PathogenesisModel(params, comorbidity_factor) {}
    ~MuellerPathogenesis() {}

    virtual double get_p_episode(double time_step_max_density, double total_density);
};

/// Pyrogenic threshold presentation model.
class PyrogenPathogenesis : public PathogenesisModel {
protected:
    /// Critical density for fever (clinical episodes)
    double pyrogen_thres;
    /// Determine the current pyrogenic threshold.
    virtual void update_pyrogen_thres(double total_density);

public:
    PyrogenPathogenesis(const PathogenesisParams& params, double comorbidity_factor);
    virtual ~PyrogenPathogenesis() {}
    virtual void summarize(const Host::Human& human);
    virtual double get_p_episode(double time_step_max_density, double total_density);

protected:
    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);
};

/// Predetermined episodes presentation model.
class PredetPathogenesis : public PyrogenPathogenesis {
public:
    PredetPathogenesis(const PathogenesisParams& params, double comorbidity_factor)
        : PyrogenPathogenesis(params, comorbidity_factor) {}
    ~PredetPathogenesis() {}

    virtual double get_p_episode(double time_step_max_density, double total_density);
};

}
}
}
#endif
