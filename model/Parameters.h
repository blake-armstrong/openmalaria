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

#ifndef HMOD_INPUT_PARAMETERS
#define HMOD_INPUT_PARAMETERS

#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <schema/scenario.h>
#include "util/ModelNameProvider.h"
#include "util/CommandLine.h"
#include "util/errors.h"

namespace scnXml {
class Parameters;
}
namespace OM {

/*
* Defines the names that clients of the Parameters class use in order to
* read the values of parameters.
*
* NOTE: any time that a new parameter is added here, a corresponding entry
* *must* be added to the map in the Parameters class.  Such an entry relates
* a parameter codes (positive ingeger) to the new parameter names.  If no
* such entry is added, the new parameter will not be usable in the simulation.
*/
enum class Parameter {
    /// @b Infection incidence model parameters
    //@{
    NegLogOneMinusSinf,
    EStar,
    SIMM,
    XStarP,
    GammaP,
    //@}
    /// @b Immunity parameters, mostly on infections
    //@{
    SigmaISq, ///< Host (not infection) parameter
    CumulativeYStar,
    CumulativeHStar,
    NegLogOneMinusAlphaM,
    DecayM,
    //@}
    /// @b DescriptiveInfection specific
    //@{
    Sigma0Sq,
    XNuStar,
    //@}
    /// @b Used in PathogenesisModel
    //@{
    YStarSq,
    ALPHA,
    //@}
    DensityBiasNonGarki,       ///< Used in Diagnostic
    BaselineAvailabilityShape, ///< Used in InfectionIncidenceModel
    LogOddsRatioCfCommunity,   ///< Used in CaseManagementModel
    IndirectRiskCofactor,      ///< Used in PathogenesisModel
    NonMalariaInfantMortality, ///< Used in Summary
    DensityBiasGarki,          ///< Used in Diagnostic
    SevereMalariaThreshhold,   ///< Used in PathogenesisModel
    ImmunityPenalty,           ///< Used in WHFalciparum
    ImmuneEffectorDecay,       ///< Used in WHFalciparum
    /// @b Used in PathogenesisModel
    //@{
    ComorbidityIntercept,
    YStarHalfLife,
    YStar1,
    //@}
    AsexualImmunityDecay, ///< Used in WHFalciparum
    /// @b Used in PathogenesisModel
    //@{
    YStar0,

    CriticalAgeForComorbidity,
    MuellerRateMultiplier,
    MuellerDensityExponent,
    //@}
    /// EventScheduler: v in "Case Fatality Rate proposal"
    CfrScaleFactor,

    /// @b Molineaux: sampling parameters (not pairwise mode only)
    MeanLocalMaxDensity,
    SdLocalMaxDensity,
    MeanDiffPosDays,
    SdDiffPosDays,

    /// EventScheduler: exp(-CFR_NEG_LOG_ALPHA) is the proportion of deaths occuring on the first day
    CfrNegLogAlpha,
};

/*
* This class' job is to make the values of model parameters available to clients when the simulation
* is running.
*
* From a user perspective, each parameter has these 3 parts:
*
* 1. The name of the parameter as written in XML.
*       OpenMalaria completely disregards this.
*
* 2. The numerical ID, "index" which identfies the parameter in XML.
*       OpenMalaria relies on this.
*
* 3. The value assigned to the parameter in XML.
*       OpenMalaria associates this with the numerical ID.
*
* From a developer perspective, each "parameter" can be thought of as having these 3 parts:
*
* 1. A parameter name (see Parameter enum class).
*       Clients of this class use a parameter name to refer to the parameter.
*
* 2. A numerical ID to identify the parameter.
*       This is used to map parameters in XML to parameters in OpenMalaria,
*       and for error reporting.
*
* 2. A (possible) floating point number representing the actual value assigned to the parameter.
*       If the user does not explicitly or implicitly set a value, an empty
*       value is assigned.  If a client of this class attempts to read the
*       parameter's value, but the value is empty, this is considered an
*       error - and this class is responsible for handling such errors.
*/
class Parameters {
public:
    /*
    * Expects that the scenario XML either explicitly describes a collection of parameters and values
    * or describes the name of a model to use.
    */
    Parameters() = default;

