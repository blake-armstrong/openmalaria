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

#include "PopulationAgeStructure.h"
#include "Global.h"
#include "util/errors.h"
#include "schema/demography.h"

#include <cmath>
#include <gsl_vector_double.h>
#include <gsl/gsl_multimin.h>
#include <fstream>

namespace OM {
using namespace OM::util;

namespace {

struct DemographyFit {
    explicit DemographyFit(const scnXml::Demography& demography);

    double set_demo_parameters(double param1, double param2);
    void calc_cum_age_prop(vector<double>& cum_age_prop) const;

    vector<double> age_group_bounds;
    vector<double> age_group_percent;
    double mu0 = 0.0;
    double mu1 = 0.0;
    double alpha0 = 0.0;
    double alpha1 = 0.0;
    static constexpr double Rho = 0.0;
};

double w_calc_rss(const gsl_vector* v, void* params) {
    double x = NAN, y = NAN;
    x = gsl_vector_get(v, 0);
    y = gsl_vector_get(v, 1);

    return static_cast<DemographyFit*>(params)->set_demo_parameters(x, y);
}

void minimize_calc_rss(DemographyFit& fit, double param1, double param2) {
    gsl_vector* step_size = gsl_vector_alloc(2);
    gsl_vector_set_all(step_size, 0.1);

    gsl_vector* initial_values = gsl_vector_alloc(2);
    gsl_vector_set(initial_values, 0, param1);
    gsl_vector_set(initial_values, 1, param2);

    gsl_multimin_function minex_func;
    minex_func.f = &w_calc_rss;
    minex_func.params = &fit;
    minex_func.n = 2;

    const gsl_multimin_fminimizer_type* t = gsl_multimin_fminimizer_nmsimplex;
    gsl_multimin_fminimizer* minimizer = gsl_multimin_fminimizer_alloc(t, 2);
    gsl_multimin_fminimizer_set(minimizer, &minex_func, initial_values, step_size);

    for (size_t iter = 0; iter < 100; ++iter) {
        if (gsl_multimin_fminimizer_iterate(minimizer))
            throw TRACED_EXCEPTION("gsl_multimin_fminimizer_iterate failed", util::Error::GSL);

        double size = gsl_multimin_fminimizer_size(minimizer);
        int status = gsl_multimin_test_size(size, 1e-2);
        if (status == GSL_SUCCESS)
            break;
    }
    // Call again to set final value.
    w_calc_rss(minimizer->x, &fit);

    gsl_vector_free(initial_values);
    gsl_vector_free(step_size);
    gsl_multimin_fminimizer_free(minimizer);
}

DemographyFit::DemographyFit(const scnXml::Demography& demography) {
    // mu1, alpha1: These are estimated here
    // mu0, alpha0: These are fixed alpha0=4.0, mu0 calculated from the other parameters
    // rho - population growth rate (input)

    //Get lower and upper age bounds for age groups and cumulative precentage of population from field data
    double sumperc = 0.0;
    const scnXml::DemogAgeGroup::GroupSequence& group = demography.getAgeGroup().getGroup();

    size_t ngroups = group.size() + 1;
    age_group_bounds.resize(ngroups + 1, 0);
    age_group_percent.resize(ngroups, 0);

    //Add age group for first month of life
    age_group_bounds[0] = 0.0;
    age_group_bounds[1] = 1.0 / 12.0;
    age_group_percent[0] = 0.0;
    for (size_t i = 1; i < ngroups; i++) {
        age_group_bounds[i + 1] = group[i - 1].getUpperbound();
        age_group_percent[i] = group[i - 1].getPoppercent();
        sumperc += age_group_percent[i];
    }
    sumperc = 100.0 / sumperc; // multiplier to get percentages
    for (size_t i = 0; i < ngroups; i++) {
        age_group_percent[i] = age_group_percent[i] * sumperc;
    }

    /* NOTE: unused --- why?
    double tol = 0.00000000001;
    int maxcal = 500000;
    int npar = 2;
    int iw = 3; */
    double p1 = 0.371626412;
    double p2 = 0.841209593;
    // returns "double rss":
    minimize_calc_rss(*this, p1, p2);
}

double DemographyFit::set_demo_parameters(double param1, double param2) {
    const double imr = 0.1;
    double m_inf = -log(1 - imr);

    mu1 = exp(param1) / 100;
    alpha1 = exp(param2) / 100;
    alpha0 = 4.0;
    mu0 = (m_inf - mu1 * (exp(alpha1 * 0.5) - 1) * alpha0) / (alpha1 * (1 - exp(-alpha0 * 0.5)));

    size_t ngroups = age_group_percent.size();
    double sumpred = 0.0;
    vector<double> m(ngroups, 0);
    vector<double> pred(ngroups, 0);

    for (size_t i = 0; i < ngroups - 1; i++) {
        double midpt = (age_group_bounds[i + 1] + age_group_bounds[i]) * 0.5;
        double m1 = mu0 * (1.0 - exp(-alpha0 * midpt)) / alpha0;
        double m2 = mu1 * (exp(alpha1 * midpt) - 1.0) / alpha1;
        m[i] = m1 + m2;
        pred[i] = (age_group_bounds[i + 1] - age_group_bounds[i]) * exp(-Rho * midpt - m[i]);
        sumpred += pred[i];
    }
    for (size_t i = 0; i < ngroups - 1; i++) {
        pred[i] = pred[i] / sumpred * 100.0;
    }
    double l_inf = exp(-Rho * 0.5 - m[1]);
    double m_nn = -log(1.0 - 0.4 * (1 - exp(-m[1])));
    double l1 = 1.0 / 12.0 * exp(-Rho / 24.0 - m_nn);
    double perc_inf = age_group_percent[0] + age_group_percent[1];
    age_group_percent[0] = perc_inf * l1 / l_inf;
    age_group_percent[1] = perc_inf - age_group_percent[0];

    double result = 0.0;
    for (size_t i = 0; i < ngroups - 1; i++) {
        double residual = log(pred[i]) - log(age_group_percent[i]);
        result += residual * residual;
    }
    return result;
}

void DemographyFit::calc_cum_age_prop(vector<double>& cum_age_prop) const {
    cum_age_prop[0] = 0.0;
    for (size_t j = 1; j < cum_age_prop.size(); ++j) {
        SimTime age = Sim::from_ts(cum_age_prop.size() - j - 1);
        double age_years = Sim::in_years(age);
        double m1s = (mu0 * (1.0 - exp(-alpha0 * age_years)) / alpha0);
        double m2s = (mu1 * (exp(alpha1 * age_years) - 1.0) / alpha1);
        double ms = m1s + m2s;
        double predperc = exp(-Rho * age_years - ms);
        if (age >= Sim::max_human_age()) {
            predperc = 0.0;
        }
        cum_age_prop[j] = cum_age_prop[j - 1] + predperc;
    }
    double total_cum_pc = cum_age_prop[cum_age_prop.size() - 1];
    for (size_t j = 1; j < cum_age_prop.size(); ++j) {
        //Scale using the total cumAgeProp
        cum_age_prop[j] = cum_age_prop[j] / total_cum_pc;
    }
}

}

AgeStructure::AgeStructure(const scnXml::Demography& demography) {
    // this number of cells are needed:
    cum_age_prop.resize(Sim::in_steps(Sim::max_human_age()) + 1);

    const DemographyFit fit(demography);
    fit.calc_cum_age_prop(cum_age_prop);
}

int AgeStructure::target_cum_pop(size_t age_steps, int target_pop) const {
    size_t index = cum_age_prop.size() - 1 - age_steps;
    return (int)floor(cum_age_prop[index] * target_pop + 0.5);
}

}
