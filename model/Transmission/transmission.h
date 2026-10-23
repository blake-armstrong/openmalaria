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

#ifndef Hmod_Transmission
#define Hmod_Transmission

#include "Transmission/NonVectorModel.h"
#include "Transmission/VectorModel.h"
#include "Transmission/Anopheles/AnophelesModel.h"
#include "Transmission/Anopheles/SimpleMPDAnophelesModel.h"
#include "Transmission/Anopheles/EmergenceRateEstimator.h"
#include "util/ModelOptions.h"

#include <memory>
#include <vector>

namespace OM {
namespace Transmission {
using namespace OM::util;

inline SimulationMode readMode(const string& str) {
    if (str == "forced")
        return forcedEIR;
    else if (str == "dynamic")
        return dynamicEIR;
    else
        // Note: originally 3 (transientEIRknown) could be specified; now it's
        // set automatically.
        throw util::xml_scenario_error(string("mode attribute invalid: ").append(str));
}

inline NonVectorModel* createNonVectorModel(const scnXml::Entomology& entoData, size_t n_genotypes, SimTime adult_age) {
    const scnXml::NonVector& nonVectorData = entoData.getNonVector().get();

    int interventionMode = readMode(entoData.getMode());
    int eipDuration = nonVectorData.getEipDuration();

    vector<double> initialisationEIR;
    initialisationEIR.assign(Sim::steps_per_year(), 0.0);

    return new NonVectorModel(initialisationEIR, interventionMode, entoData, nonVectorData, eipDuration, n_genotypes,
                              adult_age);
}

inline bool anophelesCompare(const scnXml::AnophelesParams& a1, const scnXml::AnophelesParams& a2) {
    return (a1.getSeasonality().getAnnualEIR().get() > a2.getSeasonality().getAnnualEIR().get());
}

inline Anopheles::AnophelesModel* createAnophelesModel(size_t i, const scnXml::AnophelesParams& anoph,
                                                       vector<double>& initialisationEIR, int populationSize,
                                                       int interventionMode, PerHostParams& per_host_params,
                                                       const WithinHost::GenotypeRegistry& genotypes) {
    Anopheles::AnophelesModel* anophModel;

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

        SimTime developmentDuration = Sim::from_days(smpd.getDevelopmentDuration().getValue());
        if (!(developmentDuration > Sim::zero()))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.developmentDuration: "
                                           "must be positive");
        double probPreadultSurvival = smpd.getDevelopmentSurvival().getValue();
        if (!(0.0 <= probPreadultSurvival && probPreadultSurvival <= 1.0))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.developmentSurvival: "
                                           "must be a probability (in range [0,1]");
        double fEggsLaidByOviposit = smpd.getFemaleEggsLaidByOviposit().getValue();
        if (!(fEggsLaidByOviposit > 0.0))
            throw util::xml_scenario_error("entomology.vector.simpleMPD.femaleEggsLaidByOviposit: "
                                           "must be positive");

