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

#include "Transmission/VectorModel.h"
#include "Population.h"
#include "Host/WithinHost/WHInterface.h"
#include "mon/Continuous.h"
#include "util/vectors.h"
#include "util/ModelOptions.h"
#include "util/SpeciesIndexChecker.h"
#include "util/StreamValidator.h"
#include "Transmission/Anopheles/SimpleMPDAnophelesModel.h"

#include <fstream>
#include <map>
#include <cmath>
#include <set>

namespace OM::Transmission {
using namespace OM::util;
using Anopheles::AnophelesModel;
using Anopheles::SimpleMPDAnophelesModel;

void VectorModel::cts_cb_n_v0(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_n_v0();
}
void VectorModel::cts_cb_p_a(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PA);
}
void VectorModel::cts_cb_p_amu(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PAmu);
}
void VectorModel::cts_cb_p_a1(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PA1);
}
void VectorModel::cts_cb_p_ah(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PAh);
}
void VectorModel::cts_cb_p_df(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PDF);
}
void VectorModel::cts_cb_p_dif(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::PDIF);
}
void VectorModel::cts_cb_n_v(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::NV);
}
void VectorModel::cts_cb_o_v(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::OV);
}
void VectorModel::cts_cb_s_v(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_last_vec_stat(Anopheles::SV);
}
void VectorModel::cts_cb_alpha(const vector<Host::Human>& population, ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i) {
        double total = 0.0;
        for (const Host::Human& human : population) {
            total += human.perHostTransmission.ento_availability_full(i, Sim::in_years(human.age(Sim::now())));
        }
        stream << '\t' << total / population.size();
    }
}
void VectorModel::cts_cb_p_b(const vector<Host::Human>& population, ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i) {
        double total = 0.0;
        for (const Host::Human& human : population) {
            total += human.perHostTransmission.prob_mosq_biting(i);
        }
        stream << '\t' << total / population.size();
    }
}
void VectorModel::cts_cb_p_cd(const vector<Host::Human>& population, ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i) {
        double total = 0.0;
        for (const Host::Human& human : population) {
            total += human.perHostTransmission.prob_mosq_resting(i);
        }
        stream << '\t' << total / population.size();
    }
}
void VectorModel::cts_net_insecticide_content(const vector<Host::Human>& population, ostream& stream) {
    //     double meanVar = 0.0;
    //     int n = 0;
    //     for(const Host::Human &human : population) {
    //         if( human.perHostTransmission.getITN().timeOfDeployment() >= sim::zero() ){
    //             ++n;
    //             meanVar += human.perHostTransmission.getITN().getInsecticideContent(_ITNParams);
    //         }
    //     }
    //     stream << '\t' << meanVar/n;
}
void VectorModel::cts_irs_insecticide_content(const vector<Host::Human>& population, ostream& stream) {
    // TODO(monitoring): work out how this applies when multiple IRS effects are allowed
    //     double totalInsecticide = 0.0;
    //     for(const Host::Human &human : population) {
    //         totalInsecticide += human.perHostTransmission.getIRS().getInsecticideContent(_IRSParams);
    //     }
    //     stream << '\t' << totalInsecticide / population.size();
}
void VectorModel::cts_irs_effects(const vector<Host::Human>& population, ostream& stream) {
    // TODO(monitoring): work out how this applies when multiple IRS effects are allowed
    //     for( size_t i = 0; i < speciesIndex.size(); ++i ){
    //         const interventions::IRSAnophelesParams& params = species[i]->getHumanBaseParams().irs;
    //         double totalRA = 0.0, totalPrePSF = 0.0, totalPostPSF = 0.0;
    //         for(const Host::Human &human : population) {
    //             totalRA += human.perHostTransmission.getIRS().relativeAttractiveness(params);
    //             totalPrePSF += human.perHostTransmission.getIRS().preprandialSurvivalFactor(params);
    //             totalPostPSF += human.perHostTransmission.getIRS().postprandialSurvivalFactor(params);
    //         }
    //         stream << '\t' << totalRA / population.size()
    //             << '\t' << totalPrePSF / population.size()
    //             << '\t' << totalPostPSF / population.size();
    //     }
}

void VectorModel::cts_cb_res_availability(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_res_availability();
}
void VectorModel::cts_cb_res_requirements(ostream& stream) {
    for (size_t i = 0; i < species_index.size(); ++i)
        stream << '\t' << species[i]->get_res_requirements();
}

const string& reverse_lookup(const map<string, size_t>& m, size_t i) {
    for (const auto& it : m) {
        if (it.second == i)
            return it.first;
    }
    throw TRACED_EXCEPTION_DEFAULT("reverseLookup: key not found"); // shouldn't ever happen
}

