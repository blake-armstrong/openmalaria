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

#include "util/ModelOptions.h"
#include "util/CommandLine.h"
#include "util/errors.h"
#include "util/logging.h"
#include "schema/util.h"

#include <array>
#include <format>

namespace OM::util {
// Utility: converts option strings to codes and back
class OptionCodeMap {
    // Lookup table to translate the strings used in the XML file to the internal enumerated values:
    map<string, OptionCodes> code_map;
    set<string> ignore_options;
    bool deprecation_warnings;

public:
    explicit OptionCodeMap(bool deprecation_warnings) : deprecation_warnings(deprecation_warnings) {
        // 	    codeMap["PENALISATION_EPISODES"] = PENALISATION_EPISODES;
        code_map["NEGATIVE_BINOMIAL_MASS_ACTION"] = NegativeBinomialMassAction;
        // codeMap["ATTENUATION_ASEXUAL_DENSITY"] = ATTENUATION_ASEXUAL_DENSITY;
        code_map["LOGNORMAL_MASS_ACTION"] = LognormalMassAction;
        code_map["NO_PRE_ERYTHROCYTIC"] = NoPreErythrocytic;
        code_map["MAX_DENS_CORRECTION"] = MaxDensCorrection;
        code_map["INNATE_MAX_DENS"] = InnateMaxDens;
        // 	codeMap["MAX_DENS_RESET"] = MAX_DENS_RESET;
        code_map["DUMMY_WITHIN_HOST_MODEL"] = DummyWithinHostModel;
        code_map["PREDETERMINED_EPISODES"] = PredeterminedEpisodes;
        code_map["NON_MALARIA_FEVERS"] = NonMalariaFevers;
        ignore_options.insert("INCLUDES_PK_PD");
        code_map["CLINICAL_EVENT_SCHEDULER"] = ClinicalEventScheduler;
        code_map["MUELLER_PRESENTATION_MODEL"] = MuellerPresentationModel;
        code_map["TRANS_HET"] = TransHet;
        code_map["COMORB_HET"] = ComorbHet;
        code_map["TREAT_HET"] = TreatHet;
        code_map["COMORB_TRANS_HET"] = ComorbTransHet;
        code_map["TRANS_TREAT_HET"] = TransTreatHet;
        code_map["COMORB_TREAT_HET"] = ComorbTreatHet;
        code_map["TRIPLE_HET"] = TripleHet;
        code_map["EMPIRICAL_WITHIN_HOST_MODEL"] = EmpiricalWithinHostModel;
        code_map["MOLINEAUX_WITHIN_HOST_MODEL"] = MolineauxWithinHostModel;
        code_map["PENNY_WITHIN_HOST_MODEL"] = PennyWithinHostModel;
        code_map["GARKI_DENSITY_BIAS"] = GarkiDensityBias;
        //      codeMap["IPTI_SP_MODEL"] = IPTI_SP_MODEL;
        //      codeMap["REPORT_ONLY_AT_RISK"] = REPORT_ONLY_AT_RISK;
        code_map["MEAN_DURATION_GAMMA"] = MeanDurationGamma;
        code_map["FIRST_LOCAL_MAXIMUM_GAMMA"] = FirstLocalMaximumGamma;
        code_map["IMMUNE_THRESHOLD_GAMMA"] = ImmuneThresholdGamma;
        code_map["UPDATE_DENSITY_GAMMA"] = UpdateDensityGamma;
        code_map["PARASITE_REPLICATION_GAMMA"] = ParasiteReplicationGamma;
        code_map["VECTOR_LIFE_CYCLE_MODEL"] = VectorLifeCycleModel;
        code_map["VECTOR_SIMPLE_MPD_MODEL"] = VectorSimpleMpdModel;
        code_map["MOLINEAUX_PAIRWISE_SAMPLE"] = MolineauxPairwiseSample;
        ignore_options.insert("PROPHYLACTIC_DRUG_ACTION_MODEL");
        code_map["VIVAX_SIMPLE_MODEL"] = VivaxSimpleModel;
        code_map["INDIRECT_MORTALITY_FIX"] = IndirectMortalityFix;
        code_map["VACCINE_GENOTYPE"] = VaccineGenotype;
        code_map["CFR_PF_USE_HOSPITAL"] = CfrPfUseHospital;
        code_map["HEALTH_SYSTEM_MEMORY_FIX"] = HealthSystemMemoryFix;
        code_map["USE_EXACT_NV0_SOLVER"] = UseExactNv0Solver;
    }

