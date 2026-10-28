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

#ifndef OM_IMPORTED_INFECTIONS
#define OM_IMPORTED_INFECTIONS

#include "Global.h"
#include "schema/interventions.h"
#include "util/errors.h"
#include "util/UnitParse.h"
#include "Host/Human.h"
#include "Host/WithinHost/WHInterface.h"
#include "Host/WithinHost/Infection/Infection.h"

#include <algorithm>

namespace OM {
class Population;

namespace Host {

class ImportedInfections {
public:
    ImportedInfections() : period(Sim::zero()) {}

    /** Initialise, passing intervention description
         * 
         * @param elt The scenario element with description of rates
         * @returns True if any infections are imported
         */
    void init(const scnXml::ImportedInfections& elt, const Clock& clock, const util::CommandLineOptions& command_line) {
        sim_clock = &clock;
        const scnXml::ImportedInfections::TimedType& timed_elt = elt.getTimed();
        try {
            //NOTE: if changing XSD, this should not have a default unit:
            period = unit_parse::read_duration(timed_elt.getPeriod(), unit_parse::STEPS, clock, command_line);
            if (period < Sim::zero()) {
                throw util::format_error("cannot be negative");
            }
        } catch (const util::format_error& e) {
            throw util::xml_scenario_error(
                string("interventions/importedInfections/timed/period: ").append(e.message()));
        }
        rate.reserve(timed_elt.getRate().size());
        try {
            for (const auto& rate_elt : timed_elt.getRate()) {
                const SimTime date = unit_parse::read_date(
                    rate_elt.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                    command_line);
                // convert to per-time-step, per-person
                const double rate_val = rate_elt.getValue() * clock.years_per_step() * (1.0 / 1000.0);
                rate.emplace_back(date - clock.start_date(), rate_val);
            }
            std::ranges::sort(rate, {}, &Rate::time);
            if (rate.size() > 0) {
                if (period != Sim::zero() && rate[0].time != Sim::zero()) {
                    throw util::xml_scenario_error("must specify rate at time zero when period is not zero");
                }
                // remove useless repeated entries from list
                double last_rate = rate[0].value;
                for (size_t i = 1; i < rate.size();) {
                    if (rate[i].value == last_rate) {
                        rate.erase(rate.begin() + i);
                    } else {
                        last_rate = rate[i].value;
                        ++i;
                    }
                }
            }
        } catch (const util::format_error& e) {
            throw util::xml_scenario_error(string("interventions/importedInfections/timed/time: ").append(e.message()));
        }
    }

    /** Import this time-step's imported infections, according to initialised rates
         * 
         *  The probability of an host to import an infection is calculated
         *  from the importedInfectionsPerThousandHosts. The bernoulli distribution
         *  is then used to predict if an human has imported the infection in the
         *  population or not. A maximum of one infection can be imported per
         *  person.
         * 
         * @param pop The Population class encapsulating all humans */
    void import(vector<Human>& population) {
        if (rate.size() == 0)
            return; // no imported infections
        SimTime now = sim_clock->interv_time();
        assert(now >= Sim::zero());
        if (period > Sim::zero()) {
            now = mod_nn(now, period);
        }
        if (rate[last_index].time > now) {
            last_index = 0; // gone round in a loop: back to start of period
        }
        if (now > rate[last_index].time && rate.size() > last_index + 1) {
            if (rate[last_index + 1].time <= now) {
                ++last_index;
                // won't be necessary to do this more than once, since import() is called every time step
            }
        }

        double rate_now = rate[last_index].value;
        if (rate_now > 0.0) {
            for (Human& human : population) {
                if (human.host_rng.bernoulli(rate_now)) {
                    human.within_host_model->import_infection(human.host_rng);
                    mon::record_event(mon::measure("nNewInfections"), human);
                    mon::record_event(mon::measure("nNewInfections_Imported"), human);
                }
            }
        }
    }

    /// Checkpointing
    template <class S> void operator&(S& stream) {
        using namespace OM::util::checkpoint;
        // period and rate are set from XML and not changed
        last_index & stream;
    }

private:
    SimTime period = Sim::never();
    uint32_t last_index{0};
    const Clock* sim_clock = nullptr;
    struct Rate {
        Rate(SimTime t, double v) : time(t), value(v) {}
        SimTime time = Sim::never();
        double value;
        inline bool operator<(const Rate& that) const { return time < that.time; }
    };
    vector<Rate> rate;
};
}
}

#endif
