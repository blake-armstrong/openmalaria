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

    double setDemoParameters(double param1, double param2);
    void calcCumAgeProp(vector<double>& cumAgeProp) const;

    vector<double> ageGroupBounds;
    vector<double> ageGroupPercent;
    double mu0 = 0.0;
    double mu1 = 0.0;
    double alpha0 = 0.0;
    double alpha1 = 0.0;
    static constexpr double rho = 0.0;
};

double wCalcRSS(const gsl_vector* v, void* params) {
    double x, y;
    x = gsl_vector_get(v, 0);
    y = gsl_vector_get(v, 1);

    return static_cast<DemographyFit*>(params)->setDemoParameters(x, y);
}

void minimizeCalc_rss(DemographyFit& fit, double param1, double param2) {
    gsl_vector* stepSize = gsl_vector_alloc(2);
    gsl_vector_set_all(stepSize, 0.1);

    gsl_vector* initialValues = gsl_vector_alloc(2);
    gsl_vector_set(initialValues, 0, param1);
    gsl_vector_set(initialValues, 1, param2);

    gsl_multimin_function minex_func;
    minex_func.f = &wCalcRSS;
    minex_func.params = &fit;
    minex_func.n = 2;

    const gsl_multimin_fminimizer_type* T = gsl_multimin_fminimizer_nmsimplex;
    gsl_multimin_fminimizer* minimizer = gsl_multimin_fminimizer_alloc(T, 2);
    gsl_multimin_fminimizer_set(minimizer, &minex_func, initialValues, stepSize);

    for (size_t iter = 0; iter < 100; ++iter) {
        if (gsl_multimin_fminimizer_iterate(minimizer))
            throw TRACED_EXCEPTION("gsl_multimin_fminimizer_iterate failed", util::Error::GSL);

        double size = gsl_multimin_fminimizer_size(minimizer);
        int status = gsl_multimin_test_size(size, 1e-2);
        if (status == GSL_SUCCESS)
            break;
    }
    // Call again to set final value.
    wCalcRSS(minimizer->x, &fit);

    gsl_vector_free(initialValues);
    gsl_vector_free(stepSize);
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
    ageGroupBounds.resize(ngroups + 1, 0);
    ageGroupPercent.resize(ngroups, 0);

    //Add age group for first month of life
    ageGroupBounds[0] = 0.0;
    ageGroupBounds[1] = 1.0 / 12.0;
    ageGroupPercent[0] = 0.0;
    for (size_t i = 1; i < ngroups; i++) {
        ageGroupBounds[i + 1] = group[i - 1].getUpperbound();
        ageGroupPercent[i] = group[i - 1].getPoppercent();
        sumperc += ageGroupPercent[i];
    }
    sumperc = 100.0 / sumperc; // multiplier to get percentages
    for (size_t i = 0; i < ngroups; i++) {
        ageGroupPercent[i] = ageGroupPercent[i] * sumperc;
    }

    /* NOTE: unused --- why?
    double tol = 0.00000000001;
    int maxcal = 500000;
    int npar = 2;
    int iw = 3; */
    double p1 = 0.371626412;
    double p2 = 0.841209593;
    // returns "double rss":
    minimizeCalc_rss(*this, p1, p2);
}

double DemographyFit::setDemoParameters(double param1, double param2) {
    const double IMR = 0.1;
    double M_inf = -log(1 - IMR);

    mu1 = exp(param1) / 100;
    alpha1 = exp(param2) / 100;
    alpha0 = 4.0;
    mu0 = (M_inf - mu1 * (exp(alpha1 * 0.5) - 1) * alpha0) / (alpha1 * (1 - exp(-alpha0 * 0.5)));

    size_t ngroups = ageGroupPercent.size();
    double sumpred = 0.0;
    vector<double> M(ngroups, 0);
    vector<double> pred(ngroups, 0);

    for (size_t i = 0; i < ngroups - 1; i++) {
        double midpt = (ageGroupBounds[i + 1] + ageGroupBounds[i]) * 0.5;
        double M1 = mu0 * (1.0 - exp(-alpha0 * midpt)) / alpha0;
        double M2 = mu1 * (exp(alpha1 * midpt) - 1.0) / alpha1;
        M[i] = M1 + M2;
        pred[i] = (ageGroupBounds[i + 1] - ageGroupBounds[i]) * exp(-rho * midpt - M[i]);
        sumpred += pred[i];
    }
    for (size_t i = 0; i < ngroups - 1; i++) {
        pred[i] = pred[i] / sumpred * 100.0;
    }
    double L_inf = exp(-rho * 0.5 - M[1]);
    double M_nn = -log(1.0 - 0.4 * (1 - exp(-M[1])));
    double L1 = 1.0 / 12.0 * exp(-rho / 24.0 - M_nn);
    double perc_inf = ageGroupPercent[0] + ageGroupPercent[1];
    ageGroupPercent[0] = perc_inf * L1 / L_inf;
    ageGroupPercent[1] = perc_inf - ageGroupPercent[0];

    double result = 0.0;
    for (size_t i = 0; i < ngroups - 1; i++) {
        double residual = log(pred[i]) - log(ageGroupPercent[i]);
        result += residual * residual;
    }
    return result;
}

void DemographyFit::calcCumAgeProp(vector<double>& cumAgeProp) const {
    cumAgeProp[0] = 0.0;
    for (size_t j = 1; j < cumAgeProp.size(); ++j) {
        SimTime age = Sim::from_ts(cumAgeProp.size() - j - 1);
        double ageYears = Sim::in_years(age);
        double M1s = (mu0 * (1.0 - exp(-alpha0 * ageYears)) / alpha0);
        double M2s = (mu1 * (exp(alpha1 * ageYears) - 1.0) / alpha1);
        double Ms = M1s + M2s;
        double predperc = exp(-rho * ageYears - Ms);
        if (age >= Sim::max_human_age()) {
            predperc = 0.0;
        }
        cumAgeProp[j] = cumAgeProp[j - 1] + predperc;
    }
    double totalCumPC = cumAgeProp[cumAgeProp.size() - 1];
    for (size_t j = 1; j < cumAgeProp.size(); ++j) {
        //Scale using the total cumAgeProp
        cumAgeProp[j] = cumAgeProp[j] / totalCumPC;
    }
}

}

AgeStructure::AgeStructure(const scnXml::Demography& demography) {
    // this number of cells are needed:
    cum_age_prop.resize(Sim::in_steps(Sim::max_human_age()) + 1);

    const DemographyFit fit(demography);
    fit.calcCumAgeProp(cum_age_prop);
}

int AgeStructure::target_cum_pop(size_t age_steps, int target_pop) const {
    size_t index = cum_age_prop.size() - 1 - age_steps;
    return (int)floor(cum_age_prop[index] * target_pop + 0.5);
}

}