VectorModel::VectorModel(vector<double> init_eir, int intervention_mode,
                         vector<std::unique_ptr<Anopheles::AnophelesModel>> species_list,
                         vector<std::unique_ptr<Anopheles::EmergenceRateEstimator>> emergence_rate_estimators_list,
                         map<string, size_t> species_index_list, int population_size, util::MasterRng& transmission_rng,
                         size_t n_genotypes, SimTime adult_age)
    : TransmissionModel(std::move(init_eir), intervention_mode, n_genotypes, adult_age), rng(transmission_rng),
      quinquennial_kappa(5 * Sim::steps_per_year(), 0.0), species(std::move(species_list)),
      emergence_rate_estimators(std::move(emergence_rate_estimators_list)),
      species_index(std::move(species_index_list)),
      opt_vaccine_genotype(util::ModelOptions::option(util::VaccineGenotype)) {
    // set VACCINE_GENOTYPE option

    // Calculate total annual EIR
    annual_eir = vectors::sum(initialisation_eir);

    // -----  Continuous reporting  -----
    ostringstream cts_nv0, cts_pa, cts_p_amu, cts_p_a1, cts_p_ah, cts_pdf, cts_pdif, cts_nv, cts_ov, cts_sv, cts_alpha,
        cts_pb, cts_pcd, cts_irs_effects, cts_ra, cts_rr;

    size_t num_species = species.size();

    // Output in order of species so that (1) we can just iterate through this
    // list when outputting and (2) output is in order specified in XML.
    for (size_t i = 0; i < num_species; ++i) {
        // Unfortunately, we need to reverse-lookup the name.
        const string& name = reverse_lookup(species_index, i);
        cts_nv0 << "\tN_v0(" << name << ")";
        cts_pa << "\tP_A(" << name << ")";
        cts_p_amu << "\tP_Amu(" << name << ")";
        cts_p_a1 << "\tP_A1(" << name << ")";
        cts_p_ah << "\tP_Ah(" << name << ")";
        cts_pdf << "\tP_df(" << name << ")";
        cts_pdif << "\tP_dif(" << name << ")";
        cts_nv << "\tN_v(" << name << ")";
        cts_ov << "\tO_v(" << name << ")";
        cts_sv << "\tS_v(" << name << ")";
        cts_alpha << "\talpha_i(" << name << ")";
        cts_pb << "\tP_B(" << name << ")";
        cts_pcd << "\tP_C*P_D(" << name << ")";
        cts_irs_effects << "\tIRS rel attr (" << name << ")"
                        << "\tIRS preprand surv factor (" << name << ")"
                        << "\tIRS postprand surv factor (" << name << ")";
        cts_ra << "\tres avail(" << name << ")";
        cts_rr << "\tres req(" << name << ")";
    }

    mon::Continuous::registerCallback("N_v0", cts_nv0.str(), std::bind(&VectorModel::cts_cb_n_v0, this, _1));
    mon::Continuous::registerCallback("P_A", cts_pa.str(), std::bind(&VectorModel::cts_cb_p_a, this, _1));
    mon::Continuous::registerCallback("P_Amu", cts_p_amu.str(), std::bind(&VectorModel::cts_cb_p_amu, this, _1));
    mon::Continuous::registerCallback("P_A1", cts_p_a1.str(), std::bind(&VectorModel::cts_cb_p_a1, this, _1));
    mon::Continuous::registerCallback("P_Ah", cts_p_ah.str(), std::bind(&VectorModel::cts_cb_p_ah, this, _1));
    mon::Continuous::registerCallback("P_df", cts_pdf.str(), std::bind(&VectorModel::cts_cb_p_df, this, _1));
    mon::Continuous::registerCallback("P_dif", cts_pdif.str(), std::bind(&VectorModel::cts_cb_p_dif, this, _1));
    mon::Continuous::registerCallback("N_v", cts_nv.str(), std::bind(&VectorModel::cts_cb_n_v, this, _1));
    mon::Continuous::registerCallback("O_v", cts_ov.str(), std::bind(&VectorModel::cts_cb_o_v, this, _1));
    mon::Continuous::registerCallback("S_v", cts_sv.str(), std::bind(&VectorModel::cts_cb_s_v, this, _1));

    // availability to mosquitoes relative to other humans, excluding age factor
    mon::Continuous::registerCallback("alpha", cts_alpha.str(), std::bind(&VectorModel::cts_cb_alpha, this, _1, _2));
    mon::Continuous::registerCallback("P_B", cts_pb.str(), std::bind(&VectorModel::cts_cb_p_b, this, _1, _2));
    mon::Continuous::registerCallback("P_C*P_D", cts_pcd.str(), std::bind(&VectorModel::cts_cb_p_cd, this, _1, _2));

    mon::Continuous::registerCallback("resource availability", cts_ra.str(),
                                      std::bind(&VectorModel::cts_cb_res_availability, this, _1));
    mon::Continuous::registerCallback("resource requirements", cts_rr.str(),
                                      std::bind(&VectorModel::cts_cb_res_requirements, this, _1));
}

