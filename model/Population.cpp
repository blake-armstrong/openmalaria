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
const vector<double> ctsDemogAgeGroups = {1.0, 5.0, 10.0, 15.0, 25.0};

// -----  non-static methods: creation/destruction, checkpointing  -----

Population::Population(size_t size, const AgeStructure& age_structure, Host::HumanParams human_params,
                       util::MasterRng& rng)
    : age_structure(&age_structure), human_params(human_params), rng(&rng), size(size) {}

void Population::createInitialHumans() {
    /* We create a whole population here, regardless of whether humans can
    survive until start of vector init (vector model needs a whole population
    structure in any case). However, we don't update humans known not to survive
    until vector init, which saves computation and memory (no infections). */

    int cumulativePop = 0;
    for (size_t iage_prev = age_structure->max_steps_per_life(), iage = iage_prev - 1; iage_prev > 0;
         iage_prev = iage, iage -= 1) {
        const int target_pop = age_structure->target_cum_pop(iage, static_cast<int>(size));
        while (cumulativePop < target_pop) {
            SimTime dob = Sim::zero() - Sim::from_ts(iage);
            util::streamValidate(dob);
            humans.emplace_back(dob, human_params, *rng);
            ++cumulativePop;
        }
    }

    // Vector setup dependant on human population structure (we *want* to
    // include all humans, whether they'll survive to vector init phase or not).
    assert(Sim::now() == Sim::zero()); // assumed below
}

void Population::update() {
    //NOTE: other parts of code are not set up to handle changing population size. Also
    // size is assumed to be the _actual and exact_ population size by other code.
    int cumPop = 0;

    for (auto it = humans.begin(); it != humans.end();) {
        bool isDead = it->isDead();

        // if (Actual number of people so far > target population size for this age)
        // "outmigrate" some to maintain population shape
        //NOTE: better to use age(sim::ts0())? Possibly, but the difference will not be very significant.
        // Also see target_pop = ... comment above
        bool outmigrate =
            cumPop >= age_structure->target_cum_pop(Sim::in_steps(it->age(Sim::ts1())), static_cast<int>(size));

        if (isDead || outmigrate) {
            it = humans.erase(it);
            continue;
        }
        ++cumPop;
        ++it;
    } // end of per-human updates

    // increase population size to target_pop
    recentBirths += (size - cumPop);
    while (cumPop < (int)size) {
        // humans born at end of this time step = beginning of next, hence ts1
        humans.emplace_back(Sim::ts1(), human_params, *rng);
        ++cumPop;
    }
}

void Population::checkpoint(istream& stream) {
    size & stream;
    recentBirths & stream;

    for (size_t i = 0; i < size && !stream.eof(); ++i) {
        humans.emplace_back(Sim::zero(), human_params, *rng);
        humans.back().checkpoint(stream);
    }
    if (humans.size() != size)
        throw util::checkpoint_error("Population: out of data (read " + to_string(humans.size()) + " humans)");
}

void Population::checkpoint(ostream& stream) {
    size & stream;
    recentBirths & stream;
    for (Host::Human& human : humans)
        human.checkpoint(stream);
}

void register_continuous_population_callbacks(mon::Continuous& continuous) {
    ostringstream ctsDemogTitle;
    for (double ubound : ctsDemogAgeGroups)
        ctsDemogTitle << "\thost % ≤ " << ubound;

    continuous.register_callback("hosts", "\thosts", &ctsHosts);
    continuous.register_callback("host demography", ctsDemogTitle.str(), &ctsHostDemography);
    continuous.register_callback("recent births", "\trecent births", &ctsRecentBirths);
    continuous.register_callback("patent hosts", "\tpatent hosts", &ctsPatentHosts);
    continuous.register_callback("immunity h", "\timmunity h", &ctsImmunityh);
    continuous.register_callback("immunity Y", "\timmunity Y", &ctsImmunityY);
    continuous.register_callback("median immunity Y", "\tmedian immunity Y", &ctsMedianImmunityY);
    continuous.register_callback("human age availability", "\thuman age availability", &ctsMeanAgeAvailEffect);
    continuous.register_callback("ITN coverage", "\tITN coverage", &ctsITNCoverage);
    continuous.register_callback("IRS coverage", "\tIRS coverage", &ctsIRSCoverage);
    continuous.register_callback("GVI coverage", "\tGVI coverage", &ctsGVICoverage);
}

