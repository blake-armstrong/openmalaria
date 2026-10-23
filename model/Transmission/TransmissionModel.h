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

#ifndef HMOD_TRANSMISSION_MODEL
#define HMOD_TRANSMISSION_MODEL

#include <string.h>
#include <fstream>

#include "Global.h"
#include "schema/interventions.h"
#include "util/errors.h"

#include "Transmission/PerHost.h"
#include "Population.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Genotypes.h"
#include "mon/Continuous.h"
#include "mon/Monitoring.h"
#include "util/StreamValidator.h"
#include "util/CommandLine.h"
#include "util/vectors.h"
#include "util/ModelOptions.h"

#include <cmath>
#include <cfloat>
#include <gsl/gsl_vector.h>

namespace OM {
class Summary;
class Population;
namespace Host {
class Human;
}
namespace Transmission {

inline double average_eir(const scnXml::NonVector& non_vector_data) {
    // Calculates the arithmetic mean of the whole daily EIR vector read from the .XML file
    double val_average_eir = 0.0;
    size_t i = 0;
    for (const scnXml::NonVector::EIRDailySequence& daily = non_vector_data.getEIRDaily(); i < daily.size(); ++i) {
        val_average_eir += static_cast<double>(daily[i]);
    }
    if (i == 0)
        throw util::xml_scenario_error("no EIRDaily values given"); // pedantic check
    return val_average_eir / static_cast<double>(i);
}

inline double average_eir(const scnXml::DailyValues& vector_data) {
    // Calculates the arithmetic mean of the whole daily EIR vector read from the .XML file
    double val_average_eir = 0.0;
    size_t i = 0;
    for (const scnXml::DailyValues::ValueSequence& daily = vector_data.getValue(); i < daily.size(); ++i) {
        val_average_eir += static_cast<double>(daily[i]);
    }
    if (i == 0)
        throw util::xml_scenario_error("no daily values given"); // pedantic check
    return val_average_eir / static_cast<double>(i);
}

/** Variable describing current simulation mode. */
enum SimulationMode {
    /** Initial mode. Indicates that initialisation still needs to happen (i.e.
     * it is an error if this mode is still set when getEIR is called). */
    PreInit,
    /** Equilibrium mode (i.e. just apply a fixed EIR)
     *
     * This is used for the warm-up period and if we want to separate direct
     * effect of an intervention from indirect effects via transmission
     * intensity. The seasonal pattern and intensity of the EIR do not change
     * over years.
     *
     * In this mode vector calculations aren't run. */
    ForcedEir,
    /** Transient EIR known
     *
     * This is used to simulate an intervention that changes EIR, and where we
     * have measurements of the EIR over time during the intervention period.
     */
    TransientEirKnown,
    /** EIR changes
     *
     * The simulation is driven by the EIR which changes dynamically during the
     * intervention phase as a function of the characteristics of the
     * interventions.
     *
     * Dependending on whether the Vector or NonVector model is in use, this
     * EIR may be calculated from a mosquito emergence rate or be an input EIR
     * scaled by the relative infectiousness of the humans. */
    DynamicEir,
};

/// Abstract base class, defines behaviour of transmission models
class TransmissionModel {
protected:
    /// Reads all entomological parameters from the input datafile.
    /// @param entoData input configuration for model
    /// @param n_genotypes number of genotypes transmission model is using
    TransmissionModel(vector<double> initialisation_eir, int intervention_mode, size_t n_genotypes, SimTime adult_age)
        : n_genotypes(n_genotypes), initialisation_eir(std::move(initialisation_eir)),
          intervention_mode(intervention_mode), lagged_kappa(Sim::steps_per_year(), 0.0),
          annual_average_kappa(numeric_limits<double>::signaling_NaN()), adult_age(adult_age),
          opt_vaccine_genotype(util::ModelOptions::option(util::VaccineGenotype)) {
        // Set VACCINE_GENOTYPE option

        mon::Continuous::registerCallback("input EIR", "\tinput EIR",
                                          [this](ostream& stream) { cts_cb_input_eir(stream); });
        mon::Continuous::registerCallback("simulated EIR", "\tsimulated EIR",
                                          [this](ostream& stream) { cts_cb_simulated_eir(stream); });
        mon::Continuous::registerCallback("human infectiousness", "\thuman infectiousness",
                                          [this](ostream& stream) { cts_cb_kappa(stream); });
        mon::Continuous::registerCallback("num transmitting humans", "\tnum transmitting humans",
                                          [this](ostream& stream) { cts_cb_num_transmitting_humans(stream); });
    }

public:
    //! Deallocate memory for TransmissionModel parameters and clean up
    virtual ~TransmissionModel() = default;

