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

#ifndef HMOD_NON_VECTOR_MODEL
#define HMOD_NON_VECTOR_MODEL

#include "TransmissionModel.h"
#include "util/vectors.h"
#include "Host/Human.h"
#include "mon/Monitoring.h"

namespace scnXml {
class NonVector;
}

namespace OM::Transmission {

//! Base transmission model, as used in Phase A
class NonVectorModel : public TransmissionModel {
public:
    NonVectorModel(vector<double> init_eir, int intervention_mode, const scnXml::Entomology& ento_data,
                   const scnXml::NonVector& non_vector_data, int eip_duration, size_t n_genotypes, SimTime adult_age,
                   bool vaccine_genotype, const Clock& clock, mon::Continuous& continuous)
        : TransmissionModel(std::move(init_eir), intervention_mode, n_genotypes, adult_age, vaccine_genotype, clock,
                            continuous),
          n_spore(Sim::from_days(eip_duration)) {
        lagged_kappa.resize(clock.in_steps(n_spore) + 1, 0.0);

        vector<int> n_days(clock.steps_per_year(), 0);
        // The minimum EIR allowed in the array. The product of the average EIR and a constant.
        const double min_eir = min_eir_mult * average_eir(non_vector_data);

        const scnXml::NonVector::EIRDailySequence& daily = non_vector_data.getEIRDaily();
        if (daily.size() < static_cast<size_t>(Sim::one_year()))
            throw util::xml_scenario_error("insufficient EIRDaily data for a year");

        for (SimTime mpcday = Sim::zero(), end_day = Sim::from_days(daily.size()); mpcday < end_day;
             mpcday = mpcday + Sim::one_day()) {
            const double eir_daily = std::max(static_cast<double>(daily[mpcday]), min_eir);

            // Index 0 of initialisationEIR refers to the EIR affecting the
            // first day(s) of the year. Correspondingly, the first 1 or 5 values
            // of EIRDaily affect this (1- or 5-day) time-step.
            size_t i = clock.modulo_year_steps(mpcday);

            n_days[i] += 1;
            initialisation_eir[i] += eir_daily;
        }

        // Calculate total annual EIR
        // divide by number of records assigned to each interval (usually one per day)
        for (size_t ind_ts = 0; ind_ts < clock.steps_per_year(); ind_ts += 1) {
            initialisation_eir[ind_ts] *= clock.one_ts() / static_cast<double>(n_days[ind_ts]);
            annual_eir += initialisation_eir[ind_ts];
        }

        initial_kappa.assign(clock.in_steps(Sim::from_years_i(n_years_warmup_data)), 0.0);
    }

    ~NonVectorModel() override = default;

    void init2(const vector<Host::Human>& population) override {
        simulation_mode = ForcedEir;
    } // no set-up needed; just indicate we're ready to roll

    void scale_eir(double factor) override {
        util::vectors::scale(initialisation_eir, factor);
        annual_eir = util::vectors::sum(initialisation_eir);
    }

    SimTime init_iterate() override {
        simulation_mode = intervention_mode;
        if (simulation_mode != DynamicEir) {
            return Sim::zero();
        }

        // initialKappa is used in calculateEIR
        assert(initial_kappa.size() >= clock().steps_per_year());
        assert(mod_nn(initial_kappa.size(), clock().steps_per_year()) == 0);
        for (size_t i = clock().steps_per_year(); i < initial_kappa.size(); ++i) {
            initial_kappa[i % clock().steps_per_year()] += initial_kappa[i];
        }
        const double factor = static_cast<double>(clock().steps_per_year()) / static_cast<double>(initial_kappa.size());
        initial_kappa.resize(clock().steps_per_year());
        for (double& i : initial_kappa) {
            i *= factor;
            // error check:
            if (!(i > 0.0)) // if not positive
                throw TRACED_EXCEPTION("initialKappa is invalid", util::Error::InitialKappa);
        }

        return Sim::zero(); // nothing to do
    }

    void change_eir_intervention(const scnXml::NonVector& non_vector_data) override {
        // Note: requires sim::intervTime() >= sim::zero(), but this can only be
        // called in intervention period anyway.
        simulation_mode = TransientEirKnown;

        if (n_spore != Sim::from_days(non_vector_data.getEipDuration()))
            throw util::xml_scenario_error("change-of-EIR intervention cannot change EIP duration");

        const scnXml::NonVector::EIRDailySequence& daily = non_vector_data.getEIRDaily();
        vector<int> n_days(clock().in_steps(Sim::from_days(static_cast<int>(daily.size()) - 1)) + 1, 0);
        intervention_eir.assign(n_days.size(), 0.0);
        const auto required_days = static_cast<size_t>(clock().end_date() - clock().start_date()) + 1;
        if (daily.size() < required_days) {
            cerr << "Days: " << daily.size() << "\nIntervals: " << n_days.size() << "\nRequired: " << required_days
                 << endl;
            throw util::xml_scenario_error("Insufficient intervention phase EIR values provided");
        }
        // The minimum EIR allowed in the array. The product of the average EIR and a constant.
        const double min_eir = min_eir_mult * average_eir(non_vector_data);
        for (SimTime mpcday = Sim::zero(), end_day = Sim::from_days(daily.size()); mpcday < end_day;
             mpcday = mpcday + Sim::one_day()) {
            const double eir_daily = std::max(static_cast<double>(daily[mpcday]), min_eir);

            // istep is the time period to which the day is assigned.
            size_t istep = clock().in_steps(mpcday);
            n_days[istep]++;
            intervention_eir[istep] += eir_daily;
        }
        // divide by number of records assigned to each interval (usually one per day)
        for (size_t i = 0; i < intervention_eir.size(); ++i) {
            intervention_eir[i] *= clock().one_ts() / static_cast<double>(n_days[i]);
        }

        // I've no idea what this should be, so until someone asks it can be NaN.
        // It was -9.99 and later 0.0. It could of course be recalculated from interventionEIR.
        annual_eir = numeric_limits<double>::quiet_NaN();
    }

