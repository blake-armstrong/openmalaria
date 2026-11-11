//! Unit tests porting `unittest/LSTMPkPdSuite.h`, `CMDecisionTreeSuite::testDosing`,
//! PRNG draw-order parity, `insert_dose_sorted` tie semantics, checkpoint serialization,
//! and schema validation edge cases.
#![forbid(unsafe_code)]

use om_pkpd::{
    DosageTable, Dose, DrugRegistry, InfectionKnCache, PatientPkPd, insert_dose_sorted,
};
use om_rng::LocalRng;
use om_schema::common::SampledValueLn;
use om_schema::pharmacology::{
    DosageMode, DoseStep, MedicateStep, PdPhenotype, PdSpec, PharmacologySpec, PkCompartmentPair,
    PkConversion, PkElimination, PkPdDosages, PkPdDrug, PkPdSchedule, PkSpec,
};
use std::collections::BTreeSet;

const MASS_AT_21: f64 = 55.4993;

fn cln(mean: f64) -> SampledValueLn {
    SampledValueLn { mean, cv: None, variance: None, truncate: None, distr: "const".into() }
}

fn lln(mean: f64, cv: f64) -> SampledValueLn {
    SampledValueLn { mean, cv: Some(cv), variance: None, truncate: None, distr: "lognormal".into() }
}

fn single_pd(vmax: f64, ic50: SampledValueLn, slope: f64) -> PdSpec {
    PdSpec {
        locus: None,
        phenotypes: vec![PdPhenotype {
            name: None,
            restrictions: vec![],
            max_killing_rate: vmax,
            ic50,
            slope,
        }],
    }
}

fn mq_registry() -> DrugRegistry {
    let pharm = PharmacologySpec {
        drugs: vec![PkPdDrug {
            abbrev: "MQ".into(),
            pd: single_pd(3.45, cln(0.027), 5.0),
            pk: PkSpec {
                negligible_concentration: 0.005,
                elimination: PkElimination::HalfLife(13.078),
                k_a: None,
                conversion: None,
                vol_dist: cln(20.8),
                compartment2: None,
                compartment3: None,
            },
        }],
        schedules: vec![
            PkPdSchedule {
                name: "sched1".into(),
                medications: vec![MedicateStep { drug: "MQ".into(), mg: 6.0, hour: 0.0 }],
            },
            PkPdSchedule {
                name: "sched2".into(),
                medications: vec![
                    MedicateStep { drug: "MQ".into(), mg: 2.0, hour: 0.0 },
                    MedicateStep { drug: "MQ".into(), mg: 5.0, hour: 12.0 },
                ],
            },
        ],
        dosages: vec![PkPdDosages {
            name: "dosage1".into(),
            mode: DosageMode::Age(vec![
                DoseStep { lowerbound: 0.0, dose_mult: 1.0 },
                DoseStep { lowerbound: 5.0, dose_mult: 5.0 },
            ]),
        }],
    };
    DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap()
}

fn assert_cxx_approx(actual: f64, expected: f64) {
    let tol = (1e-7 * actual.abs().max(expected.abs())).max(1e-7);
    assert!((actual - expected).abs() <= tol, "actual={actual:.17e}, expected={expected:.17e}");
}

#[test]
fn lstm_pkpd_suite_all_cases() {
    let reg = mq_registry();
    let seed = 721_347_520_444_481_703_u64;

    // 1. testNone
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    assert_eq!(proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap(), 1.0);

    // 2. testOral
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    proxy.medicate_drug(&mut rng, &reg, 0, 3000.0, 0.0).unwrap();
    let f_oral = proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap();
    assert_cxx_approx(f_oral, 0.03174563638523168);
    assert!((f_oral - 0.031_745_636_393_248_57).abs() < 1e-15);

    // 3. testOralHalves (two 1500 mg doses at t = 0.0)
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    proxy.medicate_drug(&mut rng, &reg, 0, 1500.0, 0.0).unwrap();
    proxy.medicate_drug(&mut rng, &reg, 0, 1500.0, 0.0).unwrap();
    let f_halves = proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap();
    assert_eq!(f_halves, f_oral);

    // 4. testOralSplit (0 mg dose at t = 0.5 splits analytic integration into two half-days)
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    proxy.medicate_drug(&mut rng, &reg, 0, 3000.0, 0.0).unwrap();
    proxy.medicate_drug(&mut rng, &reg, 0, 0.0, 0.5).unwrap();
    let f_split = proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap();
    assert_cxx_approx(f_split, 0.03174563639140275);
    assert!((f_split - f_oral).abs() < 1e-15);

    // 5. testOralDecayed
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    proxy.medicate_drug(&mut rng, &reg, 0, 3000.0, 0.0).unwrap();
    proxy.decay_drugs(&reg, MASS_AT_21);
    let f_dec = proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap();
    assert_cxx_approx(f_dec, 0.03174563639501896);

    // 6. testOral2Doses
    let mut rng = LocalRng::new(0, seed);
    let mut proxy = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    proxy.medicate_drug(&mut rng, &reg, 0, 3000.0, 0.0).unwrap();
    proxy.decay_drugs(&reg, MASS_AT_21);
    proxy.medicate_drug(&mut rng, &reg, 0, 3000.0, 0.0).unwrap();
    let f_two = proxy.get_drug_factor(&mut rng, &reg, 0, &mut kn, MASS_AT_21).unwrap();
    assert_cxx_approx(f_two, 0.03174563637686205);
}

