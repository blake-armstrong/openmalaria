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

#include "Host/WithinHost/Infection/DescriptiveInfection.h"
#include "util/random.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "util/logging.h"
#include "util/StreamValidator.h"

#include <sstream>
#include <string>
#include <cmath>
#include <fstream>

namespace OM::WithinHost {
using namespace util;

// ———  parameters  ———

DescriptiveInfectionParams::DescriptiveInfectionParams(const Parameters& parameters,
                                                       const util::ModelOptionFlags& options,
                                                       const util::CommandLineOptions& command_line,
                                                       const Clock& clock) {
    // Error checks
    if (clock.one_ts() != 5) {
        // To support non-5-day time-step models, either different data would
        // be needed or times need to be adjusted when accessing
        // mean_log_parasite_count. Probably the rest would be fine.
        throw util::xml_scenario_error("DescriptiveInfection only supports using an interval of 5");
    }
    // Bug fixes: these are enabled by default but may be off in old parameterisations
    bugfix_innate_max_dens = options.option(util::InnateMaxDens);
    // Warning: if MAX_DENS_CORRECTION is off, infections not yet at the blood
    // stage could result in BSVEfficacy and potentially innateImmSurvFact
    // being applied to timeStepMaxDensity more than once in some cases.
    bugfix_max_dens = options.option(util::MaxDensCorrection);

    // Read parameters
    sigma0_sq = parameters[Parameter::Sigma0Sq];
    x_nu_star = parameters[Parameter::XNuStar];

    // Read file empirical parasite densities
    const string densities_filename = command_line.lookup_resource("densities.csv");
    ifstream f_m_therapy_densities(densities_filename.c_str());
    if (!f_m_therapy_densities.good()) {
        throw util::base_exception(string("Cannot read ").append(densities_filename), util::Error::FileIO);
    }

    //read header of file (unused)
    string csv_line;
    getline(f_m_therapy_densities, csv_line);

    // read every line from the stream
    while (getline(f_m_therapy_densities, csv_line)) {
        //Empirical description of single Malaria infections in naive individuals
        //counter variables, i stands for 5 day time interval, j for duration of infection
        int i = 0;
        int j = 0;
        double meanlogdens = NAN;

        std::istringstream csv_stream(csv_line);

        string csv_field1, csv_field2, csv_field3;

        // read every element from the line that is seperated by commas
        getline(csv_stream, csv_field1, ',');
        getline(csv_stream, csv_field2, ',');
        getline(csv_stream, csv_field3, ',');

        istringstream csv_num1(csv_field1), csv_num2(csv_field2), csv_num3(csv_field3);

        csv_num1 >> i;
        csv_num2 >> j;
        csv_num3 >> meanlogdens;

        if (i < 1 || i > NumDurations || j < 1 || j > NumDurations) {
            throw util::base_exception(string("error in data file: ").append(densities_filename), Error::InputResource);
        }

        //fill initial matrix
        mean_log_parasite_count[i - 1][j - 1] = meanlogdens;
        //fill also the triangle that will not be used (to ensure everything is initialised)
        if (j != i) {
            mean_log_parasite_count[j - 1][i - 1] = 0.0;
        }
    }
}

// ———  non-static init/destruction  ———

DescriptiveInfection::DescriptiveInfection(const DescriptiveInfectionParams& params, const Clock& clock,
                                           SimTime latent_p, LocalRng& rng, uint32_t genotype, int origin)
    : Infection(clock.now_or_ts0(), latent_p, genotype, origin), duration(infection_duration(rng, clock)),
      not_printed_md_warning(true), params(&params) {
    assert(clock.one_ts() == 5);
}

SimTime DescriptiveInfection::infection_duration(LocalRng& rng, const Clock& clock) {
    // Forgive the excess precision; it just avoids having to update all expected results
    const double dur_mean = 5.1300001144409179688;
    const double dur_sigma = 0.80000001192092895508;
    const double dur = rng.log_normal(dur_mean, dur_sigma);

    //TODO:
    // Model did say infection is cleared on day dur+1 converted to a time-step
    // let interval = sim::oneTS() in:
    // ((1+floor(dur))/interval); now it says the last interval is:
    // floor((1+dur)/interval)-1 = floor((dur+1-interval)/interval)
    // Is this reasonable, or should we change?
    return Sim::from_days(static_cast<int>(1.0 + dur)) - clock.one_ts();
}

// ———  time-step updates  ———
void DescriptiveInfection::determine_densities(LocalRng& rng, const Clock& clock, double cumulativeh,
                                               double& time_step_max_density, double imm_surv_fact,
                                               double innate_imm_surv_fact, double bsv_factor) {
    // Age of patent blood stage infection. Note: liver stage is fixed at one
    // 5-day time step and prepatent blood stage is latentp - 1 time steps.
    const SimTime infage = clock.ts0() - start_date - latent_period();
    if (infage < Sim::zero()) {
        density = 0.0;
        if (params->bugfix_max_dens)
            time_step_max_density = 0.0;
    } else {
        time_step_max_density = 0.0;

        const int32_t inf_age = min(clock.in_steps(infage), MaxDurationTs);
        const int32_t inf_dur = min(clock.in_steps(duration), MaxDurationTs);
        density = max(params->mean_log_parasite_count[inf_age][inf_dur], 0.0);

        // The expected parasite density in the non naive host (AJTM p.9 eq. 9)
        // Note that in published and current implementations Dx is zero.
        density = density * imm_surv_fact;

        //Perturb m_density using a lognormal
        const double varlog = params->sigma0_sq / (1.0 + (cumulativeh / params->x_nu_star));
        const double stdlog = sqrt(varlog);

        /*
        This code samples from a log normal distribution with mean equal to the predicted density
        n.b. AJTM p.9 eq 9 implies that we sample the log of the density from a normal with mean equal to
        the log of the predicted density.  If we really did the latter then this bias correction is not needed.
        */
        if (stdlog > 0.0000001) {
            // Calculate the expected density on the day of sampling:
            const double meanlog = density - stdlog * stdlog / 2.0;
            density = rng.log_normal(meanlog, stdlog);
            // Calculate additional samples for T-1 days (T=days per step):
            time_step_max_density = rng.max_multi_log_normal(density, clock.one_ts() - 1, meanlog, stdlog);
        }
        if (time_step_max_density > MaxDens && not_printed_md_warning) {
            logging::debug("TSMD hit limit:\t{:g},\t{:g}", density, time_step_max_density);
            not_printed_md_warning = false;
        }
        density = min(density, MaxDens);
        time_step_max_density = min(time_step_max_density, MaxDens);

        //Compute the proportion of parasites remaining after innate blood stage effect
        density *= innate_imm_surv_fact;

        //Include here the effect of blood stage vaccination
        density *= bsv_factor;

        cum_exposure_j += clock.one_ts() * density;
    }

    if (params->bugfix_innate_max_dens)
        time_step_max_density *= innate_imm_surv_fact;
    time_step_max_density *= bsv_factor;
}

// ———  checkpointing  ———

DescriptiveInfection::DescriptiveInfection(const DescriptiveInfectionParams& params, SimTime latent_p, istream& stream)
    : Infection(latent_p, stream), duration(Sim::never()), params(&params) {
    duration & stream;
    not_printed_md_warning & stream;
}
void DescriptiveInfection::checkpoint(ostream& stream) {
    Infection::checkpoint(stream);
    duration & stream;
    not_printed_md_warning & stream;
}

}