    Parameters(const scnXml::Model::ParametersOptional& xml_parameter_values, const util::ModelNameProvider& mnp,
               const util::CommandLineOptions& command_line) {
        for (const std::pair<int, Parameter> pair : id_code_to_name_map()) {
            const Parameter name = pair.second;
            name_to_value_map[name] = std::nullopt;
        }

        const util::ModelNames named_model_to_use = mnp.get_model_name();
        const bool use_named_model = named_model_to_use != util::ModelNames::None;
        if (use_named_model) {
            if (named_model_to_use == util::ModelNames::Base) {
                initialize_params_base_model();
            } else {
                throw util::xml_scenario_error(
                    "No pre-set parameter values are available for the specified model name.");
            }
        }

        // Get parameters specified expicitly in input XML, if any.
        // In the event where a named model was used to set parameter values earlier, it is intended
        // that any parameter values written explicitly in XML will override any values set by the
        // named model.
        const bool use_explicit_param_values = xml_parameter_values.present();
        if (use_explicit_param_values) {
            initialize_params_from_xml(xml_parameter_values.get(),
                                       command_line.option(util::CommandLine::DeprecationWarnings));
        }
    }

    /**
     * Get a parameter, using one of the Parameter codes.
     */
    double operator[](Parameter name) const {
        // First check the parameter actually exists and has a value.
        const std::optional<double>& value = name_to_value_map.at(name);
        if (!value.has_value()) {
            for (const std::pair<int, Parameter> pair : id_code_to_name_map()) {
                if (pair.second == name) {
                    const int param_code = pair.first;
                    throw util::xml_scenario_error("Parameter with index " + to_string(param_code) +
                                                   " required but not described.");
                }
            }
            throw util::base_exception(
                "A parameter required by this simulation is missing a definition for its ID in C++ code.");
        }

        return *value;
    }

private:
    /*
    * Initializes parameters using explicit values specified in the input XML,
    * and does some validation on the specified parameters and values.
    * This method will be used if the user does not use the "base" model or any
    * other pre-set collection of parameter values.
    */
    void initialize_params_from_xml(const scnXml::Parameters& parameters, bool deprecation_warnings) {
        const scnXml::Parameters::ParameterSequence& param_seq = parameters.getParameter();

        // It's okay if a user overwrites the value of a parameter that was set before this
        // method was called.  E.g. if they used a named model, and then chose to manually
        // override the value of a parameter set by it.  It's not okay if a user sets a value
        // for the same parameter twice.  That represents a mistake in the input XML which
        // we need to handle here.  This has the potential to save users time debugging inputs.
        std::set<int> param_ids_set_by_user;

        for (auto param : param_seq) {
            const int param_id = param.getNumber();
            const double param_value_from_xml = param.getValue();

            // C++20 offers "contains" which would be more expressive than "count" here.
            const bool param_id_is_valid = id_code_to_name_map().count(param_id);
            if (!param_id_is_valid) {
                if (deprecation_warnings) {
                    cerr << "Deprecation warning: <parameter> index" << to_string(param_id) << " is no longer used"
                         << endl;
                }

                // If we throw here, old scenarios containing the deprecated parameter
                // will need more manual work to migrate to new OpenMalaria versions.
                // This isn't unacceptable.  But it's simpler here to just skip over the
                // deprecated parameter.  In particular, many tests specify values for
                // deprecated some parameters.
                continue;
            }
            const Parameter name_of_param_to_set = id_code_to_name_map().at(param_id);

            const bool param_value_already_set = !param_ids_set_by_user.insert(param_id).second;
            if (param_value_already_set) {
                throw util::xml_scenario_error("Parameter with index " + to_string(param_id) +
                                               " described twice in XML.");
            }

            name_to_value_map[name_of_param_to_set] = param_value_from_xml;
        }
    }

    /*
    * Helper method to initialize the parameter identified by paramId with the specified value.
    * The purpose of this method are to simplify code for initializing named models and to, in
    * the event of an error, provide a more helpful error message than would otherwise be provided.
    */
    void init_param(int param_id, double value) {
        // Handles the error condition where idCodeToNameMap.at() is called with a non-existant key argument,
        // e.g. for a deprecated parameter.  This is so that a more informative error message can be given.
        // std::unordered_map is not able to be made constexpr, which means it would be non-trivial to handle
        // this error condition with a compile-time check instead.
        auto retrieve_param_name = [](const int id) -> Parameter {
            try {
                return id_code_to_name_map().at(id);
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << endl;
                throw util::traced_exception("Base model attempted to set a value for a parameter with id: " +
                                                 std::to_string(id) + ", which is not currently supported.",
                                             __FILE__, __LINE__);
            }
        };
        name_to_value_map[retrieve_param_name(param_id)] = value;
    }