VectorModel::~VectorModel() = default;

void VectorModel::init2(const vector<Host::Human>& population) {
    double sum_relative_availability = 0.0;
    for (const Host::Human& human : population) {
        sum_relative_availability +=
            human.perHostTransmission.relative_availability_age(Sim::in_years(human.age(Sim::now())));
    }
    int pop_size = population.size();
    // value should be unimportant when no humans are available, though inf/nan is not acceptable
    if (pop_size > 0) {
        mean_avail = sum_relative_availability / pop_size; // mean-rel-avail
    }

    for (size_t i = 0; i < species_index.size(); ++i) {
        double sum_avail = 0.0;
        double sigma_f = 0.0;
        double sigma_df = 0.0;
        double sigma_dff = 0.0;

        for (const Host::Human& human : population) {
            const OM::Transmission::PerHost& host = human.perHostTransmission;
            double prod = host.ento_availability_full(i, Sim::in_years(human.age(Sim::now())));
            sum_avail += prod;
            prod *= host.prob_mosq_biting(i);
            sigma_f += prod;
            sigma_df += prod * host.prob_mosq_resting(i);
            sigma_dff += prod * host.prob_mosq_resting(i) * host.rel_mosq_fecundity(i);
        }

        species[i]->init2(population.size(), mean_avail, sum_avail, sigma_f, sigma_df, sigma_dff);
    }
    simulation_mode = ForcedEir; // now we should be ready to start
}

void VectorModel::init_vector_interv(const scnXml::Description::AnophelesSequence& list, size_t instance,
                                     const string& name) {
    SpeciesIndexChecker checker(name, species_index);
    for (const scnXml::VectorSpeciesIntervention& anoph : list) {
        const string& mosq = anoph.getMosquito();
        species[checker.getIndex(mosq)]->init_vector_interv(anoph, instance);
    }
    checker.checkNoneMissed();
}
void VectorModel::init_vector_trap(const scnXml::VectorTrap::DescriptionSequence list, size_t instance,
                                   const scnXml::VectorTrap::NameOptional name_opt) {
    stringstream name_ss;
    if (name_opt.present())
        name_ss << name_opt.get();
    else
        name_ss << "vector trap intervention " << (instance + 1);
    string name = name_ss.str();
    SpeciesIndexChecker checker(name, species_index);
    for (const scnXml::Description1& anoph : list) {
        const string& mosq = anoph.getMosquito();
        species[checker.getIndex(mosq)]->init_vector_trap(anoph, instance);
    }
    checker.checkNoneMissed();
}
void VectorModel::scale_eir(double factor) {
    for (size_t i = 0; i < species_index.size(); ++i)
        species[i]->scale_eir(factor);
    vectors::scale(initialisation_eir, factor);
    annual_eir = vectors::sum(initialisation_eir);
}

SimTime VectorModel::init_iterate() {
    if (intervention_mode != DynamicEir) {
        // allow forcing equilibrium mode like with non-vector model
        return Sim::zero(); // no initialization to do
    }

    // Fitting is done
    if (init_iterations < 0) {
        simulation_mode = DynamicEir;
        return Sim::zero();
    }

    if (++init_iterations > 30) {
        throw TRACED_EXCEPTION("Transmission warmup exceeded 30 iterations!", util::Error::VectorWarmup);
    }

    const vector<double> kappa = mean_annual_kappa();
    bool need_iterate = false;
    for (size_t i = 0; i < species_index.size(); ++i) {
        const bool species_needs_iterate = emergence_rate_estimators[i]->estimate(*species[i], kappa, mean_avail);
        const bool species_fitted = species[i]->init_iterate();
        need_iterate = need_iterate || species_needs_iterate || !species_fitted;
    }

    if (need_iterate) {
        // stabilization + 5 years data-collection time:
        return Sim::one_year() + Sim::from_years_i(5);
    } else {
        // One year stabilisation, then we're finished:
        init_iterations = -1;
        return Sim::one_year();
    }
}

