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

#ifndef HMOD_TRANSMISSION
#define HMOD_TRANSMISSION

#include "Transmission/NonVectorModel.h"
#include "Transmission/VectorModel.h"
#include "Transmission/Anopheles/AnophelesModel.h"
#include "Transmission/Anopheles/SimpleMPDAnophelesModel.h"
#include "Transmission/Anopheles/EmergenceRateEstimator.h"
#include "util/ModelOptions.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace OM::Transmission {
using namespace OM::util;

inline SimulationMode read_mode(const string& str) {
    if (str == "forced")
        return ForcedEir;
    else if (str == "dynamic")
        return DynamicEir;
    else
        // Note: originally 3 (transientEIRknown) could be specified; now it's
        // set automatically.
        throw util::xml_scenario_error(string("mode attribute invalid: ").append(str));
}

inline std::unique_ptr<NonVectorModel> create_non_vector_model(const scnXml::Entomology& ento_data, size_t n_genotypes,
                                                               SimTime adult_age) {
    const scnXml::NonVector& non_vector_data = ento_data.getNonVector().get();

    const int intervention_mode = read_mode(ento_data.getMode());
    const int eip_duration = non_vector_data.getEipDuration();

    vector<double> initialisation_eir;
    initialisation_eir.assign(Sim::steps_per_year(), 0.0);

    return std::make_unique<NonVectorModel>(initialisation_eir, intervention_mode, ento_data, non_vector_data,
                                            eip_duration, n_genotypes, adult_age);
}

inline bool anopheles_compare(const scnXml::AnophelesParams& a1, const scnXml::AnophelesParams& a2) {
    return (a1.getSeasonality().getAnnualEIR().get() > a2.getSeasonality().getAnnualEIR().get());
}

