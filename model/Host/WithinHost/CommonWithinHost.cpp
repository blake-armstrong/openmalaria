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
#include "Host/WithinHost/CommonWithinHost.h"
#include "Host/WithinHost/Diagnostic.h"
#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "util/AgeGroupInterpolation.h"
#include "util/random.h"
#include "util/StreamValidator.h"
#include "schema/scenario.h"

#include <algorithm>

using namespace std;

namespace OM {
namespace WithinHost {

CommonInfection* (*CommonWithinHost::create_infection)(LocalRng& rng, uint32_t prot_id, int origin);
CommonInfection* (*CommonWithinHost::checkpointed_infection)(istream& stream);

// -----  Initialization  -----

CommonWithinHostParams::CommonWithinHostParams(const scnXml::Scenario& scenario) {
    const scnXml::Human human = scenario.getModel().getHuman();
    if (!human.getWeight().present()) {
        // Technically this is needed by the PK/PD and Molineaux models
        throw util::xml_scenario_error("model->human->weight element required by certain models");
    }
    mass_by_age.set(human.getWeight().get(), "weight");
    het_mass_mult_std_dev = human.getWeight().get().getMultStdDev();
    // hetWeightMult must be large enough that birth weight is at least 0.5 kg:
    min_het_mass_mult = 0.5 / mass_by_age.eval(0.0);

    report_infected_or_patent_infected =
        mon::isUsed(mon::measure("totalInfs")) || mon::isUsed(mon::measure("totalPatentInf"));
}

CommonWithinHost::CommonWithinHost(const FalciparumParams& params, const CommonWithinHostParams& common_params,
                                   LocalRng& rng, double comorbidity_factor)
    : WHFalciparum(params, rng, comorbidity_factor), common_params(&common_params) {
    assert(Sim::one_ts() == Sim::from_days(1) || Sim::one_ts() == Sim::from_days(5));

    opt_vaccine_genotype = params.vaccine_genotype;

    // Sample a weight heterogeneity factor
#ifndef NDEBUG
    int counter = 0;
#endif
    do {
        het_mass_multiplier = rng.gauss(1.0, common_params.het_mass_mult_std_dev);
#ifndef NDEBUG
        assert(counter < 100); // too many resamples: resamples should rarely be needed...
        ++counter;
#endif
    } while (het_mass_multiplier < common_params.min_het_mass_mult);
}

CommonWithinHost::~CommonWithinHost() {
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        delete *inf;
    }
    infections.clear();
}

// -----  Simple infection adders/removers  -----

void CommonWithinHost::clear_infections(Treatments::Stages stage) {
    for (auto inf = infections.begin(); inf != infections.end();) {
        if (stage == Treatments::BOTH || (stage == Treatments::LIVER && !(*inf)->bloodStage()) ||
            (stage == Treatments::BLOOD && (*inf)->bloodStage())) {
            delete *inf;
            inf = infections.erase(inf);
        } else {
            ++inf;
        }
    }
    num_infs = infections.size();
}

// -----  interventions -----

void CommonWithinHost::treat_pk_pd(size_t schedule, size_t dosage, double age, double delay_d) {
    double mass = common_params->mass_by_age.eval(age) * het_mass_multiplier;
    pkpd_model.prescribe(schedule, dosage, age, mass, delay_d);
}
void CommonWithinHost::clear_immunity() {
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        (*inf)->clearImmunity();
    }
    cumulative_h = 0.0;
    cumulative_y_lag = 0.0;
}
void CommonWithinHost::import_infection(LocalRng& rng) {
    if (num_infs < MaxInfections) {
        cumulative_h += 1;
        num_infs += 1;
        // This is a hook, used by interventions. The newly imported infections
        // should use initial frequencies to select genotypes.
        vector<double> weights(0); // zero length: signal to use initial frequencies
        uint32_t genotype = Genotypes::sampleGenotype(rng, weights);
        infections.push_back(create_infection(rng, genotype, WithinHost::InfectionOrigin::Imported));
    }
    assert(num_infs == static_cast<int>(infections.size()));
}

