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

#include "Global.h"
#include "Host/Human.h"
#include "Host/WithinHost/DescriptiveWithinHost.h"
#include "Host/WithinHost/Diagnostic.h"
#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "util/ModelOptions.h"
#include "util/StreamValidator.h"
#include "util/errors.h"

#include <cassert>

using namespace std;

namespace OM::WithinHost {

// -----  Initialization  -----

DescriptiveWithinHostModel::DescriptiveWithinHostModel(const FalciparumParams& params,
                                                       const DescriptiveWithinHostParams& descriptive_params,
                                                       LocalRng& rng, double comorbidity_factor)
    : WHFalciparum(params, rng, comorbidity_factor), descriptive_params(&descriptive_params) {
    assert(Sim::one_ts() == Sim::from_days(5));
    opt_vaccine_genotype = params.vaccine_genotype;
}

// -----  Simple infection adders/removers  -----

std::unique_ptr<DescriptiveInfection> DescriptiveWithinHostModel::new_infection(LocalRng& rng, uint32_t genotype,
                                                                                int origin) const {
    return std::make_unique<DescriptiveInfection>(descriptive_params->infection, params().latent_p, rng, genotype,
                                                  origin);
}

void DescriptiveWithinHostModel::load_infection(istream& stream) {
    infections.push_back(
        std::make_unique<DescriptiveInfection>(descriptive_params->infection, params().latent_p, stream));
}

void DescriptiveWithinHostModel::clear_infections(Treatments::Stages stage) {
    for (auto inf = infections.begin(); inf != infections.end();) {
        if (stage == Treatments::BOTH || (stage == Treatments::LIVER && !(*inf)->bloodStage()) ||
            (stage == Treatments::BLOOD && (*inf)->bloodStage())) {
            inf = infections.erase(inf);
        } else {
            ++inf;
        }
    }
    num_infs = static_cast<int>(infections.size());
}

// -----  Interventions  -----

void DescriptiveWithinHostModel::clear_immunity() {
    for (auto& infection : infections) {
        infection->clearImmunity();
    }
    cumulative_h = 0.0;
    cumulative_y_lag = 0.0;
}

void DescriptiveWithinHostModel::import_infection(LocalRng& rng) {
    if (num_infs < MaxInfections) {
        cumulative_h += 1;
        num_infs += 1;
        // This is a hook, used by interventions. The newly imported infections
        // should use initial frequencies to select genotypes.
        vector<double> weights(0); // zero length: signal to use initial frequencies
        const uint32_t genotype = Genotypes::sampleGenotype(rng, weights);
        infections.push_back(new_infection(rng, genotype, WithinHost::InfectionOrigin::Imported));
    }
    assert(num_infs == static_cast<int>(infections.size()));
}

// -----  Density calculations  -----

void DescriptiveWithinHostModel::update(Host::Human& human, LocalRng& rng, int& n_new_infs_i, int& n_new_infs_l,
                                        vector<double>& genotype_weights_i, vector<double>& genotype_weights_l,
                                        double age_in_years) {
    // Note: adding infections at the beginning of the update instead of the end
    // shouldn't be significant since before latentp delay nothing is updated.
    const int n_new_infs_to_be_created_i = n_new_infs_i;
    const int n_new_infs_to_be_created_l = n_new_infs_l;

    n_new_infs_l = min(n_new_infs_l, MaxInfections - num_infs);
    n_new_infs_i = min(n_new_infs_i, MaxInfections - num_infs - n_new_infs_l);

    num_infs += n_new_infs_i;
    assert(num_infs >= 0 && num_infs <= MaxInfections);
    for (int i = 0; i < n_new_infs_i; ++i) {
        const uint32_t genotype = Genotypes::sampleGenotype(rng, genotype_weights_i);

        // If opt_vaccine_genotype is true the infection is discarded with probability 1-vaccineFactor
        if (opt_vaccine_genotype) {
            const double vaccine_factor = human.vaccine.getFactor(interventions::Vaccine::PEV, genotype);
            if (vaccine_factor == 1.0 || human.host_rng.bernoulli(vaccine_factor))
                infections.push_back(new_infection(rng, genotype, InfectionOrigin::Introduced));
        } else if (opt_vaccine_genotype == false)
            infections.push_back(new_infection(rng, genotype, InfectionOrigin::Introduced));
    }
    assert(num_infs == static_cast<int>(infections.size()));

    num_infs += n_new_infs_l;
    assert(num_infs >= 0 && num_infs <= MaxInfections);
    for (int i = 0; i < n_new_infs_l; ++i) {
        const uint32_t genotype = Genotypes::sampleGenotype(rng, genotype_weights_l);

        // If opt_vaccine_genotype is true the infection is discarded with probability 1-vaccineFactor
        if (opt_vaccine_genotype) {
            const double vaccine_factor = human.vaccine.getFactor(interventions::Vaccine::PEV, genotype);
            if (vaccine_factor == 1.0 || human.host_rng.bernoulli(vaccine_factor))
                infections.push_back(new_infection(rng, genotype, InfectionOrigin::Indigenous));
        } else if (opt_vaccine_genotype == false)
            infections.push_back(new_infection(rng, genotype, InfectionOrigin::Indigenous));
    }
    assert(num_infs == static_cast<int>(infections.size()));

    update_immune_status();

    total_density = 0.0;
    hrp2_density = 0.0;
    time_step_max_density = 0.0;

    const bool treatment_liver = treat_expiry_liver > Sim::ts0();
    const bool treatment_blood = treat_expiry_blood > Sim::ts0();

    for (auto inf = infections.begin(); inf != infections.end();) {
        //NOTE: it would be nice to combine this code with that in
        // CommonWithinHost.cpp, but a few changes would be needed:
        // INNATE_MAX_DENS and MAX_DENS_CORRECTION would need to be required
        // (couldn't support old parameterisations using buggy versions of code
        // any more).
        // SP drug action and the PK/PD model would need to be abstracted
        // behind a common interface.
        if ((*inf)->expired() /* infection has self-terminated */ ||
            ((*inf)->bloodStage() ? treatment_blood : treatment_liver)) {
            inf = infections.erase(inf);
            num_infs--;
            continue;
        }

        // Should be: infStepMaxDens = 0.0, but has some history.
        // See MAX_DENS_CORRECTION in DescriptiveInfection.cpp.
        double inf_step_max_dens = time_step_max_density;
        const double imm_surv_fact = immunity_survival_factor(age_in_years, (*inf)->cumulativeExposureJ());
        const double bsv_factor =
            human.vaccine.getFactor(interventions::Vaccine::BSV, opt_vaccine_genotype ? (*inf)->genotype() : 0);

        (*inf)->determineDensities(rng, cumulative_h, inf_step_max_dens, imm_surv_fact, innate_imm_surv_fact,
                                   bsv_factor);

        if (descriptive_params->infection.bugfix_max_dens)
            inf_step_max_dens = std::max(inf_step_max_dens, time_step_max_density);
        time_step_max_density = inf_step_max_dens;

        const double density = (*inf)->getDensity();
        total_density += density;
        if (!(*inf)->isHrp2Deficient()) {
            hrp2_density += density;
        }

        ++inf;
    }

    // As in AJTMH p22, cumulative_h (X_h + 1) doesn't include infections added
    // this time-step and cumulative_Y only includes past densities.
    cumulative_h += n_new_infs_i + n_new_infs_l;
    cumulative_y += Sim::one_ts() * total_density;

    util::streamValidate(total_density);
    util::streamValidate(hrp2_density);
    assert((std::isfinite)(total_density)); // inf probably wouldn't be a problem but NaN would be

    // Cache total density for infectiousness calculations
    const int y_lag_i = Sim::modulo_steps(Sim::ts1(), params().y_lag_len);

    for (size_t g = 0; g < Genotypes::N(); ++g) {
        y_lag_imported[y_lag_i * Genotypes::N() + g] = 0.0;
        y_lag_local[y_lag_i * Genotypes::N() + g] = 0.0;
    }

    for (auto& infection : infections) {
        if (infection->origin() == InfectionOrigin::Imported)
            y_lag_imported[y_lag_i * Genotypes::N() + infection->genotype()] += infection->getDensity();
        else
            y_lag_local[y_lag_i * Genotypes::N() + infection->genotype()] += infection->getDensity();
    }

    // This is a bug, we keep it this way to be consistent with old simulations
    if (opt_vaccine_genotype == false) {
        n_new_infs_i = n_new_infs_to_be_created_i;
        n_new_infs_l = n_new_infs_to_be_created_l;
    }
}

InfectionOrigin DescriptiveWithinHostModel::infection_origin() const { return get_infection_origin(infections); }

// -----  Summarize  -----

bool DescriptiveWithinHostModel::summarize(Host::Human& human) const {
    pathogenesis_model->summarize(human);

    const InfectionOrigin infection_type = get_infection_origin(infections);

    // If the number of infections is 0 and parasite density is positive we default to Indigenous
    if (infections.size() > 0) {
        mon::recordStat(mon::measure("nInfect"), human);
        if (infection_type == InfectionOrigin::Indigenous)
            mon::recordStat(mon::measure("nInfect_Indigenous"), human);
        else if (infection_type == InfectionOrigin::Introduced)
            mon::recordStat(mon::measure("nInfect_Introduced"), human);
        else
            mon::recordStat(mon::measure("nInfect_Imported"), human);

        int n_imported = 0, n_introduced = 0, n_indigenous = 0;
        for (const auto& infection : infections) {
            if (infection->origin() == InfectionOrigin::Indigenous)
                n_indigenous++;
            else if (infection->origin() == InfectionOrigin::Introduced)
                n_introduced++;
            else
                n_imported++;
        }

        // (patent) infections are reported by genotype, even though we don't have
        // genotype in this model
        mon::recordStat(mon::measure("totalInfs"), human, static_cast<int>(infections.size()));
        mon::recordStat(mon::measure("totalInfs_Imported"), human, n_imported);
        mon::recordStat(mon::measure("totalInfs_Introduced"), human, n_introduced);
        mon::recordStat(mon::measure("totalInfs_Indigenous"), human, n_indigenous);

        if (descriptive_params->report_patent_infected) {
            for (const auto& infection : infections) {
                if (diagnostics::monitoringDiagnostic().isPositive(human.monitoring_rng, infection->getDensity(),
                                                                   std::numeric_limits<double>::quiet_NaN())) {
                    mon::recordStat(mon::measure("totalPatentInf"), human);
                    if (infection->origin() == InfectionOrigin::Indigenous)
                        mon::recordStat(mon::measure("totalPatentInf_Indigenous"), human);
                    else if (infection->origin() == InfectionOrigin::Introduced)
                        mon::recordStat(mon::measure("totalPatentInf_Introduced"), human);
                    else
                        mon::recordStat(mon::measure("totalPatentInf_Imported"), human);
                }
            }
        }
        if (params().report_infections_by_genotype) {
            // accumulate total density by genotype
            map<uint32_t, double> dens_by_gtype;
            for (const auto& infection : infections)
                dens_by_gtype[infection->genotype()] += infection->getDensity();

            for (auto gtype : dens_by_gtype) {
                // we had at least one infection of this genotype
                mon::recordStat(mon::measure("nInfectByGenotype"), human, 1, 0, gtype.first, 0);
                if (diagnostics::monitoringDiagnostic().isPositive(human.monitoring_rng, gtype.second,
                                                                   std::numeric_limits<double>::quiet_NaN())) {
                    mon::recordStat(mon::measure("nPatentByGenotype"), human, 1, 0, gtype.first, 0);
                    mon::recordStat(mon::measure("logDensByGenotype"), human, log(gtype.second), 0, gtype.first, 0);
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

        if (total_density > 1e-10)
            mon::recordStat(mon::measure("sumlogDens"), human, log(total_density));
        return true; // patent
    }
    return false; // not patent
}

// -----  Data checkpointing  -----

void DescriptiveWithinHostModel::checkpoint(istream& stream) {
    WHFalciparum::checkpoint(stream);
    for (int i = 0; i < num_infs; ++i) {
        load_infection(stream); // create infections using a virtual function call
    }
    assert(num_infs == static_cast<int>(infections.size()));
}
void DescriptiveWithinHostModel::checkpoint(ostream& stream) {
    WHFalciparum::checkpoint(stream);
    for (const auto& inf : infections)
        *inf& stream;
}

char const* const NotImpl = "feature not available with the \"descriptive\" within-host model";
void DescriptiveWithinHostModel::treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d) {
    throw TRACED_EXCEPTION(NotImpl, util::Error::WHFeatures);
}

}
