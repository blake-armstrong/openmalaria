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

#ifndef HMOD_UTIL_MODEL_OPTIONS
#define HMOD_UTIL_MODEL_OPTIONS

#include "Global.h"
#include "util/ModelNameProvider.h"
#include <schema/scenario.h>
#include <bitset>
#include <cassert>
#include <optional>

class UnittestUtil;

namespace scnXml {
class OptionSet;
}
namespace OM {
namespace util {
class CommandLineOptions;

/** Flags signalling which versions of some models to use. */
enum OptionCodes {
    /* @brief Clinical episodes reduce the level of acquired immunity
	* 
	* Effective cumulative exposure to blood stage parasites is reduced by a
	* clinical sickness event, so that clinical bouts have a negative effect on
	* blood stage immunity.
	* 
	* (ImmediateOutcomes model: per event; EventScheduler: once per bout.)
	* 
	* Default: Clinical events have no effect on immune status except
	* secondarily via effects of treatment. */
    // 	PENALISATION_EPISODES,

    /** @brief Baseline availability of humans is sampled from a gamma distribution
	* Infections introduced by mass action with negative binomial
	* variation in numbers of infection.
	* 
	* Default: New infections are introduced via a Poisson process as described
	* in AJTMH 75 (suppl 2) pp11-18. */
    NegativeBinomialMassAction,

    /* @brief An IPT model, no longer used
	* 
	* Does nothing if IPT is not present. */
    // 	ATTENUATION_ASEXUAL_DENSITY,

    /** @brief Baseline availability of humans is sampled from a log normal distribution
	* 
	* Infections introduced by mass action with log normal variation in
	* infection rate.
	* 
	* Default: New infections are introduced via a Poisson process as described
	* in AJTMH 75 (suppl 2) pp11-18. */
    LognormalMassAction,

    /** Infections are introduced without using preerythrocytic immunity. */
    NoPreErythrocytic,

    /** @brief Bug fixes in Descriptive & DescriptiveIPT within-host models.
	*
      * For new parameterisations, both MAX_DENS_CORRECTION and INNATE_MAX_DENS
      * should be used. When using parameter sets from an old fitting run which
      * didn't originally use these options, turn them off for consistency.
	* 
	* MAX_DENS_RESET is not used since it is unneeded when MAX_DENS_CORRECTION
	* is present and wouldn't make sense when not. */
    // @{
    MaxDensCorrection,
    InnateMaxDens,
    // 	MAX_DENS_RESET,
    //@}

    /** @brief Parasite densities are predicted from an autoregressive process
	*
	* Default: Parasite densities are determined from the descriptive model
	* given in AJTMH 75 (suppl 2) pp19-31 .*/
    DummyWithinHostModel,

    /** Clinical episodes occur if parasitaemia exceeds the pyrogenic threshold.
	* 
	* Default: Clinical episodes are a stochastic function as described in AJTMH
	* 75 (suppl 2) pp56-62. */
    PredeterminedEpisodes,

    /** @brief The presentation model includes simulation of non-malaria fevers
	* 
	* Default: Non-malaria fevers are not simulated. */
    NonMalariaFevers,

    //Deprecated: PK/PD code is enabled in all compatible within-host models
    /** @brief Use a PK & PD model for drug effects
	 *
	 * This enables simulation of the pharmacokinetics and pharmacodynamics
         * of drugs, allowing simulation of partial treatment. The alternate
         * treatment model gives drugs all-or-nothing effects, and is available
         * regardless of whether this option is enabled.
	 * 
         * The available PK/PD model, "LSTM", is that developed by colleagues
         * at the Liverpool School of Tropical Medicine.
         * 
         * Use of a 1-day infection model is required. */
    // 	INCLUDES_PK_PD,

    /** @brief Use revised clinical and case management model, ClinicalEventScheduler
	* 
	* Default: use the Tediosi et al case management model (Case management as
	* described in AJTMH 75 (suppl 2) pp90-103), ClinicalImmediateOutcomes. */
    ClinicalEventScheduler,