// -----  Density calculations  -----

void CommonWithinHost::update(Host::Human& human, LocalRng& rng, int& n_new_infs_i, int& n_new_infs_l,
                              vector<double>& genotype_weights_i, vector<double>& genotype_weights_l,
                              double age_in_years) {
    // Note: adding infections at the beginning of the update instead of the end
    // shouldn't be significant since before latentp delay nothing is updated.
    n_new_infs_l = min(n_new_infs_l, MaxInfections - num_infs);
    n_new_infs_i = min(n_new_infs_i, MaxInfections - num_infs - n_new_infs_l);

    int n_new_infs_ignored = n_new_infs_i + n_new_infs_l - (MaxInfections - num_infs);

    assert(num_infs >= 0 && num_infs <= MaxInfections);

    int n_new_infs_discarded = 0;
    for (int i = 0; i < n_new_infs_i; ++i) {
        uint32_t genotype = Genotypes::sampleGenotype(rng, genotype_weights_i);

        // If opt_vaccine_genotype is true the infection is discarded with probability 1-vaccineFactor
        if (opt_vaccine_genotype) {
            double vaccine_factor = human.vaccine.getFactor(interventions::Vaccine::PEV, genotype);
            if (vaccine_factor == 1.0 || human.host_rng.bernoulli(vaccine_factor))
                infections.push_back(create_infection(rng, genotype, InfectionOrigin::Introduced));
            else
                n_new_infs_discarded++;
        } else if (opt_vaccine_genotype == false)
            infections.push_back(create_infection(rng, genotype, InfectionOrigin::Introduced));
    }
    // Update nNewInfs, this is the number that will be reported in Human
    n_new_infs_i -= n_new_infs_discarded;
    num_infs += n_new_infs_i;

    n_new_infs_discarded = 0;
    for (int i = 0; i < n_new_infs_l; ++i) {
        uint32_t genotype = Genotypes::sampleGenotype(rng, genotype_weights_l);

        // If opt_vaccine_genotype is true the infection is discarded with probability 1-vaccineFactor
        if (opt_vaccine_genotype) {
            double vaccine_factor = human.vaccine.getFactor(interventions::Vaccine::PEV, genotype);
            if (vaccine_factor == 1.0 || human.host_rng.bernoulli(vaccine_factor))
                infections.push_back(create_infection(rng, genotype, InfectionOrigin::Indigenous));
            else
                n_new_infs_discarded++;
        } else if (opt_vaccine_genotype == false)
            infections.push_back(create_infection(rng, genotype, InfectionOrigin::Indigenous));
    }
    // Update nNewInfs, this is the number that will be reported in Human
    n_new_infs_l -= n_new_infs_discarded;
    num_infs += n_new_infs_l;

    assert(num_infs == static_cast<int>(infections.size()));

    update_immune_status();

    total_density = 0.0;
    hrp2_density = 0.0;
    time_step_max_density = 0.0;

    bool treatment_liver = treat_expiry_liver > Sim::ts0();
    bool treatment_blood = treat_expiry_blood > Sim::ts0();

    double body_mass = common_params->mass_by_age.eval(age_in_years) * het_mass_multiplier;

    for (SimTime now = Sim::ts0(), end = Sim::ts0() + Sim::one_ts(); now < end; now = now + Sim::one_day()) {
        // every day, medicate drugs, update each infection, then decay drugs
        pkpd_model.medicate(rng);

        double sum_log_dens = 0.0;

        for (auto inf = infections.begin(); inf != infections.end();) {
            // Note: this is only one treatment model; there is also the PK/PD model
            bool expires = ((*inf)->bloodStage() ? treatment_blood : treatment_liver);

            if (!expires) { /* no expiry due to simple treatment model; do update */
                const double drug_factor = pkpd_model.get_drug_factor(rng, *inf, body_mass);
                const double imm_factor = immunity_survival_factor(age_in_years, (*inf)->cumulativeExposureJ());
                const double bsv_factor =
                    human.vaccine.getFactor(interventions::Vaccine::BSV, opt_vaccine_genotype ? (*inf)->genotype() : 0);
                const double survival_factor = bsv_factor * innate_imm_surv_fact * imm_factor * drug_factor;
                // update, may result in termination of infection:
                expires = (*inf)->update(rng, survival_factor, now, body_mass);
            }

            if (expires) {
                delete *inf;
                inf = infections.erase(inf); // inf points to next infection now so don't increment with ++inf
                --num_infs;
            } else {
                double density = (*inf)->getDensity();
                total_density += density;
                if (!(*inf)->isHrp2Deficient()) {
                    hrp2_density += density;
                }
                time_step_max_density = max(time_step_max_density, density);
                if (density > 0) {
                    // Base 10 logarithms are usually used; +1 because it avoids negatives in output while having very little affect on high densities
                    sum_log_dens += log10(1.0 + density);
                }
                ++inf;
            }
        }
        pkpd_model.decay_drugs(body_mass);
    }

    // As in AJTMH p22, cumulative_h (X_h + 1) doesn't include infections added
    // this time-step and cumulative_Y only includes past densities, thus we
    // increment these after the update.
    cumulative_h += n_new_infs_i + n_new_infs_l;
    cumulative_y += total_density;

    util::streamValidate(total_density);
    util::streamValidate(hrp2_density);
    assert((std::isfinite)(total_density)); // inf probably wouldn't be a problem but NaN would be

    // Cache total density for infectiousness calculations
    int y_lag_i = Sim::modulo_steps(Sim::ts1(), params().y_lag_len);
    for (size_t g = 0; g < Genotypes::N(); ++g) {
        y_lag_imported[y_lag_i * Genotypes::N() + g] = 0.0;
        y_lag_local[y_lag_i * Genotypes::N() + g] = 0.0;
    }

    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        if ((*inf)->origin() == InfectionOrigin::Imported)
            y_lag_imported[y_lag_i * Genotypes::N() + (*inf)->genotype()] += (*inf)->getDensity();
        else
            y_lag_local[y_lag_i * Genotypes::N() + (*inf)->genotype()] += (*inf)->getDensity();
    }

    // This is a bug, we keep it this way to be consistent with old simulations
    if (n_new_infs_ignored > 0)
        n_new_infs_l += n_new_infs_ignored;
}

