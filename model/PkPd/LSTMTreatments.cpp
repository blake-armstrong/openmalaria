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

#include "PkPd/LSTMTreatments.h"
#include "PkPd/LSTMMedicate.h"
#include "PkPd/Drug/LSTMDrugType.h"
#include "PkPd/LSTMModel.h"

namespace OM {
namespace pk_pd {

void MedicateData::load(const scnXml::PKPDMedication& med) {
    drug = LSTMDrugType::find_drug(med.getDrug());
    qty = med.getMg();
    time = med.getHour() / 24.0; // convert from hours to days
}

void Schedule::load(const scnXml::PKPDSchedule::MedicateSequence& seq) {
    medications.resize(seq.size());
    size_t i = 0;
    for (const scnXml::PKPDMedication& med : seq) {
        medications[i].load(med);
        i += 1;
    }
}

vector<Schedule> schedules;
map<string, size_t> schedule_names;

void DosageTable::load(const xsd::cxx::tree::sequence<scnXml::PKPDDosageRange>& seq, bool is_body_mass) {
    use_mass = is_body_mass;
    mult_mass_kg = false;
    double last_mult = 0.0, last_age = numeric_limits<double>::quiet_NaN();
    for (const scnXml::PKPDDosageRange& age : seq) {
        if (last_age != last_age) {
            if (age.getLowerbound() != 0.0)
                throw util::xml_scenario_error("dosage table must have first lower bound equal 0");
        } else {
            if (age.getLowerbound() <= last_age) {
                throw util::xml_scenario_error("dosage table must list age groups in increasing order");
            }
            table[age.getLowerbound()] = last_mult;
        }
        last_mult = age.getDose_mult();
        last_age = age.getLowerbound();
    }
    table[numeric_limits<double>::infinity()] = last_mult;
}

vector<DosageTable> dosages;
map<string, size_t> dosages_names;

void LSTMTreatments::init(const scnXml::Treatments& data) {
    schedules.resize(data.getSchedule().size());
    size_t i = 0;
    for (const scnXml::PKPDSchedule& schedule : data.getSchedule()) {
        schedules[i].load(schedule.getMedicate());
        schedule_names[schedule.getName()] = i;
        i += 1;
    }

    dosages.resize(data.getDosages().size());
    i = 0;
    for (const scnXml::PKPDDosages& elt : data.getDosages()) {
        if (elt.getAge().size())
            dosages[i].load(elt.getAge(), false);
        else if (elt.getBodymass().size())
            dosages[i].load(elt.getBodymass(), true);
        else {
            if (!elt.getMultiply().present()) {
                throw util::xml_scenario_error("dosages table does not "
                                               "include age, bodymass or multiply element");
            }
            if (elt.getMultiply().get().getBy() != "kg") {
                throw util::xml_scenario_error("dosages table \"multipy by\" "
                                               "option is not \"kg\"");
            }
            dosages[i].use_mass = true;
            dosages[i].mult_mass_kg = true;
        }
        dosages_names[elt.getName()] = i;
        i += 1;
    }
}

void LSTMTreatments::clear() {
    schedules.clear();
    schedule_names.clear();
    dosages.clear();
    dosages_names.clear();
}

inline void invalid_model() {
    throw util::xml_scenario_error("use of PK/PD treatment requires use of a compatible infection model and "
                                   "specification of pharmacology/treatments");
}
size_t LSTMTreatments::find_schedule(const string& name) {
    if (schedule_names.size() == 0)
        invalid_model();
    auto iter = schedule_names.find(name);
    if (iter == schedule_names.end()) {
        throw util::xml_scenario_error(string("no treatment schedule with this name: ").append(name));
    }
    return iter->second;
}

size_t LSTMTreatments::find_dosages(const string& name) {
    if (dosages_names.size() == 0)
        invalid_model();
    auto iter = dosages_names.find(name);
    if (iter == dosages_names.end()) {
        throw util::xml_scenario_error(string("no dosage table with this name: ").append(name));
    }
    return iter->second;
}

}
}
