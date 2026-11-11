//! Strongly-typed OpenMalaria model parameters (`model/Parameters.h`) with base preset and overrides.

use crate::error::CoreError;
use crate::options::ModelName;
use serde::{Deserialize, Serialize};
use std::collections::HashSet;

/// Enumerated OpenMalaria epidemiological parameter codes (`OM::Parameter`).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[repr(u8)]
pub enum Parameter {
    NegLogOneMinusSinf = 0,
    EStar = 1,
    Simm = 2,
    XStarP = 3,
    GammaP = 4,
    SigmaISq = 5,
    CumulativeYStar = 6,
    CumulativeHStar = 7,
    NegLogOneMinusAlphaM = 8,
    DecayM = 9,
    Sigma0Sq = 10,
    XNuStar = 11,
    YStarSq = 12,
    Alpha = 13,
    DensityBiasNonGarki = 14,
    BaselineAvailabilityShape = 15,
    LogOddsRatioCfCommunity = 16,
    IndirectRiskCofactor = 17,
    NonMalariaInfantMortality = 18,
    DensityBiasGarki = 19,
    SevereMalariaThreshold = 20,
    ImmunityPenalty = 21,
    ImmuneEffectorDecay = 22,
    ComorbidityIntercept = 23,
    YStarHalfLife = 24,
    YStar1 = 25,
    AsexualImmunityDecay = 26,
    YStar0 = 27,
    CriticalAgeForComorbidity = 28,
    MuellerRateMultiplier = 29,
    MuellerDensityExponent = 30,
    CfrScaleFactor = 31,
    MeanLocalMaxDensity = 32,
    SdLocalMaxDensity = 33,
    MeanDiffPosDays = 34,
    SdDiffPosDays = 35,
    CfrNegLogAlpha = 36,
}

impl Parameter {
    /// Total number of active parameters (`37`; XML index `29` is deprecated and skipped).
    pub const COUNT: usize = 37;

    /// Maps an XML `<parameter number="..."/>` code (`1..=28` or `30..=38`) to `Parameter`.
    #[must_use]
    pub const fn from_xml_number(num: i32) -> Option<Self> {
        match num {
            1 => Some(Self::NegLogOneMinusSinf),
            2 => Some(Self::EStar),
            3 => Some(Self::Simm),
            4 => Some(Self::XStarP),
            5 => Some(Self::GammaP),
            6 => Some(Self::SigmaISq),
            7 => Some(Self::CumulativeYStar),
            8 => Some(Self::CumulativeHStar),
            9 => Some(Self::NegLogOneMinusAlphaM),
            10 => Some(Self::DecayM),
            11 => Some(Self::Sigma0Sq),
            12 => Some(Self::XNuStar),
            13 => Some(Self::YStarSq),
            14 => Some(Self::Alpha),
            15 => Some(Self::DensityBiasNonGarki),
            16 => Some(Self::BaselineAvailabilityShape),
            17 => Some(Self::LogOddsRatioCfCommunity),
            18 => Some(Self::IndirectRiskCofactor),
            19 => Some(Self::NonMalariaInfantMortality),
            20 => Some(Self::DensityBiasGarki),
            21 => Some(Self::SevereMalariaThreshold),
            22 => Some(Self::ImmunityPenalty),
            23 => Some(Self::ImmuneEffectorDecay),
            24 => Some(Self::ComorbidityIntercept),
            25 => Some(Self::YStarHalfLife),
            26 => Some(Self::YStar1),
            27 => Some(Self::AsexualImmunityDecay),
            28 => Some(Self::YStar0),
            30 => Some(Self::CriticalAgeForComorbidity),
            31 => Some(Self::MuellerRateMultiplier),
            32 => Some(Self::MuellerDensityExponent),
            33 => Some(Self::CfrScaleFactor),
            34 => Some(Self::MeanLocalMaxDensity),
            35 => Some(Self::SdLocalMaxDensity),
            36 => Some(Self::MeanDiffPosDays),
            37 => Some(Self::SdDiffPosDays),
            38 => Some(Self::CfrNegLogAlpha),
            _ => None,
        }
    }

