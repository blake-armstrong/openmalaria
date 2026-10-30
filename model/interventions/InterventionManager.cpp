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

#include "interventions/InterventionManager.h"
#include "Population.h"
#include "util/CommandLine.h"
#include "util/UnitParse.h"
#include "util/logging.h"
#include "interventions/GVI.h"
#include "interventions/IRS.h"
#include "interventions/ITN.h"
#include "interventions/Vaccine.h"
#include "interventions/HumanInterventionComponents.h"
#include "interventions/Deployments.h"
#include "Host/WithinHost/Diagnostic.h"
#include "Transmission/VectorModel.h"
#include "Clinical/ClinicalParams.h"
#include "Host/WithinHost/WithinHostModel.h"

#include <algorithm>
#include <memory>
#include <format>
#include <sstream>
#include <utility>

namespace OM::interventions {

namespace {

template <typename Printable> string details(const Printable& printable) {
    std::ostringstream out;
    printable.print_details(out);
    return out.str();
}

}

// ———  InterventionManager  ———

InterventionManager::InterventionManager() = default;

InterventionManager::~InterventionManager() = default;

void InterventionManager::init(const scnXml::Interventions& interv_elt, Transmission::TransmissionModel& transmission,
                               const Transmission::PerHostParams& per_host_params,
                               const WithinHost::DiagnosticRegistry& diagnostics,
                               const WithinHost::GenotypeRegistry& genotypes, WithinHost::WithinHostModel& within_host,
                               Clinical::ClinicalParams& clinical, Clinical::DecisionTreeLibrary& decision_trees,
                               const util::ModelOptionFlags& options, const util::CommandLineOptions& command_line,
                               const Clock& clock, mon::MonitoringState& monitoring) {
    sim_clock = &clock;
    next_timed = 0;
    verbose = command_line.option(util::CommandLine::Verbose);

    if (interv_elt.getChangeHS().present()) {
        const scnXml::ChangeHS& chs = interv_elt.getChangeHS().get();
        if (chs.getTimedDeployment().size() > 0) {
            // timed deployments:
            for (const auto& it : chs.getTimedDeployment()) {
                try {
                    const SimTime date = unit_parse::read_date(
                        it.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                        command_line);
                    timed.push_back(std::make_unique<TimedChangeHSDeployment>(date, it, clinical, decision_trees,
                                                                              within_host, *this, clock, command_line));
                } catch (const util::format_error& e) {
                    throw util::xml_scenario_error(
                        string("interventions/changeHS/timedDeployment/time: ").append(e.message()));
                }
            }
        }
    }
    if (interv_elt.getChangeEIR().present()) {
        const scnXml::ChangeEIR& eir = interv_elt.getChangeEIR().get();
        if (eir.getTimedDeployment().size() > 0) {
            // timed deployments:
            for (const auto& it : eir.getTimedDeployment()) {
                try {
                    const SimTime date = unit_parse::read_date(
                        it.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                        command_line);
                    timed.push_back(std::make_unique<TimedChangeEIRDeployment>(date, it, clock));
                } catch (const util::format_error& e) {
                    throw util::xml_scenario_error(
                        string("interventions/changeEIR/timedDeployment/time: ").append(e.message()));
                }
            }
        }
    }
    // species_index_map is not available with the non-vector model or
    // non-dynamic mode, so setting it (lazily) also checks sim mode:
    map<string, size_t> species_index_map;

    {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model != nullptr)
            species_index_map = vector_model->get_species_index_map();
    }