    OptionCodes operator[](const string& s) {
        auto code_it = code_map.find(s);
        if (code_it != code_map.end())
            return code_it->second;
        if (ignore_options.count(s)) {
            if (deprecation_warnings) {
                logging::warn("Deprecation warning: model option {} is no longer used", s);
            }
            return Ignore;
        }

        if (s == "PENALISATION_EPISODES" || s == "ATTENUATION_ASEXUAL_DENSITY")
            throw xml_scenario_error(std::format(
                "Please use schema 31 or earlier to use option {}; it is not available in later versions.", s));
        if (s == "IPTI_SP_MODEL")
            throw xml_scenario_error("The IPT model is no longer available. Use MDA instead.");
        if (s == "REPORT_ONLY_AT_RISK")
            throw xml_scenario_error(
                std::format("Option {} has been replaced by <SurveyOptions onlyNewEpisode=\"true\">.", s));
        throw xml_scenario_error(std::format("Unrecognised model option: {}", s));
    }
    // reverse-lookup in map; only used for error/debug printing so efficiency is unimportant
    // doesn't ensure code is unique in the map either
    string to_string(const OptionCodes code) {
        for (auto& code_it : code_map) {
            if (code_it.second == code)
                return code_it.first;
        }
        throw TRACED_EXCEPTION_DEFAULT("toString called with unknown code"); // this is a code error
    }
};

std::bitset<NumOptions> ModelOptionFlags::get_base_model_options() {
    // The "base" model is very simple
    bitset<NumOptions> all_options_off;

    return all_options_off;
}

std::bitset<NumOptions> ModelOptionFlags::get_legacy_default_model_options() {
    bitset<NumOptions> default_opt_set;

    // Kept for legacy reasons.  That is, when an input XML does not specify a model name
    // and does not set these model options to false, they are turned on *for backwards
    // compatibility*.
    default_opt_set.set(MaxDensCorrection);
    default_opt_set.set(InnateMaxDens);
    default_opt_set.set(IndirectMortalityFix);
    default_opt_set.set(HealthSystemMemoryFix);

    return default_opt_set;
}

ModelOptionFlags::ModelOptionFlags(const scnXml::Model::ModelOptionsOptional& options_elt,
                                   const util::ModelNameProvider& mnp, const CommandLineOptions& command_line,
                                   SimTime one_ts) {
    OptionCodeMap code_map(command_line.option(CommandLine::DeprecationWarnings));

    // Set options to legacy defaults, then override any given in the XML file:
    options = get_legacy_default_model_options();

    const util::ModelNames named_model_to_use = mnp.get_model_name();
    const bool use_named_model = named_model_to_use != util::ModelNames::None;
    if (use_named_model) {
        if (named_model_to_use == util::ModelNames::Base) {
            // This completely discards any legacy model options set above.
            options = get_base_model_options();
        } else {
            throw util::xml_scenario_error("No pre-set model options are available for the specified model name.");
        }
    }

    // Apply any user-specified overrides.
    if (options_elt.present()) {
        const scnXml::OptionSet::OptionSequence& opt_seq = options_elt.get().getOption();
        for (const auto& it : opt_seq) {
            const OptionCodes opt = code_map[it.getName()];
            if (opt != Ignore)
                options[opt] = it.getValue();
        }
    }

    // Print non-default model options:
    if (command_line.option(CommandLine::PrintModelOptions)) {
        const std::bitset<NumOptions> default_opt_set = get_legacy_default_model_options();
        string line = "Non-default model options:";
        for (int i = 0; i < NumOptions; ++i) {
            if (options[i] != default_opt_set[i])
                line += std::format("\t{}={:d}", code_map.to_string(OptionCodes(i)), options.test(i));
        }
        logging::info("{}", line);
    }

    // Test for incompatible options

    // An incompatibility triangle, listing options incompatible with each option
    // Doesn't check required versions
    // Originally from "description of variables for interface" excel sheet
    std::array<bitset<NumOptions>, NumOptions> incompatibilities{};

    incompatibilities[NegativeBinomialMassAction]
        .set(LognormalMassAction)
        .set(TransHet)
        .set(ComorbTransHet)
        .set(TransTreatHet)
        .set(TripleHet);
    incompatibilities[LognormalMassAction].set(TransHet).set(ComorbTransHet).set(TransTreatHet).set(TripleHet);

    // Note: MAX_DENS_CORRECTION is irrelevant when using new
    // within-host models, but we don't mark it incompatible so that we can
    // leave MAX_DENS_CORRECTION on by default.
    incompatibilities[DummyWithinHostModel]
        .set(PennyWithinHostModel)
        .set(EmpiricalWithinHostModel)
        .set(MolineauxWithinHostModel)
        .set(MeanDurationGamma)
        .set(FirstLocalMaximumGamma)
        .set(ParasiteReplicationGamma)
        .set(ImmuneThresholdGamma)
        .set(UpdateDensityGamma);

    incompatibilities[EmpiricalWithinHostModel]
        .set(MolineauxWithinHostModel)
        .set(PennyWithinHostModel)
        .set(MeanDurationGamma)
        .set(FirstLocalMaximumGamma)
        .set(ParasiteReplicationGamma)
        .set(ImmuneThresholdGamma)
        .set(UpdateDensityGamma);

    incompatibilities[MolineauxWithinHostModel]
        .set(PennyWithinHostModel)
        .set(ImmuneThresholdGamma)
        .set(UpdateDensityGamma);

    incompatibilities[PennyWithinHostModel]
        .set(MeanDurationGamma)
        .set(FirstLocalMaximumGamma)
        .set(ParasiteReplicationGamma);

    incompatibilities[NonMalariaFevers].set(MuellerPresentationModel);

    incompatibilities[TransHet].set(ComorbTransHet).set(TransTreatHet).set(ComorbTreatHet).set(TripleHet);
    incompatibilities[ComorbHet].set(ComorbTransHet).set(TransTreatHet).set(ComorbTreatHet).set(TripleHet);
    incompatibilities[TreatHet].set(ComorbTransHet).set(TransTreatHet).set(ComorbTreatHet).set(TripleHet);
    incompatibilities[ComorbTransHet].set(TransTreatHet).set(ComorbTreatHet).set(TripleHet);
    incompatibilities[TransTreatHet].set(ComorbTreatHet).set(TripleHet);
    incompatibilities[ComorbTreatHet].set(TripleHet);

    incompatibilities[MolineauxPairwiseSample].set(FirstLocalMaximumGamma).set(MeanDurationGamma);

    incompatibilities[VivaxSimpleModel]
        .set(DummyWithinHostModel)
        .set(EmpiricalWithinHostModel)
        .set(MolineauxWithinHostModel)
        .set(PennyWithinHostModel);

    for (size_t i = 0; i < NumOptions; ++i) {
        if (options[i] && (options & incompatibilities[i]).any()) {
            std::string msg = std::format("Incompatible model options: {}={:d} is incompatible with flags:",
                                          code_map.to_string(OptionCodes(i)), options.test(i));
            const bitset<NumOptions> incompat = (options & incompatibilities[i]);
            for (size_t j = 0; j < NumOptions; ++j) {
                if (incompat[j])
                    msg += std::format("\t{}={:d}", code_map.to_string(OptionCodes(j)), options.test(j));
            }
            throw xml_scenario_error(msg);
        }
    }

    // Required options (above table can't check these):
    if (options[InnateMaxDens] && !options[MaxDensCorrection])
        throw xml_scenario_error("INNATE_MAX_DENS requires MAX_DENS_CORRECTION");
    if (!options[MolineauxWithinHostModel] &&
        (options[FirstLocalMaximumGamma] || options[MeanDurationGamma] || options[ParasiteReplicationGamma]))
        throw xml_scenario_error("Molineaux model option used without MOLINEAUX_WITHIN_HOST_MODEL option");
    if (!options[PennyWithinHostModel] && (options[ImmuneThresholdGamma] || options[UpdateDensityGamma]))
        throw xml_scenario_error("Penny model option used without PENNY_WITHIN_HOST_MODEL option");

    if (one_ts == Sim::from_days(5)) {
        // 5 day TS is okay; some tests specific to this TS:
        bitset<NumOptions> require_one_day_ts;
        require_one_day_ts.set(ClinicalEventScheduler);

        for (size_t i = 0; i < NumOptions; ++i) {
            if (options[i] && require_one_day_ts[i]) {
                throw xml_scenario_error(std::format("Model option {} is only compatible with a 1-day time step.",
                                                     code_map.to_string(OptionCodes(i))));
            }
        }
    } else if (one_ts == Sim::from_days(1)) {
        // 1 day TS is also okay
    } else {
        throw xml_scenario_error(std::format("Time step set to {} days but only 1 and 5 days are supported.", one_ts));
    }
}

}