void CommonWithinHost::add_prophylactic_effects(const vector<double>& p_clearance_by_time) {
    // this should actually be easy; it just isn't needed yet
    throw util::unimplemented_exception("prophylactic effects on 1-day time step");
}

InfectionOrigin CommonWithinHost::infection_origin() const { return get_infection_origin(infections); }

// -----  Summarize  -----

struct InfGenotypeSorter {
    bool operator()(CommonInfection* i, CommonInfection* j) { return i->genotype() < j->genotype(); }
} inf_genotype_sorter;

bool CommonWithinHost::summarize(Host::Human& human) const {
    pathogenesis_model->summarize(human);
    pkpd_model.summarize(human);

    InfectionOrigin infection_type = get_infection_origin(infections);

    // If the number of infections is 0 and parasite density is positive we default to Indigenous
    if (infections.size() > 0) {
        mon::recordStat(mon::measure("nInfect"), human);
        if (infection_type == InfectionOrigin::Indigenous)
            mon::recordStat(mon::measure("nInfect_Indigenous"), human);
        else if (infection_type == InfectionOrigin::Introduced)
            mon::recordStat(mon::measure("nInfect_Introduced"), human);
        else
            mon::recordStat(mon::measure("nInfect_Imported"), human);

        if (common_params->report_infected_or_patent_infected) {
            for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
                uint32_t genotype = (*inf)->genotype();
                mon::recordStat(mon::measure("totalInfs"), human, 1, 0, genotype, 0);
                if ((*inf)->origin() == InfectionOrigin::Indigenous)
                    mon::recordStat(mon::measure("totalInfs_Indigenous"), human, 1, 0, genotype, 0);
                else if ((*inf)->origin() == InfectionOrigin::Introduced)
                    mon::recordStat(mon::measure("totalInfs_Introduced"), human, 1, 0, genotype, 0);
                else
                    mon::recordStat(mon::measure("totalInfs_Imported"), human, 1, 0, genotype, 0);

                if (diagnostics::monitoringDiagnostic().isPositive(human.monitoring_rng, (*inf)->getDensity(),
                                                                   std::numeric_limits<double>::quiet_NaN())) {
                    mon::recordStat(mon::measure("totalPatentInf"), human, 1, 0, genotype, 0);
                    if ((*inf)->origin() == InfectionOrigin::Indigenous)
                        mon::recordStat(mon::measure("totalPatentInf_Indigenous"), human, 1, 0, genotype, 0);
                    else if ((*inf)->origin() == InfectionOrigin::Introduced)
                        mon::recordStat(mon::measure("totalPatentInf_Introduced"), human, 1, 0, genotype, 0);
                    else
                        mon::recordStat(mon::measure("totalPatentInf_Imported"), human, 1, 0, genotype, 0);
                }
            }
        }

        if (params().report_infections_by_genotype) {
            // Instead of storing nInfs and total density by genotype we sort
            // infections by genotype and report each in sequence.
            // We don't sort in place since that would affect random number sampling
            // order when updating, and the monitoring system should not in my
            // opinion affect outputs (since it would make testing harder).
            vector<CommonInfection*> sorted_infs(infections.begin(), infections.end());
            std::ranges::sort(sorted_infs, inf_genotype_sorter);
            auto inf = sorted_infs.begin();
            while (inf != sorted_infs.end()) {
                uint32_t genotype = (*inf)->genotype();
                double dens = 0.0;
                do { // at start: genotype is that of the current infection, dens is 0
                    dens += (*inf)->getDensity();
                    ++inf;
                } while (inf != sorted_infs.end() && (*inf)->genotype() == genotype);
                // we had at least one infection of this genotype
                mon::recordStat(mon::measure("nInfectByGenotype"), human, 1, 0, genotype, 0);
                if (diagnostics::monitoringDiagnostic().isPositive(human.monitoring_rng, dens,
                                                                   std::numeric_limits<double>::quiet_NaN())) {
                    mon::recordStat(mon::measure("nPatentByGenotype"), human, 1, 0, genotype, 0);
                    mon::recordStat(mon::measure("logDensByGenotype"), human, log(dens), 0, genotype, 0);
                }
            }
        }
    }

    // Some treatments (simpleTreat with steps=-1) clear infections immediately
    // (and are applied after update()), thus infections.size() may be 0 while
    // totalDensity > 0. Here we report the last calculated density.
    if (diagnostics::monitoringDiagnostic().isPositive(human.monitoring_rng, total_density,
                                                       std::numeric_limits<double>::quiet_NaN())) {
        mon::recordStat(mon::measure("nPatent"), human);
        if (infection_type == InfectionOrigin::Imported)
            mon::recordStat(mon::measure("nPatent_Imported"), human);
        else if (infection_type == InfectionOrigin::Introduced)
            mon::recordStat(mon::measure("nPatent_Introduced"), human);
        else if (infection_type == InfectionOrigin::Indigenous)
            mon::recordStat(mon::measure("nPatent_Indigenous"), human);

        if (total_density == 0.0)
            mon::recordStat(mon::measure("sumlogDens"), human, 0.0);
        else
            mon::recordStat(mon::measure("sumlogDens"), human, log(total_density));
        return true; // patent
    }
    return false; // not patent
}

void CommonWithinHost::checkpoint(istream& stream) {
    WHFalciparum::checkpoint(stream);
    het_mass_multiplier & stream;
    pkpd_model & stream;
    for (int i = 0; i < num_infs; ++i) {
        infections.push_back(checkpointed_infection(stream));
    }
    assert(num_infs == static_cast<int>(infections.size()));
}

void CommonWithinHost::checkpoint(ostream& stream) {
    WHFalciparum::checkpoint(stream);
    het_mass_multiplier & stream;
    pkpd_model & stream;
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        (**inf) & stream;
    }
}
}
}
