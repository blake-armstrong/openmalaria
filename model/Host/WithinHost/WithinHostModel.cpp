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

#include "Host/WithinHost/WithinHostModel.h"

#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Infection/CommonInfectionModel.h"
#include "Host/WithinHost/Infection/DescriptiveInfection.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "PkPd/PkPdParams.h"
#include "mon/Monitoring.h"
#include "util/UnitParse.h"
#include "util/errors.h"

#include "schema/scenario.h"

#include <cassert>
#include <string>

namespace OM::WithinHost {

namespace {

SimTime read_latent_p(const scnXml::Model& model, const Clock& clock, const util::CommandLineOptions& command_line) {
    // Default value to use if no <parameters> element is present in XML.
    SimTime latent_p = unit_parse::read_short_duration("15d", unit_parse::DAYS, clock, command_line);
    try {
        if (model.getParameters().present()) {
            latent_p = unit_parse::read_short_duration(model.getParameters().get().getLatentp(), unit_parse::NONE,
                                                       clock, command_line);
        }
    } catch (const util::format_error& e) {
        throw util::xml_scenario_error(std::string("model/parameters/latentP: ").append(e.message()));
    }
    return latent_p;
}

bool any_patent_infected_measure_used(const mon::MonitoringState& monitoring) {
    return mon::is_used(monitoring, mon::measure("totalPatentInf")) ||
           mon::is_used(monitoring, mon::measure("totalPatentInf_Imported")) ||
           mon::is_used(monitoring, mon::measure("totalPatentInf_Introduced")) ||
           mon::is_used(monitoring, mon::measure("totalPatentInf_Indigenous"));
}

}

WithinHostModel::WithinHostModel(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                                 const util::CommandLineOptions& command_line, const GenotypeRegistry& genotypes,
                                 const DiagnosticRegistry& diagnostics, const mon::MonitoringState& monitoring,
                                 const scnXml::Scenario& scenario, const Clock& clock) {
    if (options.option(util::VivaxSimpleModel)) {
        assert(genotypes.count() == 1);
        kind = Kind::Vivax;
        vivax = VivaxParams(scenario.getModel(), read_latent_p(scenario.getModel(), clock, command_line),
                            command_line.option(util::CommandLine::DeprecationWarnings));
        pathogenesis = Pathogenesis::PathogenesisParams(parameters, options, scenario.getModel().getClinical(), true,
                                                        clock, command_line);
        return;
    }

    falciparum = FalciparumParams(parameters, options, genotypes, diagnostics, monitoring);
    pathogenesis = Pathogenesis::PathogenesisParams(parameters, options, scenario.getModel().getClinical(), false,
                                                    clock, command_line);
    falciparum.latent_p = read_latent_p(scenario.getModel(), clock, command_line);

    CommonInfectionModel infection;
    if (options.option(util::DummyWithinHostModel)) {
        kind = Kind::Common;
        infection.kind = CommonInfectionModel::Kind::Dummy;
    } else if (options.option(util::EmpiricalWithinHostModel)) {
        kind = Kind::Common;
        infection.kind = CommonInfectionModel::Kind::Empirical;
        infection.empirical = EmpiricalInfectionParams(command_line); // 1-day time step check
    } else if (options.option(util::MolineauxWithinHostModel)) {
        kind = Kind::Common;
        infection.kind = CommonInfectionModel::Kind::Molineaux;
        infection.molineaux = MolineauxInfectionParams(parameters, options);
    } else if (options.option(util::PennyWithinHostModel)) {
        kind = Kind::Common;
        infection.kind = CommonInfectionModel::Kind::Penny;
        infection.penny = PennyInfectionParams(options);
    } else {
        descriptive.infection = DescriptiveInfectionParams(parameters, options, command_line); // 5-day time step check
    }

    if (kind == Kind::Common) {
        common = CommonWithinHostParams(scenario, infection, monitoring, clock, command_line);
        common.pk_pd = pk_pd::PkPdParams(scenario, genotypes, command_line);
    } else {
        descriptive.report_patent_infected = any_patent_infected_measure_used(monitoring);
    }
}

std::unique_ptr<WHInterface> WithinHostModel::create(util::LocalRng& rng, double comorbidity_factor) const {
    switch (kind) {
    case Kind::Vivax:
        return std::make_unique<WHVivax>(vivax, pathogenesis, treatments, rng, comorbidity_factor);
    case Kind::Common:
        return std::make_unique<CommonWithinHost>(falciparum, pathogenesis, treatments, common, rng,
                                                  comorbidity_factor);
    case Kind::Descriptive:
        return std::make_unique<DescriptiveWithinHostModel>(falciparum, pathogenesis, treatments, descriptive, rng,
                                                            comorbidity_factor);
    }
    throw TRACED_EXCEPTION_DEFAULT("unknown within-host model kind");
}

}
