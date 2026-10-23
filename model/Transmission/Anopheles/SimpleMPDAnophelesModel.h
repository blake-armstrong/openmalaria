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

#ifndef HMOD_SIMPLE_MPD_ANOPHELES_MODEL
#define HMOD_SIMPLE_MPD_ANOPHELES_MODEL

#include "Transmission/Anopheles/AnophelesModel.h"

namespace OM::Transmission::Anopheles {

class SimpleMPDAnophelesModel : public AnophelesModel {
public:
    ///@brief Initialisation and destruction
    //@{
    SimpleMPDAnophelesModel(SimTime development_duration, double prob_preadult_survival, double f_eggs_laid_by_oviposit)
        : development_duration(development_duration), prob_preadult_survival(prob_preadult_survival),
          f_eggs_laid_by_oviposit(f_eggs_laid_by_oviposit) {
        quinquennial_ovipositing.resize(Sim::from_years_i(5), 0.0);
        inv_larval_resources.resize(Sim::one_year(), 0.0);
        n_ovipositing_delayed.resize(development_duration, 0.0);
    }

    /** Initialisation which must wait until a human population is available.
     * This is only called when a checkpoint is not loaded.
     *
     * @param nHumans Human population size
     * @param meanPopAvail The mean availability of age-based relative
     * availability of humans to mosquitoes across populations.
     * @param sum_avail sum_i α_i * N_i for human hosts i
     * @param sigma_f sum_i α_i * N_i * P_Bi for human hosts i
     * @param sigma_df sum_i α_i * N_i * P_Bi * P_Ci * P_Di for human hosts i
     * @param sigma_dff sum_i α_i * N_i * P_Bi * P_Ci * P_Di * rel_mosq_fecundity for human hosts i
     *
     * Can only usefully run its calculations when not checkpointing, due to
     * population not being the same when loaded from a checkpoint. */
    void init2(int n_humans, double mean_pop_avail, double sum_avail, double sigma_f, double sigma_df,
               double sigma_dff) override {
        AnophelesModel::init2(n_humans, mean_pop_avail, sum_avail, sigma_f, sigma_df, sigma_dff);

        // Recompute tsp_dff locally
        double leave_rate = sum_avail + nhh_avail + mosq.seeking_death_rate;
        sigma_df += nhh_sigma_df;
        sigma_dff += nhh_sigma_dff;

        double ts_p_a = exp(-leave_rate * mosq.seeking_duration);
        double avail_divisor = (1.0 - ts_p_a) / leave_rate; // α_d
        double ts_p_dff = sigma_dff * avail_divisor * mosq.prob_mosq_survival_ovipositing;

        // Initialise nOvipositingDelayed
        int y1 = Sim::one_year();
        int tau = mosq.rest_duration;
        for (int t = 0; t < development_duration; t++) {
            n_ovipositing_delayed[util::mod_nn(t + tau, development_duration)] =
                ts_p_dff * init_nv_from_sv * forced_s_v[t];
        }

        // Used when calculating invLarvalResources (but not a hard constraint):
        assert(tau + development_duration <= y1);
        for (int t = 0; t < Sim::one_year(); t++) {
            double yt = f_eggs_laid_by_oviposit * ts_p_dff * init_nv_from_sv *
                        forced_s_v[util::mod_nn(t + y1 - tau - development_duration, y1)];
            inv_larval_resources[t] = (prob_preadult_survival * yt - mosq_emerge_rate[t]) / (mosq_emerge_rate[t] * yt);
        }
    }

    void scale(double factor) override {
        AnophelesModel::scale(factor);
        vectors::scale(n_ovipositing_delayed, factor);
    }

    /** Work out whether another interation is needed for initialisation and if
     * so, make necessary changes.
     *
     * @returns false if another iteration is needed. */
    bool init_iterate() override {
        bool fitted = AnophelesModel::init_iterate();

        int y1 = Sim::one_year(), y2 = Sim::from_years_i(2), y3 = Sim::from_years_i(3), y4 = Sim::from_years_i(4),
            y5 = Sim::from_years_i(5);

        assert((int)mosq_emerge_rate.size() == y1);

        for (int t = 0; t < y1; t++) {
            int ttj = t - development_duration;
            // b · P_df · avg_N_v(t - θj - τ):
            double yt = f_eggs_laid_by_oviposit * 0.2 *
                        (quinquennial_ovipositing[ttj + y1] + quinquennial_ovipositing[ttj + y2] +
                         quinquennial_ovipositing[ttj + y3] + quinquennial_ovipositing[ttj + y4] +
                         quinquennial_ovipositing[util::mod_nn(ttj + y5, y5)]);
            inv_larval_resources[t] = (prob_preadult_survival * yt - mosq_emerge_rate[t]) / (mosq_emerge_rate[t] * yt);
        }

        const bool resources_valid = std::ranges::none_of(inv_larval_resources, [](double v) { return v < 0.0; });
        return fitted && resources_valid;
    }
    //@}