#[test]
fn dosing_tables_and_delayed_prescriptions() {
    let reg = mq_registry();
    let sched1 = reg.find_schedule("sched1").unwrap();
    let sched2 = reg.find_schedule("sched2").unwrap();
    let dosage1 = reg.find_dosages("dosage1").unwrap();

    let prescribed_mg = |sched: usize, age: f64| -> f64 {
        let mut p = PatientPkPd::new();
        p.prescribe(&reg, sched, dosage1, age, 50.0, 0.0).unwrap();
        p.pending_medications().iter().map(|m| m.qty).sum()
    };

    assert!((prescribed_mg(sched1, 0.0) - 6.0).abs() < 1e-8);
    assert!((prescribed_mg(sched1, 4.9) - 6.0).abs() < 1e-8);
    assert!((prescribed_mg(sched1, 5.0) - 30.0).abs() < 1e-8);
    assert!((prescribed_mg(sched1, 99.0) - 30.0).abs() < 1e-8);
    assert!((prescribed_mg(sched2, 0.0) - 7.0).abs() < 1e-8);
    assert!((prescribed_mg(sched2, 99.0) - 35.0).abs() < 1e-8);

    // Multiply-by-kg and BodyMass step brackets
    let mult_kg = DosageTable::from_spec(&PkPdDosages {
        name: "kg".into(),
        mode: DosageMode::Multiply { by: "kg".into() },
    })
    .unwrap();
    assert!(mult_kg.uses_body_mass() && mult_kg.multiplies_by_kg());
    assert_eq!(mult_kg.get_multiplier(42.5).unwrap(), 42.5);

    let bm_tbl = DosageTable::from_spec(&PkPdDosages {
        name: "bm".into(),
        mode: DosageMode::BodyMass(vec![
            DoseStep { lowerbound: 0.0, dose_mult: 0.5 },
            DoseStep { lowerbound: 15.0, dose_mult: 1.0 },
        ]),
    })
    .unwrap();
    assert_eq!(bm_tbl.get_multiplier(14.999).unwrap(), 0.5);
    assert_eq!(bm_tbl.get_multiplier(15.0).unwrap(), 1.0);

    // Delayed prescription (delay_days = 1.5 -> stays in medicate_queue on day 0, activates on day 1 at t = 0.5)
    let mut rng = LocalRng::new(1, 99);
    let mut p = PatientPkPd::new();
    p.prescribe(&reg, sched1, dosage1, 10.0, 30.0, 1.5).unwrap();
    p.medicate(&mut rng, &reg).unwrap();
    assert!(p.drugs().is_empty());
    assert_eq!(p.pending_medications()[0].time, 0.5);
    p.medicate(&mut rng, &reg).unwrap();
    assert_eq!(p.drugs().len(), 1);
    assert!(p.pending_medications().is_empty());
}

#[test]
fn dose_insertion_matches_cxx_std_lower_bound() {
    let mut doses = Vec::new();
    insert_dose_sorted(&mut doses, 0.5, 10.0);
    insert_dose_sorted(&mut doses, 0.0, 20.0);
    insert_dose_sorted(&mut doses, 0.5, 30.0);
    insert_dose_sorted(&mut doses, 1.0, 40.0);
    insert_dose_sorted(&mut doses, 0.5, 50.0);
    // `std::lower_bound` (`d.time < time`) inserts BEFORE existing equal timestamps:
    assert_eq!(
        doses,
        vec![
            Dose { time: 0.0, qty: 20.0 },
            Dose { time: 0.5, qty: 50.0 },
            Dose { time: 0.5, qty: 30.0 },
            Dose { time: 0.5, qty: 10.0 },
            Dose { time: 1.0, qty: 40.0 },
        ]
    );
}

