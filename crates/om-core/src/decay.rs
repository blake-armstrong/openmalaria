//! Zero-per-host-allocation decay curves (`model/util/DecayFunction.cpp`).

use crate::error::CoreError;
use crate::sampler::{LognormalSampler, NormalSample};
use crate::time::{SIM_FUTURE, SimClock, SimTime};
use crate::unit_parse::{DefaultUnit, duration_to_days};
use om_rng::LocalRng;
use serde::{Deserialize, Serialize};

/// Binary arithmetic combinator for composite `<decay function="plus|minus|divides|multiplies"/>`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum DecayOp {
    Plus,
    Minus,
    Divides,
    Multiplies,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
enum DecayShape {
    Constant,
    Step { inv_l: f64 },
    Linear { inv_l: f64 },
    Exponential { inv_l: f64 },
    Weibull { const_over_lambda: f64, k: f64 },
    Hill { inv_l: f64, k: f64 },
    SmoothCompact { inv_l: f64, k: f64 },
    Operator {
        op: DecayOp,
        f1: Box<DecayFunction>,
        f2: Box<DecayFunction>,
    },
    Emax {
        f: Box<DecayFunction>,
        emax: f64,
        ic50_pow: f64,
        slope: f64,
        initial_concentration: f64,
    },
}

/// Raw configuration inputs parsed from an XML `<decay .../>` element.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DecaySpec {
    pub function: String,
    pub l: Option<String>,
    pub k: f64,
    pub cv: f64,
    pub increasing: bool,
    pub initial_efficacy: f64,
    pub emax: Option<f64>,
    pub ic50: Option<f64>,
    pub slope: Option<f64>,
    pub initial_concentration: Option<f64>,
    pub children: Vec<DecaySpec>,
}

impl Default for DecaySpec {
    fn default() -> Self {
        Self {
            function: "constant".into(),
            l: None,
            k: 1.0,
            cv: 0.0,
            increasing: false,
            initial_efficacy: 1.0,
            emax: None,
            ic50: None,
            slope: None,
            initial_concentration: None,
            children: Vec::new(),
        }
    }
}

/// Per-individual heterogeneity sample (`Copy`, 8 bytes; replaces heap `unique_ptr<DecayFunction>`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct DecaySample {
    het_factor: f64,
}

impl Default for DecaySample {
    fn default() -> Self {
        Self {
            het_factor: f64::INFINITY,
        }
    }
}

impl DecaySample {
    /// Wraps an explicit `het_factor` (`1.0` when `CV == 0.0`).
    #[must_use]
    pub const fn from_factor(het_factor: f64) -> Self {
        Self { het_factor }
    }

    /// Returns the underlying `het_factor`.
    #[must_use]
    pub const fn factor(self) -> f64 {
        self.het_factor
    }
}

/// Configured decay curve supporting all 12 OpenMalaria v50 decay functions.
///
/// # Examples
/// ```
/// use om_core::{DecayFunction, DecaySpec, SimClock};
/// use om_rng::LocalRng;
///
/// let clock = SimClock::new(5, 90.0).unwrap();
/// let spec = DecaySpec {
///     function: "exponential".into(),
///     l: Some("1y".into()),
///     ..DecaySpec::default()
/// };
/// let decay = DecayFunction::from_spec(&spec, Some(&clock), "itn").unwrap();
/// let mut rng = LocalRng::from_words(1, 2, 3, 4);
/// let s = decay.het_sample(&mut rng);
/// assert!((decay.eval(365.0, s) - 0.5).abs() < 1e-12);
/// ```
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DecayFunction {
    increasing: bool,
    initial_efficacy: f64,
    het: LognormalSampler,
    shape: DecayShape,
}

impl DecayFunction {
    /// Constant `1.0` decay function with `CV = 0.0`.
    #[must_use]
    pub fn constant() -> Self {
        Self {
            increasing: false,
            initial_efficacy: 1.0,
            het: LognormalSampler::from_mean_cv(1.0, 0.0, None).expect("constant het"),
            shape: DecayShape::Constant,
        }
    }