    double get_emergence_rate(const SimTime& d0, const std::vector<double>& mosq_emerge_rate,
                              double n_ovipositing) override {
        // // Get emergence at start of step:
        // SimTime dYear1 = mod_nn(d0, sim::oneYear());
        // // Simple model: fixed emergence scaled by larviciding
        // return mosqEmergeRate[dYear1];

        // Simple Mosquito Population Dynamics model: emergence depends on the
        // adult population, resources available, and larviciding.
        // See: A Simple Periodically-Forced Difference Equation Model for
        // Mosquito Population Dynamics, N. Chitnis, 2012. TODO: publish & link.
        int d1 = d0 + 1;

        double yt = f_eggs_laid_by_oviposit * n_ovipositing_delayed[util::mod_nn(d1, development_duration)];
        double emergence = prob_preadult_survival * yt / (1.0 + inv_larval_resources[mod_nn(d0, Sim::one_year())] * yt);
        n_ovipositing_delayed[util::mod_nn(d1, development_duration)] = n_ovipositing;
        quinquennial_ovipositing[util::mod_nn(d1, Sim::from_years_i(5))] = n_ovipositing;

        if (Sim::interv_time() > 0 && emergence < 0) {
            std::ostringstream oss;
            oss << "Error: SimpleMPD model: negative emergence at t=" << d0 << ": " << emergence << ". ";
            oss << "Check the seasonality and make sure it does not go too low. For monthly EIR values, the lowest "
                   "point should not be below 1% of the peak EIR.";
            throw util::base_exception(oss.str());
        }

        return std::max(emergence, 0.0);
    }

    ///@brief Interventions and reporting
    //@{
    double get_res_availability() const override {
        // TODO: why offset by one time step? This is effectively getting the resources available on the last time step
        // TODO: only have to add one year because of offset
        SimTime start = Sim::now() - Sim::one_ts() + Sim::one_year();
        double total = 0;
        for (SimTime i = start, end = start + Sim::one_ts(); i < end; i = i + Sim::one_day()) {
            SimTime d_year1 = mod_nn(i, Sim::one_year());
            total += 1.0 / inv_larval_resources[d_year1];
        }
        return total / Sim::one_ts();
    }

    double get_res_requirements() const override { return numeric_limits<double>::quiet_NaN(); }

    void checkpoint(istream& stream) override { (*this) & stream; }
    void checkpoint(ostream& stream) override { (*this) & stream; }

private:
    template <class S> void operator&(S& stream) {
        AnophelesModel::checkpoint(stream);

        quinquennial_ovipositing & stream;
        development_duration & stream;
        prob_preadult_survival & stream;
        f_eggs_laid_by_oviposit & stream;
        inv_larval_resources & stream;
        n_ovipositing_delayed & stream;
    }

    // -----  model parameters (loaded from XML)  -----

    /** Duration of development (time from egg laying to emergence) in days. */
    SimTime development_duration = Sim::never();

    /** Survival probability of a mosquito from egg to emergence in the absence
     * of density dependent mortality. */
    double prob_preadult_survival;

    /** Mean number of female eggs laid when a mosquito oviposites. */
    double f_eggs_laid_by_oviposit;

    // -----  parameters (constant after initialisation)  -----

    /** As quinquennialS_v, but for N_v*P_df (units: animals). */
    std::vector<double> quinquennial_ovipositing;

    /** Resources for mosquito larvae (or rather 1 over resources); γ(t) in
     * model description.
     *
     * Unlike model description, we allow special values 0 for no density
     * dependence and infinity for zero emergence.
     *
     * Index t should correspond to the resources available to mosquitoes
     * emerging at t (i.e. same index in mosqEmergeRate).
     *
     * Has annual periodicity: length is 365. First value (index 0) corresponds
     * to first day of year (1st Jan or something else if rebased). In 5-day
     * time-step model values at indecies 0 through 4 are used to calculate the
     * state at time-step 1.
     *
     * Units: 1 / animals per day.
     *
     * vecDay be checkpointed. */
    std::vector<double> inv_larval_resources;

    /** Vector for storing values of nOvipositing for the last
     * developmentDuration time steps. Index 0 should correspond to
     * nOvipositing developmentDuration days before
     * get(0, dYear1, nOvipositing) is called. */
    std::vector<double> n_ovipositing_delayed;
};

} // namespace OM::Transmission::Anopheles

#endif