        anophModel =
            new Anopheles::SimpleMPDAnophelesModel(developmentDuration, probPreadultSurvival, fEggsLaidByOviposit);
    } else
        anophModel = new Anopheles::AnophelesModel();

    const scnXml::Seasonality& seasonality = anoph.getSeasonality();

    if (seasonality.getInput() != "EIR")
        throw util::xml_scenario_error("entomology.anopheles.seasonality.input: must be EIR (for now)");

    vector<double> initEIR365;
    vector<double> FSCoeffic;
    double EIRRotateAngle = 0;
    double targetEIR = seasonality.getAnnualEIR().get();
    if (seasonality.getFourierSeries().present()) {
        const scnXml::FourierSeries& seasFC = seasonality.getFourierSeries().get();
        const scnXml::FourierSeries::CoefficSequence& fsCoeffic = seasFC.getCoeffic();

        FSCoeffic.reserve(2 * fsCoeffic.size() + 1);

        FSCoeffic.push_back(0.0); // value doesn't matter; EIR will be scaled
        for (auto it = fsCoeffic.begin(); it != fsCoeffic.end(); ++it) {
            FSCoeffic.push_back(it->getA());
            FSCoeffic.push_back(it->getB());
        }

        EIRRotateAngle = 0;
        if (seasFC.getEIRRotateAngle().present())
            EIRRotateAngle = seasFC.getEIRRotateAngle().get();

        // EIR for this species, with index 0 refering to value over first interval
        initEIR365.resize(Sim::one_year(), 0.0);

        // Now we rescale to get an EIR of targetEIR.
        // Calculate current sum as is usually done.
        vectors::expIDFT(initEIR365, FSCoeffic, EIRRotateAngle);

        // And scale (also acts as a unit conversion):
        FSCoeffic[0] += log(targetEIR / vectors::sum(initEIR365));

        // Calculate forced EIR for pre-intervention phase from FSCoeffic:
        vectors::expIDFT(initEIR365, FSCoeffic, EIRRotateAngle);
    } else if (seasonality.getMonthlyValues().present()) {
        const scnXml::MonthlyValues& seasM = seasonality.getMonthlyValues().get();
        if (seasM.getSmoothing() == "fourier") {
            const size_t N_m = 12;
            const scnXml::MonthlyValues::ValueSequence seq = seasM.getValue();
            assert(seq.size() == N_m); // enforced by schema
            vector<double> months(N_m);
            double maxEIR = months[0];
            for (size_t i = 0; i < N_m; ++i) {
                months[i] = seq[i];
                if (months[i] > maxEIR)
                    maxEIR = months[i];
            }
            // arbitrary minimum we allow (cannot have zeros since we take the logarithm)
            double min = maxEIR / 100.0;
            for (size_t i = 0; i < N_m; ++i) {
                if (months[i] < min)
                    months[i] += min;
            }

            FSCoeffic.assign(5, 0.0);
            // TODO: determine whether to use Fourier Series Coefficient method or
            // Discrete Fourier Transform. Former is designed for integrals (which
            // roughly what we have?), the latter for discrete values. DFT doesn't
            // work well when number of intervals changes?
            util::vectors::logFourierCoefficients(months, FSCoeffic);

            // Assume the values are for the start of the month
            EIRRotateAngle = 0;
            if (seasM.getEIRRotateAngle().present())
                EIRRotateAngle = seasM.getEIRRotateAngle().get();

            // EIR for this species, with index 0 refering to value over first interval
            initEIR365.resize(Sim::one_year());

            // Now we rescale to get an EIR of targetEIR.
            // Calculate current sum as is usually done.
            vectors::expIDFT(initEIR365, FSCoeffic, EIRRotateAngle);
            // And scale (also acts as a unit conversion):
            FSCoeffic[0] += log(targetEIR / vectors::sum(initEIR365));

            // Calculate forced EIR for pre-intervention phase from FSCoeffic:
            vectors::expIDFT(initEIR365, FSCoeffic, EIRRotateAngle);
        } else if (seasM.getSmoothing() == "none") {
            if (interventionMode != forcedEIR) {
                throw util::xml_scenario_error("entomology.anopheles.seasonality.monthlyValues.smoothing: smoothing "
                                               "mode \"none\" is not allowed with monthly EIR values");
            }
        } else {
            if (interventionMode != forcedEIR) {
                throw util::xml_scenario_error(
                    "entomology.anopheles.seasonality.monthlyValues.smoothing: unknown smoothing mode");
            }
        }
    } else {
        assert(seasonality.getDailyValues().present()); // XML loading code should enforce this

        if (interventionMode != forcedEIR) {
            throw util::xml_scenario_error(
                "entomology.anopheles.seasonality.dailyValues: daily values are only allowed with forced EIR");
        }

        const scnXml::DailyValues& seasM = seasonality.getDailyValues().get();
        const scnXml::DailyValues::ValueSequence& daily = seasM.getValue();

        // The minimum EIR allowed in the array. The product of the average EIR and a constant.
        double minEIR = 0.01 * averageEIR(seasM);

        if (daily.size() < static_cast<size_t>(Sim::one_year()))
            throw util::xml_scenario_error(
                "entomology.anopheles.seasonality.dailyValues insufficient daily data for a year");

        // EIR for this species, with index 0 refering to value over first interval
        initEIR365.resize(Sim::one_year(), 0.0);
        vector<int> nDays(Sim::one_year(), 0.0);

        for (SimTime d = Sim::zero(), endDay = daily.size(); d < endDay; d = d + Sim::one_day()) {
            double EIRdaily = std::max(static_cast<double>(daily[d]), minEIR);

            // Index 0 of initialisationEIR refers to the EIR affecting the
            // first day(s) of the year. Correspondingly, the first 1 or 5 values
            // of EIRDaily affect this (1- or 5-day) time-step.
            size_t i = mod_nn(d, Sim::one_year());

            nDays[i] += 1;
            initEIR365[i] += EIRdaily;
        }

        // Calculate total annual EIR
        // divide by number of records assigned to each interval (usually one per day)
        for (SimTime d = Sim::zero(); d < Sim::one_year(); d = d + Sim::one_day())
            initEIR365[d] *= 1.0 / static_cast<double>(nDays[d]);
    }

    if (!seasonality.getAnnualEIR().present())
        throw util::xml_scenario_error("entomology.anopheles.seasonality.annualEIR is required at the moment");

    // -----  Set model variables  -----
    const scnXml::Mosq& mosq = anoph.getMosq();

    Anopheles::MosquitoParams mosqParams;
    mosqParams.name = anoph.getMosquito();
    mosqParams.laidEggsSameDayProportion = mosq.getMosqLaidEggsSameDayProportion().getValue();
    mosqParams.survivalFeedingCycleProbability = mosq.getMosqSurvivalFeedingCycleProbability().getValue();
    mosqParams.humanBloodIndex = mosq.getMosqHumanBloodIndex().getValue();
    mosqParams.probBiting = mosq.getMosqProbBiting().getMean();
    mosqParams.probFindRestSite = mosq.getMosqProbFindRestSite().getMean();
    mosqParams.probResting = mosq.getMosqProbResting().getMean();
    mosqParams.probOvipositing = mosq.getMosqProbOvipositing().getValue();
    mosqParams.seekingDuration = mosq.getMosqSeekingDuration().getValue();
    mosqParams.probMosqSurvivalOvipositing = mosq.getMosqProbOvipositing().getValue();
    mosqParams.restDuration = Sim::from_days(mosq.getMosqRestDuration().getValue());
    mosqParams.EIPDuration = Sim::from_days(mosq.getExtrinsicIncubationPeriod().getValue());
    mosqParams.minInfectedThreshold = mosq.getMinInfectedThreshold();

    if (Sim::one_day() > mosqParams.restDuration || mosqParams.restDuration * 2 >= mosqParams.EIPDuration) {
        // TODO: limit was EIPDuration >= mosqRestDuration >= 1
        // but in usage of ftauArray this wasn't enough. Check why.
        throw util::xml_scenario_error("Code expects EIPDuration > 2*mosqRestDuration >= 2");
    }

    const scnXml::AnophelesParams::NonHumanHostsSequence& xmlSeqNNHs = anoph.getNonHumanHosts();

    vector<Anopheles::NhhParams> nhhs;
    for (const scnXml::NonHumanHosts& xmlNNH : xmlSeqNNHs) {
        Anopheles::NhhParams nhh;
        nhh.mosqRelativeEntoAvailability = xmlNNH.getMosqRelativeEntoAvailability().getValue();
        nhh.mosqProbBiting = xmlNNH.getMosqProbBiting().getValue();
        nhh.mosqProbFindRestSite = xmlNNH.getMosqProbFindRestSite().getValue();
        nhh.mosqProbResting = xmlNNH.getMosqProbResting().getValue();
        nhh.hostFecundityFactor =
            xmlNNH.getHostFecundityFactor().present() ? xmlNNH.getHostFecundityFactor().get().getValue() : 1.0;
        nhh.name = xmlNNH.getName();
        nhhs.push_back(nhh);
    }

    // Initial estimate of the proportion of mosquitoes which are infectious, s: S_v(t) = s*N_v(t). Used as a starting value and then fit.
    double propInfectious = 0.021;
    // Initial guess of the proportion of mosquitoes which are infected, o: O_v(t) = o*N_v(t). Only used as a starting value.
    double propInfected = 0.078;

    anophModel->initialise(i, mosqParams, per_host_params, genotypes);
    anophModel->initAvailability(i, nhhs, populationSize, per_host_params);
    anophModel->initEIR(initEIR365, FSCoeffic, EIRRotateAngle, propInfectious, propInfected);

    for (SimTime i = Sim::zero(); i < Sim::one_year(); i = i + Sim::one_day())
        initialisationEIR[mod_nn(Sim::in_steps(i), Sim::steps_per_year())] += initEIR365[i];

    return anophModel;
}