inline std::unique_ptr<Anopheles::AnophelesModel>
create_anopheles_model(size_t i, const scnXml::AnophelesParams& anoph, vector<double>& initialisation_eir,
                       int population_size, int intervention_mode, PerHostParams& per_host_params,
                       const WithinHost::GenotypeRegistry& genotypes) {
    std::unique_ptr<Anopheles::AnophelesModel> anoph_model;

    if (util::ModelOptions::option(util::VectorLifeCycleModel)) {
        throw util::xml_scenario_error("VECTOR_LIFE_CYCLE_MODEL not yet "
                                       "implemented. Use VECTOR_SIMPLE_MPD_MODEL instead.");
        // TODO
        //  * Note: this model is older than SimpleMPD and more complicated.
        //  * Difficulties are in parameterisation and estimation of resources.
        // if (!lcOpt.present())
        //     throw util::xml_scenario_error(
        //         "VECTOR_LIFE_CYCLE_MODEL: requires <lifeCycle> element with "
        //         "model parameters for each anopheles species");
        // emergence = unique_ptr<EmergenceModel>( new LCEmergence() );
        // emergence->initLifeCycle( lcOpt.get() );
    } else if (util::ModelOptions::option(util::VectorSimpleMpdModel)) {
        if (!anoph.getSimpleMPD().present())
            throw util::xml_scenario_error("VECTOR_SIMPLE_MPD_MODEL: requires <simpleMPD> element with "
                                           "model parameters for each anopheles species");

        const scnXml::SimpleMPD& smpd = anoph.getSimpleMPD().get();

        const SimTime development_duration = Sim::from_days(smpd.getDevelopmentDuration().getValue());
        if (!(development_duration > Sim::zero()))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.developmentDuration: "
                                           "must be positive");
        const double prob_preadult_survival = smpd.getDevelopmentSurvival().getValue();
        if (!(0.0 <= prob_preadult_survival && prob_preadult_survival <= 1.0))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.developmentSurvival: "
                                           "must be a probability (in range [0,1]");
        const double f_eggs_laid_by_oviposit = smpd.getFemaleEggsLaidByOviposit().getValue();
        if (!(f_eggs_laid_by_oviposit > 0.0))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.femaleEggsLaidByOviposit: "
                                           "must be positive");

        anoph_model = std::make_unique<Anopheles::SimpleMPDAnophelesModel>(development_duration, prob_preadult_survival,
                                                                           f_eggs_laid_by_oviposit);
    } else
        anoph_model = std::make_unique<Anopheles::AnophelesModel>();

    const scnXml::Seasonality& seasonality = anoph.getSeasonality();

    if (seasonality.getInput() != "EIR")
        throw util::xml_scenario_error("entomology.anopheles.seasonality.input: must be EIR (for now)");

    vector<double> init_eir365;
    vector<double> fs_coeffic;
    double eir_rotate_angle = 0;
    const double target_eir = seasonality.getAnnualEIR().get();
    if (seasonality.getFourierSeries().present()) {
        const scnXml::FourierSeries& seas_fc = seasonality.getFourierSeries().get();
        const scnXml::FourierSeries::CoefficSequence& xml_coeffic = seas_fc.getCoeffic();

        fs_coeffic.reserve(2 * xml_coeffic.size() + 1);

        fs_coeffic.push_back(0.0); // value doesn't matter; EIR will be scaled
        for (const auto& it : xml_coeffic) {
            fs_coeffic.push_back(it.getA());
            fs_coeffic.push_back(it.getB());
        }

        eir_rotate_angle = 0;
        if (seas_fc.getEIRRotateAngle().present())
            eir_rotate_angle = seas_fc.getEIRRotateAngle().get();

        // EIR for this species, with index 0 refering to value over first interval
        init_eir365.resize(Sim::one_year(), 0.0);

        // Now we rescale to get an EIR of targetEIR.
        // Calculate current sum as is usually done.
        vectors::expIDFT(init_eir365, fs_coeffic, eir_rotate_angle);

        // And scale (also acts as a unit conversion):
        fs_coeffic[0] += log(target_eir / vectors::sum(init_eir365));

        // Calculate forced EIR for pre-intervention phase from FSCoeffic:
        vectors::expIDFT(init_eir365, fs_coeffic, eir_rotate_angle);
    } else if (seasonality.getMonthlyValues().present()) {
        const scnXml::MonthlyValues& seas_m = seasonality.getMonthlyValues().get();
        if (seas_m.getSmoothing() == "fourier") {
            const size_t n_m = 12;
            const scnXml::MonthlyValues::ValueSequence seq = seas_m.getValue();
            assert(seq.size() == n_m); // enforced by schema
            vector<double> months(n_m);
            double max_eir = months[0];
            for (size_t i = 0; i < n_m; ++i) {
                months[i] = seq[i];
                if (months[i] > max_eir)
                    max_eir = months[i];
            }
            // arbitrary minimum we allow (cannot have zeros since we take the logarithm)
            const double min = max_eir / 100.0;
            for (size_t i = 0; i < n_m; ++i) {
                if (months[i] < min)
                    months[i] += min;
            }

            fs_coeffic.assign(5, 0.0);
            // TODO: determine whether to use Fourier Series Coefficient method or
            // Discrete Fourier Transform. Former is designed for integrals (which
            // roughly what we have?), the latter for discrete values. DFT doesn't
            // work well when number of intervals changes?
            util::vectors::logFourierCoefficients(months, fs_coeffic);

            // Assume the values are for the start of the month
            eir_rotate_angle = 0;
            if (seas_m.getEIRRotateAngle().present())
                eir_rotate_angle = seas_m.getEIRRotateAngle().get();

            // EIR for this species, with index 0 refering to value over first interval
            init_eir365.resize(Sim::one_year());

            // Now we rescale to get an EIR of targetEIR.
            // Calculate current sum as is usually done.
            vectors::expIDFT(init_eir365, fs_coeffic, eir_rotate_angle);
            // And scale (also acts as a unit conversion):
            fs_coeffic[0] += log(target_eir / vectors::sum(init_eir365));

            // Calculate forced EIR for pre-intervention phase from FSCoeffic:
            vectors::expIDFT(init_eir365, fs_coeffic, eir_rotate_angle);
        } else if (seas_m.getSmoothing() == "none") {
            if (intervention_mode != ForcedEir) {
                throw util::xml_scenario_error("entomology.anopheles.seasonality.monthlyValues.smoothing: smoothing "
                                               "mode \"none\" is not allowed with monthly EIR values");
            }
        } else {
            if (intervention_mode != ForcedEir) {
                throw util::xml_scenario_error(
                    "entomology.anopheles.seasonality.monthlyValues.smoothing: unknown smoothing mode");
            }
        }
    } else {
        assert(seasonality.getDailyValues().present()); // XML loading code should enforce this

        if (intervention_mode != ForcedEir) {
            throw util::xml_scenario_error(
                "entomology.anopheles.seasonality.dailyValues: daily values are only allowed with forced EIR");
        }

        const scnXml::DailyValues& seas_m = seasonality.getDailyValues().get();
        const scnXml::DailyValues::ValueSequence& daily = seas_m.getValue();

        // The minimum EIR allowed in the array. The product of the average EIR and a constant.
        const double min_eir = 0.01 * average_eir(seas_m);

        if (daily.size() < static_cast<size_t>(Sim::one_year()))
            throw util::xml_scenario_error(
                "entomology.anopheles.seasonality.dailyValues insufficient daily data for a year");

        // EIR for this species, with index 0 refering to value over first interval
        init_eir365.resize(Sim::one_year(), 0.0);
        vector<int> n_days(Sim::one_year(), 0.0);

        for (SimTime d = Sim::zero(), end_day = daily.size(); d < end_day; d = d + Sim::one_day()) {
            const double eir_daily = std::max(static_cast<double>(daily[d]), min_eir);

            // Index 0 of initialisationEIR refers to the EIR affecting the
            // first day(s) of the year. Correspondingly, the first 1 or 5 values
            // of EIRDaily affect this (1- or 5-day) time-step.
            const size_t i = mod_nn(d, Sim::one_year());

            n_days[i] += 1;
            init_eir365[i] += eir_daily;
        }

        // Calculate total annual EIR
        // divide by number of records assigned to each interval (usually one per day)
        for (SimTime d = Sim::zero(); d < Sim::one_year(); d = d + Sim::one_day())
            init_eir365[d] *= 1.0 / static_cast<double>(n_days[d]);
    }

    if (!seasonality.getAnnualEIR().present())
        throw util::xml_scenario_error("entomology.anopheles.seasonality.annualEIR is required at the moment");

    // -----  Set model variables  -----
    const scnXml::Mosq& mosq = anoph.getMosq();

    Anopheles::MosquitoParams mosq_params;
    mosq_params.name = anoph.getMosquito();
    mosq_params.laid_eggs_same_day_proportion = mosq.getMosqLaidEggsSameDayProportion().getValue();
    mosq_params.survival_feeding_cycle_probability = mosq.getMosqSurvivalFeedingCycleProbability().getValue();
    mosq_params.human_blood_index = mosq.getMosqHumanBloodIndex().getValue();
    mosq_params.prob_biting = mosq.getMosqProbBiting().getMean();
    mosq_params.prob_find_rest_site = mosq.getMosqProbFindRestSite().getMean();
    mosq_params.prob_resting = mosq.getMosqProbResting().getMean();
    mosq_params.prob_ovipositing = mosq.getMosqProbOvipositing().getValue();
    mosq_params.seeking_duration = mosq.getMosqSeekingDuration().getValue();
    mosq_params.prob_mosq_survival_ovipositing = mosq.getMosqProbOvipositing().getValue();
    mosq_params.rest_duration = Sim::from_days(mosq.getMosqRestDuration().getValue());
    mosq_params.eip_duration = Sim::from_days(mosq.getExtrinsicIncubationPeriod().getValue());
    mosq_params.min_infected_threshold = mosq.getMinInfectedThreshold();

    if (Sim::one_day() > mosq_params.rest_duration || mosq_params.rest_duration * 2 >= mosq_params.eip_duration) {
        // TODO: limit was EIPDuration >= mosqRestDuration >= 1
        // but in usage of ftauArray this wasn't enough. Check why.
        throw util::xml_scenario_error("Code expects EIPDuration > 2*mosqRestDuration >= 2");
    }

    const scnXml::AnophelesParams::NonHumanHostsSequence& xml_seq_nnhs = anoph.getNonHumanHosts();

    vector<Anopheles::NhhParams> nhhs;
    for (const scnXml::NonHumanHosts& xml_nnh : xml_seq_nnhs) {
        Anopheles::NhhParams nhh;
        nhh.mosq_relative_ento_availability = xml_nnh.getMosqRelativeEntoAvailability().getValue();
        nhh.mosq_prob_biting = xml_nnh.getMosqProbBiting().getValue();
        nhh.mosq_prob_find_rest_site = xml_nnh.getMosqProbFindRestSite().getValue();
        nhh.mosq_prob_resting = xml_nnh.getMosqProbResting().getValue();
        nhh.host_fecundity_factor =
            xml_nnh.getHostFecundityFactor().present() ? xml_nnh.getHostFecundityFactor().get().getValue() : 1.0;
        nhh.name = xml_nnh.getName();
        nhhs.push_back(nhh);
    }

    // Initial estimate of the proportion of mosquitoes which are infectious, s: S_v(t) = s*N_v(t). Used as a starting value and then fit.
    const double prop_infectious = 0.021;
    // Initial guess of the proportion of mosquitoes which are infected, o: O_v(t) = o*N_v(t). Only used as a starting value.
    const double prop_infected = 0.078;

    anoph_model->initialise(i, mosq_params, per_host_params, genotypes);
    anoph_model->init_availability(i, nhhs, population_size, per_host_params);
    anoph_model->init_eir(init_eir365, fs_coeffic, eir_rotate_angle, prop_infectious, prop_infected);

    for (SimTime i = Sim::zero(); i < Sim::one_year(); i = i + Sim::one_day())
        initialisation_eir[mod_nn(Sim::in_steps(i), Sim::steps_per_year())] += init_eir365[i];

    return anoph_model;
}

