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

#include "PkPd/Drug/LSTMDrugConversion.h"
#include "Host/WithinHost/Infection/CommonInfection.h"
#include "util/errors.h"
#include "util/StreamValidator.h"
#include "util/vectors.h"

#include <gsl/gsl_integration.h>
#include <cmath>
#include <limits>

using namespace std;

namespace OM::pk_pd {

LSTMDrugConversion::LSTMDrugConversion(const LSTMDrugType& parent, const LSTMDrugType& metabolite, LocalRng& rng)
    : LSTMDrug(parent.sample_vd(rng)), parent_type(parent), metabolite_type(metabolite), nka(-parent.sample_ka(rng)),
      nk_p_sample(-parent.sample_elim_rate(rng)), nconv_sample(-parent.sample_conv_rate(rng)),
      nk_m_sample(-metabolite.sample_elim_rate(rng)), vol_dist_metabolite(metabolite.sample_vd(rng)) {}

//LSTMDrugConversion::~LSTMDrugConversion(){}

size_t LSTMDrugConversion::get_index() const {
    return parent_type.get_index(); // parent drug's index should work
}
double LSTMDrugConversion::get_concentration(size_t index) const {
    if (index == parent_type.get_index()) {
        return get_parent_concentration();
    } else if (index == metabolite_type.get_index()) {
        return get_metabolite_concentration();
    } else
        return 0.0;
}
double LSTMDrugConversion::get_parent_concentration() const { return qty_p / (vol_dist * last_bm); }
double LSTMDrugConversion::get_metabolite_concentration() const { return qty_m / (vol_dist_metabolite * last_bm); }

/// Parameters for func_convFactor and LSTMDrugConversion::calculateFactor
struct ParamsConvFactor {
    // Quantities of parent in gut, parent in circulation and metabolite in
    // circulation, in mg (A, B, C in paper):
    double qty_g, qty_p, qty_m;
    // nka = -x (negative of absorption rate)
    // nkM = -k
    // nl = -(y + z)
    double nka, nk_m, nl;
    // terms involving x, y, z and the molecular weight ratio:
    double f, g, h, i, j;