inline VectorModel* createVectorModel(const scnXml::Entomology& entoData, int populationSize,
                                      PerHostParams& per_host_params, util::MasterRng& transmission_rng,
                                      const WithinHost::GenotypeRegistry& genotypes) {
    const scnXml::Vector& vectorData = entoData.getVector().get();
    scnXml::Vector::AnophelesSequence anophelesList = vectorData.getAnopheles();

    int interventionMode = readMode(entoData.getMode());

    vector<double> initialisationEIR;
    initialisationEIR.assign(Sim::steps_per_year(), 0.0);

    vector<std::unique_ptr<Anopheles::AnophelesModel>> species;
    vector<std::unique_ptr<Anopheles::EmergenceRateEstimator>> emergenceRateEstimators;
    map<string, size_t> speciesIndex;

    size_t numSpecies = anophelesList.size();

    if (numSpecies < 1)
        throw util::xml_scenario_error("Can't use Vector model without data for at least one anopheles species!");

    // Sort Anopheles by Decreasing EIR
    sort(anophelesList.begin(), anophelesList.end(), anophelesCompare);

    for (size_t i = 0; i < numSpecies; ++i) {
        auto anoph = anophelesList[i];

        per_host_params.add_species(anoph.getMosq());

        Anopheles::AnophelesModel* anophModel = createAnophelesModel(i, anoph, initialisationEIR, populationSize,
                                                                     interventionMode, per_host_params, genotypes);
        std::unique_ptr<Anopheles::EmergenceRateEstimator> estimator;
        if (util::ModelOptions::option(util::UseExactNv0Solver))
            estimator = std::make_unique<Anopheles::EmergenceRateSolver>(*anophModel, populationSize);
        else
            estimator = std::make_unique<Anopheles::EmergenceRateAdaptiveFitter>(*anophModel);

        species.push_back(std::unique_ptr<Anopheles::AnophelesModel>(anophModel));
        emergenceRateEstimators.push_back(std::move(estimator));
        speciesIndex[anophModel->mosq.name] = i;
    }

    if (interventionMode == forcedEIR) {
        // We don't need these anymore (now we have initialisationEIR); free memory
        // numSpecies = 0;
        // species.clear();
        // speciesIndex.clear();
    }

    return new VectorModel(initialisationEIR, interventionMode, std::move(species), std::move(emergenceRateEstimators),
                           speciesIndex, populationSize, transmission_rng, genotypes.count(),
                           per_host_params.adult_age());
}

