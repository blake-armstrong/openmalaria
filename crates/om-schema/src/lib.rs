//! Pure-Rust OpenMalaria Schema v50 XML parser, AST, validator, and fluent builder.
//!
//! Replaces the 15,000+ line CodeSynthesis XSD/Xerces-C generated layer (`schema/*.xsd` + `XMLChecker.h`)
//! with strongly-typed, `serde`-serializable Rust structs and `roxmltree` zero-unsafe parsing.
//!
//! # Examples
//! ```
//! use om_schema::Scenario;
//!
//! let path = concat!(env!("CARGO_MANIFEST_DIR"), "/../../test/scenario1.xml");
//! let scenario = Scenario::from_xml_file(path).unwrap().with_seed(99).with_pop_size(250);
//! assert_eq!(scenario.schema_version, 50);
//! assert_eq!(scenario.model.iseed, 99);
//! assert_eq!(scenario.demography.pop_size, 250);
//! ```
#![forbid(unsafe_code)]

pub mod common;
pub mod decision_tree;
pub mod demog_mon;
pub mod entomology;
pub mod error;
pub mod health_system;
pub mod interventions;
pub mod interventions_human;
pub mod parser_clin;
pub mod parser_dt;
pub mod parser_ento_pharm;
pub mod parser_interv;
pub mod parser_interv_comp;
pub mod parser_root;
pub mod pharmacology;
pub mod scenario;
pub mod validate;
pub mod xml_helpers;

pub use error::SchemaError;
pub use scenario::Scenario;
pub use validate::SCHEMA_VERSION;
