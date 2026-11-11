//! Bit-exact PRNGs (`MasterRng`, `LocalRng`) and pure-Rust GSL 2.8 probability distributions and CDFs.
//!
//! Replaces `contrib/chacha.h`, `model/util/xoshiro.h`, `model/util/random.h`, and `libgsl`
//! with 100% Safe Rust (`#![forbid(unsafe_code)]`) and zero global mutable state.
//!
//! # Examples
//! ```
//! use om_rng::{LocalRng, MasterRng};
//!
//! let mut master = MasterRng::new(42, 0);
//! let mut local = LocalRng::from_master(&mut master);
//! let u = local.uniform_01();
//! assert!((0.0..1.0).contains(&u));
//! ```
#![forbid(unsafe_code)]

mod binomial_tpe;
pub mod cdf;
mod cdf_beta_gamma;
mod chacha;
mod distributions;
mod xoshiro;
mod ziggurat;
mod ziggurat_tables;

pub use chacha::OmChaCha;
pub use xoshiro::OmXoshiro256Plus;

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Errors returned by random variate samplers when given non-finite parameters.
#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum RngError {
    /// `lambda` passed to `poisson` was non-finite (`NaN` or `inf`), matching `Error::InfLambda`.
    #[error("poisson lambda is not finite")]
    InfiniteLambda,
}

/// Low-level random word and `[0, 1)` double generator trait matching OpenMalaria's GSL bridge.
pub trait RngCore {
    /// Generates a 32-bit unsigned integer (`sample_ulong` in GSL).
    fn gen_u32(&mut self) -> u32;

    /// Generates a 64-bit unsigned integer for child RNG seeding.
    fn gen_u64(&mut self) -> u64;

    /// Generates an IEEE-754 `f64` in `[0.0, 1.0)` (`sample_double01` in GSL).
    fn gen_double(&mut self) -> f64;

    /// Generates a strictly positive `f64` in `(0.0, 1.0)` matching `gsl_rng_uniform_pos`.
    #[inline]
    fn uniform_pos(&mut self) -> f64 {
        loop {
            let x = self.gen_double();
            if x != 0.0 {
                return x;
            }
        }
    }
}

/// High-level OpenMalaria random number generator and GSL distribution sampler (`OM::util::RNG<T>`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct OmRng<G: RngCore> {
    core: G,
}

/// Master cryptographic-grade `ChaCha<8>` RNG used to deterministically seed local RNGs.
pub type MasterRng = OmRng<OmChaCha<8>>;

/// Fast 256-bit `Xoshiro256+` local RNG owned by individual hosts and transmission submodels.
pub type LocalRng = OmRng<OmXoshiro256Plus>;

impl MasterRng {
    /// Creates a `MasterRng` with the given 64-bit `seed` and `stream` using OpenMalaria's custom IV.
    #[must_use]
    pub fn new(seed: u64, stream: u64) -> Self {
        Self {
            core: OmChaCha::new(seed, stream),
        }
    }

    /// Re-seeds this `MasterRng` in place.
    pub fn seed(&mut self, seed: u64, stream: u64) {
        self.core.seed(seed, stream);
    }
}

impl LocalRng {
    /// Creates a `LocalRng` from explicit `(seed, stream)` words.
    #[must_use]
    pub const fn new(seed: u64, stream: u64) -> Self {
        Self {
            core: OmXoshiro256Plus::from_seed_stream(seed, stream),
        }
    }

    /// Creates a `LocalRng` from explicit 4x64-bit state words.
    #[must_use]
    pub const fn from_words(a: u64, b: u64, c: u64, d: u64) -> Self {
        Self {
            core: OmXoshiro256Plus::from_words(a, b, c, d),
        }
    }

    /// Deterministically spawns a child `LocalRng` from a `MasterRng` (4 x `gen_u64()` draws).
    #[must_use]
    pub fn from_master(master: &mut MasterRng) -> Self {
        Self {
            core: OmXoshiro256Plus::from_parent(&mut master.core),
        }
    }

    /// Deterministically spawns a child `LocalRng` from another `LocalRng`.
    #[must_use]
    pub fn from_local(parent: &mut Self) -> Self {
        Self {
            core: OmXoshiro256Plus::from_parent(&mut parent.core),
        }
    }

