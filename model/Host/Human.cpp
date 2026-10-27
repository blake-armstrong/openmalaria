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

#include "Host/Human.h"
#include "Host/InfectionIncidenceModel.h"
#include "Clinical/ClinicalModel.h"
#include "Clinical/ClinicalParams.h"
#include "Host/WithinHost/WithinHostModel.h"

#include "Transmission/TransmissionModel.h"
#include "util/ModelOptions.h"
#include "util/vectors.h"
#include "util/StreamValidator.h"
#include "Population.h"
#include "interventions/InterventionManager.h"
#include "mon/Monitoring.h"
#include "schema/scenario.h"

namespace OM::Host {
using namespace OM::util;
using interventions::ComponentId;

struct HumanHet {
    /* Human heterogeneity; affects:
     * comorbidityFactor (stored in PathogenesisModel)
     * treatmentSeekingFactor (stored in CaseManagementModel)
     * availabilityFactor (stored in Transmission::PerHost) */
    double comorbidity_factor{1.0};
    double treatment_seeking_factor{1.0};
    double availability_factor{1.0};

    HumanHet() = default;
};

HumanHet het_sample(util::LocalRng& rng, const util::ModelOptionFlags& options) {
    HumanHet het;
    if (options.option(util::TransHet)) {
        het.availability_factor = 0.2;
        if (rng.bernoulli(0.5)) {
            het.availability_factor = 1.8;
        }
    }
    if (options.option(util::ComorbHet)) {
        het.comorbidity_factor = 0.2;
        if (rng.bernoulli(0.5)) {
            het.comorbidity_factor = 1.8;
        }
    }
    if (options.option(util::TreatHet)) {
        het.treatment_seeking_factor = 0.2;
        if (rng.bernoulli(0.5)) {
            het.treatment_seeking_factor = 1.8;
        }
    }
    if (options.option(util::TransTreatHet)) {
        het.treatment_seeking_factor = 0.2;
        het.availability_factor = 1.8;
        if (rng.bernoulli(0.5)) {
            het.treatment_seeking_factor = 1.8;
            het.availability_factor = 0.2;
        }
    } else if (options.option(util::ComorbTreatHet)) {
        if (rng.bernoulli(0.5)) {
            het.comorbidity_factor = 1.8;
            het.treatment_seeking_factor = 0.2;
        } else {
            het.comorbidity_factor = 0.2;
            het.treatment_seeking_factor = 1.8;
        }
    } else if (options.option(util::ComorbTransHet)) {
        het.availability_factor = 1.8;
        het.comorbidity_factor = 1.8;
        if (rng.bernoulli(0.5)) {
            het.availability_factor = 0.2;
            het.comorbidity_factor = 0.2;
        }
    } else if (options.option(util::TripleHet)) {
        het.availability_factor = 1.8;
        het.comorbidity_factor = 1.8;
        het.treatment_seeking_factor = 0.2;
        if (rng.bernoulli(0.5)) {
            het.availability_factor = 0.2;
            het.comorbidity_factor = 0.2;
            het.treatment_seeking_factor = 1.8;
        }
    }
    return het;
}

Human::Human(SimTime date_of_birth, const HumanParams& params, util::MasterRng& rng)
    : inf_incidence(params.infection_incidence->create()), host_rng(rng), clinical_rng(rng), intervention_rng(rng),
      monitoring_rng(rng), dob(date_of_birth), intervention_manager(params.interventions),
      monitoring_state(params.monitoring) {
    // Initial humans are created at time 0 and may have date_of_birth in past. Otherwise date_of_birth must be now.
    assert(date_of_birth == Sim::now_or_ts1() || (Sim::now() == Sim::zero() && date_of_birth < Sim::now()));

    const HumanHet het = het_sample(host_rng, *params.options);
    within_host_model = params.within_host->create(host_rng, het.comorbidity_factor);
    const double ii_factor = inf_incidence->get_availability_factor(host_rng, 1.0);
    per_host_transmission.initialise(*params.per_host, host_rng, het.availability_factor * ii_factor);
    clinical_model = params.clinical->create(het.treatment_seeking_factor, *params.monitoring);
}

void Human::add_to_cohort(ComponentId id, SimTime duration) {
    if (duration <= Sim::zero())
        return; // nothing to do
    sub_pop_exp[id] = Sim::now_or_ts1() + duration;
    cohort_set = mon::update_cohort_set(monitoring(), cohort_set, id, true);
}

void Human::remove_from_cohort(interventions::ComponentId id) { sub_pop_exp.erase(id); }

void Human::remove_first_event(interventions::sub_pop_remove::RemoveAtCode code) {
    for (const ComponentId& id : intervention_manager->remove_at_ids(code)) {
        auto exp_it = sub_pop_exp.find(id);
        if (exp_it != sub_pop_exp.end()) {
            if (exp_it->second >= Sim::now_or_ts0()) {
                // removeFirstEvent() is used for onFirstBout, onFirstTreatment
                // and onFirstInfection cohort options. Health system memory must
                // be reset for this to work properly; in theory the memory should
                // be independent for each cohort, but this is a usable approximation.
                clinical_model->flush_reports(); // reset HS memory

                // report removal due to first infection/bout/treatment
                mon::record_event(mon::measure("nSubPopRemovalFirstEvent"), *this);
            }
            cohort_set = mon::update_cohort_set(monitoring(), cohort_set, exp_it->first, false);
            // remove (affects reporting, restrictToSubPop and cumulative deployment):
            sub_pop_exp.erase(exp_it);
        }
    }
}

void Human::update_cohort_set() {
    // check sub-pop expiry
    for (auto exp_it = sub_pop_exp.begin(), exp_end = sub_pop_exp.end(); exp_it != exp_end;) {
        if (!(exp_it->second >= Sim::ts0())) { // membership expired
            // don't flush reports
            // report removal due to expiry
            mon::record_event(mon::measure("nSubPopRemovalTooOld"), *this);
            cohort_set = mon::update_cohort_set(monitoring(), cohort_set, exp_it->first, false);
            // erase element, but continue iteration
            exp_it = sub_pop_exp.erase(exp_it);
        } else {
            ++exp_it;
        }
    }
}

void Human::checkpoint(istream& stream) {
    per_host_transmission.checkpoint(stream, *intervention_manager);
    inf_incidence & stream;
    within_host_model & stream;
    clinical_model & stream;
    host_rng.checkpoint(stream);
    clinical_rng.checkpoint(stream);
    intervention_rng.checkpoint(stream);
    monitoring_rng.checkpoint(stream);
    dob & stream;
    vaccine.checkpoint(stream, *intervention_manager);
    monitoring_age_group & stream;
    cohort_set & stream;
    next_cts_dist & stream;
    sub_pop_exp & stream;
}

void Human::checkpoint(ostream& stream) {
    per_host_transmission & stream;
    inf_incidence & stream;
    within_host_model & stream;
    clinical_model & stream;
    host_rng.checkpoint(stream);
    clinical_rng.checkpoint(stream);
    intervention_rng.checkpoint(stream);
    monitoring_rng.checkpoint(stream);
    dob & stream;
    vaccine & stream;
    monitoring_age_group & stream;
    cohort_set & stream;
    next_cts_dist & stream;
    sub_pop_exp & stream;
}

// -----  Non-static functions: per-time-step update  -----
void summarize(Human& human, bool survey_only_new_ep) {
    if (survey_only_new_ep && human.clinical_model->is_existing_case()) {
        // This modifies the denominator to treat the health-system-memory
        // period immediately after a bout as 'not at risk'.
        return;
    }

    mon::record_stat(mon::measure("nHost"), human);
    mon::record_stat(mon::measure("sumAge"), human, Sim::in_years(human.age(Sim::now())));
    const bool patent = human.within_host_model->summarize(human);
    human.inf_incidence->summarize(human);

    if (patent && mon::is_reported(human.monitoring())) {
        // this should happen after all other reporting!
        human.remove_first_event(interventions::sub_pop_remove::OnFirstInfection);
    }
}

void update(Human& human, Transmission::TransmissionModel& transmission, const NeonatalMortality& neonatal_mortality,
            Clinical::InfantMortality& infant_mortality, NewInfections& new_infections) {
    // For integer age checks we use age0 to e.g. get 73 steps comparing less than 1 year old
    const SimTime age0 = human.age(Sim::ts0());
    if (human.clinical_model->is_dead(age0)) {
        human.kill();
        return;
    }

    util::streamValidate(age0);

    // monitoringAgeGroup is the group for the start of the time step.
    mon::update_age_group(human.monitoring(), human.monitoring_age_group, age0);

    human.update_cohort_set();

    // Age at  the end of the update period. In most cases
    // the difference between this and age at the start is not especially
    // important in the model design, but since we parameterised with
    // age1 we should stick with it.
    const double age1 = Sim::in_years(human.age(Sim::ts1()));

    // age1 used only in PerHost::relativeAvailabilityAge(); difference to age0 should be minor
    vector<double> eir_per_genotype_i, eir_per_genotype_l;
    transmission.get_eir(human, age0, age1, eir_per_genotype_i, eir_per_genotype_l);

    const double eir_i = util::vectors::sum(eir_per_genotype_i);
    const double eir_l = util::vectors::sum(eir_per_genotype_l);
    const double eir = eir_i + eir_l;

    double x = 0.0;

    if (eir > 0)
        x = eir_i / eir;

    const double expected_n_new_infs = human.inf_incidence->expected_num_new_infections(human, eir);
    const double expected_n_new_infs_i = x * expected_n_new_infs;
    const double expected_n_new_infs_l = (1.0 - x) * expected_n_new_infs;

    // One more rng call if imported infection
    int n_new_infs_l = human.inf_incidence->num_new_infections(human, expected_n_new_infs_l);
    int n_new_infs_i = human.inf_incidence->num_new_infections(human, expected_n_new_infs_i);

    // age1 used when medicating drugs (small effect) and in immunity model (which was parameterised for it)
    human.within_host_model->update(human, human.host_rng, n_new_infs_i, n_new_infs_l, eir_per_genotype_i,
                                    eir_per_genotype_l, age1);
    human.inf_incidence->report_num_new_infections(human, n_new_infs_i, n_new_infs_l, new_infections);

    // age1 used to get case fatality and sequelae probabilities, determine pathogenesis
    human.clinical_model->update(human, age1, age0 == Sim::zero(), neonatal_mortality);
    human.clinical_model->update_infant_deaths(age0, infant_mortality);
}

}