    /** @brief Clinical episodes occur in response to a simple parasite density trigger
	* 
	* Default: Use the Ross et al presentation model (Clinical episodes are a
	* stochastic function as described in AJTMH 75 (suppl 2) pp56-62). */
    MuellerPresentationModel,

    /** @brief Simple heterogeneity
	* 
	* Defaults: No heterogeneity.
	* 
	* (Transmission) heterogeneity is incompatible with
	* NEGATIVE_BINOMIAL_MASS_ACTION and LOGNORMAL_MASS_ACTION because both try
	* to adjust _EIRFactor and it is not confirmed that the ways they do this is
	* compatible. */
    // @{
    /// @brief Allow simple heterogeneity in transmission
    TransHet,
    /// @brief Allow simple heterogeneity in comorbidity
    ComorbHet,
    /// @brief Allow simple heterogeneity in treatment seeking
    TreatHet,
    /// @brief Allow correlated heterogeneities in transmission and comorbidity
    ComorbTransHet,
    /// @brief Allow correlated heterogeneities in transmission and treatment seeking
    TransTreatHet,
    /// @brief Allow correlated heterogeneities comorbidity and treatment seeking
    ComorbTreatHet,
    /// @brief Allow correlated heterogeneities in transmission, comorbidity and treatment seeking
    TripleHet,
    // @}

    /** @brief Selection of within host models
	*/
    //@{
    /// Parasite densities are predicted from an empirical model
    EmpiricalWithinHostModel,

    /// Use Molineaux within host model
    MolineauxWithinHostModel,

    /** Use Penny infection model. */
    PennyWithinHostModel,
    // @}

    /** @brief Selection of gamma distribution for Molineaux or Penny within host models
	 */
    //@{
    /** Use Gamma distribution for mean duration (Molineaux model) */
    MeanDurationGamma,
    /** Use Gamma distribution for first local maximum (Molineaux model) */
    FirstLocalMaximumGamma,
    /** Use Gamma distribution for first local maximum (Molineaux model) */
    ParasiteReplicationGamma,

    /** Use Gamma distribution for immune threshold (Penny model) */
    ImmuneThresholdGamma,

    /** Use Gamma distribution for update density (Penny model) */
    UpdateDensityGamma,
    //@}

    /** Use the Garki density bias instead of the default one in the detection limit.
	 *
	 * The default bias corresponds to counting parasites and white blood cells
	 * (assuming a white blood cell density of 8000 per µl), the Garki bias to
	 * estimations from a probability function. */
    GarkiDensityBias,

    /** This has been removed; mass drug interventions can be used as a
         * replacement. */
    // 	IPTI_SP_MODEL,

    /* Replaced by "onlyNewEpisode" attribute on SurveyOptions. */
    // 	REPORT_ONLY_AT_RISK,

    /** Turn on vector life-cycle model. Allows better larviciding and
         * vector population dynamics modelling.
         * 
         * Requires vector model. */
    VectorLifeCycleModel,

    /** Turn on the simple mosquito population dynamics model (simpler
         * version of life-cycle model).
         * 
         * Requires vector model. */
    VectorSimpleMpdModel,

    /** Sample case-specific densities P*c and P*m as a pair from one of
         * the 35 patient records. */
    MolineauxPairwiseSample,

    /** Use a simple Vivax model instead of Falciparum.
         * 
         * See "Individual-Based Model for Plasmodium vvivax", Ross, Briet,
         * Hardy, Chitnis (unpublished?). */
    VivaxSimpleModel,

    /** No longer used. */
    //         PROPHYLACTIC_DRUG_ACTION_MODEL,

    /** Bug fixes:
         * 
         * Without this, the 5-day case management leaves uncomplicated cases
         * with indirect mortality untreated, and the 1-day case management 
         * forgets to apply indirect mortality if the sickness state doesn't
         * change.
         * 
         * This option fixes both bugs (though only one would have any effect,
         * depending on which case management model is used). */
    IndirectMortalityFix,

    /** Potentially a bug-fix: for in-hospital severe malaria patients
         * where treatment fails to clear parasites, enabling this option
         * selects the use of hospital Case-Fatality-Rate (as described in the
         * AJTMH supplement); otherwise the community CFR is used (old model
         * behaviour, whether a bug or intended behaviour).
         * 
         * This is disabled by default since model parameters have been fitted
         * with this disabled. New models could be fit with the option enabled.
         */
    CfrPfUseHospital,

