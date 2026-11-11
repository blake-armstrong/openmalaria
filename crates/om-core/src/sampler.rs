//! Normal and log-normal parametric samplers (`model/util/sampler.h`, `sampler.cpp`).

use crate::error::CoreError;
pub use crate::sampler_gamma_beta::{AnySampler, BetaSampler, GammaSampler, WeibullSampler};
use om_rng::LocalRng;
use om_rng::cdf::lognormal_p;
use serde::{Deserialize, Serialize};

/// Standard normal variate `N(0, 1)` used to construct correlated normal and log-normal draws.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct NormalSample {
    x: f64,
}

impl NormalSample {
    /// Wraps an explicit `N(0, 1)` variate.
    #[must_use]
    pub const fn from_raw(x: f64) -> Self {
        Self { x }
    }

    /// Returns the underlying `N(0, 1)` variate.
    #[must_use]
    pub const fn raw(self) -> f64 {
        self.x
    }

    /// Draws `NormalSample(rng.gauss(0.0, 1.0))` (1 Gaussian call).
    pub fn generate(rng: &mut LocalRng) -> Self {
        Self {
            x: rng.gauss(0.0, 1.0),
        }
    }

    /// Generates a correlated `N(0, 1)` variate (`factor = sqrt(1 - corr^2)`).
    ///
    /// Consumes **0** RNG calls when `correlation == 1.0`.
    pub fn generate_correlated(
        base: Self,
        correlation: f64,
        factor: f64,
        rng: &mut LocalRng,
    ) -> Self {
        if correlation == 1.0 {
            return base;
        }
        let e = rng.gauss(0.0, factor);
        Self {
            x: base.x * correlation + e,
        }
    }

    /// Maps this `N(0, 1)` variate to `N(mu, sigma^2)`.
    #[inline]
    #[must_use]
    pub fn as_normal(self, mu: f64, sigma: f64) -> f64 {
        sigma * self.x + mu
    }

    /// Maps this `N(0, 1)` variate to `LogNormal(mu, sigma^2)`.
    #[inline]
    #[must_use]
    pub fn as_lognormal(self, mu: f64, sigma: f64) -> f64 {
        (sigma * self.x + mu).exp()
    }
}

/// Normal distribution sampler (`OM::util::NormalSampler`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct NormalSampler {
    mu: f64,
    sigma: f64,
}

impl NormalSampler {
    /// Creates a `NormalSampler` with explicit `(mu, sigma)`.
    #[must_use]
    pub const fn new(mu: f64, sigma: f64) -> Self {
        Self { mu, sigma }
    }

    /// Builds a `NormalSampler` from XML `SampledValueN` attributes (`mean`, `distr`, `sd`).
    pub fn from_xml(mean: f64, distr: &str, sd: Option<f64>) -> Result<Self, CoreError> {
        if distr == "const" {
            if let Some(s) = sd
                && s != 0.0
            {
                return Err(CoreError::Scenario(
                    "attribute SD must be zero or omitted when distr=\"const\" or is omitted".into(),
                ));
            }
            return Ok(Self {
                mu: mean,
                sigma: 0.0,
            });
        }
        if distr != "normal" {
            return Err(CoreError::Scenario(
                "expected distr to be one of \"const\", \"normal\"".into(),
            ));
        }
        let sigma = sd.ok_or_else(|| {
            CoreError::Scenario(
                "attribute \"SD\" required for sampled normal value when distr is not const".into(),
            )
        })?;
        Ok(Self { mu: mean, sigma })
    }

    /// Draws from `N(mu, sigma^2)` (consumes **0** RNG calls when `sigma == 0.0`).
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        if self.sigma == 0.0 {
            self.mu
        } else {
            rng.gauss(self.mu, self.sigma)
        }
    }

    /// Evaluates at an existing `NormalSample`.
    #[inline]
    #[must_use]
    pub fn sample_normal(&self, s: NormalSample) -> f64 {
        s.as_normal(self.mu, self.sigma)
    }

    /// Mean `mu`.
    #[must_use]
    pub const fn mu(&self) -> f64 {
        self.mu
    }

    /// Standard deviation `sigma`.
    #[must_use]
    pub const fn sigma(&self) -> f64 {
        self.sigma
    }
}

