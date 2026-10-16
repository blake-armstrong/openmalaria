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

/*
 * This file contains code internal to the LSTM PK/PD model to do with
 * dosing/scheduling.
 */
#ifndef HMOD_LSTM_MEDICATE
#define HMOD_LSTM_MEDICATE

#include "Global.h"
#include "PkPd/LSTMModel.h"
#include "util/errors.h"
#include "schema/pharmacology.h"

#include <vector>
#include <map>

using std::map;
using std::size_t;
using std::vector;

class UnittestUtil;

namespace OM::pk_pd {

struct Schedule {
    void load(const scnXml::PKPDSchedule::MedicateSequence& seq);

    vector<MedicateData> medications;
};

struct DosageTable {
    void load(const xsd::cxx::tree::sequence<scnXml::PKPDDosageRange>& seq, bool is_body_mass);

    /** Get dosage multilier from age or body mass.
     * 
     * Dosings may be given as a table using age or body mass as the key (first
     * column), or dose may be specified as mg drug / kg body mass. */
    double get_multiplier(double key) {
        if (mult_mass_kg)
            return key;
        else {
            auto iter = table.upper_bound(key);
            if (iter == table.end())
                throw TRACED_EXCEPTION("bad age/dosage table", util::Error::PkPd);
            return iter->second;
        }
    }

    bool use_mass{};     // false: dosing by age; true: dosing by body mass
    bool mult_mass_kg{}; // multiply by mass instead of using table
    map<double, double> table;
};

extern vector<Schedule> schedules;
extern vector<DosageTable> dosages;

}

#endif
