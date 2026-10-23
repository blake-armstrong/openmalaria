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

#ifndef HMOD_EMERGENCE_RATE_ESTIMATOR
#define HMOD_EMERGENCE_RATE_ESTIMATOR

#include "Global.h"
#include "Transmission/Anopheles/AnophelesModel.h"
#include "Transmission/PerHost.h"
#include "util/SimpleDecayingValue.h"
#include "util/vectors.h"
#include "util/CommandLine.h"
#include "util/errors.h"

#include "CalcNvO.h"

#include <vector>
#include <limits>
#include <numbers>

namespace OM::Transmission::Anopheles {
using std::numeric_limits;
using namespace OM::util;

inline int argmax(const std::vector<double>& vec) {
    int imax = 0;
    double max = 0.0;
    for (size_t i = 0; i < vec.size(); i++) {
        double v = vec[i];
        if (v >= max) {
            max = v;
            imax = i;
        }
    }
    return imax;
}

inline double find_angle(const double eir_rotate_angle, const std::vector<double>& fs_coeffic,
                         const std::vector<double>& sim) {
    std::vector<double> temp(sim.size(), 0.0);
    const auto squared_distance = [&](double angle) {
        vectors::expIDFT(temp, fs_coeffic, eir_rotate_angle + angle);
        double sum = 0.0;
        for (int i = 0; i < Sim::one_year(); i++) {
            const double difference = temp[i] - sim[i];
            sum += difference * difference;
        }
        return sum;
    };

    constexpr int AngleSteps = 365;
    double best_distance = squared_distance(0.0);
    double best_angle = 0.0;
    for (int step = 0; step < AngleSteps; ++step) {
        const double angle = -std::numbers::pi + (2.0 * std::numbers::pi * step / AngleSteps);
        const double distance = squared_distance(angle);
        if (distance < best_distance) {
            best_distance = distance;
            best_angle = angle;
        }
    }
    return best_angle;
}

class EmergenceRateEstimator {
public:
    virtual ~EmergenceRateEstimator() = default;

    // Interface: returns true if further fitting iterations are needed.
    // EmergenceRateSolver returns false because it computes the exact solution in one step.
    virtual bool estimate(AnophelesModel& m, const std::vector<double>& lagged_kappa, double mean_avail) = 0;
};

class EmergenceRateAdaptiveFitter final : public EmergenceRateEstimator {
public:
    EmergenceRateAdaptiveFitter(AnophelesModel& m)
        : shift_angle(m.eir_rotate_angle - (m.mosq.eip_duration + 10) / 365. * 2. * M_PI) {
        // usually around 20 days; no real analysis for effect of changing EIPDuration or mosqRestDuration
    }

    bool estimate(AnophelesModel& m, const std::vector<double>& lagged_kappa, double mean_avail) override {
        std::vector<double> avg_annual_s_v(Sim::one_year(), 0.0);
        for (SimTime i = Sim::zero(); i < Sim::from_years_i(5); i = i + Sim::one_day()) {
            avg_annual_s_v[mod_nn(i, Sim::one_year())] += m.quinquennial_s_v[i] / 5.0;
        }

        double factor = vectors::sum(m.forced_s_v) / vectors::sum(avg_annual_s_v);

        // cout << "check: " << vectors::sum(forcedS_v) << " " << vectors::sum(avgAnnualS_v) << endl;
        // cout << "Pre-calced Sv, dynamic Sv:\t"<<sumAnnualForcedS_v<<'\t'<<vectors::sum(annualS_v)<<endl;
        if (!(factor > 1e-6 && factor < 1e6)) {
            if (factor > 1e6 && vectors::sum(m.quinquennial_s_v) < 1e-3) {
                throw util::base_exception("Simulated S_v is approx 0 (i.e.\
     mosquitoes are not infectious, before interventions). Simulator cannot handle this; perhaps\
     increase EIR or change the entomology model.",
                                           util::Error::VectorFitting);
            }
            if (vectors::sum(m.forced_s_v) == 0.0) {
                return false; // no EIR desired: nothing to do
            }
            cerr << "Input S_v for this vector:\t" << vectors::sum(m.forced_s_v) << endl;
            cerr << "Simulated S_v:\t\t\t" << vectors::sum(m.quinquennial_s_v) / 5.0 << endl;
            throw TRACED_EXCEPTION("factor out of bounds", util::Error::VectorFitting);
        }

        const double limit = 0.1;

        if (fabs(factor - 1.0) > limit) {
            scaled = false;
            double factor_diff = (scale_factor * factor - scale_factor) * 1.0;
            scale_factor += factor_diff;
        } else
            scaled = true;

        constexpr double RotationLimit = 0.2;

        double rotation = find_angle(m.eir_rotate_angle, m.fs_coeffic, avg_annual_s_v);
        shift_angle += rotation;
        rotated = std::fabs(rotation) <= RotationLimit;

        // Compute forced_sv from the Fourrier Coeffs EIR
        vectors::expIDFT(m.mosq_emerge_rate, m.fs_coeffic, -shift_angle);

        // Scale the vector according to initSvFromEIR and initNv0FromSv to get the mosqEmergerate
        // scaleFactor scales the vector to correct the ratio between simulated and input EIR
        vectors::scale(m.mosq_emerge_rate, scale_factor * m.init_sv_from_eir * m.init_nv0_from_sv);

        // initNvFromSv *= scaleFactor;     //(not currently used)

        // What factor exactly these should be scaled by isn't obvious; in any case
        // they should reach stable values quickly.
        m.scale(factor);

        return !(scaled && rotated);
    }

private:
    double scale_factor{1.0}, shift_angle;
    bool rotated{false}, scaled{false};
};

class EmergenceRateSolver final : public EmergenceRateEstimator {
public:
    EmergenceRateSolver(AnophelesModel& m, int population_size) : population_size(population_size) {}

