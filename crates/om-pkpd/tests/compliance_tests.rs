//! Full 6-day pharmacokinetic & pharmacodynamic compliance suite ported directly from
//! `unittest/PkPdComplianceSuite.h` and `unittest/UnittestUtil.h::PkPdSuiteSetup`.
#![forbid(unsafe_code)]

use om_pkpd::{DrugRegistry, InfectionKnCache, PatientPkPd};
use om_rng::LocalRng;
use om_schema::common::SampledValueLn;
use om_schema::pharmacology::{
    PdPhenotype, PdSpec, PharmacologySpec, PkCompartmentPair, PkConversion, PkElimination,
    PkPdDrug, PkSpec,
};
use std::collections::BTreeSet;

const PKPD_CONC_REL_TOL: f64 = 1e-5;
const PKPD_FACT_REL_TOL: f64 = 5e-3;
const PKPD_FACT_ABS_TOL: f64 = 1e-20;
const BODY_MASS: f64 = 50.0;

fn cln(mean: f64) -> SampledValueLn {
    SampledValueLn { mean, cv: None, variance: None, truncate: None, distr: "const".into() }
}

fn pd(vmax: f64, ic50: f64, slope: f64) -> PdSpec {
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

fn d_1c(abbrev: &str, vd: f64, negl: f64, k: f64, me: f64, pd: PdSpec) -> PkPdDrug {
    PkPdDrug {
        abbrev: abbrev.into(),
        pd,
        pk: PkSpec {
            negligible_concentration: negl,
            elimination: PkElimination::EliminationRate { k: cln(k), m_exponent: me },
            k_a: None,
            conversion: None,
            vol_dist: cln(vd),
            compartment2: None,
            compartment3: None,
        },
    }
}

fn d_hl(abbrev: &str, vd: f64, negl: f64, hl: f64, pd: PdSpec) -> PkPdDrug {
    PkPdDrug {
        abbrev: abbrev.into(),
        pd,
        pk: PkSpec {
            negligible_concentration: negl,
            elimination: PkElimination::HalfLife(hl),
            k_a: None,
            conversion: None,
            vol_dist: cln(vd),
            compartment2: None,
            compartment3: None,
        },
    }
}

#[allow(clippy::too_many_arguments)]
fn d_conv(
    abbrev: &str,
    vd: f64,
    negl: f64,
    ka: f64,
    met: &str,
    conv: f64,
    mwr: f64,
    corr: f64,
    pd: PdSpec,
) -> PkPdDrug {
    PkPdDrug {
        abbrev: abbrev.into(),
        pd,
        pk: PkSpec {
            negligible_concentration: negl,
            elimination: PkElimination::EliminationRate { k: cln(0.0), m_exponent: 0.0 },
            k_a: Some(cln(ka)),
            conversion: Some(PkConversion {
                metabolite: met.into(),
                rate: cln(conv),
                mol_ratio: mwr,
                ic50_log_correlation: corr,
            }),
            vol_dist: cln(vd),
            compartment2: None,
            compartment3: None,
        },
    }
}

#[allow(clippy::too_many_arguments)]
fn d_mc(
    abbrev: &str,
    vd: f64,
    negl: f64,
    k: f64,
    me: f64,
    ka: f64,
    c2: (f64, f64),
    c3: Option<(f64, f64)>,
    pd: PdSpec,
) -> PkPdDrug {
    PkPdDrug {
        abbrev: abbrev.into(),
        pd,
        pk: PkSpec {
            negligible_concentration: negl,
            elimination: PkElimination::EliminationRate { k: cln(k), m_exponent: me },
            k_a: Some(cln(ka)),
            conversion: None,
            vol_dist: cln(vd),
            compartment2: Some(PkCompartmentPair { k_in: cln(c2.0), k_out: cln(c2.1) }),
            compartment3: c3.map(|(i, o)| PkCompartmentPair { k_in: cln(i), k_out: cln(o) }),
        },
    }
}

fn build_suite_registry() -> DrugRegistry {
    let pharm = PharmacologySpec {
        schedules: vec![],
        dosages: vec![],
        drugs: vec![
            d_1c("AR1", 17.4, 1e-17, 3.96, 0.0, pd(27.6, 0.0023, 4.0)),
            d_1c("DHA_AR", 15.0, 1e-17, 44.15, 0.0, pd(27.6, 0.009, 4.0)),
            d_conv("AR", 46.6, 1e-17, 23.98, "DHA_AR", 11.98, 0.9547587, 1.0, pd(27.6, 0.0023, 4.0)),
            d_1c("AS1", 2.75, 1e-17, 16.6, 0.0, pd(27.6, 0.0016, 4.0)),
            d_1c("DHA_AS", 1.49, 1e-35, 25.4, 0.0, pd(27.6, 0.009, 4.0)),
            d_conv("AS", 7.1, 1e-45, 252.0, "DHA_AS", 30.96, 0.741155, 1.0, pd(27.6, 0.0016, 4.0)),
            d_1c("DHA", 1.49, 1e-17, 19.8, 0.0, pd(27.6, 0.009, 4.0)),
            d_hl("CQ", 300.0, 0.00036, 30.006, pd(3.45, 0.02, 1.6)),
            d_1c("LF", 21.0, 0.00032, 0.16, 0.0, pd(3.45, 0.032, 4.0)),
            d_hl("MQ", 20.8, 0.005, 13.078, pd(3.45, 0.027, 5.0)),
            d_1c("PPQ", 150.0, 0.005, 0.03, 0.0, pd(3.45, 0.020831339, 6.0)),
            d_mc(
                "PPQ2", 173.0, 0.005, 0.2452253, 0.25, 11.16,
                (0.2014864, 0.07870968), None, pd(3.45, 0.020831339, 6.0),
            ),
            d_mc(
                "PPQ3", 57.5625, 0.005, 16.314788273615637, 1.0, 3.4825,
                (1.854166666666667, 1.1545945945945946),
                Some((0.9027777777777778, 0.07948639559767655)),
                pd(3.45, 0.020831339, 6.0),
            ),
        ],
    };
    let genotypes = [BTreeSet::from([0_u32])];
    DrugRegistry::new(Some(&pharm), &genotypes, |_, _| None).unwrap()
}

fn assert_approx_tol(actual: f64, expected: f64, rel_tol: f64, abs_tol: f64, label: &str) {
    if expected != 0.0 && ((actual / expected).abs() - 1.0).abs() < rel_tol {
        return;
    }
    let diff = (actual - expected).abs();
    assert!(diff <= abs_tol, "{label}: actual={actual:.12e}, expected={expected:.12e}");
}

fn triple(dose: f64) -> Vec<(usize, f64, f64)> {
    vec![(0, 0.0, dose), (1, 0.0, dose), (2, 0.0, dose)]
}

fn hex(dose: f64) -> Vec<(usize, f64, f64)> {
    vec![
        (0, 0.0, dose), (0, 0.5, dose),
        (1, 0.0, dose), (1, 0.5, dose),
        (2, 0.0, dose), (2, 0.5, dose),
    ]
}

fn run_sim(
    drug_name: &str,
    drug2_name: Option<&str>,
    schedule: &[(usize, f64, f64)],
    exp_conc: &[f64; 6],
    exp_conc2: Option<&[f64; 6]>,
    exp_factors: &[f64; 6],
) {
    let mut registry = build_suite_registry();
    let idx1 = registry.use_drug(drug_name).unwrap();
    let idx2 = drug2_name.map(|n| registry.use_drug(n).unwrap());
    let tol1 = registry.drug(idx1).unwrap().negligible_concentration();
    let tol2 = idx2.map_or(0.0, |i| registry.drug(i).unwrap().negligible_concentration());

    let mut rng = LocalRng::new(0, 721_347_520_444_481_703);
    let mut patient = PatientPkPd::new();
    let mut kn_cache = InfectionKnCache::new();
    let mut total_fac = 1.0;

    for day in 0..6 {
        total_fac *= patient
            .get_drug_factor(&mut rng, &registry, 0, &mut kn_cache, BODY_MASS)
            .unwrap();
        assert_approx_tol(total_fac, exp_factors[day], PKPD_FACT_REL_TOL, PKPD_FACT_ABS_TOL, drug_name);

        patient.decay_drugs(&registry, BODY_MASS);
        assert_approx_tol(
            patient.drug_concentration(idx1), exp_conc[day], PKPD_CONC_REL_TOL, tol1, drug_name,
        );
        if let (Some(i2), Some(e2)) = (idx2, exp_conc2) {
            assert_approx_tol(
                patient.drug_concentration(i2), e2[day], PKPD_CONC_REL_TOL, tol2, drug_name,
            );
        }
        for &(d, t, qty) in schedule {
            if d == day {
                patient.medicate_drug(&mut rng, &registry, idx1, qty, t).unwrap();
            }
        }
    }
}

#[test]
fn test_ar1() {
    run_sim(
        "AR1", None, &hex(1.7 * BODY_MASS),
        &[0.0, 0.01535201, 0.01564467, 0.01565025, 0.0002983425, 5.687336e-06], None,
        &[1.0, 1.033933e-12, 1.068873e-24, 1.103296e-36, 1.734223e-42, 1.729046e-42],
    );
}

#[test]
fn test_ar_conversion() {
    run_sim(
        "AR", Some("DHA_AR"), &hex(1.7 * BODY_MASS),
        &[0.0, 0.0001825220, 0.0001825231, 0.0001825231, 1.146952e-09, 7.189475e-15],
        Some(&[0.0, 0.0002013114, 0.0002013126, 0.0002013126, 1.266891e-09, 7.941293e-15]),
        &[1.0, 1.695266e-07, 2.838279e-14, 4.740382e-21, 4.751844e-21, 4.751846e-21],
    );
}

#[test]
fn test_as1() {
    run_sim(
        "AS1", None, &triple(4.0 * BODY_MASS),
        &[0.0, 8.983362e-08, 8.983362e-08, 8.983362e-08, 5.54818e-15, 3.42659e-22], None,
        &[1.0, 1.204675e-05, 1.451241e-10, 1.748061e-15, 1.748273e-15, 1.748272e-15],
    );
}

#[test]
fn test_as_conversion() {
    run_sim(
        "AS", Some("DHA_AS"), &triple(4.0 * BODY_MASS),
        &[0.0, 2.301305e-14, 2.301305e-14, 2.301305e-14, 8.245500e-28, 2.954336e-41],
        Some(&[0.0, 1.142491e-10, 1.142491e-10, 1.142491e-10, 1.067784e-21, 9.940541e-33]),
        &[1.0, 0.0005152782, 2.655117e-07, 1.368124e-10, 1.368124e-10, 1.368124e-10],
    );
}

#[test]
fn test_dha() {
    run_sim(
        "DHA", None, &triple(4.0 * BODY_MASS),
        &[0.0, 6.758386e-09, 6.758386e-09, 6.758386e-09, 1.701423e-17, 4.28333e-26], None,
        &[1.0, 0.0003552336, 1.261909e-07, 4.482726e-11, 4.482726e-11, 4.482726e-11],
    );
}

#[test]
fn test_cq() {
    let cq_sched = [(0, 0.0, 500.0), (1, 0.0, 500.0), (2, 0.0, 250.0)];
    run_sim(
        "CQ", None, &cq_sched,
        &[0.0, 0.03257216, 0.06440052, 0.07921600, 0.07740709, 0.07563948], None,
        &[1.0, 9.259311e-02, 4.623815e-03, 2.057661e-04, 9.262133e-06, 4.218529e-07],
    );
}

#[test]
fn test_lf() {
    run_sim(
        "LF", None, &hex(12.0 * BODY_MASS),
        &[0.0, 1.014434363, 1.878878305, 2.615508841, 2.228789614, 1.899249226], None,
        &[1.0, 0.03174632, 0.001007809, 3.199346e-05, 1.015654e-06, 3.224254e-08],
    );
}

#[test]
fn test_mq() {
    run_sim(
        "MQ", None, &triple(8.3 * BODY_MASS),
        &[0.0, 0.378440101, 0.737345129, 1.077723484, 1.022091411, 0.969331065], None,
        &[1.0, 0.03174581, 0.001007791, 3.199298e-05, 1.015638e-06, 3.224205e-08],
    );
}

#[test]
fn test_ppq_1c() {
    run_sim(
        "PPQ", None, &triple(18.0 * BODY_MASS),
        &[0.0, 0.116453464, 0.2294652081, 0.339137, 0.3291139387, 0.3193871518], None,
        &[1.0, 0.03174892, 0.001007891, 3.199625e-05, 1.015747e-06, 3.224518e-08],
    );
}

#[test]
fn test_ppq_hodel2013_2c() {
    run_sim(
        "PPQ2", None, &triple(18.0 * BODY_MASS),
        &[0.0, 0.08022449, 0.1416033, 0.18962337, 0.14792303, 0.11829172], None,
        &[1.0, 0.03422595, 0.001086594, 3.449438e-05, 1.095144e-06, 3.479034e-08],
    );
}

#[test]
fn test_ppq_tarning2012_3c() {
    run_sim(
        "PPQ3", None, &triple(18.0 * BODY_MASS),
        &[0.0, 0.075305088, 0.118119866, 0.150210662, 0.100426437, 0.078729041], None,
        &[1.0, 0.03421756, 0.001086307, 3.448539e-05, 1.094894e-06, 3.478302e-08],
    );
}
