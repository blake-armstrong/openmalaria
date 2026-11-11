//! Pure-Rust ports of GSL 2.8 continuous variates (`gaussian`, `lognormal`, `beta`, `weibull`) matching `model/util/random.h`.

use crate::RngCore;
use crate::cdf::ugaussian_pinv;
use crate::ziggurat::gamma_mt;

/// Exact port of `gsl_ran_gaussian` (polar Box-Muller using `uniform_pos` and the second coordinate `y`).
#[inline]
pub(crate) fn gaussian_polar<G: RngCore>(rng: &mut G, sigma: f64) -> f64 {
    let (y, r2) = loop {
        let x = -1.0 + 2.0 * rng.uniform_pos();
        let y = -1.0 + 2.0 * rng.uniform_pos();
        let r2 = x * x + y * y;
        if r2 <= 1.0 && r2 != 0.0 {
            break (y, r2);
        }
    };
    sigma * y * (-2.0 * r2.ln() / r2).sqrt()
}

/// Exact port of `gsl_ran_lognormal` (polar Box-Muller using `gen_double` and the first coordinate `u`).
#[inline]
pub(crate) fn lognormal_polar<G: RngCore>(rng: &mut G, zeta: f64, sigma: f64) -> f64 {
    let (u, r2) = loop {
        let u = -1.0 + 2.0 * rng.gen_double();
        let v = -1.0 + 2.0 * rng.gen_double();
        let r2 = u * u + v * v;
        if r2 <= 1.0 && r2 != 0.0 {
            break (u, r2);
        }
    };
    let normal = u * (-2.0 * r2.ln() / r2).sqrt();
    (sigma * normal + zeta).exp()
}

/// Exact port of OpenMalaria's `RNG::max_multi_log_normal`.
#[inline]
pub(crate) fn max_multi_log_normal<G: RngCore>(
    rng: &mut G,
    start: f64,
    n: i32,
    meanlog: f64,
    stdlog: f64,
) -> f64 {
    let normp = rng.gen_double().powf(1.0 / f64::from(n));
    let zval = ugaussian_pinv(normp);
    let multi_sample = (meanlog + stdlog * zval).exp();
    start.max(multi_sample)
}

/// Exact port of `gsl_ran_beta` (`a <= 1 && b <= 1` Johnk's method; else ratio of two `gsl_ran_gamma`).
#[inline]
pub(crate) fn beta_gsl<G: RngCore>(rng: &mut G, a: f64, b: f64) -> f64 {
    if a <= 1.0 && b <= 1.0 {
        loop {
            let u = rng.uniform_pos();
            let v = rng.uniform_pos();
            let x = u.powf(1.0 / a);
            let y = v.powf(1.0 / b);
            if x + y <= 1.0 {
                if x + y > 0.0 {
                    return x / (x + y);
                }
                let mut logx = u.ln() / a;
                let mut logy = v.ln() / b;
                let logm = logx.max(logy);
                logx -= logm;
                logy -= logm;
                return (logx - (logx.exp() + logy.exp()).ln()).exp();
            }
        }
    } else {
        let x1 = gamma_mt(rng, a, 1.0);
        let x2 = gamma_mt(rng, b, 1.0);
        x1 / (x1 + x2)
    }
}

/// Exact port of `gsl_ran_weibull(r, a, b)` where `a = lambda` (scale) and `b = k` (shape).
#[inline]
pub(crate) fn weibull_gsl<G: RngCore>(rng: &mut G, lambda: f64, k: f64) -> f64 {
    let x = rng.uniform_pos();
    lambda * (-x.ln()).powf(1.0 / k)
}