    bool estimate(AnophelesModel& m, const std::vector<double>& lagged_kappa, double mean_avail) override {
        std::vector<double>& mosq_emerge_rate_vector = m.mosq_emerge_rate;
        int thetap = Sim::DaysInYear;
        int tau = m.mosq.rest_duration;
        int thetas = m.mosq.eip_duration;
        std::vector<double> ni{static_cast<double>(population_size)};
        std::vector<double> alphai{m.init_avail * mean_avail};
        double muv_a = m.mosq.seeking_death_rate;
        double thetad = m.mosq.seeking_duration;
        std::vector<double> p_bi{m.mosq.prob_biting};
        std::vector<double> p_ci{m.mosq.prob_find_rest_site};
        std::vector<double> p_di{m.mosq.prob_resting};
        std::vector<double> p_ei{m.mosq.prob_ovipositing};
        std::vector<double>& sv_init = m.forced_s_v;
        std::vector<double> nv_oguess(m.mosq_emerge_rate.begin(), m.mosq_emerge_rate.end());

        // Append active NHH host types (malaria-non-susceptible)
        for (const auto& [name, nhh] : m.nhh_instances) {
            // Interpret nhh.avail_i as total availability α_i * N_i
            ni.push_back(1.0);
            alphai.push_back(nhh.avail_i);

            p_bi.push_back(nhh.p_b_i);
            p_ci.push_back(nhh.p_c_i);
            p_di.push_back(nhh.p_d_i);
            p_ei.push_back(m.mosq.prob_ovipositing); // Using the same probability for nhh
        }

        int n_host = static_cast<int>(ni.size());
        int n_mal_host = 1; // only humans are malaria-susceptible here

        std::vector<double> human_kappa(thetap, 0.0);

        // Linear interpolation 73 -> daily (5-day blocks)
        for (size_t i = 0; i < lagged_kappa.size(); ++i) {
            const size_t start = i * 5;
            const size_t end = std::min(start + 5, static_cast<size_t>(thetap));

            const double v0 = lagged_kappa[i];
            const double v1 = lagged_kappa[(i + 1) % lagged_kappa.size()];

            for (size_t d = start; d < end; ++d) {
                const double t = double(d - start) / 5.0;
                human_kappa[d] = (1.0 - t) * v0 + t * v1;
            }
        }

        std::vector<double> kvi_init(static_cast<size_t>(thetap) * n_host, 0.0);

        // Fill all malaria-susceptible human groups with the humanKappa series.
        // If later each group has its own kappa, fill each column separately.
        for (int i = 0; i < n_mal_host; ++i) {
            for (int k = 0; k < thetap; ++k) {
                kvi_init[static_cast<size_t>(k) * n_host + i] = human_kappa[k];
            }
        }

        calc_init_mosq_emerge_rate(mosq_emerge_rate_vector,
                                   thetap,     // $\theta_p$: daysInYear
                                   tau,        // $\tau$: mosqRestDuration
                                   thetas,     // $\theta_s$: EIPDuration
                                   n_host,     // $n$: nHostTypes
                                   n_mal_host, // $m$: nMalHostTypes
                                   ni,         // $N_i$: popSize (length n)
                                   alphai,     // $\alpha_i$: hostAvailabilityRate	(length n)
                                   muv_a,      // $\mu_{vA}: mosqSeekingDeathRate
                                   thetad,     // $\theta_d$: mosqSeekingDuration
                                   p_bi,       // $P_{B_i}$: mosqProbBiting (length n)
                                   p_ci,       // $P_{C_i}$: mosqProbFindRestSite (length n)
                                   p_di,       // $P_{D_i}$: mosqProbResting (length n)
                                   p_ei,       // $P_{E_i}$: mosqProbOvipositing
                                   kvi_init,   // Kvi (size n * thetap)
                                   sv_init     // Sv (length n)
        );

        return false;
    }

private:
    int population_size;
};

}

#endif
