//! Structured error types for `om-core` configuration, unit parsing, and numerical solvers.

use thiserror::Error;

/// Errors produced by core OpenMalaria configuration, unit parsing, and numerical routines.
#[derive(Debug, Clone, PartialEq, Error)]
pub enum CoreError {
    /// Scenario XML configuration or semantic validation error (`OM::util::xml_scenario_error`).
    #[error("scenario configuration error: {0}")]
    Scenario(String),

    /// Duration or date string format error (`OM::util::format_error`).
    #[error("unit/date format error: {0}")]
    Format(String),

    /// Required parameter index was accessed by a submodel without being defined in the scenario.
    #[error("Parameter with index {0} required but not described.")]
    MissingParameter(u32),

    /// Numerical solver or quadrature convergence error (`Error::GSL`).
    #[error("numerical solver error: {0}")]
    Numerical(String),
}