#[test]
fn conversion_correlated_kn_rng_parity_and_early_exit() {
    let make_reg = |corr: f64, ic50: SampledValueLn| {
        let pharm = PharmacologySpec {
            drugs: vec![
                PkPdDrug {
                    abbrev: "MET".into(),
                    pd: single_pd(27.6, ic50.clone(), 4.0),
                    pk: PkSpec {
                        negligible_concentration: 0.1,
                        elimination: PkElimination::EliminationRate { k: cln(20.0), m_exponent: 0.0 },
                        k_a: None,
                        conversion: None,
                        vol_dist: cln(15.0),
                        compartment2: None,
                        compartment3: None,
                    },
                },
                PkPdDrug {
                    abbrev: "PAR".into(),
                    pd: single_pd(27.6, ic50, 4.0),
                    pk: PkSpec {
                        negligible_concentration: 0.1,
                        elimination: PkElimination::EliminationRate { k: cln(0.0), m_exponent: 0.0 },
                        k_a: Some(cln(24.0)),
                        conversion: Some(PkConversion {
                            metabolite: "MET".into(),
                            rate: cln(12.0),
                            mol_ratio: 0.95,
                            ic50_log_correlation: corr,
                        }),
                        vol_dist: cln(46.0),
                        compartment2: None,
                        compartment3: None,
                    },
                },
            ],
            schedules: vec![PkPdSchedule {
                name: "s".into(),
                medications: vec![MedicateStep { drug: "PAR".into(), mg: 100.0, hour: 0.0 }],
            }],
            dosages: vec![],
        };
        DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap()
    };

    // corr == 1.0 draws EXACTLY 1 Gaussian on first infection encounter even when IC50 is const!
    let reg1 = make_reg(1.0, cln(0.0023));
    assert_eq!(reg1.drugs_in_use(), &[0, 1]);
    let mut rng_a = LocalRng::new(7, 12345);
    let mut rng_ref = rng_a.clone();
    let mut pat = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    pat.medicate_drug(&mut rng_a, &reg1, 1, 100.0, 0.0).unwrap();
    assert_eq!(rng_a, rng_ref, "const PK creation must draw 0 RNGs");

    let _ = pat.get_drug_factor(&mut rng_a, &reg1, 0, &mut kn, 50.0).unwrap();
    let _ = rng_ref.gauss(0.0, 1.0);
    assert_eq!(rng_a, rng_ref, "corr=1.0 must consume exactly 1 Gaussian");
    assert_eq!(kn.len(), 2);

    // Second day with cached Kn consumes 0 RNGs; after decaying below negligible (0.1), early-exit draws 0 RNGs
    let _ = pat.get_drug_factor(&mut rng_a, &reg1, 0, &mut kn, 50.0).unwrap();
    assert_eq!(rng_a, rng_ref);
    for _ in 0..5 {
        pat.decay_drugs(&reg1, 50.0);
    }
    assert_eq!(pat.drug_concentration(0), 0.0);
    assert_eq!(pat.drug_concentration(1), 0.0);
    assert_eq!(pat.drugs().len(), 1, "per-human drug instance persists for lifetime");
    let mut fresh_kn = InfectionKnCache::new();
    assert_eq!(pat.get_drug_factor(&mut rng_a, &reg1, 0, &mut fresh_kn, 50.0).unwrap(), 1.0);
    assert!(fresh_kn.is_empty(), "zero-conc early exit must not sample Kn");
    assert_eq!(rng_a, rng_ref);

    // corr < 1.0 draws EXACTLY 2 Gaussians on first encounter
    let reg_half = make_reg(0.5, lln(0.005, 0.3));
    let mut rng_b = LocalRng::new(9, 54321);
    let mut rng_b_ref = rng_b.clone();
    let mut pat_b = PatientPkPd::new();
    let mut kn_b = InfectionKnCache::new();
    pat_b.medicate_drug(&mut rng_b, &reg_half, 1, 100.0, 0.0).unwrap();
    let _ = pat_b.get_drug_factor(&mut rng_b, &reg_half, 0, &mut kn_b, 50.0).unwrap();
    let _ = rng_b_ref.gauss(0.0, 1.0);
    let _ = rng_b_ref.gauss(0.0, (1.0 - 0.25_f64).sqrt());
    assert_eq!(rng_b, rng_b_ref, "corr=0.5 must consume exactly 2 Gaussians");
}

