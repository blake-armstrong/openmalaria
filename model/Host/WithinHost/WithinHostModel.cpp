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

#include "Host/WithinHost/Infection/DescriptiveInfection.h"
#include "Host/WithinHost/Infection/DummyInfection.h"
#include "Host/WithinHost/Infection/EmpiricalInfection.h"
#include "Host/WithinHost/Infection/MolineauxInfection.h"
#include "Host/WithinHost/Infection/PennyInfection.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "Host/WithinHost/WHVivax.h"
#include "PkPd/LSTMModel.h"
#include "mon/Monitoring.h"
#include "util/UnitParse.h"
#include "util/errors.h"

#include "schema/scenario.h"

#include <string>

namespace OM::WithinHost {

namespace {

SimTime read_latent_p(const scnXml::Model& model) {
    // Default value to use if no <parameters> element is present in XML.
    SimTime latent_p = UnitParse::readShortDuration("15d", UnitParse::DAYS);
    try {
        if (model.getParameters().present()) {
            latent_p = UnitParse::readShortDuration(model.getParameters().get().getLatentp(), UnitParse::NONE);
        }
    } catch (const util::format_error& e) {
        throw util::xml_scenario_error(std::string("model/parameters/latentP: ").append(e.message()));
    }
    return latent_p;
}

bool any_patent_infected_measure_used() {
    return mon::isUsed(mon::measure("totalPatentInf")) || mon::isUsed(mon::measure("totalPatentInf_Imported")) ||
           mon::isUsed(mon::measure("totalPatentInf_Introduced")) ||
           mon::isUsed(mon::measure("totalPatentInf_Indigenous"));
}

}

WithinHostModel::WithinHostModel(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                                 const scnXml::Scenario& scenario) {
    if (options.option(util::VivaxSimpleModel)) {
        kind = Kind::Vivax;
        WHVivax::init(parameters, scenario.getModel());
        return;
    }

    falciparum = FalciparumParams(parameters, options);
    Pathogenesis::PathogenesisModel::init(parameters, scenario.getModel().getClinical(), false);
    Infection::init(read_latent_p(scenario.getModel()));

    if (options.option(util::DummyWithinHostModel)) {
        kind = Kind::Common;
        DummyInfection::init();
    } else if (options.option(util::EmpiricalWithinHostModel)) {
        kind = Kind::Common;
        EmpiricalInfection::init(); // 1-day time step check
    } else if (options.option(util::MolineauxWithinHostModel)) {
        kind = Kind::Common;
        MolineauxInfection::init(parameters);
    } else if (options.option(util::PennyWithinHostModel)) {
        kind = Kind::Common;
        PennyInfection::init();
    } else {
        DescriptiveInfection::init(parameters); // 5-day time step check
    }

    if (kind == Kind::Common) {
        common = CommonWithinHostParams(scenario);
        pk_pd::LSTMModel::init(scenario);
    } else {
        descriptive.report_patent_infected = any_patent_infected_measure_used();
    }
}

std::unique_ptr<WHInterface> WithinHostModel::create(util::LocalRng& rng, double comorbidity_factor) const {
    switch (kind) {
    case Kind::Vivax:
        return std::make_unique<WHVivax>(rng, comorbidity_factor);
    case Kind::Common:
        return std::make_unique<CommonWithinHost>(falciparum, common, rng, comorbidity_factor);
    case Kind::Descriptive:
        return std::make_unique<DescriptiveWithinHostModel>(falciparum, descriptive, rng, comorbidity_factor);
    }
    throw TRACED_EXCEPTION_DEFAULT("unknown within-host model kind");
}

}
