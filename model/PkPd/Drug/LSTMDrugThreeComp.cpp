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

#include "PkPd/Drug/LSTMDrugThreeComp.h"
#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/errors.h"
#include "util/StreamValidator.h"

#include <gsl/gsl_integration.h>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <numbers>

using namespace std;

namespace OM::pk_pd {

LSTMDrugThreeComp::LSTMDrugThreeComp(const LSTMDrugType& type, LocalRng& rng)
    : LSTMDrug(type.sample_vd(rng)), type_data(type), elim_sample(type.sample_elim_rate(rng)),
      k12(type.sample_k12(rng)), k21(type.sample_k21(rng)), k13(type.sample_k13(rng)), k31(type.sample_k31(rng)),
      nka(-type.sample_ka(rng)), last_bm(numeric_limits<double>::quiet_NaN()), na(numeric_limits<double>::quiet_NaN()),
      nb(numeric_limits<double>::quiet_NaN()), ng(numeric_limits<double>::quiet_NaN()),
      a(numeric_limits<double>::quiet_NaN()), b(numeric_limits<double>::quiet_NaN()),
      c(numeric_limits<double>::quiet_NaN()) {
    // These are from the Monolix article, pp38-39.
}

size_t LSTMDrugThreeComp::get_index() const { return type_data.get_index(); }
double LSTMDrugThreeComp::get_concentration(size_t index) const {
    if (index == type_data.get_index())
        return conc();
    else
        return 0.0;
}

void LSTMDrugThreeComp::update_cached(double body_mass) const {
    if (last_bm == body_mass)
        return;

    const double k = elim_sample * pow(body_mass, type_data.neg_m_exponent());

    const double a0 = k * k21 * k31;
    const double a1 = k * k31 + k21 * k31 + k21 * k13 + k * k21 + k31 * k12;
    const double a2 = k + k12 + k13 + k21 + k31;

    const double third = 1.0 / 3.0;
    const double p = a1 - third * a2 * a2;
    const double q = (2.0 / 27.0) * a2 * a2 * a2 - third * a1 * a2 + a0;

    const double at = third * a2;
    const double rt = -third * p;
    const double r2 = 2.0 * sqrt(rt);

    const double phi = acos(-q / (rt * r2)) * third;
    const double pi23 = (2.0 / 3.0) * std::numbers::pi;

    // negative alpha, beta, gamma:
    na = r2 * cos(phi) - at;
    nb = r2 * cos(phi + pi23) - at;
    ng = r2 * cos(phi + 2.0 * pi23) - at;

    // A, B, C from Monolix 1.3.3 (p44):
    const double ka_ov = -nka / (vol_dist * body_mass);
    a = ka_ov * (k21 + na) * (k31 + na) / ((na - nka) * (nb - na) * (ng - na));
    b = ka_ov * (k21 + nb) * (k31 + nb) / ((nb - nka) * (na - nb) * (ng - nb));
    c = ka_ov * (k21 + ng) * (k31 + ng) / ((ng - nka) * (nb - ng) * (na - ng));

    last_bm = body_mass;
}

/// Parameters for func_fC
struct ParamsFC {
    double c_a, c_b, c_c, c_abc; // concentration parameters
    double na, nb, ng, nka;      // decay parameters
    double n;                    // slope: unitless
    double v;                    // max killing rate: unitless
    double kn;                   // IC50^n: (mg/kg) ^ n
};
/** Function for calculating concentration and then killing function at time t
 * 
 * @param t The variable being integrated over (in this case, time since start
 *      of day or last dose, units days)
 * @param pp Pointer to a Params_fC struct
 * @return killing rate (unitless)
 */
double func_f_c(double t, void* pp) {
    const ParamsFC& p = *static_cast<const ParamsFC*>(pp);

    // exponential decay of drug concentration:
    const double conc_a = p.c_a * exp(p.na * t);
    const double conc_b = p.c_b * exp(p.nb * t);
    const double conc_c = p.c_c * exp(p.ng * t);
    const double conc_abc = p.c_abc * exp(p.nka * t);
    const double conc = conc_a + conc_b + conc_c - conc_abc; // mg/l

    const double cn = pow(conc, p.n);          // (mg/l) ^ n
    const double f_c = p.v * cn / (cn + p.kn); // unitless
    return f_c;
}
const size_t GslIntgMaxIter = 1000; // 10 seems enough, but no harm in using a higher value

namespace {
gsl_integration_workspace* integration_workspace() {
    thread_local std::unique_ptr<gsl_integration_workspace, decltype(&gsl_integration_workspace_free)> workspace(
        gsl_integration_workspace_alloc(GslIntgMaxIter), &gsl_integration_workspace_free);
    return workspace.get();
}
}

double LSTMDrugThreeComp::calculate_factor(const ParamsFC& p, double duration) const {
    gsl_function f;
    f.function = &func_f_c;
    // gsl_function doesn't accept const; we re-apply const later
    f.params = static_cast<void*>(const_cast<ParamsFC*>(&p));

    // NOTE: tolerances are arbitrary, but seem to be sufficient
    const double abs_eps = 1e-4, rel_eps = 1e-4;
    // NOTE: 1 through 6 are different algorithms of increasing complexity
    const int qag_rule = 1; // alg 1 seems to be good enough
    double intf_c = NAN, err_eps = NAN;

    const int r = gsl_integration_qag(&f, 0.0, duration, abs_eps, rel_eps, GslIntgMaxIter, qag_rule,
                                      integration_workspace(), &intf_c, &err_eps);
    if (r != 0) {
        throw TRACED_EXCEPTION("calculateFactor: error from gsl_integration_qag", util::Error::GSL);
    }
    if (err_eps > 5e-2) {
        // This could be a warning, except that warnings tend to be ignored.
        throw TRACED_EXCEPTION(
            std::format("calculateFactor: error epsilon is large: {:g} (integral is {:g})", err_eps, intf_c),
            util::Error::GSL);
    }

    return 1.0 / exp(intf_c); // drug factor
}

// TODO: in high transmission, is this going to get called more often than updateConcentration?
// When does it make sense to try to optimise (avoid doing decay calcuations here)?
double LSTMDrugThreeComp::calculate_drug_factor(LocalRng& rng, WithinHost::CommonInfection* inf,
                                                double body_mass) const {
    if (conc() == 0.0 && doses.size() == 0)
        return 1.0; // nothing to do
    update_cached(body_mass);

    ParamsFC p{};
    p.c_a = conc_a;
    p.c_b = conc_b;
    p.c_c = conc_c;
    p.c_abc = conc_abc;
    p.na = na;
    p.nb = nb;
    p.ng = ng;
    p.nka = nka;
    const LSTMDrugPD& pd = type_data.get_pd(inf->genotype());
    p.n = pd.slope();
    p.v = pd.max_killing_rate();
    p.kn = pd.ic50_pow_slope(rng, type_data.get_index(), inf);

    double time = 0.0;         // time since start of day
    double total_factor = 1.0; // survival factor for whole day

    using TimeConc = pair<double, double>;
    for (const TimeConc& time_conc : doses) {
        // we iteratate through doses in time order (since doses are sorted)
        if (time_conc.first < 1.0 /*i.e. today*/) {
            if (time < time_conc.first) {
                const double duration = time_conc.first - time;
                total_factor *= calculate_factor(p, duration);
                p.c_a *= exp(p.na * duration);
                p.c_b *= exp(p.nb * duration);
                p.c_c *= exp(p.ng * duration);
                p.c_abc *= exp(p.nka * duration);
                time = time_conc.first;
            } else {
                assert(time == time_conc.first);
            }
            // add dose:
            p.c_a += a * time_conc.second;
            p.c_b += b * time_conc.second;
            p.c_c += c * time_conc.second;
            p.c_abc += (a + b + c) * time_conc.second;
        } else /*i.e. tomorrow or later*/ {
            // ignore
        }
    }
    if (time < 1.0) {
        total_factor *= calculate_factor(p, 1.0 - time);
    }

    return total_factor;
}

void LSTMDrugThreeComp::update_concentration(double body_mass) {
    if (conc() == 0.0 && doses.size() == 0)
        return; // nothing to do
    update_cached(body_mass);

    // exponential decay of existing quantities:
    //TODO(performance): is it faster to pre-calculate these and either store extra
    // parameters or adapt uses of alpha, beta, gamma, etc. below?
    conc_a *= exp(na);
    conc_b *= exp(nb);
    conc_c *= exp(ng);
    conc_abc *= exp(nka);

    size_t doses_taken = 0;
    using TimeConc = pair<double, double>;
    for (TimeConc& time_conc : doses) {
        // we iteratate through doses in time order (since doses are sorted)
        if (time_conc.first < 1.0 /*i.e. today*/) {
            // add dose:
            const double qty = time_conc.second;
            conc_a += a * qty * exp(na * (1.0 - time_conc.first));
            conc_b += b * qty * exp(nb * (1.0 - time_conc.first));
            conc_c += c * qty * exp(ng * (1.0 - time_conc.first));
            conc_abc += (a + b + c) * qty * exp(nka * (1.0 - time_conc.first));
            doses_taken += 1;
        } else /*i.e. tomorrow or later*/ {
            time_conc.first -= 1.0;
        }
    }
    //NOTE: would be faster if elements were stored in reverse order — though prescribing would probably be slower
    doses.erase(doses.begin(), doses.begin() + static_cast<ptrdiff_t>(doses_taken));

    util::streamValidate(conc());
    if (conc() < type_data.get_negligible_concentration()) {
        // once negligible, try to optimise so that we don't have to do
        // anything next time step
        conc_a = 0.0;
        conc_b = 0.0;
        conc_c = 0.0;
        conc_abc = 0.0;
    }
}

void LSTMDrugThreeComp::checkpoint(ostream& stream) {
    conc_a & stream;
    conc_b & stream;
    conc_c & stream;
    conc_abc & stream;
    elim_sample & stream;
    k12 & stream;
    k21 & stream;
    k13 & stream;
    k31 & stream;
    nka & stream;
    last_bm & stream;
    na & stream;
    nb & stream;
    ng & stream;
    a & stream;
    b & stream;
    c & stream;
}
void LSTMDrugThreeComp::checkpoint(istream& stream) {
    conc_a & stream;
    conc_b & stream;
    conc_c & stream;
    conc_abc & stream;
    elim_sample & stream;
    k12 & stream;
    k21 & stream;
    k13 & stream;
    k31 & stream;
    nka & stream;
    last_bm & stream;
    na & stream;
    nb & stream;
    ng & stream;
    a & stream;
    b & stream;
    c & stream;
}

}
