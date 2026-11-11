//! Strongly-typed `ModelName`, `OptionCode`, and `ModelOptions` with full validation (`model/util/ModelOptions.cpp`).

use crate::error::CoreError;
use crate::time::SimTime;
use serde::{Deserialize, Serialize};

/// Pre-set named model selection (`<ModelName name="base"/>` or omitted).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum ModelName {
    /// No `<ModelName>` element specified; uses legacy default option flags and explicit `<parameters>`.
    #[default]
    None,
    /// `<ModelName name="base"/>`: 5-day step, all options `false`, and base parameters `1..=28, 30`.
    Base,
}

impl ModelName {
    /// Parses an optional XML `<ModelName name="..."/>` string.
    pub fn from_xml(name: Option<&str>) -> Result<Self, CoreError> {
        match name {
            None => Ok(Self::None),
            Some("base") => Ok(Self::Base),
            Some(other) => Err(CoreError::Scenario(format!(
                "Unrecognized model name: {other}"
            ))),
        }
    }
}

/// Enumerated OpenMalaria v50 model option flags (`OM::util::OptionCodes`).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[repr(u8)]
pub enum OptionCode {
    NegativeBinomialMassAction = 0,
    LognormalMassAction = 1,
    NoPreErythrocytic = 2,
    MaxDensCorrection = 3,
    InnateMaxDens = 4,
    DummyWithinHostModel = 5,
    PredeterminedEpisodes = 6,
    NonMalariaFevers = 7,
    ClinicalEventScheduler = 8,
    MuellerPresentationModel = 9,
    TransHet = 10,
    ComorbHet = 11,
    TreatHet = 12,
    ComorbTransHet = 13,
    TransTreatHet = 14,
    ComorbTreatHet = 15,
    TripleHet = 16,
    EmpiricalWithinHostModel = 17,
    MolineauxWithinHostModel = 18,
    PennyWithinHostModel = 19,
    MeanDurationGamma = 20,
    FirstLocalMaximumGamma = 21,
    ParasiteReplicationGamma = 22,
    ImmuneThresholdGamma = 23,
    UpdateDensityGamma = 24,
    GarkiDensityBias = 25,
    VectorLifeCycleModel = 26,
    VectorSimpleMpdModel = 27,
    MolineauxPairwiseSample = 28,
    VivaxSimpleModel = 29,
    IndirectMortalityFix = 30,
    CfrPfUseHospital = 31,
    VaccineGenotype = 32,
    HealthSystemMemoryFix = 33,
    UseExactNv0Solver = 34,
}

impl OptionCode {
    /// Total number of active v50 option codes (`NUM_OPTIONS = 35`).
    pub const COUNT: usize = 35;

    /// All option variants in canonical index order.
    pub const ALL: [Self; Self::COUNT] = [
        Self::NegativeBinomialMassAction,
        Self::LognormalMassAction,
        Self::NoPreErythrocytic,
        Self::MaxDensCorrection,
        Self::InnateMaxDens,
        Self::DummyWithinHostModel,
        Self::PredeterminedEpisodes,
        Self::NonMalariaFevers,
        Self::ClinicalEventScheduler,
        Self::MuellerPresentationModel,
        Self::TransHet,
        Self::ComorbHet,
        Self::TreatHet,
        Self::ComorbTransHet,
        Self::TransTreatHet,
        Self::ComorbTreatHet,
        Self::TripleHet,
        Self::EmpiricalWithinHostModel,
        Self::MolineauxWithinHostModel,
        Self::PennyWithinHostModel,
        Self::MeanDurationGamma,
        Self::FirstLocalMaximumGamma,
        Self::ParasiteReplicationGamma,
        Self::ImmuneThresholdGamma,
        Self::UpdateDensityGamma,
        Self::GarkiDensityBias,
        Self::VectorLifeCycleModel,
        Self::VectorSimpleMpdModel,
        Self::MolineauxPairwiseSample,
        Self::VivaxSimpleModel,
        Self::IndirectMortalityFix,
        Self::CfrPfUseHospital,
        Self::VaccineGenotype,
        Self::HealthSystemMemoryFix,
        Self::UseExactNv0Solver,
    ];

