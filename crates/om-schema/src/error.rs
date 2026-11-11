//! Structured error types for OpenMalaria Schema v50 XML loading and validation.

use om_core::CoreError;
use thiserror::Error;

/// Errors emitted while parsing or post-validating an OpenMalaria scenario XML document.
#[derive(Debug, Error)]
pub enum SchemaError {
    /// Underlying XML syntax error from `roxmltree`.
    #[error("XML syntax error: {0}")]
    Xml(#[from] roxmltree::Error),

    /// Filesystem I/O failure when loading a scenario from disk.
    #[error("I/O error loading scenario: {0}")]
    Io(#[from] std::io::Error),

    /// Required child XML element was missing.
    #[error("missing required element <{child}> inside <{parent}>")]
    MissingElement {
        parent: &'static str,
        child: &'static str,
    },

    /// Required XML attribute was missing.
    #[error("missing required attribute '{attr}' on <{element}>")]
    MissingAttr { element: String, attr: &'static str },

    /// Attribute or element text could not be parsed or violated an XSD enumeration/bound.
    #[error("invalid value on <{element}> ({field}): {message}")]
    InvalidValue {
        element: String,
        field: &'static str,
        message: String,
    },

    /// Post-validation rule (`XMLChecker` / `DocumentLoader` / `ModelNameProvider`) failed.
    #[error("scenario validation error: {0}")]
    Validation(String),

    /// Core parameter/option/unit validation error.
    #[error(transparent)]
    Core(#[from] CoreError),
}
