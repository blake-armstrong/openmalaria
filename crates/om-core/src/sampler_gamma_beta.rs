//! Gamma, Beta, Weibull, and polymorphic `AnySampler` (`model/util/sampler.h`, `sampler.cpp`).

use crate::error::CoreError;
use crate::sampler::LognormalSampler;
use om_rng::LocalRng;
use om_rng::cdf::gamma_p;
use serde::{Deserialize, Serialize};

/// Gamma distribution sampler (`OM::util::GammaSampler`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct GammaSampler {
    mu: f64,
    k: f64,
    theta: f64,
    truncate: Option<f64>,
}

impl GammaSampler {
    /// Configures `Gamma` from `mean` and `cv`.
    pub fn from_mean_cv(
        mean: f64,
        cv: f64,
        truncate: Option<f64>,
    ) -> Result<Self, CoreError> {
        if mean <= 0.0 {
            return Err(CoreError::Scenario("gamma: required mean > 0".into()));
        }
        if cv < 0.0 {
            return Err(CoreError::Scenario("gamma: required CV >= 0".into()));
        }
        if cv == 0.0 {
            return Ok(Self {
                mu: mean,
                k: f64::NAN,
                theta: f64::NAN,
                truncate,
            });
        }
        let k = 1.0 / (cv * cv);
        Ok(Self {
            mu: mean,
            k,
            theta: mean / k,
            truncate,
        })
    }

    /// Configures `Gamma` from `mean` and `variance`.
    pub fn from_mean_variance(
        mean: f64,
        variance: f64,
        truncate: Option<f64>,
    ) -> Result<Self, CoreError> {
        if mean <= 0.0 {
            return Err(CoreError::Scenario("gamma: required mean > 0".into()));
        }
        if variance < 0.0 {
            return Err(CoreError::Scenario("gamma: required variance >= 0".into()));
        }
        if variance == 0.0 {
            return Ok(Self {
                mu: mean,
                k: f64::NAN,
                theta: f64::NAN,
                truncate,
            });
        }
        let k = (mean * mean) / variance;
        Ok(Self {
            mu: mean,
            k,
            theta: mean / k,
            truncate,
        })
    }

    /// Mean `mu`.
    #[must_use]
    pub const fn mean(&self) -> f64 {
        self.mu
    }

    /// Draws from `Gamma(k, theta)` (0 RNG calls when degenerate `theta.is_nan()`).
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        let val = if self.theta.is_nan() {
            self.mu
        } else {
            rng.gamma(self.k, self.theta)
        };
        match self.truncate {
            Some(t) if val > t => t,
            _ => val,
        }
    }

    /// Evaluates the CDF `P(X <= x)`.
    #[must_use]
    pub fn cdf(&self, x: f64) -> f64 {
        if x <= 0.0 {
            return 0.0;
        }
        if self.theta.is_nan() {
            return if x >= self.mu { 1.0 } else { 0.0 };
        }
        gamma_p(x, self.k, self.theta)
    }
}

/// Polymorphic `Lognormal` / `Gamma` sampler replacing `std::unique_ptr<OM::util::Sampler>`.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum AnySampler {
    Lognormal(LognormalSampler),
    Gamma(GammaSampler),
}

impl AnySampler {
    /// Builds an `AnySampler` from XML `SampledValueCV` fields.
    pub fn from_xml_cv(
        mean: f64,
        distr: &str,
        cv: Option<f64>,
        variance: Option<f64>,
        truncate: Option<f64>,
    ) -> Result<Self, CoreError> {
        if distr == "const" {
            if cv.is_some_and(|v| v != 0.0) || variance.is_some_and(|v| v != 0.0) {
                return Err(CoreError::Scenario(
                    "CV and variance must be zero or omitted when distr=\"const\"".into(),
                ));
            }
            return Ok(Self::Lognormal(LognormalSampler::from_mean_cv(
                mean, 0.0, None,
            )?));
        }
        if cv.is_some() == variance.is_some() {
            return Err(CoreError::Scenario(format!(
                "distr={distr}: exactly one of CV or variance must be specified"
            )));
        }
        match distr {
            "lognormal" => {
                let s = if let Some(c) = cv {
                    LognormalSampler::from_mean_cv(mean, c, truncate)?
                } else {
                    LognormalSampler::from_mean_variance(mean, variance.unwrap_or(0.0), truncate)?
                };
                Ok(Self::Lognormal(s))
            }
            "gamma" => {
                let s = if let Some(c) = cv {
                    GammaSampler::from_mean_cv(mean, c, truncate)?
                } else {
                    GammaSampler::from_mean_variance(mean, variance.unwrap_or(0.0), truncate)?
                };
                Ok(Self::Gamma(s))
            }
            _ => Err(CoreError::Scenario(format!(
                "distr={distr}: expected \"const\", \"lognormal\" or \"gamma\""
            ))),
        }
    }

    /// Samples a value from the underlying distribution.
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        match self {
            Self::Lognormal(s) => s.sample(rng),
            Self::Gamma(s) => s.sample(rng),
        }
    }

    /// Evaluates the analytical mean.
    #[must_use]
    pub fn mean(&self) -> f64 {
        match self {
            Self::Lognormal(s) => s.mean(),
            Self::Gamma(s) => s.mean(),
        }
    }

    /// Evaluates the CDF `P(X <= x)`.
    #[must_use]
    pub fn cdf(&self, x: f64) -> f64 {
        match self {
            Self::Lognormal(s) => s.cdf(x),
            Self::Gamma(s) => s.cdf(x),
        }
    }
}

/// Beta distribution sampler (`OM::util::BetaSampler`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct BetaSampler {
    a: f64,
    b: f64,
}

impl BetaSampler {
    /// Creates from explicit `(alpha, beta)` (when `beta == 0.0`, `alpha` is returned deterministically).
    #[must_use]
    pub const fn from_ab(a: f64, b: f64) -> Self {
        Self { a, b }
    }

    /// Creates from `(mean, variance)` (`BetaSampler::setParamsMV`).
    pub fn from_mean_variance(mean: f64, variance: f64) -> Result<Self, CoreError> {
        if variance > 0.0 {
            if variance >= mean * (1.0 - mean) {
                return Err(CoreError::Scenario(
                    "BetaSampler: require variance < mean*(1.0 - mean)".into(),
                ));
            }
            let c = mean * ((1.0 - mean) / variance) - 1.0;
            let a = mean * c;
            Ok(Self { a, b: c - a })
        } else if variance == 0.0 {
            Ok(Self { a: mean, b: 0.0 })
        } else {
            Err(CoreError::Scenario(
                "BetaSampler: require variance >= 0".into(),
            ))
        }
    }

    /// Draws from `Beta(a, b)` (0 RNG calls when `b == 0.0`).
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        if self.b == 0.0 {
            self.a
        } else {
            rng.beta(self.a, self.b)
        }
    }
}

/// Weibull distribution sampler (`OM::util::WeibullSampler`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct WeibullSampler {
    scale: f64,
    shape: f64,
}

impl WeibullSampler {
    /// Creates a `WeibullSampler` with `scale` (`lambda`) and `shape` (`k`).
    #[must_use]
    pub const fn new(scale: f64, shape: f64) -> Self {
        Self { scale, shape }
    }

    /// Draws from `Weibull(scale, shape)`.
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        rng.weibull(self.scale, self.shape)
    }
}
