//! Post-validation checks (`model/util/XMLChecker.h`, `DocumentLoader.cpp`, `ModelNameProvider.h`).

use crate::error::SchemaError;
use crate::scenario::Scenario;
use om_core::{ModelName, ModelOptions, Parameters, SimClock};

/// Supported latest OpenMalaria XML schema version (`SCHEMA_VERSION = 50`).
pub const SCHEMA_VERSION: i32 = 50;

/// Executes all C++ `DocumentLoader`, `XMLChecker`, `ModelNameProvider`, `ModelOptions`,
/// `SimClock`, and `Parameters` validation rules on [`Scenario`].
///
/// Returns `Ok(Some(deprecation_warning))` if `scenario.schema_version < 50`.
pub fn validate_scenario(scenario: &Scenario) -> Result<Option<String>, SchemaError> {
    if scenario.schema_version > SCHEMA_VERSION {
        return Err(SchemaError::Validation(
            "Error: new schema version unsupported".into(),
        ));
    }
    let old_schema_warning = (scenario.schema_version < SCHEMA_VERSION).then(|| {
        format!(
            "Warning: scenario '{}' uses an old schema version {} (latest is {SCHEMA_VERSION}).",
            scenario.name, scenario.schema_version
        )
    });

    // XMLChecker::CheckModelOptionsAndParams (`model/util/XMLChecker.h:65`)
    let has_model_name = scenario.model.model_name.is_some();
    let has_parameters = scenario.model.parameters.is_some();
    let has_model_options = scenario.model.model_options.is_some();
    if !has_model_name && (!has_parameters || !has_model_options) {
        return Err(SchemaError::Validation(
            "If a model name is not specified then both <ModelOptions> and <parameters> must be specified"
                .into(),
        ));
    }

    if scenario.demography.pop_size <= 0 {
        return Err(SchemaError::Validation(format!(
            "demography popSize must be >= 1, got {}",
            scenario.demography.pop_size
        )));
    }
    if scenario.demography.maximum_age_yrs <= 0.0 {
        return Err(SchemaError::Validation(
            "demography maximumAgeYrs must be > 0".into(),
        ));
    }

    let model_name = ModelName::from_xml(scenario.model.model_name.as_deref())?;
    let interval = scenario
        .model
        .parameters
        .as_ref()
        .map_or(5, |p| p.interval);
    let clock = SimClock::new(interval, scenario.demography.maximum_age_yrs)?;

    let opt_iter = scenario
        .model
        .model_options
        .iter()
        .flatten()
        .map(|o| (o.name.as_str(), o.value));
    let _options = ModelOptions::from_xml_entries(model_name, opt_iter, clock.one_ts())?;

    let param_iter = scenario
        .model
        .parameters
        .iter()
        .flat_map(|p| p.entries.iter().map(|e| (e.number, e.value)));
    let _params = Parameters::from_xml_entries(model_name, param_iter)?;

    Ok(old_schema_warning)
}