    /** Extra initialisation when not loading from a checkpoint, requiring
     * information from the human population structure. */
    virtual void init2(const vector<Host::Human>& population) = 0;

    /// Checkpointing
    template <class S> void operator&(S& stream) { checkpoint(stream); }
    //@}

    /** Set some summary items.
     *
     * Overriding functions should call this base version too. */
    virtual void summarize() {
        mon::recordStat(mon::measure("nTransmit"), lagged_kappa[Sim::modulo_steps(Sim::now(), lagged_kappa.size())]);
        mon::recordStat(mon::measure("annAvgK"), annual_average_kappa);

        if (!mon::isReported())
            return; // cannot use counters below when not reporting

        const double duration = Sim::in_steps(Sim::now() - last_survey_time);
        if (duration > 0.0) {
            mon::recordStat(mon::measure("inputEIR"), survey_input_eir / duration);
            mon::recordStat(mon::measure("simulatedEIR"), survey_simulated_eir / duration);
            mon::recordStat(mon::measure("simulatedEIR_Introduced"), survey_simulated_eir_i / duration);
            mon::recordStat(mon::measure("simulatedEIR_Indigenous"), survey_simulated_eir_l / duration);
        }

        survey_input_eir = 0.0;
        survey_simulated_eir = 0.0;
        survey_simulated_eir_i = 0.0;
        survey_simulated_eir_l = 0.0;
        last_survey_time = Sim::now();
    }

    /** Scale the EIR used by the model.
     *
     * EIR is scaled in memory (so will affect this simulation).
     * XML data is not touched. */
    virtual void scale_eir(double factor) = 0;

    virtual SimTime init_iterate() = 0;

    /** Needs to be called each step of the simulation before Human::update().
     *
     * when the vector model is used this updates mosquito populations. */
    virtual void vector_update(const vector<Host::Human>& population) {};

    virtual void change_eir_intervention(const scnXml::NonVector&) = 0;

    /** Does per-time-step updates and returns the EIR (inoculation rate per host
     * per time step). Should be called exactly once per time-step (at least,
     * during the intervention period when ITNs may be in use).
     *
     * Non-vector:
     * During the pre-intervention phase, the EIR is forced, using values from
     * the XML file. During the main simulation phase, it may be calculated or
     * obtained from data in the XML file.
     *
     * Vector:
     * During vector initialisation phase, EIR is forced based on the EIR given
     * in the XML file as a Fourier Series. After endVectorInitPeriod() is called
     * the simulation switches to using dynamic EIR. advanceStep _must_ be
     * called before this function in order to return the correct value.
     *
     * @param human A reference to the human who's EIR is being calculated.
     *    The human's "per host transmission" potentially needs updating.
     * @param age Age of the human in time units
     * @param ageYears Age of the human in years
     * @param EIR Out-vector of EIR per parasite genotype. The length is also set
     *    by the called function. Where genotype tracking is not supported (e.g.
     *    the non-vector model), the length is set to one.
     * @returns the sum of EIR across genotypes
     */
    double get_eir(Host::Human& human, SimTime age, double age_years, vector<double>& genotype_eir_i,
                   vector<double>& genotype_eir_l) {
        /* For the NonVector model, the EIR should just be multiplied by the
         * availability. For the Vector model, the availability is also required
         * for internal calculations, but again the EIR should be multiplied by the
         * availability. */
        calculate_eir(human, age_years, genotype_eir_i, genotype_eir_l);

        const double sum_eir_i = util::vectors::sum(genotype_eir_i);
        const double sum_eir_l = util::vectors::sum(genotype_eir_l);

        const double all_eir = sum_eir_i + sum_eir_l;
        if (age >= adult_age) {
            ts_adult_ento_inocs_i += sum_eir_i;
            ts_adult_ento_inocs_l += sum_eir_l;
            ts_adult_ento_inocs += all_eir;
            ts_num_adults += 1;
        }
        return all_eir;
    }

    /** Remove all current infections to mosquitoes, such that without re-
     * infection, humans will then be exposed to zero EIR. */
    virtual void uninfect_vectors() = 0;