    if (interv_elt.getHuman().present()) {
        const scnXml::HumanInterventions& human = interv_elt.getHuman().get();
        bool first_vaccine = true;

        // 1. Read components
        for (const auto& component : human.getComponent()) {
            if (identifier_map.count(component.getId()) > 0) {
                throw util::xml_scenario_error(
                    std::format("The id attribute of interventions.human.component elements must be unique; found "
                                "\"{}\" twice.",
                                static_cast<const std::string&>(component.getId())));
            }
            const ComponentId id(human_components.size()); // i.e. index of next item
            identifier_map.insert(make_pair(component.getId(), id));

            SimTime expire_after = Sim::future();
            if (component.getSubPopRemoval().present()) {
                const scnXml::SubPopRemoval& remove_opts = component.getSubPopRemoval().get();
                if (remove_opts.getOnFirstBout()) {
                    remove_at_lists[sub_pop_remove::OnFirstBout].push_back(id);
                }
                if (remove_opts.getOnFirstInfection()) {
                    remove_at_lists[sub_pop_remove::OnFirstInfection].push_back(id);
                }
                if (remove_opts.getOnFirstTreatment()) {
                    remove_at_lists[sub_pop_remove::OnFirstTreatment].push_back(id);
                }
                if (remove_opts.getAfterYears().present()) {
                    expire_after = clock.from_years_n(remove_opts.getAfterYears().get());
                }
            }

            unique_ptr<HumanInterventionComponent> hi_component;
            if (component.getScreen().present()) {
                hi_component = std::make_unique<ScreenComponent>(id, component.getScreen().get(), *this, diagnostics);
            } else if (component.getTreatSimple().present()) {
                hi_component = std::make_unique<TreatSimpleComponent>(id, component.getTreatSimple().get(), options,
                                                                      clock, command_line);
            } else if (component.getTreatPKPD().present()) {
                hi_component =
                    std::make_unique<TreatPKPDComponent>(id, component.getTreatPKPD().get(), within_host.common.pk_pd);
            } else if (component.getDecisionTree().present()) {
                hi_component =
                    std::make_unique<DecisionTreeComponent>(id, component.getDecisionTree().get(), decision_trees);
            } else if (component.getPEV().present()) {
                hi_component = std::make_unique<VaccineComponent>(id, component.getPEV().get(), vaccines::PEV,
                                                                  std::exchange(first_vaccine, false), options,
                                                                  genotypes, clock, command_line);
            } else if (component.getBSV().present()) {
                hi_component = std::make_unique<VaccineComponent>(id, component.getBSV().get(), vaccines::BSV,
                                                                  std::exchange(first_vaccine, false), options,
                                                                  genotypes, clock, command_line);
            } else if (component.getTBV().present()) {
                hi_component = std::make_unique<VaccineComponent>(id, component.getTBV().get(), vaccines::TBV,
                                                                  std::exchange(first_vaccine, false), options,
                                                                  genotypes, clock, command_line);
            } else if (component.getITN().present()) {
                hi_component = std::make_unique<ITNComponent>(id, component.getITN().get(), species_index_map, clock,
                                                              command_line);
            } else if (component.getIRS().present()) {
                hi_component = std::make_unique<IRSComponent>(id, component.getIRS().get(), species_index_map, clock,
                                                              command_line);
            } else if (component.getGVI().present()) {
                hi_component = std::make_unique<GVIComponent>(id, component.getGVI().get(), species_index_map, clock,
                                                              command_line);
            } else if (component.getRecruitmentOnly().present()) {
                hi_component = std::make_unique<RecruitmentOnlyComponent>(id);
            } else if (component.getClearImmunity().present()) {
                hi_component = std::make_unique<ClearImmunityComponent>(id);
            } else {
                throw util::xml_scenario_error("expected intervention.human.component element to have a "
                                               "child, didn't find it (perhaps I need updating)");
            }
            hi_component->set_expire_after(expire_after);
            human_components.push_back(std::move(hi_component));
        }

        // 2. Read the list of deployments
        for (const auto& elt : human.getDeployment()) {
            // 2.a intervention components
            auto intervention =
                std::make_shared<HumanIntervention>(elt.getComponent(), elt.getCondition(), *this, monitoring);

            // 2.b intervention deployments
            for (const auto& cts_it : elt.getContinuous()) {
                ComponentId sub_pop = ComponentId::whole_pop();
                bool complement = false;
                if (cts_it.getRestrictToSubPop().present()) {
                    const string& sub_pop_str = cts_it.getRestrictToSubPop().get().getId();
                    sub_pop = get_component_id(sub_pop_str);
                    complement = cts_it.getRestrictToSubPop().get().getComplement();
                }
                const scnXml::ContinuousList::DeploySequence& cts_seq = cts_it.getDeploy();
                for (const auto& it2 : cts_seq) {
                    try {
                        SimTime begin = clock.start_date();
                        if (it2.getBegin().present()) {
                            begin = unit_parse::read_date(
                                it2.getBegin().get(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/,
                                clock, command_line);
                        }
                        SimTime end = Sim::future();
                        if (it2.getEnd().present()) {
                            end = unit_parse::read_date(it2.getEnd().get(),
                                                        unit_parse::STEPS /*STEPS is only for backwards compatibility*/,
                                                        clock, command_line);
                        }
                        continuous.emplace_back(begin, end, it2, intervention, sub_pop, complement, per_host_params,
                                                clock);
                    } catch (const util::format_error& e) {
                        throw util::xml_scenario_error(
                            string("interventions/human/deployment/continuous/deploy: ").append(e.message()));
                    }
                }
            }
            for (auto timed_it = elt.getTimed().begin(); timed_it != elt.getTimed().end(); ++timed_it) {
                ComponentId sub_pop = ComponentId::whole_pop();
                bool complement = false;
                if (timed_it->getRestrictToSubPop().present()) {
                    const string& sub_pop_str = timed_it->getRestrictToSubPop().get().getId();
                    sub_pop = get_component_id(sub_pop_str);
                    complement = timed_it->getRestrictToSubPop().get().getComplement();
                }
                try {
                    multimap<SimTime, const scnXml::MassDeployment*> deploy_times;
                    for (const auto& it2 : timed_it->getDeploy()) {
                        SimTime date = unit_parse::read_date(
                            it2.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                            command_line);

                        if (it2.getRepeatStep().present() != it2.getRepeatEnd().present()) {
                            throw util::xml_scenario_error("deploy: use of repeatStep or repeatEnd without other");
                        }
                        if (it2.getRepeatStep().present()) {
                            const SimTime step = unit_parse::read_duration(it2.getRepeatStep().get(), unit_parse::NONE,
                                                                           clock, command_line);
                            if (step < clock.one_ts()) {
                                throw util::xml_scenario_error("deploy: repeatStep must be >= 1");
                            }
                            const SimTime end =
                                unit_parse::read_date(it2.getRepeatEnd().get(), unit_parse::NONE, clock, command_line);
                            while (date < end) {
                                deploy_times.insert(make_pair(date, &it2));
                                date = date + step;
                            }
                        } else {
                            deploy_times.insert(make_pair(date, &it2));
                        }
                    }
                    SimTime last_date = Sim::never();
                    for (auto& deploy_time : deploy_times) {
                        if (deploy_time.first == last_date) {
                            ostringstream msg;
                            msg << "Timed deployment of components ";
                            bool first = true;
                            for (const auto& cp : elt.getComponent()) {
                                if (!first) {
                                    msg << ", ";
                                } else {
                                    first = false;
                                }
                                msg << cp.getId();
                            }
                            msg << " has multiple deplyoments at date " << last_date << " (step "
                                << clock.in_steps(last_date - clock.start_date()) << ')';
                            throw util::xml_scenario_error(msg.str());
                        }
                        last_date = deploy_time.first;
                    }

                    if (timed_it->getCumulativeCoverage().present()) {
                        const scnXml::CumulativeCoverage& cum_cov = timed_it->getCumulativeCoverage().get();
                        const ComponentId cum_cov_component = get_component_id(cum_cov.getComponent());
                        for (auto& deploy_time : deploy_times) {
                            timed.push_back(std::make_unique<TimedCumulativeHumanDeployment>(
                                deploy_time.first, *deploy_time.second, intervention, sub_pop, complement,
                                cum_cov_component, per_host_params, clock));
                        }
                    } else {
                        for (auto& deploy_time : deploy_times) {
                            timed.push_back(std::make_unique<TimedHumanDeployment>(
                                deploy_time.first, *deploy_time.second, intervention, sub_pop, complement,
                                per_host_params, clock));
                        }
                    }
                } catch (const util::format_error& e) {
                    throw util::xml_scenario_error(
                        string("interventions/human/deployment/timed/deploy/time: ").append(e.message()));
                }
            }
        }
    }
    if (interv_elt.getImportedInfections().present()) {
        const scnXml::ImportedInfections& ii = interv_elt.getImportedInfections().get();
        imported_infections.init(ii, clock, command_line);
    }
    // Must come after vaccines are initialised:
    if (interv_elt.getInsertR_0Case().present()) {
        throw util::xml_scenario_error("R_0 code is disabled to reduce "
                                       "maintenance. If you need it, either use an old OpenMalaria "
                                       "version (pre 32) or request it be reenabled.");
        // code disabled; search for R_0 to find more blocks like this:
#if 0
        const scnXml::InsertR_0Case& elt = intervElt.getInsertR_0Case().get();
        if( elt.getTimedDeployment().size() > 0 ){
            Vaccine::verifyEnabledForR_0();
            // timed deployments:
            for( auto it = elt.getTimedDeployment().begin(); it != elt.getTimedDeployment().end(); ++it ){
                SimTime date = UnitParse::readDate(it->getTime(), UnitParse::STEPS /*STEPS is only for backwards compatibility*/, clock, command_line);
                timed.push_back( new TimedR_0Deployment( date ) );
            }
        }
#endif
    }
    if (interv_elt.getUninfectVectors().present()) {
        const scnXml::UninfectVectors& elt = interv_elt.getUninfectVectors().get();
        if (elt.getTimedDeployment().size() > 0) {
            // timed deployments:
            for (const auto& it : elt.getTimedDeployment()) {
                const SimTime date = unit_parse::read_date(
                    it.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock, command_line);
                timed.push_back(std::make_unique<TimedUninfectVectorsDeployment>(date, clock));
            }
        }
    }
    if (interv_elt.getVectorPop().present()) {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            using SeqT = scnXml::VectorPop::InterventionSequence;
            const SeqT& seq = interv_elt.getVectorPop().get().getIntervention();
            size_t instance = 0;
            for (const auto& elt : seq) {
                if (elt.getTimed().present()) {
                    vector_model->init_vector_interv(elt.getDescription().getAnopheles(), instance, elt.getName(),
                                                     clock, command_line);

                    const scnXml::TimedBaseList::DeploySequence& seq = elt.getTimed().get().getDeploy();
                    for (const auto& it : seq) {
                        const SimTime date = unit_parse::read_date(
                            it.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                            command_line);
                        timed.push_back(std::make_unique<TimedVectorDeployment>(date, instance, clock));
                    }
                    instance++;
                }
            }
        } else
            throw util::xml_scenario_error("vector model interventions can not be used with the non-vector model");
    }
    if (interv_elt.getAddNonHumanHosts().present()) {
        Transmission::VectorModel const* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            using SeqT = scnXml::AddNonHumanHosts::NonHumanHostsSequence;
            const SeqT& seq = interv_elt.getAddNonHumanHosts().get().getNonHumanHosts();
            size_t instance = 0;
            for (const auto& elt : seq) {
                if (elt.getTimed().present()) {
                    for (const scnXml::Deploy2& deploy : elt.getTimed().get().getDeploy()) {
                        const SimTime date = unit_parse::read_date(
                            deploy.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                            command_line);
                        const SimTime lifespan =
                            unit_parse::read_duration(deploy.getLifespan(), unit_parse::NONE, clock, command_line);
                        timed.push_back(std::make_unique<TimedAddNonHumanHostsDeployment>(
                            date, elt.getName(), lifespan, elt.getDescription().getAnopheles(), transmission,
                            per_host_params, clock));
                    }
                    instance++;
                }
            }
        } else
            throw util::xml_scenario_error("vector model interventions can not be used with the non-vector model");
    }
    if (interv_elt.getNonHumanHostsModifications().present()) {
        Transmission::VectorModel const* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            using SeqT = scnXml::NonHumanHostsModifications::InterventionSequence;
            const SeqT& seq = interv_elt.getNonHumanHostsModifications().get().getIntervention();
            size_t instance = 0;
            for (const auto& elt : seq) {
                if (elt.getTimed().present()) {
                    const scnXml::DecayFunction& decay = elt.getDecay();
                    const scnXml::TimedBaseList::DeploySequence& seq = elt.getTimed().get().getDeploy();
                    for (const auto& it : seq) {
                        const SimTime date = unit_parse::read_date(
                            it.getTime(), unit_parse::STEPS /*STEPS is only for backwards compatibility*/, clock,
                            command_line);
                        timed.push_back(std::make_unique<TimedNonHumanHostsDeployment>(
                            date, instance, elt.getNonHumanHostsName(), elt.getDescription().getAnopheles(), decay,
                            transmission, clock, command_line));
                    }
                    instance++;
                }
            }
        } else
            throw util::xml_scenario_error("vector model interventions can not be used with the non-vector model");
    }
    if (interv_elt.getVectorTrap().present()) {
        auto* vector_model = dynamic_cast<Transmission::VectorModel*>(&transmission);
        if (vector_model) {
            size_t instance = 0;
            for (const scnXml::VectorTrap& trap : interv_elt.getVectorTrap().get().getIntervention()) {
                vector_model->init_vector_trap(trap.getDescription(), instance, trap.getName(), clock, command_line);
                if (trap.getTimed().present()) {
                    for (const scnXml::Deploy1& deploy : trap.getTimed().get().getDeploy()) {
                        const SimTime date =
                            unit_parse::read_date(deploy.getTime(), unit_parse::STEPS, clock, command_line);
                        const double ratio = deploy.getRatioToHumans();
                        const SimTime lifespan =
                            unit_parse::read_duration(deploy.getLifespan(), unit_parse::NONE, clock, command_line);
                        timed.push_back(std::make_unique<TimedTrapDeployment>(date, instance, ratio, lifespan, clock));
                    }
                }
                instance += 1;
            }
        } else
            throw util::xml_scenario_error("vector model interventions can not be used with the non-vector model");
    }

    // lists must be sorted, increasing
    // For reproducability, we need to use stable_sort, not sort.
    std::ranges::stable_sort(continuous, ByDeployTime{});
    std::ranges::stable_sort(timed, ByDeployTime{});

    // make sure the list ends with something always in the future, so we don't
    // have to check nextTimed is within range:
    timed.push_back(std::make_unique<DummyTimedDeployment>(clock));

    if (command_line.option(util::CommandLine::PrintInterventions)) {
        logging::info("Continuous deployments:");
        logging::info("begin\tend\tage\tsub pop\tcompl\tcoverag\tcomponents");
        for (auto& deployment : continuous)
            logging::info("{}", details(deployment));
        logging::info("Timed deployments:");
        logging::info("time\tmin age\tmax age\tsub pop\tcompl\tcoverag\tcomponents");
        for (auto& deploy : timed)
            logging::info("{}", details(*deploy));
        logging::info("Human components:");
        for (auto& component : human_components)
            logging::info("{}", details(*component));
    }
}

