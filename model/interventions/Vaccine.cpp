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

#include "interventions/Vaccine.h"
#include "interventions/HumanComponents.h"
#include "Host/Human.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/ModelOptions.h"
#include "schema/interventions.h"
#include "util/StreamValidator.h"
#include "Host/WithinHost/Genotypes.h"
#include "interventions/InterventionManager.h"

#include <limits>
#include <cmath>

namespace OM::interventions {
using namespace OM::util;

VaccineComponent::VaccineComponent(ComponentId component, const scnXml::VaccineDescription& vd, vaccines::Types type,
                                   bool report_deployments)
    : HumanInterventionComponent(component), type(type), decay_func(DecayFunction::makeObject(vd.getDecay(), "decay")),
      efficacy_b(vd.getEfficacyB().getValue()), report_deployments(report_deployments) {
    if (type == vaccines::BSV && ModelOptions::option(util::VivaxSimpleModel))
        throw util::unimplemented_exception("blood stage vaccines (BSV) cannot be used with vivax model");

    opt_vaccine_genotype = util::ModelOptions::option(util::VaccineGenotype);

    const scnXml::VaccineDescription::InitialEfficacySequence& ies = vd.getInitialEfficacy();
    const scnXml::VaccineDescription::PhenotypeSequence& ps = vd.getPhenotype();

    //cout << ies.size() << " " << ps.size() << " " << WithinHost::Genotypes::N() << endl;

    if (opt_vaccine_genotype == false && ies.size() == 0)
        throw util::xml_scenario_error("Vaccine: initialEfficacy is required but is missing");

    if (opt_vaccine_genotype == true && ies.size() != 0)
        throw util::xml_scenario_error("Vaccine: initialEfficacy outside of phenotype definition with VACCINE_GENOTYPE "
                                       "option enabled is not allowed");

    /*    if(opt_vaccine_genotype == true && ies.size() == 0 && ps.size() != WithinHost::Genotypes::N())
        throw util::xml_scenario_error( "Vaccine: Phenotype sequence must reference all genotypes" );
    */
    if (opt_vaccine_genotype == true) {
        per_genotype_initial_mean_efficacy.resize(WithinHost::Genotypes::count());

        for (const auto& p : ps) {
            //cout << "Phenotype: " << ps[i].getName() << endl;

            const scnXml::VaccinePhenotype::RestrictionSequence& restrictions = p.getRestriction();
            const scnXml::VaccinePhenotype::InitialEfficacySequence& vpies = p.getInitialEfficacy();

            for (const auto& restriction : restrictions) {
                //cout << "\tonLocus: " << restrictions[j].getOnLocus() << ", " << "toAllelle: " << restrictions[j].getToAllele() << endl;
                //cout << "\tmatching allelle code: " << WithinHost::Genotypes::findAlleleCode(restrictions[j].getOnLocus(), restrictions[j].getToAllele()) << endl;

                const uint32_t allele_code =
                    WithinHost::Genotypes::find_allele_code(restriction.getOnLocus(), restriction.getToAllele());

                if (allele_code == numeric_limits<uint32_t>::max()) {
                    throw util::xml_scenario_error("phenotype has "
                                                   "restriction on locus " +
                                                   string(restriction.getOnLocus()) + " allele " +
                                                   string(restriction.getToAllele()) +
                                                   " but this locus/allele "
                                                   "has not been defined in parasiteGenetics section");
                }

                if (allele_code >= WithinHost::Genotypes::count())
                    throw util::xml_scenario_error(string("Vaccine phenotype: Invalid Locus ") +
                                                   restriction.getOnLocus() + string(" and allele ") +
                                                   restriction.getToAllele());

                if (per_genotype_initial_mean_efficacy[allele_code].size() != 0)
                    throw util::xml_scenario_error(
                        string("Vaccine phenotype: there are multiple restrictions locus " +
                               string(restriction.getOnLocus() + string(" and allele ") + restriction.getToAllele())));

                if (vpies.size() == 0)
                    throw util::xml_scenario_error(
                        string("Vaccine phenotype: initialEfficacy is required but is missing for phenotype: " +
                               string(restriction.getOnLocus() + string(" ") + restriction.getToAllele())));

                per_genotype_initial_mean_efficacy[allele_code].resize(vpies.size());

                for (size_t k = 0; k < vpies.size(); ++k)
                    per_genotype_initial_mean_efficacy[allele_code][k] = vpies[k].getValue();
            }
        }

        // Check that all genotypes have been adressed
        for (size_t i = 0; i < WithinHost::Genotypes::count(); ++i) {
            if (per_genotype_initial_mean_efficacy[i].size() == 0)
                throw util::xml_scenario_error(
                    "Vaccine phenotype: missing restriction for at least one locus and allele pair");
        }
    } else {
        per_genotype_initial_mean_efficacy.resize(1);
        per_genotype_initial_mean_efficacy[0].resize(ies.size());
        for (size_t i = 0; i < ies.size(); ++i)
            per_genotype_initial_mean_efficacy[0][i] = ies[i].getValue();
    }

    /* initialMeanEfficacy.resize (ies.size());
    for(size_t i = 0; i < initialMeanEfficacy.size(); ++i)
        initialMeanEfficacy[i] = ies[i].getValue();*/
}

void VaccineComponent::deploy(Host::Human& human, mon::DeployMethod method, VaccineLimits vacc_limits) const {
    const bool administered = human.vaccine.possibly_vaccinate(human, *this, vacc_limits);
    if (administered && report_deployments) {
        mon::record_deploy(mon::measure("nMassVaccinations"), mon::measure("nEPIVaccinations"), human, method);
    }
    assert(type == vaccines::PEV || type == vaccines::BSV || type == vaccines::TBV);
    if (method == mon::DeployMethod::Treat)
        mon::record_event(mon::measure("nTreatDeployments"), human);
}

component_types::Type VaccineComponent::component_type() const {
    if (type == vaccines::PEV)
        return component_types::PEV;
    else if (type == vaccines::BSV)
        return component_types::BSV;
    else if (type == vaccines::TBV)
        return component_types::TBV;
    else
        throw SWITCH_DEFAULT_EXCEPTION;
}

void VaccineComponent::print_details(ostream& out) const {
    out << id().id << "\t" << (type == vaccines::PEV ? "PEV" : (type == vaccines::BSV ? "BSV" : "TBV"));
}

double VaccineComponent::get_initial_efficacy(LocalRng& rng, size_t num_prev_doses, uint32_t genotype) const {
    /* If initialMeanEfficacy.size or more doses have already been given, use
     * the last efficacy. */
    if (num_prev_doses >= per_genotype_initial_mean_efficacy[genotype].size())
        num_prev_doses = per_genotype_initial_mean_efficacy[genotype].size() - 1;
    const double ime = per_genotype_initial_mean_efficacy[genotype][num_prev_doses];
    //NOTE(validation): With extra valiadation in random, the first difference is noticed here:
    util::streamValidate(ime);
    util::streamValidate(efficacy_b);
    if (ime == 0.0) {
        return 0.0;
    } else if (ime < 1.0) {
        const double result = rng.beta_with_mean(ime, efficacy_b);
        //NOTE(validation):: Without extra validation in random, the first difference is noticed here:
        util::streamValidate(result);
        //TODO(validation):: Why the difference? Bug in/limitation of StreamValidatior?
        // Extra memory allocation due to the extra logging causes some bad
        // memory usage to manifest differently? Am I forgetting to initialise
        // some variable?
        return result;
    } else {
        return 1.0;
    }
}

#if 0
/* R_0: this used to use _the_ vaccine configuration; this is just a check.
 * If reimplementing, use a new vaccine instance; implement some hook to ensure
 * that TBV is given to everyone after enabling the intervention, etc. */
void Vaccine::verifyEnabledForR_0 (){
    if( !types[PEV].active || !types[TBV].active )
        throw util::xml_scenario_error("PEV and TBV vaccines must have a "
                "description to use the insertR_0Case intervention");
}
#endif

// this is only used for checkpointing; loaded values are unimportant
PerEffectPerHumanVaccine::PerEffectPerHumanVaccine() : component(ComponentId::whole_pop()), num_doses_administered(0) {}

PerEffectPerHumanVaccine::PerEffectPerHumanVaccine(LocalRng& rng, const VaccineComponent& params)
    : component(params.id()), num_doses_administered(0), params(&params) {
    het_sample = params.decay_func->hetSample(rng);
}

void PerEffectPerHumanVaccine::checkpoint_het(ostream& stream) {
    params->decay_func->checkpointSample(het_sample, stream);
}

void PerEffectPerHumanVaccine::checkpoint_het(istream& stream) {
    params = dynamic_cast<const VaccineComponent*>(&bridge::intervention_manager().get_component(component));
    assert(params != nullptr);
    params->decay_func->checkpointSample(het_sample, stream);
}

double PerHumanVaccine::get_factor(vaccines::Types type, uint32_t genotype) const {
    double factor = 1.0;
    for (const auto& effect : effects) {
        if (effect.params->type == type) {
            const SimTime age = Sim::ts1() - effect.time_last_deployment; // implies age 1 TS on first use
            const double decay_factor = effect.het_sample->eval(age);
            factor *= 1.0 - effect.per_genotype_initial_efficacy[genotype] * decay_factor;
        }
    }
    return factor;
}

bool PerHumanVaccine::possibly_vaccinate(Host::Human& human, const VaccineComponent& params,
                                         interventions::VaccineLimits vacc_limits) {
    PerEffectPerHumanVaccine* effect = nullptr;
    for (auto& it : effects) {
        if (it.component == params.id()) {
            effect = &it;
            break;
        }
    }

    const uint32_t num_doses_administered = (effect == nullptr) ? 0 : effect->num_doses_administered;
    if (num_doses_administered < vacc_limits.min_prev_doses || num_doses_administered >= vacc_limits.max_cum_doses)
        return false; // no vaccination (this replaces the old schedule for continuous doses)

    if (effect == nullptr) {
        effects.push_back(PerEffectPerHumanVaccine(human.intervention_rng, params));
        effect = &effects.back();
    }

    effect->per_genotype_initial_efficacy.resize(WithinHost::Genotypes::count());

    for (size_t g = 0; g < params.per_genotype_initial_mean_efficacy.size(); g++)
        effect->per_genotype_initial_efficacy[g] =
            params.get_initial_efficacy(human.intervention_rng, num_doses_administered, g);

    util::streamValidate(effect->per_genotype_initial_efficacy);

    effect->num_doses_administered = num_doses_administered + 1;
    effect->time_last_deployment = Sim::now_or_ts1();

    return true;
}

}