double VectorModel::update_kappa(const vector<Host::Human>& population) {
    const double kappa = TransmissionModel::update_kappa(population);
    quinquennial_kappa[Sim::modulo_steps(Sim::ts1(), static_cast<int>(quinquennial_kappa.size()))] = kappa;
    return kappa;
}

vector<double> VectorModel::mean_annual_kappa() const {
    const size_t steps_per_year = Sim::steps_per_year();
    vector<double> mean(steps_per_year, 0.0);
    for (size_t step = 0; step < steps_per_year; ++step) {
        double total = 0.0;
        for (size_t year = 0; year < 5; ++year)
            total += quinquennial_kappa[(year * steps_per_year) + step];
        mean[step] = total / 5;
    }
    return mean;
}

void VectorModel::calculate_eir(Host::Human& human, double age_years, vector<double>& genotype_eir_i,
                                vector<double>& genotype_eir_l) const {

    PerHost& host = human.perHostTransmission;
    host.update(human);
    if (simulation_mode == TransientEirKnown) {
        genotype_eir_i.assign(1, 0.0);
        genotype_eir_l.assign(1, 0.0);

        assert(simulation_mode == ForcedEir);
        for (size_t i = 0; i < species_index.size(); ++i) {
            /* Calculates EIR per individual (hence N_i == 1).
             *
             * See comment in AnophelesModel::advancePeriod for method. */
            double eir = species[i]->get_intervention_eir() * host.relative_availability_age(age_years) *
                         host.ento_availability_het_vec_itv(i);
            mon::recordStat(mon::measure("innoculationsPerAgeGroup"), human, eir);
            mon::recordStat(mon::measure("innoculationsPerVector"), human, eir, i);
            // cout << host.availBite(i) << endl;
            // cout << "species: " << eir << " " << host.entoAvailabilityHetVecItv(i) << " " << host.entoAvailabilityHetVecItv(i) * (1000*0.00112608) << endl;
            genotype_eir_l[0] += eir;
        }
    } else if (simulation_mode == ForcedEir) {
        // double eir = initialisationEIR[sim::moduloYearSteps(sim::ts0())] * host.relativeAvailabilityHetAge(ageYears);

        // cout << "global: " << eir << " " <<  endl;
        // mon::recordStat(mon::measure("innoculationsPerAgeGroup"), human, eir);
        // EIR.assign(1, eir);

        genotype_eir_i.assign(1, 0.0);
        genotype_eir_l.assign(1, 0.0);

        assert(simulation_mode == ForcedEir);
        for (size_t i = 0; i < species_index.size(); ++i) {
            /* Calculates EIR per individual (hence N_i == 1).
             *
             * See comment in AnophelesModel::advancePeriod for method. */
            double eir = species[i]->get_init_partial_eir() * host.relative_availability_age(age_years) *
                         host.ento_availability_het_vec_itv(i);
            mon::recordStat(mon::measure("innoculationsPerAgeGroup"), human, eir);
            mon::recordStat(mon::measure("innoculationsPerVector"), human, eir, i);
            // cout << host.availBite(i) << endl;
            // cout << "species: " << eir << " " << host.entoAvailabilityHetVecItv(i) << " " << host.entoAvailabilityHetVecItv(i) * (1000*0.00112608) << endl;
            genotype_eir_l[0] += eir;
        }
    } else {
        assert(simulation_mode == DynamicEir);
        genotype_eir_i.assign(num_genotypes(), 0.0);
        genotype_eir_l.assign(num_genotypes(), 0.0);
        const double age_factor = host.relative_availability_age(age_years);
        for (size_t i = 0; i < species_index.size(); ++i) {
            const vector<double>& partial_eir_i = species[i]->get_partial_eir_imported();
            const vector<double>& partial_eir_l = species[i]->get_partial_eir_local();

            assert(genotype_eir_i.size() == partial_eir_i.size());
            assert(genotype_eir_l.size() == partial_eir_l.size());

            // if ((std::isnan)(vectors::sum(partialEIR_i))) { cerr << "partialEIR is not a number; " << i << endl; }

            /* Calculates EIR per individual (hence N_i == 1).
             *
             * See comment in AnophelesModel::advancePeriod for method. */
            double ento_factor = age_factor * host.avail_bite(i);
            for (size_t g = 0; g < genotype_eir_i.size(); ++g) {
                double eir_i = partial_eir_i[g] * ento_factor;
                double eir_l = partial_eir_l[g] * ento_factor;
                mon::recordStat(mon::measure("innoculationsPerAgeGroup"), human, eir_i + eir_l, 0, g);
                mon::recordStat(mon::measure("innoculationsPerVector"), human, eir_i + eir_l, i, g);
                genotype_eir_i[g] += eir_i;
                genotype_eir_l[g] += eir_l;
            }
        }
    }
}

