//! Pure-Rust port of GSL 2.8 Voss 128-step Gaussian Ziggurat and Marsaglia-Tsang / Ahrens-Dieter Gamma samplers.

use crate::RngCore;
use crate::ziggurat_tables::{ZIG_KTAB, ZIG_WTAB, ZIG_YTAB};

const ZIG_R: f64 = 3.444_286_476_76;
const NEG_ZIG_R: f64 = -3.444_286_476_76;
const NEG_HALF_ZIG_R: f64 = -1.722_143_238_38;

/// Exact port of `gsl_ran_gaussian_ziggurat` (Voss 128-step Ziggurat) for `OM_RNG` (`[0, u32::MAX]`).
#[inline]
pub(crate) fn gaussian_ziggurat<G: RngCore>(rng: &mut G, sigma: f64) -> f64 {
    loop {
        let o = u64::from(rng.gen_u32());
        let j = (o >> 8) & 0x00ff_ffff;
        let i = (o & 0x7f) as usize;
        let mut x = (j as f64) * ZIG_WTAB[i];

        if j < ZIG_KTAB[i] {
            let signed_sigma = if (o & 0x80) == 0 { -sigma } else { sigma };
            return signed_sigma * x;
        }

        let y = if i == 127 {
            let u0 = 1.0 - rng.gen_double();
            let u1 = rng.gen_double();
            x = (u0.ln() / NEG_ZIG_R) + ZIG_R;
            u1 * ((x + NEG_HALF_ZIG_R) * NEG_ZIG_R).exp()
        } else {
            let y0 = ZIG_YTAB[i];
            let y1 = ZIG_YTAB[i + 1];
            y1 + (y0 - y1) * rng.gen_double()
        };

        if y < ((x * -0.5) * x).exp() {
            let signed_sigma = if (o & 0x80) == 0 { -sigma } else { sigma };
            return signed_sigma * x;
        }
    }
}

/// Exact port of `gsl_ran_gamma` (Marsaglia & Tsang 2000 for `a >= 1`, boost reduction for `a < 1`).
#[inline]
pub(crate) fn gamma_mt<G: RngCore>(rng: &mut G, a: f64, b: f64) -> f64 {
    if a < 1.0 {
        let u = rng.uniform_pos();
        return gamma_mt(rng, 1.0 + a, b) * u.powf(1.0 / a);
    }

    let d = a - 1.0 / 3.0;
    let c = (1.0 / 3.0) / d.sqrt();

    loop {
        let (x, mut v) = loop {
            let x = gaussian_ziggurat(rng, 1.0);
            let v = 1.0 + c * x;
            if v > 0.0 {
                break (x, v);
            }
        };

        v = v * v * v;
        let u = rng.uniform_pos();
        let x2 = x * x;

        if u < 1.0 - 0.0331 * x2 * x2 {
            return b * d * v;
        }

        if u.ln() < 0.5 * x2 + d * (1.0 - v + v.ln()) {
            return b * d * v;
        }
    }
}

/// Exact port of GSL `gamma_large` (`randist/gamma.c` Cauchy-tangent rejection, used by `gsl_ran_gamma_int`).
pub(crate) fn gamma_large<G: RngCore>(rng: &mut G, a: f64) -> f64 {
    let sqa = (2.0 * a - 1.0).sqrt();
    let a_minus_1 = a - 1.0;

    loop {
        let (x, y) = loop {
            let y = (std::f64::consts::PI * rng.gen_double()).tan();
            let x = sqa * y + a_minus_1;
            if x > 0.0 {
                break (x, y);
            }
        };

        let v = rng.gen_double();
        let t = (1.0 + y * y) * (a_minus_1 * (x / a_minus_1).ln() - sqa * y).exp();
        if v <= t {
            return x;
        }
    }
}

/// Exact port of `gsl_ran_gamma_int` (`a < 12` product of `uniform_pos`, `a >= 12` `gamma_large`).
pub(crate) fn gamma_int<G: RngCore>(rng: &mut G, a: u32) -> f64 {
    if a < 12 {
        let mut prod = 1.0;
        for _ in 0..a {
            prod *= rng.uniform_pos();
        }
        -prod.ln()
    } else {
        gamma_large(rng, f64::from(a))
    }
}