ComponentId InterventionManager::get_component_id(const string& text_id) const {
    auto it = identifier_map.find(text_id);
    if (it == identifier_map.end()) {
        throw util::xml_scenario_error(std::format(
            "unable to find an intervention component with id \"{}\" (wrong name, no definition or used before "
            "definition?)",
            text_id));
    }
    return it->second;
}

void InterventionManager::load_from_checkpoint(vector<Host::Human>& population,
                                               Transmission::TransmissionModel& transmission) {
    const SimTime date = clock().interv_date();
    // We need to re-deploy changeHS and changeEIR interventions, but nothing
    // else. nextTimed should be zero so we can go through all past interventions.
    // Only redeploy those which happened before this time step.
    assert(next_timed == 0);
    while (timed[next_timed]->date < date) {
        TimedDeployment* deployment = &*timed[next_timed];
        if (dynamic_cast<TimedChangeHSDeployment*>(deployment) != nullptr ||
            dynamic_cast<TimedChangeEIRDeployment*>(deployment) != nullptr) {
            // Note: neither changeHS nor changeEIR interventions care what the
            // current time step is when they are deployed, so we don't need to
            // tell them the deployment date.
            deployment->deploy(population, transmission);
        }
        next_timed += 1;
    }
}

void InterventionManager::deploy(vector<Host::Human>& population, Transmission::TransmissionModel& transmission) {
    if (clock().interv_time() < Sim::zero())
        return;

    // deploy imported infections (not strictly speaking an intervention)
    imported_infections.import(population);

    // deploy timed interventions
    const SimTime now = clock().interv_date();
    while (timed[next_timed]->date <= now) {
        if (verbose) {
            logging::info("deploy::{}::Timed{}", now, details(*timed[next_timed]));
        }

        timed[next_timed]->deploy(population, transmission);
        next_timed += 1;
    }

    // deploy continuous interventions
    for (Host::Human& human : population) {
        uint32_t next_cts_dist = human.next_cts_dist;
        // deploy continuous interventions
        while (next_cts_dist < continuous.size()) {
            if (verbose) {
                logging::info("deploy::{}::Continuous{}", now, details(continuous[next_cts_dist]));
            }
            if (!continuous[next_cts_dist].filter_and_deploy(human))
                break; // deployment (and all remaining) happens in the future
            next_cts_dist = ++human.next_cts_dist;
        }
    }
}

} // namespace OM::interventions
