//! Unit and property-based tests for `om-core` primitives.
#![forbid(unsafe_code)]

use om_core::fourier::{exp_idft, log_fourier_coefficients};
use om_core::solvers::{nmsimplex_2d, qag_integrate, secant_root};
use om_core::unit_parse::{duration_to_days, parse_date, read_date, read_duration, read_short_duration};
use om_core::{
    AgeGroupInterpolator, AnySampler, BetaSampler, DecayFunction, DecaySpec, DefaultUnit,
    InterpKind, LognormalSampler, ModelName, ModelOptions, NormalSample, NormalSampler, OptionCode,
    Parameter, Parameters, SimClock, WeibullSampler, mod_nn,
};
use om_rng::{LocalRng, MasterRng};
use proptest::prelude::*;

#[test]
fn clock_and_date_unit_parsing() {
    let mut clock = SimClock::new(5, 90.0).unwrap();
    let d_start = parse_date(&clock, "2000-01-1").unwrap();
    clock.set_start_date(d_start);
    assert_eq!(clock.steps_per_year(), 73);
    assert_eq!(clock.max_human_age(), clock.from_ts(6570));

    assert_eq!(
        read_short_duration(&clock, "30d", DefaultUnit::Days).unwrap(),
        clock.from_ts(6)
    );
    assert_eq!(
        read_duration(&clock, "1y", DefaultUnit::Years).unwrap(),
        clock.from_ts(73)
    );
    assert!((duration_to_days(Some(&clock), "2y", DefaultUnit::Days).unwrap() - 730.0).abs() < 1e-12);
    assert_eq!(
        read_date(&clock, "2001-01-01", DefaultUnit::Steps).unwrap(),
        d_start + clock.from_ts(73)
    );
    assert_eq!(mod_nn(5, 73), 5);
    assert_eq!(mod_nn(73, 73), 0);
}

#[test]
fn model_options_and_parameters_validation() {
    // ModelName::None enables default options (`MaxDensCorrection`, `InnateMaxDens`, `IndirectMortalityFix`);
    // ModelName::Base starts with all options `false` unless explicitly enabled.
    let none_opts = ModelOptions::from_xml_entries(ModelName::None, [], 5).unwrap();
    assert!(none_opts.get(OptionCode::MaxDensCorrection));
    assert!(none_opts.get(OptionCode::InnateMaxDens));
    assert!(none_opts.get(OptionCode::IndirectMortalityFix));

    let base_opts = ModelOptions::from_xml_entries(ModelName::Base, [], 5).unwrap();
    assert!(!base_opts.get(OptionCode::VivaxSimpleModel));

    // Incompatible empirical + molineaux fails cleanly
    let bad = ModelOptions::from_xml_entries(
        ModelName::Base,
        [
            ("EMPIRICAL_WITHIN_HOST_MODEL", true),
            ("MOLINEAUX_WITHIN_HOST_MODEL", true),
        ],
        1,
    );
    assert!(bad.is_err());

    // 5-day interval rejects CLINICAL_EVENT_SCHEDULER
    let bad_ts = ModelOptions::from_xml_entries(
        ModelName::Base,
        [("CLINICAL_EVENT_SCHEDULER", true)],
        5,
    );
    assert!(bad_ts.is_err());

    let mut params = Parameters::from_xml_entries(ModelName::Base, [(1, 0.123), (29, 99.0)]).unwrap();
    assert!((params.try_get(Parameter::NegLogOneMinusSinf).unwrap() - 0.123).abs() < 1e-12);
    params.set(Parameter::Alpha, 0.5);
    assert!((params.try_get(Parameter::Alpha).unwrap() - 0.5).abs() < 1e-12);
}

#[test]
fn age_group_interpolation_linear_and_piecewise() {
    let groups = vec![(0.0, 0.2), (10.0, 0.6), (20.0, 1.0), (20.0, 1.0)];
    let lin = AgeGroupInterpolator::from_groups(&groups, InterpKind::Linear, 32850, "avail").unwrap();
    // Midpoints at 5.0 -> 0.2, 15.0 -> 0.6, 20.0 -> 1.0
    assert!((lin.eval(2.0) - 0.2).abs() < 1e-12);
    assert!((lin.eval(10.0) - 0.4).abs() < 1e-12);
    assert!((lin.eval(25.0) - 1.0).abs() < 1e-12);

    let pw = AgeGroupInterpolator::from_groups(
        &[(0.0, 0.25), (5.0, 0.75)],
        InterpKind::PiecewiseConstant,
        32850,
        "cfr",
    )
    .unwrap();
    assert!((pw.eval(4.99) - 0.25).abs() < 1e-12);
    assert!((pw.eval(5.0) - 0.75).abs() < 1e-12);
}