    /// Re-seeds this `LocalRng` from a `MasterRng`.
    pub fn reseed_from_master(&mut self, master: &mut MasterRng) {
        self.core.seed_from_parent(&mut master.core);
    }
}

impl<G: RngCore> OmRng<G> {
    /// Wraps an arbitrary `RngCore` implementation.
    #[must_use]
    pub const fn from_core(core: G) -> Self {
        Self { core }
    }

    /// Borrows the underlying `RngCore`.
    #[must_use]
    pub const fn core(&self) -> &G {
        &self.core
    }

    /// Mutably borrows the underlying `RngCore`.
    pub fn core_mut(&mut self) -> &mut G {
        &mut self.core
    }

    /// Generates a uniform random `f64` in `[0.0, 1.0)`.
    #[inline]
    pub fn uniform_01(&mut self) -> f64 {
        self.core.gen_double()
    }

    /// Generates a uniform integer in `0..n` matching `static_cast<int>(uniform_01() * n)`.
    #[inline]
    pub fn uniform(&mut self, n: i32) -> i32 {
        debug_assert!(n > 0);
        (self.uniform_01() * f64::from(n)) as i32
    }

    /// Returns `true` with probability `prob` (`uniform_01() < prob`).
    #[inline]
    pub fn bernoulli(&mut self, prob: f64) -> bool {
        debug_assert!(prob.is_finite());
        self.uniform_01() < prob
    }

    /// Samples from `N(mean, std^2)` (`gsl_ran_gaussian(&gen, std) + mean`).
    #[inline]
    pub fn gauss(&mut self, mean: f64, std: f64) -> f64 {
        distributions::gaussian_polar(&mut self.core, std) + mean
    }

    /// Samples from `Gamma(a, b)` with shape `a` and scale `b` (`gsl_ran_gamma`).
    #[inline]
    pub fn gamma(&mut self, a: f64, b: f64) -> f64 {
        ziggurat::gamma_mt(&mut self.core, a, b)
    }

    /// Samples from `LogNormal(meanlog, stdlog)` (`gsl_ran_lognormal`).
    #[inline]
    pub fn log_normal(&mut self, meanlog: f64, stdlog: f64) -> f64 {
        distributions::lognormal_polar(&mut self.core, meanlog, stdlog)
    }

    /// Returns the maximum of `start` and `n` i.i.d. `LogNormal(meanlog, stdlog)` draws via inverse CDF.
    #[inline]
    pub fn max_multi_log_normal(&mut self, start: f64, n: i32, meanlog: f64, stdlog: f64) -> f64 {
        distributions::max_multi_log_normal(&mut self.core, start, n, meanlog, stdlog)
    }

    /// Samples from `Beta(a, b)` (`gsl_ran_beta`).
    #[inline]
    pub fn beta(&mut self, a: f64, b: f64) -> f64 {
        distributions::beta_gsl(&mut self.core, a, b)
    }

    /// Samples from `Beta(a, b)` where `a = m * b / (1 - m)` gives mean `m`.
    #[inline]
    pub fn beta_with_mean(&mut self, m: f64, b: f64) -> f64 {
        let a = m * b / (1.0 - m);
        self.beta(a, b)
    }

    /// Samples from `Poisson(lambda)` (`gsl_ran_poisson`), returning `Err` if `lambda` is non-finite.
    #[inline]
    pub fn try_poisson(&mut self, lambda: f64) -> Result<i32, RngError> {
        if !lambda.is_finite() {
            return Err(RngError::InfiniteLambda);
        }
        if lambda <= 0.0 {
            return Ok(0);
        }
        Ok(binomial_tpe::poisson_gsl(&mut self.core, lambda) as i32)
    }

    /// Samples from `Poisson(lambda)`, panicking if `lambda` is non-finite (legacy convenience wrapper).
    #[inline]
    pub fn poisson(&mut self, lambda: f64) -> i32 {
        self.try_poisson(lambda)
            .expect("poisson lambda must be finite")
    }

    /// Samples from `Weibull(lambda, k)` with scale `lambda` and shape `k` (`gsl_ran_weibull`).
    #[inline]
    pub fn weibull(&mut self, lambda: f64, k: f64) -> f64 {
        distributions::weibull_gsl(&mut self.core, lambda, k)
    }
}
