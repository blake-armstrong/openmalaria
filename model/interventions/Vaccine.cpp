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

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace OM::interventions {
using namespace OM::util;

namespace {

using LocusAlleles = vector<vector<uint32_t>>;

string phenotype_name(const scnXml::VaccinePhenotype& phenotype, size_t index) {
    return phenotype.getName().present() ? string(phenotype.getName().get()) : to_string(index);
}

set<string> restricted_loci(const scnXml::VaccinePhenotype& phenotype) {
    set<string> loci;
    for (const auto& restriction : phenotype.getRestriction())
        loci.insert(restriction.getOnLocus());
    return loci;
}

util::xml_scenario_error loci_mismatch(const string& locus, const string& restricting, const string& other) {
    return util::xml_scenario_error("Vaccine phenotype: locus " + locus + " included in restriction of phenotype " +
                                    restricting + " but not " + other);
}

LocusAlleles read_restrictions(const scnXml::VaccinePhenotype& phenotype,
                               const WithinHost::GenotypeRegistry& genotypes) {
    map<string, size_t> locus_index;
    LocusAlleles alleles;
    for (const auto& restriction : phenotype.getRestriction()) {
        const uint32_t allele_code = genotypes.find_allele_code(restriction.getOnLocus(), restriction.getToAllele());

        if (allele_code == numeric_limits<uint32_t>::max()) {
            throw util::xml_scenario_error("phenotype has "
                                           "restriction on locus " +
                                           string(restriction.getOnLocus()) + " allele " +
                                           string(restriction.getToAllele()) +
                                           " but this locus/allele "
                                           "has not been defined in parasiteGenetics section");
        }

        if (phenotype.getInitialEfficacy().empty())
            throw util::xml_scenario_error(
                string("Vaccine phenotype: initialEfficacy is required but is missing for phenotype: " +
                       string(restriction.getOnLocus() + string(" ") + restriction.getToAllele())));

        const auto [it, inserted] = locus_index.try_emplace(restriction.getOnLocus(), alleles.size());
        if (inserted)
            alleles.emplace_back();
        alleles[it->second].push_back(allele_code);
    }
    return alleles;
}

bool matches(const LocusAlleles& restrictions, const WithinHost::Genotype& genotype) {
    return std::ranges::all_of(restrictions, [&](const vector<uint32_t>& locus_alleles) {
        return std::ranges::any_of(locus_alleles, [&](uint32_t allele) { return genotype.alleles.contains(allele); });
    });
}

vector<vector<double>> per_genotype_efficacy(const scnXml::VaccineDescription::PhenotypeSequence& phenotypes,
                                             const WithinHost::GenotypeRegistry& genotypes) {
    const set<string> loci = phenotypes.empty() ? set<string>{} : restricted_loci(phenotypes[0]);
    vector<LocusAlleles> restrictions;
    restrictions.reserve(phenotypes.size());
    for (size_t i = 0; i < phenotypes.size(); ++i) {
        const set<string> these_loci = restricted_loci(phenotypes[i]);
        for (const string& locus : these_loci) {
            if (!loci.contains(locus))
                throw loci_mismatch(locus, phenotype_name(phenotypes[i], i), phenotype_name(phenotypes[0], 0));
        }
        for (const string& locus : loci) {
            if (!these_loci.contains(locus))
                throw loci_mismatch(locus, phenotype_name(phenotypes[0], 0), phenotype_name(phenotypes[i], i));
        }
        restrictions.push_back(read_restrictions(phenotypes[i], genotypes));
    }

    if (loci.empty() && phenotypes.size() > 1)
        throw util::xml_scenario_error("Vaccine phenotype: restrictions required when num. phenotypes > 1");

    const vector<WithinHost::Genotype>& genotype_list = genotypes.genotypes();
    vector<vector<double>> efficacy(genotype_list.size());
    for (size_t g = 0; g < genotype_list.size(); ++g) {
        optional<size_t> phenotype;
        for (size_t i = 0; i < phenotypes.size(); ++i) {
            if (!matches(restrictions[i], genotype_list[g]))
                continue;
            if (phenotype.has_value())
                throw util::xml_scenario_error("Vaccine phenotype: phenotype restrictions not restrictive enough: "
                                               "multiple phenotypes match some genotype");
            phenotype = i;
        }
        if (!phenotype.has_value())
            throw util::xml_scenario_error(
                "Vaccine phenotype: missing restriction for at least one locus and allele pair");
        for (const auto& value : phenotypes[*phenotype].getInitialEfficacy())
            efficacy[g].push_back(value.getValue());
    }
    return efficacy;
}

}

VaccineComponent::VaccineComponent(ComponentId component, const scnXml::VaccineDescription& vd, vaccines::Types type,
                                   bool report_deployments, const util::ModelOptionFlags& options,
                                   const WithinHost::GenotypeRegistry& genotypes, const Clock& clock,
                                   const util::CommandLineOptions& command_line)
    : HumanInterventionComponent(component), type(type),
      decay_func(DecayFunction::make_object(vd.getDecay(), "decay", clock, command_line)),
      n_genotypes(genotypes.count()), efficacy_b(vd.getEfficacyB().getValue()), report_deployments(report_deployments),
      sim_clock(&clock) {
    if (type == vaccines::BSV && options.option(util::VivaxSimpleModel))
        throw util::unimplemented_exception("blood stage vaccines (BSV) cannot be used with vivax model");

    opt_vaccine_genotype = options.option(util::VaccineGenotype);

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
        per_genotype_initial_mean_efficacy = per_genotype_efficacy(ps, genotypes);
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
    het_sample = params.decay_func->het_sample(rng);
}

void PerEffectPerHumanVaccine::checkpoint_het(ostream& stream) {
    params->decay_func->checkpoint_sample(het_sample, stream);
}

void PerEffectPerHumanVaccine::checkpoint_het(istream& stream, const InterventionManager& interventions) {
    params = dynamic_cast<const VaccineComponent*>(&interventions.get_component(component));
    assert(params != nullptr);
    params->decay_func->checkpoint_sample(het_sample, stream);
}

void PerHumanVaccine::checkpoint(istream& stream, const InterventionManager& interventions) {
    size_t l = 0;
    l & stream;
    validateListSize(static_cast<long>(l));
    effects.resize(l);
    for (PerEffectPerHumanVaccine& effect : effects) {
        effect.checkpoint(stream, interventions);
    }
}

double PerHumanVaccine::get_factor(vaccines::Types type, uint32_t genotype) const {
    double factor = 1.0;
    for (const auto& effect : effects) {
        if (effect.params->type == type) {
            const SimTime age =
                effect.params->clock().ts1() - effect.time_last_deployment; // implies age 1 TS on first use
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

    effect->per_genotype_initial_efficacy.resize(params.n_genotypes);

    for (size_t g = 0; g < params.per_genotype_initial_mean_efficacy.size(); g++)
        effect->per_genotype_initial_efficacy[g] =
            params.get_initial_efficacy(human.intervention_rng, num_doses_administered, g);

    util::streamValidate(effect->per_genotype_initial_efficacy);

    effect->num_doses_administered = num_doses_administered + 1;
    effect->time_last_deployment = params.clock().now_or_ts1();

    return true;
}

}