    /*
    * Initializes some hardcoded values for some parameters, encapsulating the parameter values
    * that make up the base model.
    */
    void initialize_params_base_model() {
        init_param(1, 0.050736);                // '-ln(1-Sinf)'
        init_param(2, 0.03247);                 // Estar
        init_param(3, 0.138161050830301);       // Simm
        init_param(4, 1514.385853233699891);    // Xstar_p
        init_param(5, 2.03692533424484);        // gamma_p
        init_param(6, 10.173598698525799);      // sigma2i
        init_param(7, 35158523.31132510304451); // CumulativeYstar
        init_param(8, 97.334652723897705);      // CumulativeHstar
        init_param(9, 2.33031045876193);        // '-ln(1-alpha_m)'
        init_param(10, 2.53106547375805);       // decay_m
        init_param(11, 0.655747311168152);      // sigma2_0
        init_param(12, 0.916181104713054);      // Xstar_v
        init_param(13, 6502.26335600001039);    // Ystar2
        init_param(14, 142601.912520000012591); // alpha
        init_param(15, 0.177378570987455);      // Density bias (non Garki)
        init_param(16, 1.0);                    //  sigma2
        init_param(17, 0.736202);               // log oddsr CF community
        init_param(18, 0.018777338);            // Indirect risk cofactor
        init_param(19, 49.539046599999999);     // Non-malaria infant mortality
        init_param(20, 4.79610772546704);       // Density bias (Garki)
        init_param(21, 784455.599999999976717); // Severe Malaria Threshhold
        init_param(22, 1);                      // Immunity Penalty
        init_param(23, 0);                      // Immune effector decay
        init_param(24, 0.0968);                 // comorbidity intercept
        init_param(25, 0.275437402);            // Ystar half life
        init_param(26, 0.596539864);            // Ystar1
        init_param(27, 0);                      // Asexual immunity decay
        init_param(28, 296.302437899999973);    // Ystar0
        init_param(30, 0.117383);               // critical age for comorbidity
    }

    /*
    * Defines the map from parameter ID numbers (AKA parameter codes) to parameter names.
    *
    * Each parameter "code"/"index"/"ID number" is a positive integer that identifies the
    * given parameter in the input XML.
    *
    * The keys of this map need not form a contiguous sequence.
    * E.g. in the case where a parameter is deleted altogether from the simulation code.
    */
    static const std::unordered_map<int, Parameter>& id_code_to_name_map() {
        static const std::unordered_map<int, Parameter> IdCodeToName{
            {1, Parameter::NegLogOneMinusSinf},
            {2, Parameter::EStar},
            {3, Parameter::SIMM},
            {4, Parameter::XStarP},
            {5, Parameter::GammaP},
            {6, Parameter::SigmaISq},
            {7, Parameter::CumulativeYStar},
            {8, Parameter::CumulativeHStar},
            {9, Parameter::NegLogOneMinusAlphaM},
            {10, Parameter::DecayM},
            {11, Parameter::Sigma0Sq},
            {12, Parameter::XNuStar},
            {13, Parameter::YStarSq},
            {14, Parameter::ALPHA},
            {15, Parameter::DensityBiasNonGarki},
            {16, Parameter::BaselineAvailabilityShape},
            {17, Parameter::LogOddsRatioCfCommunity},
            {18, Parameter::IndirectRiskCofactor},
            {19, Parameter::NonMalariaInfantMortality},
            {20, Parameter::DensityBiasGarki},
            {21, Parameter::SevereMalariaThreshhold},
            {22, Parameter::ImmunityPenalty},
            {23, Parameter::ImmuneEffectorDecay},
            {24, Parameter::ComorbidityIntercept},
            {25, Parameter::YStarHalfLife},
            {26, Parameter::YStar1},
            {27, Parameter::AsexualImmunityDecay},
            {28, Parameter::YStar0},
            // 29 corresponds to a now-deprecated parameter.  If adding a new parameter,
            // don't use 29.  Since some old scenarios likely define a parameter value
            // with index 29 already.
            {30, Parameter::CriticalAgeForComorbidity},
            {31, Parameter::MuellerRateMultiplier},
            {32, Parameter::MuellerDensityExponent},
            {33, Parameter::CfrScaleFactor},
            {34, Parameter::MeanLocalMaxDensity},
            {35, Parameter::SdLocalMaxDensity},
            {36, Parameter::MeanDiffPosDays},
            {37, Parameter::SdDiffPosDays},
            {38, Parameter::CfrNegLogAlpha}};
        return IdCodeToName;
    }

    /*
    * Defines the map from parameter names to parameter (optional) values.
    *
    * The map values are made optional because many scenarios will specify
    * values for less than the full set of parameters.
    *
    * Any given Parameter has a value (other than std::nullopt) iff
    * either the XML explicitly states a value for the parameter or the
    * XML explicitly states to use some model which itself contains the
    * given parameter among its preset values.
    */
    std::unordered_map<Parameter, std::optional<double>> name_to_value_map;
};

}
#endif
