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

#include "Population.h"
#include "mon/Continuous.h"

#include "Host/Human.h"
#include "Host/NeonatalMortality.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Diagnostic.h"
#include "Clinical/ClinicalModel.h"
#include "Transmission/TransmissionModel.h"

#include "util/errors.h"
#include "util/random.h"
#include "util/ModelOptions.h"
#include "util/StreamValidator.h"
#include <schema/scenario.h>

#include <cmath>

namespace OM {
using namespace std::placeholders;
using namespace OM::util;
using Transmission::TransmissionModel;

/** Variables for continuous reporting */
const vector<double> CtsDemogAgeGroups = {1.0, 5.0, 10.0, 15.0, 25.0};

// -----  non-static methods: creation/destruction, checkpointing  -----

Population::Population(size_t size, const AgeStructure& age_structure, Host::HumanParams human_params,
                       util::MasterRng& rng)
    : age_structure(&age_structure), human_params(human_params), rng(&rng), size(size) {}

void Population::create_initial_humans() {
    /* We create a whole population here, regardless of whether humans can
    survive until start of vector init (vector model needs a whole population
    structure in any case). However, we don't update humans known not to survive
    until vector init, which saves computation and memory (no infections). */

    int cumulative_pop = 0;
    for (size_t iage_prev = age_structure->max_steps_per_life(), iage = iage_prev - 1; iage_prev > 0;
         iage_prev = iage, iage -= 1) {
        const int target_pop = age_structure->target_cum_pop(iage, static_cast<int>(size));
        while (cumulative_pop < target_pop) {
            SimTime dob = Sim::zero() - Sim::from_ts(iage);
            util::streamValidate(dob);
            humans.emplace_back(dob, human_params, *rng);
            ++cumulative_pop;
        }
    }

    // Vector setup dependant on human population structure (we *want* to
    // include all humans, whether they'll survive to vector init phase or not).
    assert(Sim::now() == Sim::zero()); // assumed below
}

void Population::update() {
    //NOTE: other parts of code are not set up to handle changing population size. Also
    // size is assumed to be the _actual and exact_ population size by other code.
    int cum_pop = 0;

    for (auto it = humans.begin(); it != humans.end();) {
        bool is_dead = it->is_dead();

        // if (Actual number of people so far > target population size for this age)
        // "outmigrate" some to maintain population shape
        //NOTE: better to use age(sim::ts0())? Possibly, but the difference will not be very significant.
        // Also see target_pop = ... comment above
        bool outmigrate =
            cum_pop >= age_structure->target_cum_pop(Sim::in_steps(it->age(Sim::ts1())), static_cast<int>(size));

        if (is_dead || outmigrate) {
            it = humans.erase(it);
            continue;
        }
        ++cum_pop;
        ++it;
    } // end of per-human updates

    // increase population size to target_pop
    recent_births += (size - cum_pop);
    while (cum_pop < (int)size) {
        // humans born at end of this time step = beginning of next, hence ts1
        humans.emplace_back(Sim::ts1(), human_params, *rng);
        ++cum_pop;
    }
}

void Population::checkpoint(istream& stream) {
    size & stream;
    recent_births & stream;

    for (size_t i = 0; i < size && !stream.eof(); ++i) {
        humans.emplace_back(Sim::zero(), human_params, *rng);
        humans.back().checkpoint(stream);
    }
    if (humans.size() != size)
        throw util::checkpoint_error("Population: out of data (read " + to_string(humans.size()) + " humans)");
}

void Population::checkpoint(ostream& stream) {
    size & stream;
    recent_births & stream;
    for (Host::Human& human : humans)
        human.checkpoint(stream);
}

void register_continuous_population_callbacks(mon::Continuous& continuous) {
    ostringstream cts_demog_title;
    for (double ubound : CtsDemogAgeGroups)
        cts_demog_title << "\thost % ≤ " << ubound;

    continuous.register_callback("hosts", "\thosts", &cts_hosts);
    continuous.register_callback("host demography", cts_demog_title.str(), &cts_host_demography);
    continuous.register_callback("recent births", "\trecent births", &cts_recent_births);
    continuous.register_callback("patent hosts", "\tpatent hosts", &cts_patent_hosts);
    continuous.register_callback("immunity h", "\timmunity h", &cts_immunity_h);
    continuous.register_callback("immunity Y", "\timmunity Y", &cts_immunity_y);
    continuous.register_callback("median immunity Y", "\tmedian immunity Y", &cts_median_immunity_y);
    continuous.register_callback("human age availability", "\thuman age availability", &cts_mean_age_avail_effect);
    continuous.register_callback("ITN coverage", "\tITN coverage", &cts_itn_coverage);
    continuous.register_callback("IRS coverage", "\tIRS coverage", &cts_irs_coverage);
    continuous.register_callback("GVI coverage", "\tGVI coverage", &cts_gvi_coverage);
}

void cts_hosts(Population& population, ostream& stream) {
    // this option is intended for debugging human initialization; normally this should equal size.
    stream << '\t' << population.get_size();
}

void cts_host_demography(Population& population, ostream& stream) {
    auto iter = population.humans.crbegin();
    int cum_count = 0;
    for (double ubound : CtsDemogAgeGroups) {
        while (iter != population.humans.crend() && Sim::in_years(iter->age(Sim::now())) < ubound) {
            ++cum_count;
            ++iter;
        }
        stream << '\t' << cum_count;
    }
}

void cts_recent_births(Population& population, ostream& stream) {
    stream << '\t' << population.get_recent_births();
    population.reset_recent_births();
}

void cts_patent_hosts(Population& population, ostream& stream) {
    int patent = 0;
    for (Host::Human& human : population.humans) {
        auto diag = WithinHost::Diagnostics::monitoring_diagnostic();
        if (human.within_host_model->diagnostic_result(human.monitoring_rng, diag))
            ++patent;
    }
    stream << '\t' << patent;
}

void cts_immunity_h(Population& population, ostream& stream) {
    double x = 0.0;
    for (const Host::Human& human : population.humans) {
        x += human.within_host_model->get_cumulative_h();
    }
    x /= population.get_size();
    stream << '\t' << x;
}

void cts_immunity_y(Population& population, ostream& stream) {
    double x = 0.0;
    for (const Host::Human& human : population.humans) {
        x += human.within_host_model->get_cumulative_y();
    }
    x /= population.get_size();
    stream << '\t' << x;
}

void cts_median_immunity_y(Population& population, ostream& stream) {
    vector<double> list;
    list.reserve(population.get_size());
    for (const Host::Human& human : population.humans) {
        list.push_back(human.within_host_model->get_cumulative_y());
    }
    sort(list.begin(), list.end());
    double x;
    if (mod_nn(population.get_size(), 2) == 0) {
        size_t i = population.get_size() / 2;
        x = (list[i - 1] + list[i]) / 2.0;
    } else {
        x = list[population.get_size() / 2];
    }
    stream << '\t' << x;
}

void cts_mean_age_avail_effect(Population& population, ostream& stream) {
    int n_humans = 0;
    double avail = 0.0;
    for (Host::Human& human : population.humans) {
        if (!human.per_host_transmission.outside_transmission) {
            ++n_humans;
            avail += human.per_host_transmission.relative_availability_age(Sim::in_years(human.age(Sim::now())));
        }
    }
    stream << '\t' << avail / n_humans;
}

void cts_itn_coverage(Population& population, ostream& stream) {
    int n_active = 0;
    for (const Host::Human& human : population.humans) {
        n_active += human.per_host_transmission.has_active_interv(interventions::component_types::ITN,
                                                                  population.interventions());
    }
    double coverage = static_cast<double>(n_active) / population.get_size();
    stream << '\t' << coverage;
}

void cts_irs_coverage(Population& population, ostream& stream) {
    int n_active = 0;
    for (const Host::Human& human : population.humans) {
        n_active += human.per_host_transmission.has_active_interv(interventions::component_types::IRS,
                                                                  population.interventions());
    }
    double coverage = static_cast<double>(n_active) / population.get_size();
    stream << '\t' << coverage;
}

void cts_gvi_coverage(Population& population, ostream& stream) {
    int n_active = 0;
    for (const Host::Human& human : population.humans) {
        n_active += human.per_host_transmission.has_active_interv(interventions::component_types::GVI,
                                                                  population.interventions());
    }
    double coverage = static_cast<double>(n_active) / population.get_size();
    stream << '\t' << coverage;
}

}
