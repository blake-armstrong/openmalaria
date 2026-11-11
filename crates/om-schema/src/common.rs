//! Shared Schema v50 utility AST types (`schema/util.xsd`).

pub use om_core::DecaySpec;
use serde::{Deserialize, Serialize};

/// Boolean `<option name="..." value="..."/>` element (`value` defaults to `true` in XSD).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct OptionElt {
    pub name: String,
    pub value: bool,
}

/// Single `<group lowerbound="..." value="..."/>` knot inside an `AgeGroupValues` table.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct AgeGroupPoint {
    pub lowerbound: f64,
    pub value: f64,
}

/// Age-indexed piecewise table (`om:AgeGroupValues`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AgeGroupValues {
    /// `"linear"` (default when omitted) or `"none"`.
    pub interpolation: Option<String>,
    pub groups: Vec<AgeGroupPoint>,
}

/// Normal or constant sampled parameter (`om:SampledValueN`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SampledValueN {
    pub mean: f64,
    pub sd: Option<f64>,
    pub distr: String,
}

/// Coefficient-of-variation or variance heterogeneity spec (`om:SampledValueCV`, e.g. `<availability/>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SampledValueCv {
    pub cv: Option<f64>,
    pub variance: Option<f64>,
    pub truncate: Option<f64>,
    pub distr: String,
}

impl Default for SampledValueCv {
    fn default() -> Self {
        Self {
            cv: None,
            variance: None,
            truncate: None,
            distr: "const".into(),
        }
    }
}

/// Log-normal / gamma / constant sampled value (`om:SampledValueLN`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SampledValueLn {
    pub mean: f64,
    pub cv: Option<f64>,
    pub variance: Option<f64>,
    pub truncate: Option<f64>,
    pub distr: String,
}

/// Beta distribution parameterized by `(mean, variance)` (`om:BetaMeanSample`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct BetaMeanSample {
    pub mean: f64,
    pub variance: f64,
}

/// Weibull distribution parameterized by `(scale, shape)` (`om:WeibullSample`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WeibullSample {
    pub scale: f64,
    pub shape: f64,
    pub distr: String,
}

/// Age-gated sub-list of triggered component deployments (`om:TriggeredDeployments`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct TriggeredDeployGroup {
    pub min_age: f64,
    pub max_age: Option<f64>,
    pub p: f64,
    pub components: Vec<String>,
}
