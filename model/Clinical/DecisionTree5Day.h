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

#ifndef HMOD_DECISION_TREE_5_DAY
#define HMOD_DECISION_TREE_5_DAY

#include "Clinical/CM5DayCommon.h"

namespace OM::Clinical {

/**
 * This models case management with 5-day time steps, optionally including
 * PK/PD modeling for uncomplicated cases.
 * 
 * Uncomplicated cases: access, otherwise known as "seeking any type of
 * treatment", is determined by a fixed-function decision, which may be
 * modified by a treatment-seeking factor. Treatment decisions (type of
 * treatment, use of diagnostics, effectiveness) are determined by a
 * programmable decision tree.
 * 
 * Severe cases: all decisions and outcomes are calculated via a fixed-function
 * probability tree, using the same logic for handling severe cases as has long
 * been used.
 */
class DecisionTree5Day : public CM5DayCommon {
public:
    DecisionTree5Day(const ClinicalParams& params, double tsf) : CM5DayCommon(params, tsf) {}

protected:
    void uncomplicated_event(Human& human, Episode::State pg_state) override;
};

}

#endif