inline std::unique_ptr<VectorModel> create_vector_model(const scnXml::Entomology& ento_data, int population_size,
                                                        PerHostParams& per_host_params,
                                                        util::MasterRng& transmission_rng,
                                                        const WithinHost::GenotypeRegistry& genotypes) {
    const scnXml::Vector& vector_data = ento_data.getVector().get();
    scnXml::Vector::AnophelesSequence anopheles_list = vector_data.getAnopheles();

    const int intervention_mode = read_mode(ento_data.getMode());

    vector<double> initialisation_eir;
    initialisation_eir.assign(Sim::steps_per_year(), 0.0);

    vector<std::unique_ptr<Anopheles::AnophelesModel>> species;
    vector<std::unique_ptr<Anopheles::EmergenceRateEstimator>> emergence_rate_estimators;
    map<string, size_t> species_index;

    const size_t num_species = anopheles_list.size();

    if (num_species < 1)
        throw util::xml_scenario_error("Can't use Vector model without data for at least one anopheles species!");

    // Sort Anopheles by Decreasing EIR
    std::ranges::sort(anopheles_list, anopheles_compare);

    for (size_t i = 0; i < num_species; ++i) {
        const auto& anoph = anopheles_list[i];

        per_host_params.add_species(anoph.getMosq());

        std::unique_ptr<Anopheles::AnophelesModel> anoph_model = create_anopheles_model(
            i, anoph, initialisation_eir, population_size, intervention_mode, per_host_params, genotypes);
        std::unique_ptr<Anopheles::EmergenceRateEstimator> estimator;
        if (util::ModelOptions::option(util::UseExactNv0Solver))
            estimator = std::make_unique<Anopheles::EmergenceRateSolver>(*anoph_model, population_size);
        else
            estimator = std::make_unique<Anopheles::EmergenceRateAdaptiveFitter>(*anoph_model);

        species_index[anoph_model->mosq.name] = i;
        species.push_back(std::move(anoph_model));
        emergence_rate_estimators.push_back(std::move(estimator));
    }

    if (intervention_mode == ForcedEir) {
        // We don't need these anymore (now we have initialisationEIR); free memory
        // numSpecies = 0;
        // species.clear();
        // speciesIndex.clear();
    }

    return std::make_unique<VectorModel>(initialisation_eir, intervention_mode, std::move(species),
                                         std::move(emergence_rate_estimators), species_index, population_size,
                                         transmission_rng, genotypes.count(), per_host_params.adult_age());
}