    /// Constructs a `DecayFunction` tree from a `DecaySpec` and optional `SimClock`.
    pub fn from_spec(
        spec: &DecaySpec,
        clock: Option<&SimClock>,
        elt_name: &str,
    ) -> Result<Self, CoreError> {
        let het = LognormalSampler::from_mean_cv(1.0, spec.cv, None)?;
        let read_l = || -> Result<f64, CoreError> {
            let l_str = spec
                .l
                .as_deref()
                .ok_or_else(|| CoreError::Scenario("decay function: attribute L required".into()))?;
            duration_to_days(clock, l_str, DefaultUnit::Years)
        };

        let shape = match spec.function.as_str() {
            "constant" => DecayShape::Constant,
            "step" => DecayShape::Step {
                inv_l: 1.0 / read_l()?,
            },
            "linear" => DecayShape::Linear {
                inv_l: 1.0 / read_l()?,
            },
            "exponential" => DecayShape::Exponential {
                inv_l: 2.0_f64.ln() / read_l()?,
            },
            "weibull" => DecayShape::Weibull {
                const_over_lambda: (2.0_f64.ln()).powf(1.0 / spec.k) / read_l()?,
                k: spec.k,
            },
            "hill" => DecayShape::Hill {
                inv_l: 1.0 / read_l()?,
                k: spec.k,
            },
            "smooth-compact" => DecayShape::SmoothCompact {
                inv_l: 1.0 / read_l()?,
                k: spec.k,
            },
            "plus" | "minus" | "divides" | "multiplies" => {
                if spec.children.len() != 2 {
                    return Err(CoreError::Scenario(format!(
                        "Operator decay function expects two decay functions, {} were given.",
                        spec.children.len()
                    )));
                }
                let op = match spec.function.as_str() {
                    "plus" => DecayOp::Plus,
                    "minus" => DecayOp::Minus,
                    "divides" => DecayOp::Divides,
                    _ => DecayOp::Multiplies,
                };
                DecayShape::Operator {
                    op,
                    f1: Box::new(Self::from_spec(&spec.children[0], clock, "Operator::f1")?),
                    f2: Box::new(Self::from_spec(&spec.children[1], clock, "Operator::f2")?),
                }
            }
            "emax" => {
                if spec.children.len() != 1 {
                    return Err(CoreError::Scenario(format!(
                        "The Emax function expects exactly one user function, but {} were given.",
                        spec.children.len()
                    )));
                }
                let emax = spec.emax.ok_or_else(|| {
                    CoreError::Scenario("Emax function: the Emax parameter is not declared".into())
                })?;
                let raw_ic50 = spec.ic50.ok_or_else(|| {
                    CoreError::Scenario("Emax function: the IC50 parameter is not declared".into())
                })?;
                let slope = spec.slope.ok_or_else(|| {
                    CoreError::Scenario("Emax function: the Slope parameter is not declared".into())
                })?;
                let c0 = spec.initial_concentration.ok_or_else(|| {
                    CoreError::Scenario(
                        "Emax function: the InitialConcentration parameter is not declared".into(),
                    )
                })?;
                if !(0.0..=1.0).contains(&emax) {
                    return Err(CoreError::Scenario(format!(
                        "Emax function: Emax must be in [0, 1], got {emax}"
                    )));
                }
                if raw_ic50 <= 0.0 || slope <= 0.0 || c0 < 0.0 {
                    return Err(CoreError::Scenario(
                        "Emax function: require IC50 > 0, Slope > 0, InitialConcentration >= 0"
                            .into(),
                    ));
                }
                DecayShape::Emax {
                    f: Box::new(Self::from_spec(&spec.children[0], clock, "Emax::f")?),
                    emax,
                    ic50_pow: raw_ic50.powf(slope),
                    slope,
                    initial_concentration: c0,
                }
            }
            other => {
                return Err(CoreError::Scenario(format!(
                    "decay function {elt_name} of type {other} unrecognized"
                )));
            }
        };

        Ok(Self {
            increasing: spec.increasing,
            initial_efficacy: spec.initial_efficacy,
            het,
            shape,
        })
    }

    /// Draws a per-host `DecaySample` using `LocalRng` (0 RNG calls when `CV == 0.0`).
    #[inline]
    pub fn het_sample(&self, rng: &mut LocalRng) -> DecaySample {
        DecaySample {
            het_factor: self.het.sample(rng),
        }
    }

    /// Maps an existing `NormalSample` into a `DecaySample`.
    #[inline]
    #[must_use]
    pub fn het_sample_normal(&self, sample: NormalSample) -> DecaySample {
        DecaySample {
            het_factor: self.het.sample_normal(sample),
        }
    }