    double inv_vd_p, inv_vd_m; // 1.0 / (Vd * body mass) for parent and metabolite; units: 1/l
    double n_p, n_m;           // slope: unitless
    double vp, vm;             // max killing rate: unitless
    double kn_p, kn_m;         // IC50^n: (mg/kg) ^ n
};

double calculate_parent_quantity(struct ParamsConvFactor p, double exp_absorb, double exp_p_loss) {
    return p.f * p.qty_g * exp_absorb + (p.qty_p - p.f * p.qty_g) * exp_p_loss;
}

double calculate_parent_drug_factor(struct ParamsConvFactor p, double exp_absorb, double exp_p_loss) {
    const double qty_p = calculate_parent_quantity(p, exp_absorb, exp_p_loss);
    const double c_p = qty_p * p.inv_vd_p;             // concentrations; mg/l*/
    const double cn_p = pow(c_p, p.n_p);               // (mg/l) ^ n
    const double f_cp = p.vp * cn_p / (cn_p + p.kn_p); // unitless
    return f_cp;
}

double calculate_metabolite_quantity(struct ParamsConvFactor p, double exp_absorb, double exp_p_loss, double t) {
    return p.g * p.qty_g * exp_absorb + (p.h * p.qty_g - p.i * p.qty_p) * exp_p_loss +
           (p.j * p.qty_g + p.i * p.qty_p + p.qty_m) * exp(p.nk_m * t);
}

double calculate_metabolite_drug_factor(struct ParamsConvFactor p, double exp_absorb, double exp_p_loss, double t) {
    const double qty_m = calculate_metabolite_quantity(p, exp_absorb, exp_p_loss, t);
    const double c_m = qty_m * p.inv_vd_m;             // concentrations; mg/l
    const double cn_m = pow(c_m, p.n_m);               // (mg/l) ^ n
    const double f_cm = p.vm * cn_m / (cn_m + p.kn_m); // unitless
    return f_cm;
}

// size_t intg_steps = 0;
/** Function for calculating concentration and then killing function at time t
 * 
 * @param t The variable being integrated over (in this case, time since start
 *      of day or last dose, units days)
 * @param pp Pointer to a Params_convFactor struct
 * @return killing rate (unitless)
 */
double func_conv_factor(double t, void* pp) {
    //     intg_steps += 1;
    const ParamsConvFactor& p = *static_cast<const ParamsConvFactor*>(pp);
    const double exp_absorb = exp(p.nka * t), exp_p_loss = exp(p.nl * t);
    const double f_cp = calculate_parent_drug_factor(p, exp_absorb, exp_p_loss);
    const double f_cm = calculate_metabolite_drug_factor(p, exp_absorb, exp_p_loss, t);
    // use the most effective killing factor (from area under the drug kill curve), which is the one with the bigger number
    return max(f_cp, f_cm);
}

const size_t GslIntgConvMaxIter = 1000; // 10 seems enough, but no harm in using a higher value
gsl_integration_workspace* gsl_intgr_conv_wksp = gsl_integration_workspace_alloc(GslIntgConvMaxIter);
//NOTE: we "should" free, but mem-leaks at end of program aren't really important
// gsl_integration_workspace_free (gsl_intgr_conv_wksp);
double LSTMDrugConversion::calculate_factor(const ParamsConvFactor& p, double duration) const {
    gsl_function f;
    f.function = &func_conv_factor;
    // gsl_function doesn't accept const; we re-apply const later
    f.params = static_cast<void*>(const_cast<ParamsConvFactor*>(&p));

    // We use exp(-result), so small absolute differences can matter (but also
    // using smaller abs_eps is cheap). We likely don't need high rel precision.
    const double abs_eps = 1e-5, rel_eps = 1e-2;
    // NOTE: 1 through 6 are different algorithms of increasing complexity
    const int qag_rule = 1;             // alg 1 seems to be good enough
    double intf_c = NAN, err_eps = NAN; // intfC will carry our result; err_eps is a measure of accuracy of the result

    //     intg_steps = 0;
    const int r = gsl_integration_qag(&f, 0.0, duration, abs_eps, rel_eps, GslIntgConvMaxIter, qag_rule,
                                      gsl_intgr_conv_wksp, &intf_c, &err_eps);
    if (r != 0) {
        throw TRACED_EXCEPTION("calculateFactor: error from gsl_integration_qag", util::Error::GSL);
    }
    // Testing err_eps is redundant with GSL's built-in tests
    //     cout << "integration steps: " << intg_steps << endl;
    //     cout << "duration: " << duration << ", AUC: " << intfC << endl;
    return exp(-intf_c); // drug factor
}

ParamsConvFactor LSTMDrugConversion::conversion_parameters(double body_mass) const {
    ParamsConvFactor p{};
    p.qty_g = qty_g;
    p.qty_p = qty_p;
    p.qty_m = qty_m;

    // decay "constants" (dependent on body mass):
    const double nk_p = nk_p_sample * pow(body_mass, parent_type.neg_m_exponent());   // -y
    const double nconv = nconv_sample * pow(body_mass, parent_type.neg_m_exponent()); // -z
    p.nka = nka;                                                                      // -x
    p.nk_m = nk_m_sample * pow(body_mass, metabolite_type.neg_m_exponent());          // -k
    p.nl = nk_p + nconv;                                                              // -(y + z)

    p.f = nka / (p.nl - nka); // -x / (x-y-z) = x / (y+z-x)
    // Let u = parentType.molecular_weight_ratio() = M_C / M_B:
    const double rz = parent_type.molecular_weight_ratio() * nconv; // -u*z
    p.g = rz * nka / ((nka - p.nl) * (nka - p.nk_m));               // u*z*x / ((y+z-x) * (k-x))
    p.h = rz * nka / ((nka - p.nl) * (p.nk_m - p.nl));              // u*z*x / ((y+z-x) * (y+z-k))
    p.i = rz / (p.nl - p.nk_m);                                     // -u*z / (k - y - z) = u*z / (y+z-k)
    p.j = rz * nka / ((p.nk_m - p.nl) * (p.nk_m - nka));            // u*z*x / ((y+z-k) * (x-k))

    p.inv_vd_p = 1.0 / (vol_dist * body_mass);
    p.inv_vd_m = 1.0 / (vol_dist_metabolite * body_mass);
    return p;
}

void LSTMDrugConversion::set_killing_parameters(LocalRng& rng, ParamsConvFactor& p,
                                                WithinHost::CommonInfection* inf) const {
    const uint32_t genotype = inf->genotype();
    const LSTMDrugPD &pd_p = parent_type.get_pd(genotype), &pd_m = metabolite_type.get_pd(genotype);
    p.n_p = pd_p.slope();
    p.vp = pd_p.max_killing_rate();
    p.n_m = pd_m.slope();
    p.vm = pd_m.max_killing_rate();

    // Use custom code here because we need to handle covariance
    auto p_index = parent_type.get_index();
    if (inf->kn.count(p_index) != 0) {
        // Read cached values: IC50 ^ n
        p.kn_p = inf->kn.at(p_index);
        p.kn_m = inf->kn.at(metabolite_type.get_index());
    } else {
        // First usage for this infection / treatment: sample, optionally with correlation.
        auto zscore = NormalSample::generate(rng);
        p.kn_p = pd_p.ic50_pow_slope(zscore);
        inf->kn[p_index] = p.kn_p;

        auto metab_zscore = parent_type.ic50_correlated_sample(zscore, rng);
        p.kn_m = pd_m.ic50_pow_slope(metab_zscore);
        inf->kn[metabolite_type.get_index()] = p.kn_m;
    }
}

// TODO: in high transmission, is this going to get called more often than updateConcentration?
// When does it make sense to try to optimise (avoid doing decay calcuations here)?
double LSTMDrugConversion::calculate_drug_factor(LocalRng& rng, WithinHost::CommonInfection* inf,
                                                 double body_mass) const {
    if (qty_g == 0.0 && qty_p == 0.0 && qty_m == 0.0 && doses.size() == 0) {
        return 1.0; // nothing to do
    }

    ParamsConvFactor p = conversion_parameters(body_mass);
    set_killing_parameters(rng, p, inf);

    double time = 0.0;         // time since start of day
    double total_factor = 1.0; // survival factor for whole day

    using TimeConc = pair<double, double>;
    for (const TimeConc& time_conc : doses) {
        // we iterate through doses in time order (since doses are sorted)
        if (time_conc.first < 1.0 /*i.e. today*/) {
            if (time < time_conc.first) {
                const double duration = time_conc.first - time;
                total_factor *= calculate_factor(p, duration);
                const double exp_absorb = exp(nka * duration), exp_p_loss = exp(p.nl * duration);
                p.qty_m = calculate_metabolite_quantity(p, exp_absorb, exp_p_loss, duration);
                p.qty_p = calculate_parent_quantity(p, exp_absorb, exp_p_loss);
                p.qty_g *= exp_absorb;
                time = time_conc.first;
            } else {
                assert(time == time_conc.first);
            }
            // add to quantity of drug in gut:
            p.qty_g += time_conc.second; // units: mg
        } else /*i.e. tomorrow or later*/ {
            // ignore
        }
    }
    if (time < 1.0) {
        total_factor *= calculate_factor(p, 1.0 - time);
    }

    return total_factor;
}

void LSTMDrugConversion::update_concentration(double body_mass) {
    if (qty_g == 0.0 && qty_p == 0.0 && qty_m == 0.0 && doses.size() == 0) {
        return; // nothing to do
    }
    last_bm = body_mass;

    ParamsConvFactor p = conversion_parameters(body_mass);

    double time = 0.0;
    size_t doses_taken = 0;
    using TimeConc = pair<double, double>;
    for (TimeConc& time_conc : doses) {
        // we iteratate through doses in time order (since doses are sorted)
        if (time_conc.first < 1.0 /*i.e. today*/) {
            const double duration = time_conc.first - time;
            if (duration > 0.0) {
                const double exp_absorb = exp(nka * duration), exp_p_loss = exp(p.nl * duration);
                p.qty_m = calculate_metabolite_quantity(p, exp_absorb, exp_p_loss, duration);
                p.qty_p = calculate_parent_quantity(p, exp_absorb, exp_p_loss);
                p.qty_g *= exp_absorb;
                time = time_conc.first;
            } else {
                assert(time == time_conc.first);
            }
            // add to quantity of drug in gut:
            p.qty_g += time_conc.second; // units: mg
            doses_taken += 1;
        } else /*i.e. tomorrow or later*/ {
            time_conc.first -= 1.0;
        }
    }
    if (time < 1.0) {
        const double duration = 1.0 - time;
        const double exp_absorb = exp(nka * duration), exp_p_loss = exp(p.nl * duration);
        p.qty_m = calculate_metabolite_quantity(p, exp_absorb, exp_p_loss, duration);
        p.qty_p = calculate_parent_quantity(p, exp_absorb, exp_p_loss);
        p.qty_g *= exp_absorb;
    }
    //NOTE: would be faster if elements were stored in reverse order — though prescribing would probably be slower
    doses.erase(doses.begin(), doses.begin() + doses_taken);

    qty_m = p.qty_m;
    qty_p = p.qty_p;
    qty_g = p.qty_g;

    util::streamValidate(qty_m);
    if (qty_p < parent_type.get_negligible_concentration() * body_mass * vol_dist &&
        qty_m < metabolite_type.get_negligible_concentration() * body_mass * vol_dist_metabolite) {
        // once negligible, try to optimise so that we don't have to do
        // anything next time step
        qty_g = qty_p = qty_m = 0.0;
    }
}

void LSTMDrugConversion::checkpoint(ostream& stream) {
    qty_g & stream;
    qty_p & stream;
    qty_m & stream;
    nka & stream;
    nk_p_sample & stream;
    nconv_sample & stream;
    nk_m_sample & stream;
}
void LSTMDrugConversion::checkpoint(istream& stream) {
    qty_g & stream;
    qty_p & stream;
    qty_m & stream;
    nka & stream;
    nk_p_sample & stream;
    nconv_sample & stream;
    nk_m_sample & stream;
}

}