void ctsHosts(Population& population, ostream& stream) {
    // this option is intended for debugging human initialization; normally this should equal size.
    stream << '\t' << population.getSize();
}

void ctsHostDemography(Population& population, ostream& stream) {
    auto iter = population.humans.crbegin();
    int cumCount = 0;
    for (double ubound : ctsDemogAgeGroups) {
        while (iter != population.humans.crend() && Sim::in_years(iter->age(Sim::now())) < ubound) {
            ++cumCount;
            ++iter;
        }
        stream << '\t' << cumCount;
    }
}

void ctsRecentBirths(Population& population, ostream& stream) {
    stream << '\t' << population.getRecentBirths();
    population.resetRecentBirths();
}

void ctsPatentHosts(Population& population, ostream& stream) {
    int patent = 0;
    for (Host::Human& human : population.humans) {
        auto diag = WithinHost::Diagnostics::monitoring_diagnostic();
        if (human.withinHostModel->diagnostic_result(human.monitoring_rng, diag))
            ++patent;
    }
    stream << '\t' << patent;
}

void ctsImmunityh(Population& population, ostream& stream) {
    double x = 0.0;
    for (const Host::Human& human : population.humans) {
        x += human.withinHostModel->get_cumulative_h();
    }
    x /= population.getSize();
    stream << '\t' << x;
}

void ctsImmunityY(Population& population, ostream& stream) {
    double x = 0.0;
    for (const Host::Human& human : population.humans) {
        x += human.withinHostModel->get_cumulative_y();
    }
    x /= population.getSize();
    stream << '\t' << x;
}

void ctsMedianImmunityY(Population& population, ostream& stream) {
    vector<double> list;
    list.reserve(population.getSize());
    for (const Host::Human& human : population.humans) {
        list.push_back(human.withinHostModel->get_cumulative_y());
    }
    sort(list.begin(), list.end());
    double x;
    if (mod_nn(population.getSize(), 2) == 0) {
        size_t i = population.getSize() / 2;
        x = (list[i - 1] + list[i]) / 2.0;
    } else {
        x = list[population.getSize() / 2];
    }
    stream << '\t' << x;
}

void ctsMeanAgeAvailEffect(Population& population, ostream& stream) {
    int nHumans = 0;
    double avail = 0.0;
    for (Host::Human& human : population.humans) {
        if (!human.perHostTransmission.outside_transmission) {
            ++nHumans;
            avail += human.perHostTransmission.relative_availability_age(Sim::in_years(human.age(Sim::now())));
        }
    }
    stream << '\t' << avail / nHumans;
}

void ctsITNCoverage(Population& population, ostream& stream) {
    int nActive = 0;
    for (const Host::Human& human : population.humans) {
        nActive += human.perHostTransmission.has_active_interv(interventions::component_types::ITN,
                                                               population.interventions());
    }
    double coverage = static_cast<double>(nActive) / population.getSize();
    stream << '\t' << coverage;
}

void ctsIRSCoverage(Population& population, ostream& stream) {
    int nActive = 0;
    for (const Host::Human& human : population.humans) {
        nActive += human.perHostTransmission.has_active_interv(interventions::component_types::IRS,
                                                               population.interventions());
    }
    double coverage = static_cast<double>(nActive) / population.getSize();
    stream << '\t' << coverage;
}

void ctsGVICoverage(Population& population, ostream& stream) {
    int nActive = 0;
    for (const Host::Human& human : population.humans) {
        nActive += human.perHostTransmission.has_active_interv(interventions::component_types::GVI,
                                                               population.interventions());
    }
    double coverage = static_cast<double>(nActive) / population.getSize();
    stream << '\t' << coverage;
}

}