    /** Needs to be called each time-step after Human::update() to update summary
     * statististics related to transmission. Also returns kappa (the average
     * human infectiousness weighted by availability to mosquitoes). */
    virtual double update_kappa(const vector<Host::Human>& population) {
        // We calculate kappa for output and the non-vector model.
        double sum_wt_kappa = 0.0;
        double sum_weight = 0.0;
        num_transmitting_humans = 0;

        for (const Host::Human& human : population) {
            // NOTE: calculate availability relative to age at end of time step;
            // not my preference but consistent with TransmissionModel::getEIR().
            const double avail =
                human.perHostTransmission.relative_availability_het_age(Sim::in_years(human.age(Sim::ts1())));
            sum_weight += avail;

            vector<double> prob_trans_genotype_i(num_genotypes());
            vector<double> prob_trans_genotype_l(num_genotypes());
            const double p_transmit =
                human.withinHostModel->prob_transmission_to_mosquito(prob_trans_genotype_i, prob_trans_genotype_l);

            double risk_trans = 0.0;

            // Only to be consistent with old simulation runs when set to false
            // Setting this option to true will only affect reporting
            if (opt_vaccine_genotype == false)
                risk_trans = avail * p_transmit * human.vaccine.get_factor(interventions::vaccines::TBV);
            else {
                for (size_t g = 0; g < num_genotypes(); ++g)
                    risk_trans += (prob_trans_genotype_i[g] + prob_trans_genotype_l[g]) *
                                  human.vaccine.get_factor(interventions::vaccines::TBV, g);
                risk_trans *= avail;
            }

            if (risk_trans > 0.0)
                ++num_transmitting_humans;

            sum_wt_kappa += risk_trans;
        }

        const size_t l_k_mod = Sim::modulo_steps(Sim::ts1(), static_cast<int>(lagged_kappa.size())); // now
        if (population.size() == 0) {                                                                // this is valid
            lagged_kappa[l_k_mod] = 0.0; // no humans: no infectiousness
        } else {
            if (!(sum_weight > DBL_MIN * 10.0)) { // if approx. eq. 0, negative or an NaN
                ostringstream msg;
                msg << "sumWeight is invalid: " << sum_weight << ", " << sum_wt_kappa << ", " << population.size();
                throw TRACED_EXCEPTION(msg.str(), util::Error::SumWeight);
            }
            lagged_kappa[l_k_mod] = sum_wt_kappa / sum_weight;
        }

        const size_t tmod = Sim::modulo_year_steps(Sim::ts0());

        // Calculate time-weighted average of kappa
        sum_annual_kappa += lagged_kappa[l_k_mod] * initialisation_eir[tmod];
        if (tmod == Sim::steps_per_year() - 1) {
            annual_average_kappa = sum_annual_kappa / annual_eir; // inf or NaN when annualEIR is 0
            sum_annual_kappa = 0.0;
        }

        return lagged_kappa[l_k_mod]; // kappa now
    }

    virtual void survey_eir() {
        ts_adult_eir = ts_adult_ento_inocs / ts_num_adults;
        ts_adult_ento_inocs = 0.0;

        ts_adult_eir_i = ts_adult_ento_inocs_i / ts_num_adults;
        ts_adult_ento_inocs_i = 0.0;

        ts_adult_eir_l = ts_adult_ento_inocs_l / ts_num_adults;
        ts_adult_ento_inocs_l = 0.0;

        ts_num_adults = 0;

        survey_input_eir += initialisation_eir[Sim::modulo_year_steps(Sim::ts0())];

        survey_simulated_eir += ts_adult_eir;
        survey_simulated_eir_i += ts_adult_eir_i;
        survey_simulated_eir_l += ts_adult_eir_l;
    }

protected:
    /** Calculates the EIR individuals are exposed to.
     *
     * Call once per time-step: updates ITNs in vector model.
     *
     * @param host Transmission data for the human to calculate EIR for.
     * @param ageGroupData Age group of this host for availablility data.
     * @param EIR Out-vector. Set to the age- and heterogeneity-specific EIR an
     *    individual human is exposed to, per parasite genotype, in units of
     *    inoculations per day. Length set by callee. 
     *    _i for imported infections and _l for local infections */
    virtual void calculate_eir(Host::Human& human, double age_years, vector<double>& genotype_eir_i,
                               vector<double>& genotype_eir_l) const = 0;

    virtual void checkpoint(istream& stream) {
        simulation_mode & stream;
        intervention_mode & stream;
        initialisation_eir & stream;
        lagged_kappa & stream;
        annual_eir & stream;
        annual_average_kappa & stream;
        sum_annual_kappa & stream;
        ts_adult_eir & stream;
        survey_input_eir & stream;
        survey_simulated_eir & stream;
        survey_simulated_eir_i & stream;
        survey_simulated_eir_l & stream;
        last_survey_time & stream;
        adult_age & stream;
        num_transmitting_humans & stream;
    }

