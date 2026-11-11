//! Error types for pharmacokinetic / pharmacodynamic (PK/PD) configuration and simulation.

use om_core::CoreError;
use thiserror::Error;

/// Errors returned while validating `<pharmacology>` specifications or evaluating PK/PD models.
#[derive(Debug, Clone, PartialEq, Error)]
pub enum PkPdError {
    /// Underlying sampler or quadrature error from `om-core`.
    #[error(transparent)]
    Core(#[from] CoreError),

    /// Invalid `<pharmacology>`, `<drugs>`, `<treatments>`, or phenotype restriction configuration.
    #[error("PK/PD scenario error: {0}")]
    Scenario(String),

    /// Runtime lookup failure in an age- or body-mass-indexed dosage table (`Error::PkPd`).
    #[error("PK/PD dosage lookup error: {0}")]
    DosageLookup(String),

    /// Out-of-bounds drug type index.
    #[error("invalid drug type index {index} (num drug types: {count})")]
    InvalidDrugIndex {
        /// Requested drug index.
        index: usize,
        /// Total registered drug types.
        count: usize,
    },

    /// Out-of-bounds treatment schedule index.
    #[error("invalid schedule index {index} (num schedules: {count})")]
    InvalidScheduleIndex {
        /// Requested schedule index.
        index: usize,
        /// Total registered schedules.
        count: usize,
    },

    /// Out-of-bounds dosage table index.
    #[error("invalid dosage table index {index} (num dosage tables: {count})")]
    InvalidDosageIndex {
        /// Requested dosage index.
        index: usize,
        /// Total registered dosage tables.
        count: usize,
    },

    /// Out-of-bounds parasite genotype identifier.
    #[error("invalid genotype id {genotype_id} (num genotypes: {count})")]
    InvalidGenotype {
        /// Requested parasite genotype identifier.
        genotype_id: u32,
        /// Total registered genotypes for this drug.
        count: usize,
    },
}