// Every Global::interval days:
void VectorModel::vector_update(const vector<Host::Human>& population) {
    std::vector<double> prob_transmission_i, prob_transmission_l;
    std::vector<double> sum_avail(species_index.size());
    std::vector<double> sigma_df(species_index.size());
    std::vector<double> sigma_dff(species_index.size());
    std::vector<std::vector<double>> sigma_dif_i(species_index.size(), std::vector<double>(num_genotypes())),
        sigma_dif_l(species_index.size(), std::vector<double>(num_genotypes()));
    for (const Host::Human& human : population) {
        const OM::Transmission::PerHost& host = human.perHostTransmission;
        WithinHost::WHInterface& whm = *human.withinHostModel;

        prob_transmission_i.assign(num_genotypes(), 0.0);
        prob_transmission_l.assign(num_genotypes(), 0.0);
        whm.prob_transmission_to_mosquito(prob_transmission_i, prob_transmission_l);

        for (size_t s = 0; s < species_index.size(); ++s) {
            // NOTE: calculate availability relative to age at end of time step;
            // not my preference but consistent with TransmissionModel::getEIR().
            // TODO: even stranger since probTransmission comes from the previous time step
            const double avail = host.ento_availability_full(s, Sim::in_years(human.age(Sim::ts1())));
            sum_avail[s] += avail;
            const double df = avail * host.prob_mosq_biting(s) * host.prob_mosq_resting(s);
            sigma_df[s] += df;
            for (size_t g = 0; g < num_genotypes(); ++g) {
                const double tbv_fac =
                    human.vaccine.getFactor(interventions::Vaccine::TBV, opt_vaccine_genotype ? g : 0);
                sigma_dif_i[s][g] += df * prob_transmission_i[g] * tbv_fac;
                sigma_dif_l[s][g] += df * prob_transmission_l[g] * tbv_fac;
            }
            sigma_dff[s] += df * host.rel_mosq_fecundity(s);
        }
    }

    for (size_t s = 0; s < species_index.size(); ++s) {
        species[s]->advance_period(sum_avail[s], sigma_df[s], sigma_dif_i[s], sigma_dif_l[s], sigma_dff[s],
                                   simulation_mode == DynamicEir);
    }
}

const string VecModeErr = "vector interventions can only be used in "
                          "dynamic transmission mode (mode=\"dynamic\")";

void VectorModel::deploy_vector_pop_interv(size_t instance) {
    if (intervention_mode != DynamicEir) {
        throw xml_scenario_error(VecModeErr);
    }
    for (auto& specie : species) {
        specie->deploy_vector_pop_interv(rng, instance);
    }
}
void VectorModel::deploy_vector_trap(size_t instance, double number, SimTime lifespan) {
    if (intervention_mode != DynamicEir) {
        throw xml_scenario_error(VecModeErr);
    }
    for (size_t i = 0; i < species_index.size(); ++i) {
        species[i]->deploy_vector_trap(rng, i, instance, number, lifespan);
    }
}

void VectorModel::change_eir_intervention(const scnXml::NonVector& non_vector_data) {
    if ((simulation_mode != ForcedEir) && (simulation_mode != TransientEirKnown))
        throw util::xml_scenario_error("changeEIR intervention can only be used in forced EIR mode");

    simulation_mode = TransientEirKnown;

    if (species_index.size() != 1)
        throw util::xml_scenario_error("changeEIR intervention can only be used with a single anopheles species");

    species[0]->change_eir_intervention(non_vector_data);
}

void VectorModel::uninfect_vectors() {
    for (size_t i = 0; i < species_index.size(); ++i)
        species[i]->uninfect_vectors();
}

void VectorModel::summarize() {
    TransmissionModel::summarize();

    for (size_t i = 0; i < species_index.size(); ++i) {
        species[i]->summarize(i);
    }
}

void VectorModel::checkpoint(istream& stream) {
    TransmissionModel::checkpoint(stream);
    rng.checkpoint(stream);
    init_iterations & stream;
    quinquennial_kappa & stream;
    // species & stream;
    for (auto& s : species)
        s->checkpoint(stream);
}
void VectorModel::checkpoint(ostream& stream) {
    TransmissionModel::checkpoint(stream);
    rng.checkpoint(stream);
    init_iterations & stream;
    quinquennial_kappa & stream;
    for (auto& s : species)
        s->checkpoint(stream);
}

} // namespace OM::Transmission
