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

#include "Host/WithinHost/Pathogenesis/Submodels.h"
#include "Parameters.h"
#include "mon/Monitoring.h"

#include <cmath>
using namespace std;

namespace OM {
namespace WithinHost {
namespace Pathogenesis {

// ———  Müller presentation model  ———

MuellerParams::MuellerParams(const OM::Parameters& parameters)
    : rate_multiplier_31(parameters[Parameter::MuellerRateMultiplier] * Sim::years_per_step()),
      density_exponent_32(parameters[Parameter::MuellerDensityExponent]) {}

double MuellerPathogenesis::get_p_episode(double, double total_density) {
    double incidence_density =
        params->mueller.rate_multiplier_31 * (pow(total_density, params->mueller.density_exponent_32));
    return 1.0 - exp(-incidence_density);
}

// ———  Pyrogenic threshold model  ———

//Number of categories in the numerical approximation used in updatePyrogenThres()
const size_t NumCategories = 11;

namespace {

double pyrogen_b(const OM::Parameters& parameters) {
    const double delt = 1.0 / NumCategories;
    const double smu_y = -log(0.5) / (Sim::steps_per_year() * parameters[Parameter::YStarHalfLife]);
    return -smu_y * delt;
}

double pyrogen_a(const OM::Parameters& parameters) {
    const double delt = 1.0 / NumCategories;
    //alpha: factor determining increase in pyrogenic threshold
    const double alpha14 = parameters[Parameter::Alpha];
    return alpha14 * Sim::one_ts() * delt;
}

}

PyrogenParams::PyrogenParams(const OM::Parameters& parameters)
    : init_pyro_thres(parameters[Parameter::YStar0]), b(pyrogen_b(parameters)),
      ystar2_13(parameters[Parameter::YStarSq]), a(pyrogen_a(parameters)), ystar1_26(parameters[Parameter::YStar1]) {}

PyrogenPathogenesis::PyrogenPathogenesis(const PathogenesisParams& params, double comorbidity_factor)
    : PathogenesisModel(params, comorbidity_factor), pyrogen_thres(params.pyrogen.init_pyro_thres) {}

double PyrogenPathogenesis::get_p_episode(double time_step_max_density, double total_density) {
    update_pyrogen_thres(total_density);
    return time_step_max_density / (time_step_max_density + pyrogen_thres);
}

void PyrogenPathogenesis::summarize(const Host::Human& human) {
    mon::recordStat(mon::measure("sumPyrogenThresh"), human, pyrogen_thres);
    mon::recordStat(mon::measure("sumLogPyrogenThres"), human, log(pyrogen_thres + 1.0));
}

void PyrogenPathogenesis::update_pyrogen_thres(double total_density) {
    // Note: this calculation is slow (something like 5% of runtime)

    const double a = params->pyrogen.a;
    const double b = params->pyrogen.b;
    const double ystar1_26 = params->pyrogen.ystar1_26;
    const double ystar2_13 = params->pyrogen.ystar2_13;
    //Numerical approximation to equation 2, AJTMH p.57
    for (size_t i = 1; i <= NumCategories; ++i) {
        pyrogen_thres +=
            total_density * a / ((ystar1_26 + total_density) * (ystar2_13 + pyrogen_thres)) + b * pyrogen_thres;
    }
}

void PyrogenPathogenesis::checkpoint(istream& stream) {
    PathogenesisModel::checkpoint(stream);
    pyrogen_thres & stream;
}
void PyrogenPathogenesis::checkpoint(ostream& stream) {
    PathogenesisModel::checkpoint(stream);
    pyrogen_thres & stream;
}

// ———  Predetermined episodes model  ———

double PredetPathogenesis::get_p_episode(double time_step_max_density, double total_density) {
    update_pyrogen_thres(total_density);
    return (time_step_max_density > pyrogen_thres) ? 1 : 0;
}

}
}
}