///@brief Creation, destruction and checkpointing
//@{
/// Creates a derived class
inline std::unique_ptr<TransmissionModel> create_transmission_model(const scnXml::Entomology& ento_data,
                                                                    int population_size, PerHostParams& per_host_params,
                                                                    util::MasterRng& transmission_rng,
                                                                    const WithinHost::GenotypeRegistry& genotypes) {
    // Entomology contains either a list of at least one anopheles or a list of at
    // least one EIRDaily.
    std::unique_ptr<TransmissionModel> model;
    if (ento_data.getVector().present())
        model = create_vector_model(ento_data, population_size, per_host_params, transmission_rng, genotypes);
    else {
        if (!ento_data.getNonVector().present()) // should be a validation error, but anyway...
            throw util::xml_scenario_error("Neither vector nor non-vector data present in the XML!");
        if (util::ModelOptions::option(util::VectorLifeCycleModel) ||
            util::ModelOptions::option(util::VectorSimpleMpdModel))
            throw util::xml_scenario_error(
                "VECTOR_*_MODEL is only compatible with the vector model (and non-vector data is present).");
        model = create_non_vector_model(ento_data, genotypes.count(), per_host_params.adult_age());
    }

    if (ento_data.getScaledAnnualEIR().present()) {
        model->scale_eir(ento_data.getScaledAnnualEIR().get() / model->annual_eir);
        assert(util::vectors::approxEqual(model->annual_eir, ento_data.getScaledAnnualEIR().get()));
    }

    if (util::CommandLine::option(util::CommandLine::PrintAnnualEir)) {
        // Note: after internal scaling (which doesn't imply exit)
        // but before external scaling.
        cout << "Total annual EIR: " << model->annual_eir << endl;
    }

    return model;
}
} // namespace OM::Transmission

#endif