    virtual void checkpoint(ostream& stream) {
        simulation_mode & stream;
        intervention_mode & stream;
        initialisation_eir & stream;
        lagged_kappa & stream;
        annual_eir & stream;
        annual_average_kappa & stream;
        sum_annual_kappa & stream;
        ts_adult_eir & stream;
        survey_input_eir & stream;
        survey_simulated_eir & stream;
        survey_simulated_eir_i & stream;
        survey_simulated_eir_l & stream;
        last_survey_time & stream;
        adult_age & stream;
        num_transmitting_humans & stream;
    }

private:
    // The times here should be for the last updated index of arrays:
    void cts_cb_input_eir(ostream& stream) {
        const int prev_step = (Sim::now() - Sim::one_ts()) / Sim::one_ts();
        // Note: prevStep may be negative, hence util::mod not mod_nn:
        stream << '\t' << initialisation_eir[util::mod(prev_step, Sim::steps_per_year())];
    }
    void cts_cb_simulated_eir(ostream& stream) { stream << '\t' << ts_adult_eir; }
    void cts_cb_kappa(ostream& stream) {
        // The latest time-step's kappa:
        stream << '\t' << lagged_kappa[Sim::modulo_steps(Sim::now(), lagged_kappa.size())];
    }
    void cts_cb_num_transmitting_humans(ostream& stream) { stream << '\t' << num_transmitting_humans; }

protected:
    size_t num_genotypes() const { return n_genotypes; }

private:
    size_t n_genotypes;

public:
    /** Entomological inoculation rate for adults during the
     * pre-intervention phase.
     *
     * Length: time-steps per year
     *
     * Index sim::now_mod_steps_per_year() corresponds to the EIR
     * acting on the current time-step: i.e. total inoculations since the
     * previous time-step.
     * Since time-step 0 is not calculated, initialisationEIR[0] is actually the
     * last value used (to calculate the state at the start of the second year).
     *
     * Units: infectious bites per adult per time step
     *
     * Not checkpointed; doesn't need to be except when a changeEIR intervention
     * occurs. */
    vector<double> initialisation_eir;

    /** The type of EIR calculation. Checkpointed. */
    int simulation_mode{ForcedEir};
    /** New simulation mode during intervention period. Not checkpointed. */
    int intervention_mode;

    /** The probability of infection of a mosquito at each bite.
     * It is calculated as the average infectiousness per human.
     *
     * The value in index sim::ts1().moduloSteps(initialKappa.size()) is the
     * kappa from this time step (i.e. the infectiousness of humans at the end of
     * this step). Length depends on entomological incubation period from
     * non-vector model.
     *
     * Checkpointed. */
    vector<double> lagged_kappa;

    /** Total annual infectious bites per adult.
     *
     * Checkpointed. */
    double annual_eir{0.0};

private:
    /*! annAvgKappa is the overall proportion of mosquitoes that get infected
     * allowing for the different densities in different seasons (approximating
     * relative mosquito density with the EIR).
     *
     * Checkpointed. */
    double annual_average_kappa;

    /*! Used to calculate annAvgKappa.
     *
     * Checkpointed. */
    double sum_annual_kappa{0.0};

    /// Adult-only EIR over the last update
    double ts_adult_eir{0.0}, ts_adult_eir_i{}, ts_adult_eir_l{};

    /** Per-time-step input EIR summed over inter-survey period.
     * Units: infectious bites/adult/inter-survey period. */
    double survey_input_eir{0.0}, survey_input_eir_i{}, survey_input_eir_l{};

    /** Per-time-step simulated EIR summed over inter-survey period.
     * Units: infectious bites/adult/inter-survey period. */
    double survey_simulated_eir{0.0}, survey_simulated_eir_i{}, survey_simulated_eir_l{};

    /** Time of last survey. */
    SimTime last_survey_time = Sim::never();

    /// age at which an individual is considered an adult
    SimTime adult_age = Sim::never();

    /// For "num transmitting humans" cts output.
    int num_transmitting_humans{0};

    // Reporting data. Doesn't need checkpointing due to reset every time-step.
    // accumulator for time step EIR of adults
    double ts_adult_ento_inocs = 0.0, ts_adult_ento_inocs_i = 0.0, ts_adult_ento_inocs_l = 0.0;
    int ts_num_adults = 0; // accumulator for time step adults requesting EIR

    bool opt_vaccine_genotype = false;
};

} // namespace Transmission
} // namespace OM
#endif
