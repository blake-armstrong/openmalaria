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

#ifndef HMOD_LSTM_PK_PD_MODEL
#define HMOD_LSTM_PK_PD_MODEL

#include "PkPd/Drug/LSTMDrug.h"

namespace scnXml {
class PKPDMedication;
}
namespace OM {
namespace Host {
class Human;
}
namespace WithinHost {
class CommonInfection;
}
namespace pk_pd {

class DrugRegistry;
struct PkPdParams;

struct MedicateData {
    MedicateData() : qty(numeric_limits<double>::signaling_NaN()), time(numeric_limits<double>::signaling_NaN()) {}

    /// Checkpointing
    template <class S> void operator&(S& stream) {
        drug & stream;
        qty & stream;
        time & stream;
    }

private:
    void load(const scnXml::PKPDMedication& med, DrugRegistry& drugs);

    inline MedicateData multiplied(double dose_mult) const {
        MedicateData r(*this);
        r.qty *= dose_mult;
        return r;
    }

    size_t drug{0}; /// Drug type index
    double qty;     /// Quantity of drug prescribed in mg
    double time;    /// Time to medicate at, in days (0 means start of time step, may be >= 1 (thus not today))

    friend struct Schedule;
    friend class LSTMModel;
    friend class ::UnittestUtil;
};

/** Pharmacokinetic and pharmacodynamics interface, used by each human's
 * within-host model.
 *
 * This class holds per-human data: prescribed medications and drugs in the
 * body.
 * 
 * Calling order each day:
 *  * prescribe()
 *  * medicate()
 *  * getDrugFactor() for each infection
 *  * decayDrugs()
 */
class LSTMModel {
public:
    explicit LSTMModel(const PkPdParams& params) : params(&params) {}

    /// Checkpointing
    template <class S> void operator&(S& stream) { checkpoint(stream); }

    /** Prescribe a patient a course of drugs, via the Pk/Pd model
     * 
     * Note: doses sizes are modified according to age via the dosage
     * table given at the time this function is called.
     *
     * @param schedule Index of a treatment schedule
     * @param dosages Index of a dosage table
     * @param age Age of human at start of time step in years
     * @param body_mass Mass of human in kg
     * @param delay_d Delay of treatment in days
     */
    void prescribe(size_t schedule, size_t dosages, double age, double body_mass, double delay_d);

    /** Medicate drugs: human takes prescribed drugs which are to be taken this
     * day.
     * 
     * Note: poor adherence on the part of the patient is not modeled here; to
     * model, prescribe with a "poor adherence" schedule.
     */
    void medicate(LocalRng& rng);

    /** Get concentration of the drug at the beginning of the day.
     * 
     * For unit testing. Not optimised. */
    double get_drug_conc(size_t drug_index) const;

    /** This is how drugs act on infections.
     *
     * Each time step, on each infection, the parasite density is multiplied by
     * the return value of this infection. The WithinHostModels are responsible
     * for clearing infections once the parasite density is negligible. */
    double get_drug_factor(LocalRng& rng, WithinHost::CommonInfection* inf, double body_mass) const;

    /** After any resident infections have been reduced by getDrugFactor(),
     * this function is called to update drug levels to their effective level
     * at the end of the day, as well as clear data once drug concentrations
     * become negligible. */
    void decay_drugs(double body_mass);

    /** Make summaries of drug concentration data. */
    void summarize(const Host::Human& human) const;

private:
    /** Medicate drugs to an individual, which act on infections the following
     * time steps, until rendered ineffective by decayDrugs().
     *
     * \param typeIndex The index of drug type data (what LSTMDrugType::findDrug() returns).
     * \param qty The quantity in mg
     * \param time Time in days since start of this time step to medicate at
     * 
     * Due to the fact we're using a discrete time step model, the case-management
     * update (calling medicate) and within-host model update (calling
     * getDrugFactor) cannot [easily] have immediate effects on each other. The
     * implementation we use is that the within-host model update (calculating
     * new infection densities) happens first; hence medicate() will always be
     * called after getDrugFactor in a time step, and a time of zero means the
     * dose has effect from the start of the following time step. */
    void medicate_drug(LocalRng& rng, size_t type_index, double qty, double time);

    void checkpoint(istream& stream);
    void checkpoint(ostream& stream);

    /// Drugs with non-zero blood concentrations:
    vector<unique_ptr<LSTMDrug>> drugs;

    /// All pending medications
    list<MedicateData> medicate_queue;

    const PkPdParams* params;

    friend class ::UnittestUtil;
};

}
}
#endif