    /// Evaluates the decay value at `age_days` using per-individual `sample`.
    #[inline]
    #[must_use]
    pub fn eval(&self, age_days: f64, sample: DecaySample) -> f64 {
        let c = self.compute(age_days, sample.het_factor);
        if self.increasing {
            1.0 - c * self.initial_efficacy
        } else {
            c * self.initial_efficacy
        }
    }

    fn compute(&self, effective_age: f64, het_factor: f64) -> f64 {
        let sub = DecaySample { het_factor };
        match &self.shape {
            DecayShape::Constant => {
                if (effective_age * het_factor) == f64::INFINITY {
                    0.0
                } else {
                    1.0
                }
            }
            DecayShape::Step { inv_l } => {
                if effective_age * inv_l * het_factor < 1.0 {
                    1.0
                } else {
                    0.0
                }
            }
            DecayShape::Linear { inv_l } => {
                let u = effective_age * inv_l * het_factor;
                if u < 1.0 { 1.0 - u } else { 0.0 }
            }
            DecayShape::Exponential { inv_l } => (-effective_age * inv_l * het_factor).exp(),
            DecayShape::Weibull {
                const_over_lambda,
                k,
            } => {
                let p = -(effective_age * const_over_lambda * het_factor).powf(*k);
                if p < -700.0 { 0.0 } else { p.exp() }
            }
            DecayShape::Hill { inv_l, k } => {
                1.0 / (1.0 + (effective_age * inv_l * het_factor).powf(*k))
            }
            DecayShape::SmoothCompact { inv_l, k } => {
                let u = effective_age * inv_l * het_factor;
                if u < 1.0 {
                    (*k - *k / (1.0 - u * u)).exp()
                } else {
                    0.0
                }
            }
            DecayShape::Operator { op, f1, f2 } => {
                let a = f1.eval(effective_age, sub);
                let b = f2.eval(effective_age, sub);
                let res = match op {
                    DecayOp::Plus => a + b,
                    DecayOp::Minus => a - b,
                    DecayOp::Divides => a / b,
                    DecayOp::Multiplies => a * b,
                };
                res.clamp(0.0, 1.0)
            }
            DecayShape::Emax {
                f,
                emax,
                ic50_pow,
                slope,
                initial_concentration,
            } => {
                let conc_pow = (initial_concentration * f.eval(effective_age, sub)).powf(*slope);
                (emax * conc_pow / (conc_pow + ic50_pow)).clamp(0.0, 1.0)
            }
        }
    }

    /// Samples an individual object's discrete decay age (`DecayFunction::sampleAgeOfDecay`).
    pub fn sample_age_of_decay(&self, clock: &SimClock, rng: &mut LocalRng) -> SimTime {
        match &self.shape {
            DecayShape::Constant => SIM_FUTURE,
            DecayShape::Step { inv_l } => clock.round_to_ts_from_days(1.0 / inv_l),
            DecayShape::Linear { inv_l } => clock.round_to_ts_from_days(rng.uniform_01() / inv_l),
            DecayShape::Exponential { inv_l } => {
                clock.round_to_ts_from_days(-rng.uniform_01().ln() / inv_l)
            }
            DecayShape::Weibull {
                const_over_lambda,
                k,
            } => clock.round_to_ts_from_days((-rng.uniform_01().ln()).powf(1.0 / k) / const_over_lambda),
            DecayShape::Hill { inv_l, k } => {
                clock.round_to_ts_from_days((1.0 / rng.uniform_01() - 1.0).powf(1.0 / k) / inv_l)
            }
            DecayShape::SmoothCompact { inv_l, k } => {
                clock.round_to_ts_from_days((1.0 - k / (k - rng.uniform_01().ln())).sqrt() / inv_l)
            }
            DecayShape::Operator { f1, f2, .. } => {
                let d1 = f1.sample_age_of_decay(clock, rng);
                let d2 = f2.sample_age_of_decay(clock, rng);
                clock.round_to_ts_from_days(f64::from(d1.max(d2)))
            }
            DecayShape::Emax { f, .. } => {
                let d = f.sample_age_of_decay(clock, rng);
                clock.round_to_ts_from_days(f64::from(d))
            }
        }
    }
}
