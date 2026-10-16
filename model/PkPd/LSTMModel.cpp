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

#include "PkPd/Drug/LSTMDrugType.h"
#include "PkPd/Drug/LSTMDrugOneComp.h"
#include "PkPd/LSTMModel.h"
#include "PkPd/LSTMMedicate.h"
#include "PkPd/LSTMTreatments.h"
#include "mon/Monitoring.h"
#include "util/checkpoint_containers.h"
#include "util/errors.h"

#include "schema/scenario.h"

#include <cassert>

namespace OM::pk_pd {

// ———  static  ———

void LSTMModel::init(const scnXml::Scenario& scenario) {
    if (scenario.getPharmacology().present()) {
        LSTMDrugType::init(scenario.getPharmacology().get().getDrugs());
        LSTMTreatments::init(scenario.getPharmacology().get().getTreatments());
    }
}

// ———  non-static set up / tear down functions  ———

void LSTMModel::checkpoint(istream& stream) {
    // We need an RNG to call createInstance, but override the values later
    // anyway. Hacky solution: create a local RNG.
    LocalRng rng(0, 0);

    size_t num_drugs = 0; // type must be same as m_drugs.size()
    num_drugs & stream;
    util::checkpoint::validateListSize(num_drugs);
    for (size_t i = 0; i < num_drugs; ++i) {
        size_t index = 0;
        index & stream;
        drugs.push_back(LSTMDrugType::create_instance(rng, index));
        drugs.back() & stream;
    }
    medicate_queue & stream;
}

void LSTMModel::checkpoint(ostream& stream) {
    drugs.size() & stream;
    for (auto& drug : drugs) {
        drug->get_index() & stream;
        drug & stream;
    }
    medicate_queue & stream;
}

// ———  non-static simulation time functions  ———

void LSTMModel::prescribe(size_t schedule, size_t dosage, double age, double body_mass, double delay_d) {
    DosageTable& table = dosages[dosage];
    const double key = table.use_mass ? body_mass : age;
    const double dose_mult = table.get_multiplier(key);
    for (MedicateData& medicate_data : schedules[schedule].medications) {
        MedicateData data = medicate_data.multiplied(dose_mult);
        data.time += delay_d;
        medicate_queue.push_back(data);
    }
}

void LSTMModel::medicate(LocalRng& rng) {
    if (medicate_queue.empty())
        return;

    // Process pending medications (in interal queue) and apply/update:
    auto iter = medicate_queue.begin();
    while (iter != medicate_queue.end()) {
        if (iter->time < 1.0) { // Medicate medications to be prescribed starting at the next time-step
            // This function could be inlined, except for uses in testing:
            medicate_drug(rng, iter->drug, iter->qty, iter->time);
            iter = medicate_queue.erase(iter);
        } else { // and decrement treatment seeking delay for the rest
            iter->time -= 1.0;
            ++iter;
        }
    }
}

void LSTMModel::medicate_drug(LocalRng& rng, size_t type_index, double qty, double time) {
    //TODO: might be a little faster if m_drugs was pre-allocated with a slot for each drug type, using a null pointer
    for (auto& drug : drugs) {
        if (drug->get_index() == type_index) {
            drug->medicate(time, qty);
            return;
        }
    }
    // No match, so insert one:
    drugs.push_back(LSTMDrugType::create_instance(rng, type_index));
    (*drugs.back()).medicate(time, qty);
}

double LSTMModel::get_drug_conc(size_t drug_index) const {
    double c = 0.0;
    double d = 0.0;
    for (auto& drug : drugs) {
        d = drug->get_concentration(drug_index);
        if (d < 0.0) {
            // FIXME: @tph-thuering, 2015-08-11:
            // This is trying to circumvent an issue we have with negative metabolite concentration values in the implementation of the conversion model. see #53
            cerr << "ERROR! concentration is lower than zero, setting it to zero for now: " << d
                 << " drug_index: " << drug_index << endl;
            d = 0.0;
        }
        c += d;
    }
    assert(c >= 0.0);
    return c;
}

double LSTMModel::get_drug_factor(LocalRng& rng, WithinHost::CommonInfection* inf, double body_mass) const {
    double factor = 1.0; //no effect

    for (const auto& drug : drugs) {
        const double drug_factor = drug->calculate_drug_factor(rng, inf, body_mass);
        factor *= drug_factor;
    }
    return factor;
}

void LSTMModel::decay_drugs(double body_mass) {
    // Update concentrations for each drug.
    // TODO: previously we removed drugs with negligible concentration here. What now, just set concentration to 0?
    for (auto& drug : drugs) {
        drug->update_concentration(body_mass);
    }
}

void LSTMModel::summarize(const Host::Human& human) const {
    const vector<size_t>& drugs_in_use(LSTMDrugType::get_drugs_in_use());
    for (size_t index : drugs_in_use) {
        for (auto& drug : drugs) {
            const double conc = drug->get_concentration(index);
            if (conc > 0.0) {
                mon::recordStat(mon::measure("nHostDrugConcNonZero"), human, 1, 0, 0, index);
                mon::recordStat(mon::measure("sumLogDrugConcNonZero"), human, log(conc), 0, 0, index);
            }
        }
    }
}

}