    /// Returns the canonical XML `<parameter number="..."/>` code.
    #[must_use]
    pub const fn xml_number(self) -> u32 {
        let idx = self as u32;
        if idx < 28 { idx + 1 } else { idx + 2 }
    }
}

/// Dense index-backed parameter table (`O(1)` lookup, zero `unordered_map` overhead).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Parameters {
    values: Vec<Option<f64>>,
}

impl Default for Parameters {
    fn default() -> Self {
        Self::empty()
    }
}

impl Parameters {
    /// Creates an empty parameter table with no values set.
    #[must_use]
    pub fn empty() -> Self {
        Self {
            values: vec![None; Parameter::COUNT],
        }
    }

    /// Creates the hardcoded `<ModelName name="base"/>` parameter preset (`1..=28, 30`).
    #[must_use]
    #[allow(clippy::excessive_precision)]
    pub fn base_model() -> Self {
        let mut p = Self::empty();
        let preset: &[(i32, f64)] = &[
            (1, 0.050736),
            (2, 0.03247),
            (3, 0.138161050830301),
            (4, 1514.385853233699891),
            (5, 2.03692533424484),
            (6, 10.173598698525799),
            (7, 35158523.31132510304451),
            (8, 97.334652723897705),
            (9, 2.33031045876193),
            (10, 2.53106547375805),
            (11, 0.655747311168152),
            (12, 0.916181104713054),
            (13, 6502.26335600001039),
            (14, 142601.912520000012591),
            (15, 0.177378570987455),
            (16, 1.0),
            (17, 0.736202),
            (18, 0.018777338),
            (19, 49.539046599999999),
            (20, 4.79610772546704),
            (21, 784455.599999999976717),
            (22, 1.0),
            (23, 0.0),
            (24, 0.0968),
            (25, 0.275437402),
            (26, 0.596539864),
            (27, 0.0),
            (28, 296.302437899999973),
            (30, 0.117383),
        ];
        for &(id, val) in preset {
            if let Some(param) = Parameter::from_xml_number(id) {
                p.set(param, val);
            }
        }
        p
    }

    /// Builds `Parameters` from `ModelName` plus explicit `(number, value)` pairs from XML.
    ///
    /// Deprecated/unknown parameter IDs (such as `29`) are skipped cleanly without failing,
    /// while duplicate active parameter IDs within XML return an error.
    pub fn from_xml_entries<I>(model_name: ModelName, entries: I) -> Result<Self, CoreError>
    where
        I: IntoIterator<Item = (i32, f64)>,
    {
        let mut params = match model_name {
            ModelName::None => Self::empty(),
            ModelName::Base => Self::base_model(),
        };
        let mut seen = HashSet::new();
        for (num, value) in entries {
            let Some(param) = Parameter::from_xml_number(num) else {
                continue;
            };
            if !seen.insert(num) {
                return Err(CoreError::Scenario(format!(
                    "Parameter with index {num} described twice in XML."
                )));
            }
            params.set(param, value);
        }
        Ok(params)
    }

    /// Reads a required parameter value, returning `CoreError::MissingParameter` if unset.
    #[inline]
    pub fn try_get(&self, param: Parameter) -> Result<f64, CoreError> {
        self.values[param as usize].ok_or_else(|| CoreError::MissingParameter(param.xml_number()))
    }

    /// Reads an optional parameter value without failing if unset.
    #[inline]
    #[must_use]
    pub fn get_opt(&self, param: Parameter) -> Option<f64> {
        self.values[param as usize]
    }

    /// Programmatically sets or overrides a parameter value.
    #[inline]
    pub fn set(&mut self, param: Parameter, value: f64) {
        self.values[param as usize] = Some(value);
    }
}
