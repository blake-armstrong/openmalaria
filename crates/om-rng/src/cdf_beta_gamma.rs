//! Pure-Rust ports of GSL 2.8 incomplete gamma and beta CDFs (`gamma_p`, `beta_p`, `beta_pinv`).
#![allow(clippy::excessive_precision, clippy::inconsistent_digit_grouping)]

use crate::cdf::{ln_gamma, ugaussian_pinv};

/// Regularized lower incomplete gamma CDF (`gsl_cdf_gamma_P(x, a, b)` where `b` is scale).
#[must_use]
pub fn gamma_p(x: f64, a: f64, b: f64) -> f64 {
    if x <= 0.0 {
        return 0.0;
    }
    let y = x / b;
    let gln = ln_gamma(a);
    if y < a + 1.0 {
        let mut ap = a;
        let mut sum = 1.0 / a;
        let mut del = sum;
        for _ in 0..500 {
            ap += 1.0;
            del *= y / ap;
            sum += del;
            if del.abs() < sum.abs() * 1e-16 {
                break;
            }
        }
        sum * (-y + a * y.ln() - gln).exp()
    } else {
        let fpmin = 1e-300_f64;
        let mut b_cf = y + 1.0 - a;
        let mut c = 1.0 / fpmin;
        let mut d = 1.0 / b_cf.max(fpmin);
        let mut h = d;
        for i in 1..=500 {
            let an = -(i as f64) * ((i as f64) - a);
            b_cf += 2.0;
            d = an * d + b_cf;
            if d.abs() < fpmin {
                d = fpmin;
            }
            c = b_cf + an / c;
            if c.abs() < fpmin {
                c = fpmin;
            }
            d = 1.0 / d;
            let del = d * c;
            h *= del;
            if (del - 1.0).abs() < 1e-16 {
                break;
            }
        }
        1.0 - ((-y + a * y.ln() - gln).exp() * h)
    }
}

fn betacf(x: f64, a: f64, b: f64) -> f64 {
    let fpmin = 1e-300_f64;
    let qab = a + b;
    let qap = a + 1.0;
    let qam = a - 1.0;
    let mut c = 1.0;
    let mut d = 1.0 - qab * x / qap;
    if d.abs() < fpmin {
        d = fpmin;
    }
    d = 1.0 / d;
    let mut h = d;
    for m in 1..=500 {
        let m_f = m as f64;
        let m2 = 2.0 * m_f;
        let aa = m_f * (b - m_f) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if d.abs() < fpmin {
            d = fpmin;
        }
        c = 1.0 + aa / c;
        if c.abs() < fpmin {
            c = fpmin;
        }
        d = 1.0 / d;
        h *= d * c;
        let aa = -(a + m_f) * (qab + m_f) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if d.abs() < fpmin {
            d = fpmin;
        }
        c = 1.0 + aa / c;
        if c.abs() < fpmin {
            c = fpmin;
        }
        d = 1.0 / d;
        let del = d * c;
        h *= del;
        if (del - 1.0).abs() < 1e-16 {
            break;
        }
    }
    h
}

/// Regularized incomplete beta `I_x(a, b)` (`gsl_cdf_beta_P`).
#[must_use]
pub fn beta_p(x: f64, a: f64, b: f64) -> f64 {
    if x <= 0.0 {
        return 0.0;
    }
    if x >= 1.0 {
        return 1.0;
    }
    let ln_beta = ln_gamma(a) + ln_gamma(b) - ln_gamma(a + b);
    let bt = (a * x.ln() + b * (1.0 - x).ln() - ln_beta).exp();
    if x < (a + 1.0) / (a + b + 2.0) {
        bt * betacf(x, a, b) / a
    } else {
        1.0 - bt * betacf(1.0 - x, b, a) / b
    }
}

/// Inverse regularized incomplete beta `I^{-1}_p(a, b)` (`gsl_cdf_beta_Pinv`).
#[must_use]
pub fn beta_pinv(p: f64, a: f64, b: f64) -> f64 {
    if p <= 0.0 {
        return 0.0;
    }
    if p >= 1.0 {
        return 1.0;
    }
    if p > 0.5 {
        return 1.0 - beta_pinv_lower(1.0 - p, b, a);
    }
    beta_pinv_lower(p, a, b)
}

fn beta_pinv_lower(p: f64, a: f64, b: f64) -> f64 {
    let ln_beta = ln_gamma(a) + ln_gamma(b) - ln_gamma(a + b);
    let mut lo = 0.0_f64;
    let mut hi = 1.0_f64;

    let mut x = if a >= 1.0 && b >= 1.0 {
        let y = -ugaussian_pinv(p);
        let al = (y * y - 3.0) / 6.0;
        let h = 2.0 / (1.0 / (2.0 * a - 1.0) + 1.0 / (2.0 * b - 1.0));
        let w = (y * (al + h).sqrt() / h)
            - (1.0 / (2.0 * b - 1.0) - 1.0 / (2.0 * a - 1.0)) * (al + 5.0 / 6.0 - 2.0 / (3.0 * h));
        a / (a + b * (2.0 * w).exp())
    } else {
        let lna = (a / (a + b)).ln();
        let lnb = (b / (a + b)).ln();
        let t = (a * lna).exp() / a;
        let u = (b * lnb).exp() / b;
        let w = t + u;
        if p < t / w {
            (a * w * p).powf(1.0 / a)
        } else {
            1.0 - (b * w * (1.0 - p)).powf(1.0 / b)
        }
    }
    .clamp(1e-300, 1.0 - 1e-16);

    for _ in 0..64 {
        let err = beta_p(x, a, b) - p;
        if err < 0.0 {
            lo = lo.max(x);
        } else {
            hi = hi.min(x);
        }
        if err.abs() <= 1e-15 * p.max(1e-30) {
            break;
        }
        let pdf = ((a - 1.0) * x.ln() + (b - 1.0) * (1.0 - x).ln() - ln_beta).exp();
        if pdf <= 0.0 || !pdf.is_finite() {
            x = 0.5 * (lo + hi);
            continue;
        }
        let u = err / pdf;
        let halley = u / (1.0 - 0.5 * u.min(1.0) * ((a - 1.0) / x - (b - 1.0) / (1.0 - x)));
        let next = x - halley;
        x = if next > lo && next < hi {
            next
        } else {
            0.5 * (lo + hi)
        };
        if (hi - lo) <= 1e-15 * x {
            break;
        }
    }
    x
}
