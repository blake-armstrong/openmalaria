//! Integration and property tests verifying that every Schema v50 XML scenario in `test/`
//! parses cleanly, round-trips through JSON, and enforces `XMLChecker` validation rules.
#![forbid(unsafe_code)]

use om_schema::{SCHEMA_VERSION, Scenario, SchemaError};
use proptest::prelude::*;
use std::path::Path;

#[test]
fn load_all_53_v50_test_scenarios_and_json_roundtrip() {
    let test_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../test");
    let mut xml_files: Vec<_> = std::fs::read_dir(&test_dir)
        .expect("read test dir")
        .filter_map(Result::ok)
        .map(|e| e.path())
        .filter(|p| {
            p.file_name()
                .and_then(|n| n.to_str())
                .is_some_and(|s| s.starts_with("scenario") && s.ends_with(".xml"))
        })
        .collect();
    xml_files.sort();
    assert!(
        xml_files.len() >= 53,
        "expected >= 53 scenario*.xml files, found {}",
        xml_files.len()
    );

    for path in &xml_files {
        let scenario = Scenario::from_xml_file(path)
            .unwrap_or_else(|e| panic!("failed to parse {}: {e}", path.display()));
        assert_eq!(scenario.schema_version, SCHEMA_VERSION);

        // Verify bit-exact deterministic serde round-trip on the full scenario AST.
        let json = serde_json::to_string(&scenario).expect("serialize scenario");
        let decoded: Scenario = serde_json::from_str(&json).expect("deserialize scenario");
        assert_eq!(scenario, decoded, "AST roundtrip mismatch on {}", path.display());
        let json2 = serde_json::to_string(&decoded).expect("re-serialize scenario");
        assert_eq!(json, json2, "JSON roundtrip mismatch on {}", path.display());
    }
}

#[test]
fn xml_checker_rejects_missing_model_options_without_model_name() {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../test/scenario1.xml");
    let mut scenario = Scenario::from_xml_file(path).unwrap();
    scenario.model.model_name = None;
    scenario.model.model_options = None;
    let err = scenario.validate().unwrap_err();
    assert!(matches!(err, SchemaError::Validation(_)), "got {err:?}");
}

#[test]
fn unsupported_future_schema_version_fails_cleanly() {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../test/scenario1.xml");
    let mut scenario = Scenario::from_xml_file(path).unwrap();
    scenario.schema_version = 51;
    assert!(scenario.validate().is_err());

    scenario.schema_version = 49;
    let warn = scenario.validate().unwrap();
    assert!(warn.is_some());
}

proptest! {
    #[test]
    fn builder_overrides_preserve_validity(
        seed in 0i32..100_000i32,
        pop_size in 10i32..5_000i32,
        e_star in 0.005f64..0.25f64,
    ) {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../test/scenario1.xml");
        let scenario = Scenario::from_xml_file(path)
            .unwrap()
            .with_seed(seed)
            .with_pop_size(pop_size)
            .with_parameter(2, e_star)
            .with_option("MAX_DENS_CORRECTION", true);
        prop_assert!(scenario.validate().is_ok());
        prop_assert_eq!(scenario.model.iseed, seed);
        prop_assert_eq!(scenario.demography.pop_size, pop_size);
    }
}