    /** By default 'nExpected' and 'nNewInfections' outputs are affected
     	 * by the PEV vaccine factor (the vaccine reduces the 
     	 * 'expectedNumInfections'). Enabling this option causes the vaccine 
     	 * to be applied AFTER infections have been created. Some infections 
     	 * will be immediately discarded with some probability based on the 
     	 * vaccine factor. With this option, 'nExpected' will be unaffected 
     	 * by the vaccine and will be higher while 'nNewInfections' will be 
     	 * similar on average.
     	 * 
     	 * This option is required when genotypes are used in combination 
     	 * with the vaccine.
     	 */
    VaccineGenotype,

    /** This makes the health system memory behaves as expected. With this
         * option, specifying a health system memory of 30 days means that a 
         * new clinical case will be reported as early as 30 days after the 
         * initial case (6 time-steps), rather than 35d (7 time-steps) 
         * currently. Setting the health system memory to 5 days allows the 
         * model to report a new clinical case every time step.
         */
    HealthSystemMemoryFix,

    /** Use the exact emergence-rate solver to initialize Nv0.
         *
         * Enable this option to use the exact fitting method.
         * By default, the legacy adaptive fitter is used.
         */
    UseExactNv0Solver,

    // Used by tests; should be 1 more than largest option
    NumOptions,

    Ignore // special value for use in string to enum lookup
};

/// Encapsulation for "modelVersion" xml attribute
class ModelOptionFlags {
public:
    ModelOptionFlags() = default;

    /*
    * Initialize the set of model options to use, based on input XML contents.
    */
    ModelOptionFlags(const scnXml::Model::ModelOptionsOptional& options_elt, const util::ModelNameProvider& mnp,
                     const CommandLineOptions& command_line, SimTime one_ts);

    /** Return true if given option (from OptionCodes) is active. */
    bool option(OptionCodes code) const { return options.test(code); }

private:
    /*
    * Returns a bitset representing the model options used by the named "base"
    * model.
    *
    * Note: at present the base model sets values that are not considered "default".
    * This is because there are default model options which are often not desired,
    * but are turned on in the absence of anything in the input suggesting they should
    * be turned off for legacy (i.e. backwards compatibility) reasons.
    */
    static std::bitset<NumOptions> get_base_model_options();

    /*
    * Returns a bitset representing the default model options used when no named
    * model is selected.
    */
    static std::bitset<NumOptions> get_legacy_default_model_options();

    // Reset opts to default. Used by unit tests.
    void reset() {
        options.reset();
        options.set(MaxDensCorrection);
        options.set(InnateMaxDens);
    }
    void set(OptionCodes code) { options.set(code); }

    // The model options
    std::bitset<NumOptions> options;

    friend class ::UnittestUtil; // Note: class is in base namespace
};
}

namespace bridge {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline const util::ModelOptionFlags* current_model_options = nullptr;

inline const util::ModelOptionFlags& model_options() {
    assert(current_model_options != nullptr);
    return *current_model_options;
}

}

class ModelOptionsBinding {
public:
    explicit ModelOptionsBinding(const util::ModelOptionFlags& options) : previous(bridge::current_model_options) {
        bridge::current_model_options = &options;
    }
    ModelOptionsBinding(const ModelOptionsBinding&) = delete;
    ModelOptionsBinding& operator=(const ModelOptionsBinding&) = delete;
    ModelOptionsBinding(ModelOptionsBinding&&) = delete;
    ModelOptionsBinding& operator=(ModelOptionsBinding&&) = delete;
    ~ModelOptionsBinding() { bridge::current_model_options = previous; }

private:
    const util::ModelOptionFlags* previous;
};

namespace util {
class ModelOptions {
public:
    /** Return true if given option (from OptionCodes) is active.
         * 
         * Performance note: calling this a few times (e.g. during init) is
         * fine, but code needing a value repeatedly should cache it locally.
         */
    static bool option(OptionCodes code) { return bridge::model_options().option(code); }
};
}
}
#endif