#[test]
fn all_twelve_decay_functions_and_zero_alloc_het_sample() {
    let clock = SimClock::new(5, 90.0).unwrap();
    let mut master = MasterRng::new(7, 0);
    let mut rng = LocalRng::from_master(&mut master);

    let mk = |func: &str, l: Option<&str>| DecaySpec {
        function: func.into(),
        l: l.map(String::from),
        k: 1.5,
        cv: 0.0,
        increasing: false,
        initial_efficacy: 1.0,
        initial_concentration: Some(10.0),
        emax: Some(0.9),
        slope: Some(2.0),
        ic50: Some(2.0),
        children: Vec::new(),
    };

    for func in [
        "constant",
        "step",
        "linear",
        "exponential",
        "weibull",
        "hill",
        "smooth-compact",
    ] {
        let l = if func == "constant" { None } else { Some("2y") };
        let df = DecayFunction::from_spec(&mk(func, l), Some(&clock), "test").unwrap();
        let s = df.het_sample(&mut rng);
        let v0 = df.eval(0.0, s);
        let v1 = df.eval(365.0, s);
        let v3 = df.eval(1500.0, s);
        assert!(v0 >= v1 && v1 >= v3, "{func}: {v0}, {v1}, {v3}");
    }

    let mut emax_spec = mk("emax", None);
    emax_spec.children = vec![mk("exponential", Some("1y"))];
    let emax_df = DecayFunction::from_spec(&emax_spec, Some(&clock), "emax").unwrap();
    let hs = emax_df.het_sample(&mut rng);
    assert!(emax_df.eval(0.0, hs) > emax_df.eval(365.0, hs));

    for op_name in ["plus", "minus", "divides", "multiplies"] {
        let mut comp_spec = mk(op_name, None);
        comp_spec.cv = 0.2;
        comp_spec.children = vec![mk("exponential", Some("1y")), mk("constant", None)];
        let comp_df = DecayFunction::from_spec(&comp_spec, Some(&clock), op_name).unwrap();
        let hs_comp = comp_df.het_sample(&mut rng);
        let val = comp_df.eval(50.0, hs_comp);
        assert!((0.0..=1.0).contains(&val));
    }
}

#[test]
fn fourier_and_numerical_solvers() {
    let mut series = vec![0.0; 365];
    for (t, v) in series.iter_mut().enumerate() {
        let ang = 2.0 * std::f64::consts::PI * (t as f64) / 365.0;
        *v = (0.5 + 0.8 * ang.cos() - 0.3 * ang.sin()).exp();
    }
    let mut fc = vec![0.0; 5];
    log_fourier_coefficients(&series, &mut fc).unwrap();
    let mut recon = vec![0.0; 365];
    exp_idft(&mut recon, &fc, 0.0).unwrap();
    for (a, b) in series.iter().zip(&recon) {
        assert!((a - b).abs() < 1e-10);
    }

    // Integral of sin(x) on [0, pi] == 2.0
    let area = qag_integrate(f64::sin, 0.0, std::f64::consts::PI, 1e-12, 1e-12, 100).unwrap();
    assert!((area - 2.0).abs() < 1e-13);

    // Secant root of x^2 - 2 = 0
    let r = secant_root(|x| (x * x - 2.0, 2.0 * x), 1.5, 1e-12, 1e-12, 50).unwrap();
    assert!((r - std::f64::consts::SQRT_2).abs() < 1e-12);

    // Nelder-Mead minimum of (x - 3)^2 + (y + 1.5)^2
    let min_xy = nmsimplex_2d(
        |x, y| (x - 3.0).powi(2) + (y + 1.5).powi(2),
        [0.0, 0.0],
        [1.0, 1.0],
        1e-8,
        500,
    )
    .unwrap();
    assert!((min_xy[0] - 3.0).abs() < 1e-5);
    assert!((min_xy[1] + 1.5).abs() < 1e-5);
}

proptest! {
    #[test]
    fn samplers_preserve_domain_and_zero_rng_const_invariant(
        seed in 1u64..10_000u64,
        mean in 0.1f64..20.0f64,
        cv in 0.05f64..1.5f64,
    ) {
        let mut m1 = MasterRng::new(seed, 0);
        let mut r1 = LocalRng::from_master(&mut m1);
        let mut m2 = MasterRng::new(seed, 0);
        let mut r2 = LocalRng::from_master(&mut m2);

        // Degenerate const samplers must advance RNG 0 times!
        let n_const = NormalSampler::from_xml(mean, "const", None).unwrap();
        let ln_const = LognormalSampler::from_mean_cv(mean, 0.0, None).unwrap();
        let g_const = AnySampler::from_xml_cv(mean, "const", None, None, None).unwrap();
        let b_const = BetaSampler::from_mean_variance(0.4, 0.0).unwrap();
        prop_assert!((n_const.sample(&mut r1) - mean).abs() < 1e-12);
        prop_assert!((ln_const.sample(&mut r1) - mean).abs() < 1e-12);
        prop_assert!((g_const.sample(&mut r1) - mean).abs() < 1e-12);
        prop_assert!((b_const.sample(&mut r1) - 0.4).abs() < 1e-12);
        prop_assert_eq!(r1.uniform_01().to_bits(), r2.uniform_01().to_bits());

        // Stochastic draws remain strictly positive / bounded
        let ln_s = LognormalSampler::from_mean_cv(mean, cv, Some(100.0)).unwrap();
        let v = ln_s.sample(&mut r1);
        prop_assert!(v > 0.0 && v <= 100.0);
        let ns = NormalSample::generate(&mut r1);
        prop_assert!(ln_s.sample_normal(ns) > 0.0);
        let wb = WeibullSampler::new(mean, 1.5);
        prop_assert!(wb.sample(&mut r1) >= 0.0);
    }
}
