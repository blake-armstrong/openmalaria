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

#include "Clinical/ESCaseManagement.h"
#include "Clinical/CMDecisionTree.h"
#include "Clinical/EventScheduler.h"
#include "util/errors.h"
#include "util/ModelOptions.h"

#include <set>
#include <sstream>

namespace OM {
namespace Clinical {
using namespace OM::util;

// -----  ESCaseManagement  -----

void EventSchedulerParams::set_health_system(const scnXml::HSEventScheduler& es_data,
                                             util::AgeGroupInterpolator& case_fatality_rate) {
    uncomplicated_tree = &CMDecisionTree::create(es_data.getUncomplicated(), true);
    complicated_tree = &CMDecisionTree::create(es_data.getComplicated(), false);

    set_parameters(es_data, case_fatality_rate);
}

CMDTOut ESCaseManagement::execute(const EventSchedulerParams& params, const CMHostData& host_data) {
    assert(host_data.pgState & Episode::SICK);
    //TODO: should we remove any existing prescriptions?
    //TODO: Note that these trees do both "access" and "case management" decisions.
    //TODO: medicateQueue.clear();

    const CMDecisionTree& tree =
        (host_data.pgState & Episode::COMPLICATED) ? *params.complicated_tree : *params.uncomplicated_tree;
    return tree.exec(host_data);
}

}
}
