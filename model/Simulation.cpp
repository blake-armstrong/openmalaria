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

#include "Simulation.h"

#include "Parameters.h"
#include "checkpoint.h"
#include "util/CommandLine.h"
#include "util/ModelOptions.h"
#include "util/ModelNameProvider.h"
#include "util/StreamValidator.h"
#include "util/errors.h"
#include "util/random.h"

#include "mon/Continuous.h"
#include "mon/Monitoring.h"
#include "mon/init.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace OM {

namespace {

void print_progress(int last_percent, SimTime now, SimTime est_end_time) {
    const int percent = (now * 100) / est_end_time;
    if (percent != last_percent) { // avoid huge amounts of output for performance/log-file size reasons
        last_percent = percent;
        cerr << "\r" << percent << "%\t" << flush;
    }
}

void print_errno(SimTime now) {
    if (errno != 0) {
        const int error = errno;
        const std::string context = "t = " + std::to_string(static_cast<int>(now)) + " Please report! Error: ";
        errno = error;
        std::perror(context.c_str());
        errno = 0;
    }
}

}

Simulation::Simulation(std::unique_ptr<scnXml::Scenario> scenario, util::CommandLineOptions options)
    : command_line(std::move(options)), master_rngs(scenario->getModel().getComputationParameters().getIseed()),
      scenario_xml(std::move(scenario)),
      only_new_episode(scenario_xml->getMonitoring().getSurveyOptions().getOnlyNewEpisode()) {
    // 1) elements with no dependencies on other elements initialised here:
    genotypes = WithinHost::GenotypeRegistry(*scenario_xml, command_line);
    const util::ModelNameProvider model_name_provider(scenario_xml->getModel());

    // 2) elements depending on only elements initialised in (1):
    clock.init(*scenario_xml, model_name_provider, command_line, monitoring); // Also reads survey dates.

    // 3) elements depending on only elements initialised in (2).
    parameters = Parameters(scenario_xml->getModel().getParameters(), model_name_provider,
                            command_line); // Depends on ModelNameProvider.
    model_options = util::ModelOptionFlags(scenario_xml->getModel().getModelOptions(), model_name_provider,
                                           command_line, clock.one_ts()); // Depends on ModelNameProvider.

    // 4) elements depending on only elements initialised in (3).
    diagnostics = WithinHost::DiagnosticRegistry(parameters, model_options, command_line,
                                                 *scenario_xml); // Depends on Parameters.

    // 5) elements depending on only elements initialised in (4).
    mon::init_reporting(monitoring, *scenario_xml, diagnostics,
                        genotypes); // Reporting init depends on diagnostics and monitoring

    // Init models used by humans
    per_host_params = Transmission::PerHostParams(scenario_xml->getModel().getHuman().getAvailabilityToMosquitoes(),
                                                  clock, command_line);
    infection_incidence = Host::InfectionIncidenceParams(parameters, model_options);
    continuous.register_callback("new infections", "\tnew infections",
                                 [this](ostream& stream) { new_infections.report(stream); });
    within_host_model = WithinHost::WithinHostModel(parameters, model_options, command_line, genotypes, diagnostics,
                                                    monitoring, *scenario_xml, clock);
    clinical = Clinical::ClinicalParams(parameters, model_options, command_line, diagnostics, *scenario_xml, clock);
    decision_trees =
        Clinical::DecisionTreeLibrary(diagnostics, within_host_model.common.pk_pd, clinical.health_system_memory,
                                      intervention_manager, clock, command_line);
    infant_mortality = Clinical::InfantMortality(clinical.non_malaria_infant_mortality);
    neonatal_mortality = Host::NeonatalMortality(scenario_xml->getModel().getClinical(), diagnostics, command_line);
    age_structure = AgeStructure(scenario_xml->getDemography());

    // 3) elements depending on other elements; dependencies on (1) are not mentioned:
    // Transmission model initialisation depends on Transmission::PerHost and
    // genotypes (both from Human, from Population::init()) and
    // monitoring age groups (from Surveys.init()):
    // Note: PerHost dependency can be postponed; it is only used to set adultAge
    const size_t pop_size = scenario_xml->getDemography().getPopSize();

    population_model = std::make_unique<Population>(pop_size, age_structure,
                                                    Host::HumanParams{.per_host = &per_host_params,
                                                                      .infection_incidence = &infection_incidence,
                                                                      .options = &model_options,
                                                                      .within_host = &within_host_model,
                                                                      .clinical = &clinical,
                                                                      .interventions = &intervention_manager,
                                                                      .monitoring = &monitoring},
                                                    master_rngs.population);
    transmission_model = Transmission::create_transmission_model(
        scenario_xml->getEntomology(), static_cast<int>(pop_size), per_host_params, master_rngs.transmission, genotypes,
        model_options, command_line, continuous);

    register_continuous_population_callbacks(continuous, diagnostics);

    // Depends on transmission model (for species indexes):
    // MDA1D may depend on health system (too complex to verify)
    intervention_manager.init(scenario_xml->getInterventions(), *transmission_model, per_host_params, diagnostics,
                              genotypes, within_host_model, clinical, decision_trees, model_options, command_line,
                              clock, monitoring);
    clinical.set_health_system(scenario_xml->getHealthSystem(), decision_trees, within_host_model, intervention_manager,
                               clock, command_line); // Depends on interventions, PK/PD (from humanPop)
    mon::init_cohorts(monitoring, scenario_xml->getMonitoring(), intervention_manager); // Depends on interventions

    clock.reset_time();

    // Make sure warmup period is at least as long as a human lifespan, as the
    // length required by vector warmup, and is a whole number of years.
    SimTime warmup = clock.max_human_age();
    if (transmission_model->intervention_mode != Transmission::ForcedEir)
        warmup = max(warmup, Sim::from_years_i(55)); // Data is summed over 5 years; add an extra 50 for stabilization.

    warmup_length = Sim::from_years_i(static_cast<int>(ceil(Sim::in_years(warmup))));
}