/// Log-normal distribution sampler (`OM::util::LognormalSampler`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct LognormalSampler {
    mu: f64,
    sigma: f64,
    truncate: Option<f64>,
}

impl LognormalSampler {
    /// Configures `LogNormal` from target `mean` and coefficient of variation `cv`.
    pub fn from_mean_cv(
        mean: f64,
        cv: f64,
        truncate: Option<f64>,
    ) -> Result<Self, CoreError> {
        if cv == 0.0 {
            let mu = if mean == 0.0 {
                f64::NEG_INFINITY
            } else if mean > 0.0 {
                mean.ln()
            } else {
                return Err(CoreError::Scenario("const: required mean > 0".into()));
            };
            return Ok(Self {
                mu,
                sigma: 0.0,
                truncate,
            });
        }
        if mean <= 0.0 {
            return Err(CoreError::Scenario("log-normal: required mean > 0".into()));
        }
        if cv < 0.0 {
            return Err(CoreError::Scenario("log-normal: required CV >= 0".into()));
        }
        let a = 1.0 + cv * cv;
        Ok(Self {
            mu: (mean / a.sqrt()).ln(),
            sigma: a.ln().sqrt(),
            truncate,
        })
    }

    /// Configures `LogNormal` from `mean` and `variance`, preserving C++ `CV = variance / mean` parity (`sampler.cpp:145`).
    pub fn from_mean_variance(
        mean: f64,
        variance: f64,
        truncate: Option<f64>,
    ) -> Result<Self, CoreError> {
        if variance == 0.0 {
            if mean <= 0.0 {
                return Err(CoreError::Scenario("const: required mean > 0".into()));
            }
            return Ok(Self {
                mu: mean.ln(),
                sigma: 0.0,
                truncate,
            });
        }
        if mean <= 0.0 {
            return Err(CoreError::Scenario("log-normal: required mean > 0".into()));
        }
        if variance < 0.0 {
            return Err(CoreError::Scenario(
                "log-normal: required variance >= 0".into(),
            ));
        }
        let cv = variance / mean;
        let a = 1.0 + cv * cv;
        Ok(Self {
            mu: (mean / a.sqrt()).ln(),
            sigma: a.ln().sqrt(),
            truncate,
        })
    }

    /// Scales the mean by `scalar` (`mu += ln(scalar)`).
    pub fn scale_mean(&mut self, scalar: f64) {
        self.mu += scalar.ln();
    }

    /// Analytical mean `exp(mu + 0.5 * sigma^2)`.
    #[must_use]
    pub fn mean(&self) -> f64 {
        (self.mu + 0.5 * self.sigma * self.sigma).exp()
    }

    /// Draws a sample (consumes **0** RNG calls when `sigma == 0.0`), clamping to `truncate` if set.
    #[inline]
    pub fn sample(&self, rng: &mut LocalRng) -> f64 {
        let val = if self.sigma == 0.0 {
            self.mu.exp()
        } else {
            rng.log_normal(self.mu, self.sigma)
        };
        match self.truncate {
            Some(t) if val > t => t,
            _ => val,
        }
    }

    /// Maps an existing `NormalSample` via `exp(sigma * x + mu)` (note: C++ does not apply `truncate` here).
    #[inline]
    #[must_use]
    pub fn sample_normal(&self, s: NormalSample) -> f64 {
        s.as_lognormal(self.mu, self.sigma)
    }

    /// Evaluates the CDF `P(X <= x)`.
    #[must_use]
    pub fn cdf(&self, x: f64) -> f64 {
        if x <= 0.0 {
            return 0.0;
        }
        if self.sigma == 0.0 {
            return if x.ln() >= self.mu { 1.0 } else { 0.0 };
        }
        lognormal_p(x, self.mu, self.sigma)
    }
}