    void uninfect_vectors() override {
        if (simulation_mode != DynamicEir)
            cerr << "Warning: uninfectVectors is not efficacious with forced EIR" << endl;
        // reset history of human infectivity, which scales dynamic EIR:
        lagged_kappa.assign(lagged_kappa.size(), 0.0);
    }

    double update_kappa(const vector<Host::Human>& population) override {
        const double current_kappa = TransmissionModel::update_kappa(population);
        if (simulation_mode == ForcedEir) {
            initial_kappa[clock().modulo_steps(clock().ts1(), static_cast<int>(initial_kappa.size()))] = current_kappa;
        }
        return current_kappa;
    }

    void calculate_eir(Host::Human& human, double age_years, vector<double>& genotype_eir_i,
                       vector<double>& genotype_eir_l) const override {
        genotype_eir_i.resize(1);
        genotype_eir_l.resize(1); // no support for per-genotype tracking in this model (possible, but we're lazy)
        // where the full model, with estimates of human mosquito transmission is in use, use this:
        if (simulation_mode == ForcedEir) {
            genotype_eir_l[0] = initialisation_eir[clock().modulo_year_steps(clock().ts0())];
        } else if (simulation_mode == TransientEirKnown) {
            // where the EIR for the intervention phase is known, obtain this from
            // the interventionEIR array
            genotype_eir_l[0] = intervention_eir[clock().in_steps(clock().interv_time())];
        } else if (simulation_mode == DynamicEir) {
            genotype_eir_l[0] = initialisation_eir[clock().modulo_year_steps(clock().ts0())];
            if (clock().interv_time() >= Sim::zero()) {
                // we modulate the initialization based on the human infectiousness time steps ago in the
                // simulation relative to infectiousness at the same time-of-year, pre-intervention.
                // nspore gives the sporozoite development delay.
                const SimTime spore_start = clock().ts1() - n_spore;
                genotype_eir_l[0] *=
                    lagged_kappa[clock().modulo_steps(spore_start, static_cast<int>(lagged_kappa.size()))] /
                    initial_kappa[clock().modulo_year_steps(spore_start)];
            }
        } else {
            throw util::xml_scenario_error("Invalid simulation mode");
        }
#ifndef NDEBUG
        if (!(std::isfinite)(genotype_eir_l[0])) {
            const SimTime spore_start = clock().ts1() - n_spore;
            ostringstream msg;
            msg << "Error: non-vect eir is: " << genotype_eir_l[0] << "\nlaggedKappa:\t"
                << lagged_kappa[clock().modulo_steps(spore_start, static_cast<int>(lagged_kappa.size()))]
                << "\ninitialKappa:\t" << initial_kappa[clock().modulo_year_steps(spore_start)] << endl;
            throw TRACED_EXCEPTION(msg.str(), util::Error::InitialKappa);
        }
#endif
        genotype_eir_l[0] *= human.per_host_transmission.relative_availability_het_age(age_years);

        mon::record_stat(mon::measure("innoculationsPerAgeGroup"), human, genotype_eir_l[0]);
        mon::record_stat(mon::measure("innoculationsPerVector"), human, genotype_eir_l[0]);
    }

private:
    void checkpoint(istream& stream) override {
        TransmissionModel::checkpoint(stream);
        n_spore & stream;
        intervention_eir & stream;
        initial_kappa & stream;
    }

    void checkpoint(ostream& stream) override {
        TransmissionModel::checkpoint(stream);
        n_spore & stream;
        intervention_eir & stream;
        initial_kappa & stream;
    }

    //! multiplier used to calculate a positive EIR value where the measured value is zero
    /*
      0.01 was old pv(30) Now a constant. min_EIR_mult multiplies the average EIR to obtain a value used for the EIR during periods when
      it is too low to be measureable. The value of 0.01 was old pv(30) Now a constant. 0.01 was old pv(30) Now a constant.
    */
    const double min_eir_mult = 0.01;

    /** @brief Variables set by constructor.
     *
     * There shouldn't be any need to checkpoint these, at least before
     * interventions take effect. */
    //@{
    //! Variance of Infection Rate according to fielddata
    const double total_infectionrate_variance = 1.0;

    const int n_years_warmup_data = 5;

    //! The duration of sporogony in time steps
    // doesn't need checkpointing
    SimTime n_spore = Sim::never();
    //@}

    /** EIR per time interval during the intervention period. Value at index
     * sim::intervTime().inSteps() used each time-step.
     *
     * Units: inoculations per adult per time step */
    vector<double> intervention_eir;

    /** When simulationMode == dynamicEIR, this is the annual cycle of kappa
     * from the warmup phase and has length 1 year (in time steps).
     *
     * When simulationMode == equilibriumMode, this may be multiple years long and
     * is used to collect values of kappa (human infectiousness).
     *
     * In either case, sim::ts0().moduloSteps(initialKappa.size()) is the index
     * for the current infectiousness during updates. */
    vector<double> initial_kappa;
};
} // namespace OM::Transmission

#endif