Simulation::~Simulation() = default;

void Simulation::run(const CheckpointOptions& checkpointing) {
    bool started_from_checkpoint = false;
    if (checkpointing.enabled) {
        const ifstream checkpoint_file(checkpointing.file_name, ios::in);
        started_from_checkpoint = checkpoint_file.is_open();
        if (!started_from_checkpoint)
            errno = 0; // Cleanup errno if file doesn't exist
    }

    est_end_time = warmup_length + (clock.end_date() - clock.start_date()) + clock.one_ts();
    assert(est_end_time + Sim::never() < Sim::zero());

    if (started_from_checkpoint)
        resume(checkpointing.file_name);
    else
        start_new(checkpointing);

    // Main phase loop
    run_phase("Intervention period", end_time);

    finish();
}

void Simulation::resume(const std::string& checkpoint_file) {
    continuous.init(scenario_xml->getMonitoring(), true, clock, command_line);
    read_checkpoint(checkpoint_file, command_line, clock, master_rngs, genotypes, neonatal_mortality, new_infections,
                    infant_mortality, continuous, monitoring, end_time, est_end_time, *population_model,
                    *transmission_model, intervention_manager);

    /** Calculate ento availability percentiles **/
    per_host_params.calc_availability_percentiles();
}

void Simulation::start_new(const CheckpointOptions& checkpointing) {
    continuous.init(scenario_xml->getMonitoring(), false, clock, command_line);
    population_model->create_initial_humans();
    transmission_model->init2(population_model->humans);

    /** Calculate ento availability percentiles **/
    per_host_params.calc_availability_percentiles();

    /** Warm-up phase: 
     * Run the simulation using the equilibrium inoculation rates over one
     * complete lifespan (clock.max_human_age()) to reach immunological
     * equilibrium in all age classes. Don't report any events. */
    end_time = warmup_length;
    run_phase("Warmup", end_time);

    /** Transmission init phase:
     * Fit the emergence rate to the input EIR */
    SimTime iterate = transmission_model->init_iterate();
    while (iterate > Sim::zero()) {
        end_time = end_time + iterate;
        // adjust estimation of final time step: end of current period + length of main phase
        est_end_time = end_time + (clock.end_date() - clock.start_date()) + clock.one_ts();
        run_phase("EIR Calibration", end_time);
        iterate = transmission_model->init_iterate();
    }

    /** Main phase:
     * This procedure starts with the current state of the simulation 
     * It continues updating assuming:
     * (i)         the default (exponential) demographic model
     * (ii)        the entomological input defined by the EIRs in intEIR()
     * (iii)       the intervention packages defined in Intervention()
     * (iv)        the survey times defined in Survey() */
    // reset endTime and estEndTime to their exact value after initIterate()
    est_end_time = end_time = end_time + (clock.end_date() - clock.start_date()) + clock.one_ts();
    clock.start_intervention_period();
    new_infections.pre_main_sim_init();
    infant_mortality.pre_main_sim_init();
    genotypes.pre_main_sim_init();
    population_model->reset_recent_births();
    transmission_model->summarize(monitoring); // Only to reset TransmissionModel::inoculationsPerAgeGroup
    mon::init_main_sim(monitoring);

    if (checkpointing.enabled) {
        write_checkpoint(false, checkpointing.file_name, command_line, clock, master_rngs, genotypes,
                         neonatal_mortality, new_infections, infant_mortality, continuous, monitoring, end_time,
                         est_end_time, *population_model, *transmission_model, intervention_manager);
        if (checkpointing.stop_after_write)
            throw util::cmd_exception("Checkpoint test: checkpoint written", util::Error::None);
    }
}