#[test]
fn checkpoint_json_and_bincode_roundtrip_all_models() {
    let pharm = PharmacologySpec {
        drugs: vec![
            PkPdDrug {
                abbrev: "M".into(),
                pd: single_pd(3.45, lln(0.02, 0.2), 4.0),
                pk: PkSpec {
                    negligible_concentration: 1e-6,
                    elimination: PkElimination::HalfLife(5.0),
                    k_a: None,
                    conversion: None,
                    vol_dist: lln(15.0, 0.1),
                    compartment2: None,
                    compartment3: None,
                },
            },
            PkPdDrug {
                abbrev: "P".into(),
                pd: single_pd(20.0, lln(0.005, 0.2), 4.0),
                pk: PkSpec {
                    negligible_concentration: 1e-6,
                    elimination: PkElimination::EliminationRate { k: lln(0.5, 0.1), m_exponent: 0.0 },
                    k_a: Some(lln(12.0, 0.1)),
                    conversion: Some(PkConversion {
                        metabolite: "M".into(),
                        rate: lln(8.0, 0.1),
                        mol_ratio: 0.8,
                        ic50_log_correlation: 0.6,
                    }),
                    vol_dist: lln(30.0, 0.1),
                    compartment2: None,
                    compartment3: None,
                },
            },
            PkPdDrug {
                abbrev: "C3".into(),
                pd: single_pd(3.45, lln(0.02, 0.15), 5.0),
                pk: PkSpec {
                    negligible_concentration: 1e-6,
                    elimination: PkElimination::EliminationRate { k: lln(1.2, 0.1), m_exponent: 0.5 },
                    k_a: Some(lln(4.0, 0.1)),
                    conversion: None,
                    vol_dist: lln(40.0, 0.1),
                    compartment2: Some(PkCompartmentPair { k_in: cln(1.1), k_out: cln(0.9) }),
                    compartment3: Some(PkCompartmentPair { k_in: cln(0.4), k_out: cln(0.2) }),
                },
            },
        ],
        schedules: vec![PkPdSchedule {
            name: "combo".into(),
            medications: vec![
                MedicateStep { drug: "M".into(), mg: 50.0, hour: 0.0 },
                MedicateStep { drug: "P".into(), mg: 100.0, hour: 12.0 },
                MedicateStep { drug: "C3".into(), mg: 200.0, hour: 24.0 },
            ],
        }],
        dosages: vec![PkPdDosages {
            name: "d".into(),
            mode: DosageMode::Age(vec![DoseStep { lowerbound: 0.0, dose_mult: 1.0 }]),
        }],
    };
    let reg = DrugRegistry::new(Some(&pharm), &[BTreeSet::from([0_u32])], |_, _| None).unwrap();
    let mut rng = LocalRng::new(42, 777);
    let mut pat = PatientPkPd::new();
    let mut kn = InfectionKnCache::new();
    pat.prescribe(&reg, 0, 0, 20.0, 55.0, 0.0).unwrap();
    pat.medicate(&mut rng, &reg).unwrap();
    let _ = pat.get_drug_factor(&mut rng, &reg, 0, &mut kn, 55.0).unwrap();
    pat.decay_drugs(&reg, 55.0);
    assert_eq!(pat.non_zero_concentrations(&reg).count(), 3); // M from 1C + M & P from Conv

    let json = serde_json::to_string(&(&pat, &kn, &reg)).unwrap();
    let (mut pat_j, mut kn_j, reg_j): (PatientPkPd, InfectionKnCache, DrugRegistry) =
        serde_json::from_str(&json).unwrap();
    let bytes = bincode::serde::encode_to_vec((&pat, &kn), bincode::config::standard()).unwrap();
    let ((mut pat_b, mut kn_b), _): ((PatientPkPd, InfectionKnCache), usize) =
        bincode::serde::decode_from_slice(&bytes, bincode::config::standard()).unwrap();

    let mut rng_j = rng.clone();
    let mut rng_b = rng.clone();
    pat.medicate(&mut rng, &reg).unwrap();
    pat_j.medicate(&mut rng_j, &reg_j).unwrap();
    pat_b.medicate(&mut rng_b, &reg).unwrap();

    let f0 = pat.get_drug_factor(&mut rng, &reg, 0, &mut kn, 55.0).unwrap();
    let fj = pat_j.get_drug_factor(&mut rng_j, &reg_j, 0, &mut kn_j, 55.0).unwrap();
    let fb = pat_b.get_drug_factor(&mut rng_b, &reg, 0, &mut kn_b, 55.0).unwrap();
    assert_eq!(f0, fj);
    assert_eq!(f0, fb);
}
