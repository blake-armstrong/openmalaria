//! Scenario v50 integration tests across all `test/scenario*.xml` files and `proptest`
//! mathematical invariant tests (`factor in [0, 1]`, zero-dose `== 1.0`, monotonic dose-response).
#![forbid(unsafe_code)]

use om_pkpd::{DosageTable, DrugRegistry, InfectionKnCache, PatientPkPd, PkPdError, build_genotype_alleles};
use om_rng::LocalRng;
use om_schema::Scenario;
use om_schema::common::SampledValueLn;
use om_schema::pharmacology::{
    AlleleRestriction, DosageMode, DoseStep, PdPhenotype, PdSpec, PharmacologySpec,
    PkCompartmentPair, PkConversion, PkElimination, PkPdDosages, PkPdDrug, PkSpec,
};
use proptest::prelude::*;
use std::collections::BTreeSet;
use std::fs;
use std::path::Path;

fn cln(mean: f64) -> SampledValueLn {
    SampledValueLn { mean, cv: None, variance: None, truncate: None, distr: "const".into() }
}

fn single_pd(vmax: f64, ic50: f64, slope: f64) -> PdSpec {
    PdSpec {
        locus: None,
        phenotypes: vec![PdPhenotype {
            name: None,
            restrictions: vec![],
            max_killing_rate: vmax,
            ic50: cln(ic50),
            slope,
        }],
    }
}

#[test]
fn load_and_simulate_all_v50_scenarios_with_pharmacology() {
    let test_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../test");
    let mut xml_paths: Vec<_> = fs::read_dir(&test_dir)
        .unwrap()
        .filter_map(|e| {
            let p = e.ok()?.path();
            let name = p.file_name()?.to_str()?;
            (name.starts_with("scenario") && name.ends_with(".xml")).then_some(p)
        })
        .collect();
    xml_paths.sort();
    assert!(xml_paths.len() >= 50, "expected >= 50 scenario XML files, found {}", xml_paths.len());

    let mut pharm_count = 0usize;
    for path in &xml_paths {
        let scenario = Scenario::from_xml_file(path)
            .unwrap_or_else(|e| panic!("failed to parse {}: {e}", path.display()));
        let registry = DrugRegistry::from_scenario(&scenario)
            .unwrap_or_else(|e| panic!("failed to build DrugRegistry for {}: {e}", path.display()));

        if scenario.pharmacology.is_none() {
            assert_eq!(registry.num_drug_types(), 0);
            assert!(registry.drugs_in_use().is_empty());
            continue;
        }
        pharm_count += 1;
        assert!(registry.num_drug_types() > 0, "{} had empty drugs", path.display());

        let (genotypes, _) = build_genotype_alleles(scenario.parasite_genetics.as_ref());
        let n_genotypes = genotypes.len() as u32;

        // Exercise every schedule x first dosage table across all genotypes over 4 days
        if registry.num_dosages() > 0 {
            for sched_idx in 0..registry.num_schedules() {
                let mut rng = LocalRng::new(101, sched_idx as u64 + 1);
                let mut patient = PatientPkPd::new();
                patient
                    .prescribe(&registry, sched_idx, 0, 18.0, 50.0, 0.0)
                    .unwrap();

                for _day in 0..4 {
                    patient.medicate(&mut rng, &registry).unwrap();
                    for g in 0..n_genotypes {
                        let mut kn = InfectionKnCache::new();
                        let fac = patient
                            .get_drug_factor(&mut rng, &registry, g, &mut kn, 50.0)
                            .unwrap();
                        assert!(
                            fac.is_finite() && (0.0..=1.0).contains(&fac),
                            "{} sched {sched_idx} genotype {g} invalid factor {fac}",
                            path.display()
                        );
                    }
                    patient.decay_drugs(&registry, 50.0);
                    for d_idx in 0..registry.num_drug_types() {
                        let c = patient.drug_concentration(d_idx);
                        assert!(c.is_finite() && c >= 0.0);
                    }
                }
            }
        }
    }
    assert!(
        pharm_count >= 14,
        "expected at least 14 scenarios with <pharmacology>, found {pharm_count}"
    );
}