void Simulation::finish() {
    cerr << '\r' << flush; // clean last line of progress-output

    for (Host::Human& human : population_model->humans)
        human.clinical_model->flush_reports();

    mon::write_survey_data(monitoring, command_line, infant_mortality);

#ifdef OM_STREAM_VALIDATOR
    util::StreamValidator.saveStream();
#endif
}

void Simulation::run_phase(const std::string& phase, SimTime end) {
    if (command_line.option(util::CommandLine::Verbose))
        cout << "Starting " << phase << "..." << endl; // NOLINT(performance-avoid-endl)

    run_until(end);

    if (command_line.option(util::CommandLine::Verbose))
        cout << "Finishing " << phase << "..." << endl; // NOLINT(performance-avoid-endl)
}

void Simulation::run_until(SimTime end) {
    while (clock.now() < end)
        step();
}

void Simulation::step() {
    Population& population = *population_model;
    Transmission::TransmissionModel& transmission = *transmission_model;

    if (command_line.option(util::CommandLine::Verbose) && clock.interv_date() > 0)
        cout << "Time step: " << clock.now() / clock.one_ts() << ", internal days: " << clock.now() << " | "
             << est_end_time << ", Intervention Date: " << clock.interv_date()
             << endl; // NOLINT(performance-avoid-endl)

    // Monitoring. clock.now() gives time of end of last step,
    // and is when reporting happens in our time-series.
    continuous.update(population);
    if (clock.interv_date() == mon::next_survey_date(monitoring)) {
        for (Host::Human& human : population.humans)
            Host::summarize(human, only_new_episode);
        transmission.summarize(monitoring);
        mon::conclude_survey(monitoring);
    }

    // Deploy interventions, at time clock.now().
    intervention_manager.deploy(population.humans, transmission);

    // Time step updates. Time steps are mid-day to mid-day.
    // clock.ts0() gives the date at the start of the step, clock.ts1() the date at the end.
    clock.start_update();

    // This should be called before humans contract new infections in the simulation step.
    // This needs the whole population (it is an approximation before all humans are updated).
    transmission.vector_update(population.humans);

    // NOTE: no neonatal mortalities will occur in the first 20 years of warmup
    // (until humans old enough to be pregnate get updated and can be infected).
    neonatal_mortality.update(population.humans);

    for (Host::Human& human : population.humans) {
        if (human.get_dob() + clock.max_human_age() >= warmup_length) // this is last time of possible update
            Host::update(human, transmission, neonatal_mortality, infant_mortality, new_infections);
    }

    population.update();

    // Doesn't matter whether non-updated humans are included (value isn't used
    // before all humans are updated).
    transmission.update_kappa(population.humans);
    transmission.survey_eir();

    clock.end_update();

    if (command_line.option(util::CommandLine::Progress))
        print_progress(last_percent, clock.now(), est_end_time);
    print_errno(clock.now());
}

}
