//! Core simulation clock, parameter tables, options, interpolators, decay curves, and solvers.
//!
//! Eliminates all process-wide mutable `static` state from `model/sim.*`, `model/Parameters.h`,
//! and `model/util/*` so multiple `Simulation` instances can execute concurrently in-process.
//!
//! # Examples
//! ```
//! use om_core::{ModelName, ModelOptions, Parameter, Parameters, SimClock};
//!
//! let clock = SimClock::new(5, 90.0).unwrap();
//! let opts = ModelOptions::from_xml_entries(ModelName::Base, [], clock.one_ts()).unwrap();
//! let params = Parameters::from_xml_entries(ModelName::Base, []).unwrap();
//! assert!((params.try_get(Parameter::EStar).unwrap() - 0.03247).abs() < 1e-12);
//! assert!(!opts.get(om_core::OptionCode::VivaxSimpleModel));
//! ```
#![forbid(unsafe_code)]

pub mod age_interp;
pub mod decay;
pub mod error;
pub mod fourier;
pub mod options;
pub mod parameters;
pub mod sampler;
pub mod sampler_gamma_beta;
pub mod solvers;
pub mod time;
pub mod unit_parse;

pub use age_interp::{AgeGroupInterpolator, InterpKind};
pub use decay::{DecayFunction, DecayOp, DecaySample, DecaySpec};
pub use error::CoreError;
pub use options::{ModelName, ModelOptions, OptionCode};
pub use parameters::{Parameter, Parameters};
pub use sampler::{
    AnySampler, BetaSampler, GammaSampler, LognormalSampler, NormalSample, NormalSampler,
    WeibullSampler,
};
pub use time::{DAYS_IN_YEAR, SIM_FUTURE, SIM_NEVER, SimClock, SimTime, floor_to_int, in_years, mod_nn};
pub use unit_parse::DefaultUnit;