    /// Canonical XML option name.
    #[must_use]
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::NegativeBinomialMassAction => "NEGATIVE_BINOMIAL_MASS_ACTION",
            Self::LognormalMassAction => "LOGNORMAL_MASS_ACTION",
            Self::NoPreErythrocytic => "NO_PRE_ERYTHROCYTIC",
            Self::MaxDensCorrection => "MAX_DENS_CORRECTION",
            Self::InnateMaxDens => "INNATE_MAX_DENS",
            Self::DummyWithinHostModel => "DUMMY_WITHIN_HOST_MODEL",
            Self::PredeterminedEpisodes => "PREDETERMINED_EPISODES",
            Self::NonMalariaFevers => "NON_MALARIA_FEVERS",
            Self::ClinicalEventScheduler => "CLINICAL_EVENT_SCHEDULER",
            Self::MuellerPresentationModel => "MUELLER_PRESENTATION_MODEL",
            Self::TransHet => "TRANS_HET",
            Self::ComorbHet => "COMORB_HET",
            Self::TreatHet => "TREAT_HET",
            Self::ComorbTransHet => "COMORB_TRANS_HET",
            Self::TransTreatHet => "TRANS_TREAT_HET",
            Self::ComorbTreatHet => "COMORB_TREAT_HET",
            Self::TripleHet => "TRIPLE_HET",
            Self::EmpiricalWithinHostModel => "EMPIRICAL_WITHIN_HOST_MODEL",
            Self::MolineauxWithinHostModel => "MOLINEAUX_WITHIN_HOST_MODEL",
            Self::PennyWithinHostModel => "PENNY_WITHIN_HOST_MODEL",
            Self::MeanDurationGamma => "MEAN_DURATION_GAMMA",
            Self::FirstLocalMaximumGamma => "FIRST_LOCAL_MAXIMUM_GAMMA",
            Self::ParasiteReplicationGamma => "PARASITE_REPLICATION_GAMMA",
            Self::ImmuneThresholdGamma => "IMMUNE_THRESHOLD_GAMMA",
            Self::UpdateDensityGamma => "UPDATE_DENSITY_GAMMA",
            Self::GarkiDensityBias => "GARKI_DENSITY_BIAS",
            Self::VectorLifeCycleModel => "VECTOR_LIFE_CYCLE_MODEL",
            Self::VectorSimpleMpdModel => "VECTOR_SIMPLE_MPD_MODEL",
            Self::MolineauxPairwiseSample => "MOLINEAUX_PAIRWISE_SAMPLE",
            Self::VivaxSimpleModel => "VIVAX_SIMPLE_MODEL",
            Self::IndirectMortalityFix => "INDIRECT_MORTALITY_FIX",
            Self::CfrPfUseHospital => "CFR_PF_USE_HOSPITAL",
            Self::VaccineGenotype => "VACCINE_GENOTYPE",
            Self::HealthSystemMemoryFix => "HEALTH_SYSTEM_MEMORY_FIX",
            Self::UseExactNv0Solver => "USE_EXACT_NV0_SOLVER",
        }
    }

    /// Parses an XML `<option name="..."/>`, returning `Ok(None)` for deprecated ignored options.
    pub fn parse_xml(name: &str) -> Result<Option<Self>, CoreError> {
        if matches!(name, "INCLUDES_PK_PD" | "PROPHYLACTIC_DRUG_ACTION_MODEL") {
            return Ok(None);
        }
        for code in Self::ALL {
            if code.as_str() == name {
                return Ok(Some(code));
            }
        }
        let msg = match name {
            "PENALISATION_EPISODES" | "ATTENUATION_ASEXUAL_DENSITY" => format!(
                "Please use schema 31 or earlier to use option {name}; it is not available in later versions."
            ),
            "IPTI_SP_MODEL" => "The IPT model is no longer available. Use MDA instead.".into(),
            "REPORT_ONLY_AT_RISK" => format!(
                "Option {name} has been replaced by <SurveyOptions onlyNewEpisode=\"true\">."
            ),
            _ => format!("Unrecognised model option: {name}"),
        };
        Err(CoreError::Scenario(msg))
    }

    #[inline]
    const fn mask(self) -> u64 {
        1u64 << (self as u8)
    }
}

/// Instance-owned 64-bit model option bitset replacing static `OM::util::ModelOptions`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ModelOptions {
    bits: u64,
}

impl Default for ModelOptions {
    fn default() -> Self {
        Self::legacy_default()
    }
}

impl ModelOptions {
    /// `<ModelName name="base"/>` preset: all options `false`.
    #[must_use]
    pub const fn base() -> Self {
        Self { bits: 0 }
    }

    /// Legacy default option set when `<ModelName>` is omitted.
    #[must_use]
    pub const fn legacy_default() -> Self {
        Self {
            bits: OptionCode::MaxDensCorrection.mask()
                | OptionCode::InnateMaxDens.mask()
                | OptionCode::IndirectMortalityFix.mask()
                | OptionCode::HealthSystemMemoryFix.mask(),
        }
    }