#[test]
fn schema_validation_rejects_invalid_configurations() {
    let genotypes = [BTreeSet::from([0_u32]), BTreeSet::from([1_u32])];
    assert!(matches!(
        DosageTable::from_spec(&PkPdDosages {
            name: "bad".into(),
            mode: DosageMode::Age(vec![DoseStep { lowerbound: 1.0, dose_mult: 1.0 }]),
        }),
        Err(PkPdError::Scenario(_))
    ));
    let mut bad_drug = PkPdDrug {
        abbrev: "X".into(),
        pd: single_pd(3.0, 0.01, 2.0),
        pk: PkSpec {
            negligible_concentration: 1e-4,
            elimination: PkElimination::HalfLife(5.0),
            k_a: None,
            conversion: None,
            vol_dist: cln(10.0),
            compartment2: None,
            compartment3: Some(PkCompartmentPair { k_in: cln(1.0), k_out: cln(1.0) }),
        },
    };
    let wrap = |d: PkPdDrug| PharmacologySpec { drugs: vec![d], schedules: vec![], dosages: vec![] };
    assert!(DrugRegistry::new(Some(&wrap(bad_drug.clone())), &genotypes, |_, _| None).is_err());

    bad_drug.pk.compartment3 = None;
    bad_drug.pk.k_a = Some(cln(2.0));
    assert!(DrugRegistry::new(Some(&wrap(bad_drug.clone())), &genotypes, |_, _| None).is_err());

    bad_drug.pk.k_a = None;
    bad_drug.pd.phenotypes[0].restrictions =
        vec![AlleleRestriction { on_locus: "L".into(), to_allele: "A0".into() }];
    assert!(DrugRegistry::new(Some(&wrap(bad_drug)), &genotypes, |_, a| (a == "A0").then_some(0)).is_err());
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(96))]

    #[test]
    fn prop_zero_dose_yields_unit_factor(
        vd in 1.0_f64..200.0,
        hl in 0.5_f64..40.0,
        vmax in 1.0_f64..30.0,
        ic50 in 1e-4_f64..1.0,
        slope in 0.5_f64..8.0,
        body_mass in 5.0_f64..100.0,
        time in 0.0_f64..0.99,
    ) {
        let pharm = PharmacologySpec {
            drugs: vec![PkPdDrug {
                abbrev: "D".into(),
                pd: single_pd(vmax, ic50, slope),
                pk: PkSpec {
                    negligible_concentration: 1e-12,
                    elimination: PkElimination::HalfLife(hl),
                    k_a: None,
                    conversion: None,
                    vol_dist: cln(vd),
                    compartment2: None,
                    compartment3: None,
                },
            }],
            schedules: vec![],
            dosages: vec![],
        };
        let reg = DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap();
        let mut rng = LocalRng::new(1, 42);
        let mut pat = PatientPkPd::new();
        let mut kn = InfectionKnCache::new();

        // Unmedicated
        prop_assert_eq!(pat.get_drug_factor(&mut rng, &reg, 0, &mut kn, body_mass).unwrap(), 1.0);
        // Zero-quantity dose
        pat.medicate_drug(&mut rng, &reg, 0, 0.0, time).unwrap();
        let fac = pat.get_drug_factor(&mut rng, &reg, 0, &mut kn, body_mass).unwrap();
        prop_assert!((fac - 1.0).abs() < 1e-14);
        pat.decay_drugs(&reg, body_mass);
        prop_assert_eq!(pat.drug_concentration(0), 0.0);
    }

    #[test]
    fn prop_one_comp_bounded_and_monotonic_dose_response(
        vd in 2.0_f64..150.0,
        hl in 0.5_f64..30.0,
        vmax in 1.0_f64..30.0,
        ic50 in 1e-3_f64..0.5,
        slope in 1.0_f64..7.0,
        body_mass in 8.0_f64..90.0,
        dose_lo in 1.0_f64..500.0,
        dose_delta in 0.0_f64..500.0,
        dose_time in 0.0_f64..0.95,
    ) {
        let dose_hi = dose_lo + dose_delta;
        let pharm = PharmacologySpec {
            drugs: vec![PkPdDrug {
                abbrev: "D1".into(),
                pd: single_pd(vmax, ic50, slope),
                pk: PkSpec {
                    negligible_concentration: 1e-12,
                    elimination: PkElimination::HalfLife(hl),
                    k_a: None,
                    conversion: None,
                    vol_dist: cln(vd),
                    compartment2: None,
                    compartment3: None,
                },
            }],
            schedules: vec![],
            dosages: vec![],
        };
        let reg = DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap();

        let mut rng_lo = LocalRng::new(3, 999);
        let mut rng_hi = rng_lo.clone();
        let mut pat_lo = PatientPkPd::new();
        let mut pat_hi = PatientPkPd::new();
        let mut kn_lo = InfectionKnCache::new();
        let mut kn_hi = InfectionKnCache::new();

        pat_lo.medicate_drug(&mut rng_lo, &reg, 0, dose_lo, dose_time).unwrap();
        pat_hi.medicate_drug(&mut rng_hi, &reg, 0, dose_hi, dose_time).unwrap();

        let f_lo = pat_lo.get_drug_factor(&mut rng_lo, &reg, 0, &mut kn_lo, body_mass).unwrap();
        let f_hi = pat_hi.get_drug_factor(&mut rng_hi, &reg, 0, &mut kn_hi, body_mass).unwrap();

        prop_assert!(f_lo.is_finite() && (0.0..=1.0).contains(&f_lo));
        prop_assert!(f_hi.is_finite() && (0.0..=1.0).contains(&f_hi));
        prop_assert!(f_hi <= f_lo + 1e-13, "f_hi={f_hi} > f_lo={f_lo}");
    }

    #[test]
    fn prop_three_comp_and_conversion_bounded_and_monotonic(
        vd in 5.0_f64..80.0,
        ka in 12.0_f64..40.0,
        k_elim in 0.1_f64..1.5,
        k12 in 0.2_f64..1.5,
        k21 in 0.2_f64..1.5,
        conv_rate in 3.0_f64..9.0,
        body_mass in 10.0_f64..80.0,
        dose_lo in 5.0_f64..300.0,
        dose_delta in 0.0_f64..300.0,
    ) {
        let dose_hi = dose_lo + dose_delta;
        let pharm = PharmacologySpec {
            drugs: vec![
                PkPdDrug {
                    abbrev: "MET".into(),
                    pd: single_pd(15.0, 0.01, 3.5),
                    pk: PkSpec {
                        negligible_concentration: 1e-12,
                        elimination: PkElimination::EliminationRate { k: cln(1.8), m_exponent: 0.0 },
                        k_a: None,
                        conversion: None,
                        vol_dist: cln(12.0),
                        compartment2: None,
                        compartment3: None,
                    },
                },
                PkPdDrug {
                    abbrev: "CONV".into(),
                    pd: single_pd(15.0, 0.005, 3.5),
                    pk: PkSpec {
                        negligible_concentration: 1e-12,
                        elimination: PkElimination::EliminationRate { k: cln(k_elim), m_exponent: 0.0 },
                        k_a: Some(cln(ka)),
                        conversion: Some(PkConversion {
                            metabolite: "MET".into(),
                            rate: cln(conv_rate),
                            mol_ratio: 0.85,
                            ic50_log_correlation: 0.7,
                        }),
                        vol_dist: cln(vd),
                        compartment2: None,
                        compartment3: None,
                    },
                },
                PkPdDrug {
                    abbrev: "TC".into(),
                    pd: single_pd(6.0, 0.02, 4.0),
                    pk: PkSpec {
                        negligible_concentration: 1e-12,
                        elimination: PkElimination::EliminationRate { k: cln(k_elim), m_exponent: 0.25 },
                        k_a: Some(cln(ka)),
                        conversion: None,
                        vol_dist: cln(vd),
                        compartment2: Some(PkCompartmentPair { k_in: cln(k12), k_out: cln(k21) }),
                        compartment3: Some(PkCompartmentPair { k_in: cln(0.3), k_out: cln(0.1) }),
                    },
                },
            ],
            schedules: vec![],
            dosages: vec![],
        };
        let reg = DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap();

        for drug_idx in [1_usize, 2_usize] {
            let mut rng_lo = LocalRng::new(11, 2026);
            let mut rng_hi = rng_lo.clone();
            let mut pat_lo = PatientPkPd::new();
            let mut pat_hi = PatientPkPd::new();
            let mut kn_lo = InfectionKnCache::new();
            let mut kn_hi = InfectionKnCache::new();

            pat_lo.medicate_drug(&mut rng_lo, &reg, drug_idx, dose_lo, 0.0).unwrap();
            pat_hi.medicate_drug(&mut rng_hi, &reg, drug_idx, dose_hi, 0.0).unwrap();

            let f_lo = pat_lo.get_drug_factor(&mut rng_lo, &reg, 0, &mut kn_lo, body_mass).unwrap();
            let f_hi = pat_hi.get_drug_factor(&mut rng_hi, &reg, 0, &mut kn_hi, body_mass).unwrap();

            prop_assert!(f_lo.is_finite() && (0.0..=1.0).contains(&f_lo));
            prop_assert!(f_hi.is_finite() && (0.0..=1.0).contains(&f_hi));
            prop_assert!(f_hi <= f_lo + 1e-7, "drug {drug_idx}: f_hi={f_hi} > f_lo={f_lo}");

            pat_lo.decay_drugs(&reg, body_mass);
            pat_hi.decay_drugs(&reg, body_mass);
            prop_assert!(pat_hi.drug_concentration(drug_idx) >= pat_lo.drug_concentration(drug_idx));
        }
    }
}