///@brief Creation, destruction and checkpointing
//@{
/// Creates a derived class
inline TransmissionModel* createTransmissionModel(const scnXml::Entomology& entoData, int populationSize,
                                                  PerHostParams& per_host_params, util::MasterRng& transmission_rng,
                                                  const WithinHost::GenotypeRegistry& genotypes) {
    // Entomology contains either a list of at least one anopheles or a list of at
    // least one EIRDaily.
    TransmissionModel* model;
    if (entoData.getVector().present())
        model = createVectorModel(entoData, populationSize, per_host_params, transmission_rng, genotypes);
    else {
        if (!entoData.getNonVector().present()) // should be a validation error, but anyway...
            throw util::xml_scenario_error("Neither vector nor non-vector data present in the XML!");
        if (util::ModelOptions::option(util::VectorLifeCycleModel) ||
            util::ModelOptions::option(util::VectorSimpleMpdModel))
            throw util::xml_scenario_error(
                "VECTOR_*_MODEL is only compatible with the vector model (and non-vector data is present).");
        model = createNonVectorModel(entoData, genotypes.count(), per_host_params.adult_age());
    }

    if (entoData.getScaledAnnualEIR().present()) {
        model->scaleEIR(entoData.getScaledAnnualEIR().get() / model->annualEIR);
        assert(util::vectors::approxEqual(model->annualEIR, entoData.getScaledAnnualEIR().get()));
    }

    if (util::CommandLine::option(util::CommandLine::PrintAnnualEir)) {
        // Note: after internal scaling (which doesn't imply exit)
        // but before external scaling.
        cout << "Total annual EIR: " << model->annualEIR << endl;
    }

    return model;
}
} // namespace Transmission
} // namespace OM

#endif