    /// Builds and validates `ModelOptions` from `ModelName`, `(name, value)` overrides, and `interval`.
    pub fn from_xml_entries<'a, I>(
        model_name: ModelName,
        entries: I,
        interval: SimTime,
    ) -> Result<Self, CoreError>
    where
        I: IntoIterator<Item = (&'a str, bool)>,
    {
        let mut opts = match model_name {
            ModelName::None => Self::legacy_default(),
            ModelName::Base => Self::base(),
        };
        for (name, val) in entries {
            if let Some(code) = OptionCode::parse_xml(name)? {
                opts.set(code, val);
            }
        }
        opts.validate(interval)?;
        Ok(opts)
    }

    /// Returns `true` if `code` is enabled.
    #[inline]
    #[must_use]
    pub const fn get(self, code: OptionCode) -> bool {
        (self.bits & code.mask()) != 0
    }

    /// Sets `code` to `enabled`.
    #[inline]
    pub fn set(&mut self, code: OptionCode, enabled: bool) {
        if enabled {
            self.bits |= code.mask();
        } else {
            self.bits &= !code.mask();
        }
    }

    /// Returns options whose values differ from `Self::legacy_default()` (for `--print-model`).
    #[must_use]
    pub fn non_default_options(self) -> Vec<(OptionCode, bool)> {
        let def = Self::legacy_default();
        OptionCode::ALL
            .into_iter()
            .filter(|&c| self.get(c) != def.get(c))
            .map(|c| (c, self.get(c)))
            .collect()
    }

    /// Validates pairwise option incompatibilities, prerequisite options, and time-step constraints.
    pub fn validate(self, interval: SimTime) -> Result<(), CoreError> {
        use OptionCode as O;
        const PAIRS: &[(O, &[O])] = &[
            (O::NegativeBinomialMassAction, &[O::LognormalMassAction, O::TransHet, O::ComorbTransHet, O::TransTreatHet, O::TripleHet]),
            (O::LognormalMassAction, &[O::TransHet, O::ComorbTransHet, O::TransTreatHet, O::TripleHet]),
            (O::DummyWithinHostModel, &[O::PennyWithinHostModel, O::EmpiricalWithinHostModel, O::MolineauxWithinHostModel, O::MeanDurationGamma, O::FirstLocalMaximumGamma, O::ParasiteReplicationGamma, O::ImmuneThresholdGamma, O::UpdateDensityGamma]),
            (O::EmpiricalWithinHostModel, &[O::MolineauxWithinHostModel, O::PennyWithinHostModel, O::MeanDurationGamma, O::FirstLocalMaximumGamma, O::ParasiteReplicationGamma, O::ImmuneThresholdGamma, O::UpdateDensityGamma]),
            (O::MolineauxWithinHostModel, &[O::PennyWithinHostModel, O::ImmuneThresholdGamma, O::UpdateDensityGamma]),
            (O::PennyWithinHostModel, &[O::MeanDurationGamma, O::FirstLocalMaximumGamma, O::ParasiteReplicationGamma]),
            (O::NonMalariaFevers, &[O::MuellerPresentationModel]),
            (O::TransHet, &[O::ComorbTransHet, O::TransTreatHet, O::ComorbTreatHet, O::TripleHet]),
            (O::ComorbHet, &[O::ComorbTransHet, O::TransTreatHet, O::ComorbTreatHet, O::TripleHet]),
            (O::TreatHet, &[O::ComorbTransHet, O::TransTreatHet, O::ComorbTreatHet, O::TripleHet]),
            (O::ComorbTransHet, &[O::TransTreatHet, O::ComorbTreatHet, O::TripleHet]),
            (O::TransTreatHet, &[O::ComorbTreatHet, O::TripleHet]),
            (O::ComorbTreatHet, &[O::TripleHet]),
            (O::MolineauxPairwiseSample, &[O::FirstLocalMaximumGamma, O::MeanDurationGamma]),
            (O::VivaxSimpleModel, &[O::DummyWithinHostModel, O::EmpiricalWithinHostModel, O::MolineauxWithinHostModel, O::PennyWithinHostModel]),
        ];

        for &(lhs, rhs_list) in PAIRS {
            if self.get(lhs) {
                for &rhs in rhs_list {
                    if self.get(rhs) {
                        return Err(CoreError::Scenario(format!(
                            "Incompatible model options: {}=1 is incompatible with flags:\t{}=1",
                            lhs.as_str(),
                            rhs.as_str()
                        )));
                    }
                }
            }
        }
        if self.get(O::InnateMaxDens) && !self.get(O::MaxDensCorrection) {
            return Err(CoreError::Scenario("INNATE_MAX_DENS requires MAX_DENS_CORRECTION".into()));
        }
        if !self.get(O::MolineauxWithinHostModel)
            && (self.get(O::FirstLocalMaximumGamma)
                || self.get(O::MeanDurationGamma)
                || self.get(O::ParasiteReplicationGamma))
        {
            return Err(CoreError::Scenario(
                "Molineaux model option used without MOLINEAUX_WITHIN_HOST_MODEL option".into(),
            ));
        }
        if !self.get(O::PennyWithinHostModel)
            && (self.get(O::ImmuneThresholdGamma) || self.get(O::UpdateDensityGamma))
        {
            return Err(CoreError::Scenario(
                "Penny model option used without PENNY_WITHIN_HOST_MODEL option".into(),
            ));
        }
        if interval == 5 && self.get(O::ClinicalEventScheduler) {
            return Err(CoreError::Scenario(
                "Model option CLINICAL_EVENT_SCHEDULER is only compatible with a 1-day time step."
                    .into(),
            ));
        }
        if interval != 1 && interval != 5 {
            return Err(CoreError::Scenario(format!(
                "Time step set to {interval} days but only 1 and 5 days are supported."
            )));
        }
        Ok(())
    }
}
