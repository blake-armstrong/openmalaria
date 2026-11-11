//! Pharmacokinetic and pharmacodynamic (PK/PD) compartment models, treatment schedules,
//! dosage tables, and per-patient drug dynamics for OpenMalaria Schema v50.
//!
//! Replaces the process-global mutable singletons in `model/PkPd/` (`LSTMDrugType`,
//! `LSTMTreatments`, `LSTMModel`, `LSTMDrugOneComp`, `LSTMDrugThreeComp`, `LSTMDrugConversion`)
//! with 100% Safe Rust, instance-owned registries ([`DrugRegistry`]), and `serde`-serializable
//! per-host state ([`PatientPkPd`]) with bit-exact PRNG and quadrature parity.
//!
//! # Compartment Models
//! - [`OneCompDrug`]: Single-compartment model with instantaneous oral/IV absorption and
//!   Hastings & Winter (2011) closed-form Hill survival integration.
//! - [`ThreeCompDrug`]: Two- and three-compartment oral absorption models solved analytically via
//!   Monolix Viète trigonometric cubic roots (Bertrand & Mentré 2008) and integrated with
//!   adaptive 15-point Gauss-Kronrod quadrature (`abs_eps = rel_eps = 1e-4`).
//! - [`ConversionDrug`]: Parent-to-metabolite conversion model (e.g. artemether/artesunate to DHA)
//!   with correlated log-normal `IC50` draws and `max(f_parent, f_metabolite)` quadrature.
//!
//! # Examples
//!
//! Prescribing and stepping a 1-compartment mefloquine (`MQ`) course on a patient:
//! ```
//! use om_pkpd::{DrugRegistry, InfectionKnCache, PatientPkPd};
//! use om_rng::LocalRng;
//! use om_schema::common::SampledValueLn;
//! use om_schema::pharmacology::{
//!     DosageMode, DoseStep, MedicateStep, PdPhenotype, PdSpec, PharmacologySpec, PkElimination,
//!     PkPdDosages, PkPdDrug, PkPdSchedule, PkSpec,
//! };
//! use std::collections::BTreeSet;
//!
//! let const_ln = |mean: f64| SampledValueLn {
//!     mean,
//!     cv: None,
//!     variance: None,
//!     truncate: None,
//!     distr: "const".into(),
//! };
//! let pharm = PharmacologySpec {
//!     schedules: vec![PkPdSchedule {
//!         name: "MQ_single".into(),
//!         medications: vec![MedicateStep { drug: "MQ".into(), mg: 3000.0, hour: 0.0 }],
//!     }],
//!     dosages: vec![PkPdDosages {
//!         name: "unit".into(),
//!         mode: DosageMode::Age(vec![DoseStep { lowerbound: 0.0, dose_mult: 1.0 }]),
//!     }],
//!     drugs: vec![PkPdDrug {
//!         abbrev: "MQ".into(),
//!         pd: PdSpec {
//!             locus: None,
//!             phenotypes: vec![PdPhenotype {
//!                 name: None,
//!                 restrictions: vec![],
//!                 max_killing_rate: 3.45,
//!                 ic50: const_ln(0.027),
//!                 slope: 5.0,
//!             }],
//!         },
//!         pk: PkSpec {
//!             negligible_concentration: 0.005,
//!             elimination: PkElimination::HalfLife(13.078),
//!             k_a: None,
//!             conversion: None,
//!             vol_dist: const_ln(20.8),
//!             compartment2: None,
//!             compartment3: None,
//!         },
//!     }],
//! };
//!
//! let genotypes = [BTreeSet::from([0_u32])];
//! let registry = DrugRegistry::new(Some(&pharm), &genotypes, |_, _| None).unwrap();
//! let mut rng = LocalRng::new(0, 721_347_520_444_481_703);
//! let mut patient = PatientPkPd::new();
//! let mut kn_cache = InfectionKnCache::new();
//!
//! let sched = registry.find_schedule("MQ_single").unwrap();
//! let dos = registry.find_dosages("unit").unwrap();
//! patient.prescribe(&registry, sched, dos, 21.0, 55.4993, 0.0).unwrap();
//! patient.medicate(&mut rng, &registry).unwrap();
//!
//! let factor = patient
//!     .get_drug_factor(&mut rng, &registry, 0, &mut kn_cache, 55.4993)
//!     .unwrap();
//! assert!((factor - 0.031745636393248573).abs() < 1e-15);
//! patient.decay_drugs(&registry, 55.4993);
//! assert!(patient.drug_concentration(0) > 0.0);
//! ```
//!
//! Loading `<pharmacology>` and `<parasiteGenetics>` directly from a Schema v50 XML scenario:
//! ```
//! use om_pkpd::{DrugRegistry, InfectionKnCache, PatientPkPd};
//! use om_rng::LocalRng;
//! use om_schema::Scenario;
//!
//! let path = concat!(env!("CARGO_MANIFEST_DIR"), "/../../test/scenarioGenotypes.xml");
//! let scenario = Scenario::from_xml_file(path).unwrap();
//! let registry = DrugRegistry::from_scenario(&scenario).unwrap();
//! assert_eq!(registry.num_drug_types(), 1);
//! assert_eq!(registry.drugs_in_use(), &[0]);
//!
//! let mut rng = LocalRng::new(42, 1);
//! let mut patient = PatientPkPd::new();
//! let mut kn_sens = InfectionKnCache::new();
//! let mut kn_res = InfectionKnCache::new();
//! patient.prescribe(&registry, 0, 0, 25.0, 60.0, 0.0).unwrap();
//! patient.medicate(&mut rng, &registry).unwrap();
//!
//! // Sensitive genotype (0, IC50 = 0.02) experiences stronger killing than resistant (1, IC50 = 0.08).
//! let f_sens = patient.get_drug_factor(&mut rng, &registry, 0, &mut kn_sens, 60.0).unwrap();
//! let f_res = patient.get_drug_factor(&mut rng, &registry, 1, &mut kn_res, 60.0).unwrap();
//! assert!(f_sens < f_res);
//! ```
#![forbid(unsafe_code)]

pub mod conversion;
pub mod dosage;
pub mod drug_type;
pub mod error;
pub mod one_comp;
pub mod patient_model;
pub mod pd;
pub mod registry;
pub mod three_comp;

pub use conversion::ConversionDrug;
pub use dosage::{DosageTable, Dose, MedicateData, Schedule, insert_dose_sorted};
pub use drug_type::{DrugModelKind, DrugType};
pub use error::PkPdError;
pub use one_comp::OneCompDrug;
pub use patient_model::{DrugCompartmentModel, DrugInstance, PatientPkPd};
pub use pd::{DrugPd, InfectionKnCache, create_lognormal_sampler};
pub use registry::{AlleleCodeMap, DrugRegistry, build_genotype_alleles};
pub use three_comp::ThreeCompDrug;
