//! Pure-Rust port of GSL 2.8 `gsl_ran_binomial` (BTPE) and `gsl_ran_poisson` (`randist/binomial_tpe.c`, `poisson.c`).

use crate::RngCore;
use crate::ziggurat::gamma_int;

const SMALL_MEAN: f64 = 14.0;
const BINV_CUTOFF: i32 = 110;
const FAR_FROM_MEAN: i32 = 20;

#[inline]
fn gsl_pow_uint(mut x: f64, mut n: u32) -> f64 {
    let mut value = 1.0;
    while n != 0 {
        if (n & 1) != 0 {
            value *= x;
        }
        n >>= 1;
        if n != 0 {
            x *= x;
        }
    }
    value
}

#[inline]
fn stirling(y1: f64) -> f64 {
    let y2 = y1 * y1;
    (13860.0 - (462.0 - (132.0 - (99.0 - 140.0 / y2) / y2) / y2) / y2) / y1 / 166320.0
}

/// Exact port of `gsl_ran_binomial` / `gsl_ran_binomial_tpe` (Kachitvichyanukul & Schmeiser 1988).
pub(crate) fn binomial_tpe<G: RngCore>(rng: &mut G, mut p: f64, n: u32) -> u32 {
    if n == 0 {
        return 0;
    }

    let flipped = if p > 0.5 {
        p = 1.0 - p;
        true
    } else {
        false
    };

    let q = 1.0 - p;
    let s = p / q;
    let np = f64::from(n) * p;

    let ix: i32 = if np < SMALL_MEAN {
        let f0 = gsl_pow_uint(q, n);
        'outer: loop {
            let mut f = f0;
            let mut u = rng.gen_double();
            for i in 0..=BINV_CUTOFF {
                if u < f {
                    break 'outer i;
                }
                u -= f;
                f *= s * f64::from(n as i32 - i) / f64::from(i + 1);
            }
        }
    } else {
        let ffm = np + p;
        let m = ffm as i32;
        let fm = f64::from(m);
        let xm = fm + 0.5;
        let npq = np * q;

        let p1 = (2.195 * npq.sqrt() - 4.6 * q).floor() + 0.5;
        let xl = xm - p1;
        let xr = xm + p1;

        let c = 0.134 + 20.5 / (15.3 + fm);
        let al = (ffm - xl) / (ffm - xl * p);
        let xll = al * (1.0 + 0.5 * al);
        let ar = (xr - ffm) / (xr * q);
        let xlr = ar * (1.0 + 0.5 * ar);
        let p2 = p1 * (1.0 + c + c);
        let p3 = p2 + c / xll;
        let p4 = p3 + c / xlr;

        loop {
            let u = rng.gen_double() * p4;
            let mut v = rng.gen_double();

            let cand = if u <= p1 {
                (xm - p1 * v + u) as i32
            } else if u <= p2 {
                let x = xl + (u - p1) / c;
                v = v * c + 1.0 - (x - xm).abs() / p1;
                if v > 1.0 || v <= 0.0 {
                    continue;
                }
                x as i32
            } else if u <= p3 {
                let cand = (xl + v.ln() / xll) as i32;
                if cand < 0 {
                    continue;
                }
                v *= (u - p2) * xll;
                cand
            } else {
                let cand = (xr - v.ln() / xlr) as i32;
                if cand > n as i32 {
                    continue;
                }
                v *= (u - p3) * xlr;
                cand
            };

            let k = (cand - m).abs();
            if k <= FAR_FROM_MEAN {
                let g = (f64::from(n) + 1.0) * s;
                let mut f = 1.0;
                if m < cand {
                    for i in (m + 1)..=cand {
                        f *= g / f64::from(i) - s;
                    }
                } else if m > cand {
                    for i in (cand + 1)..=m {
                        v *= g / f64::from(i) - s;
                    }
                }
                if v <= f {
                    break cand;
                }
            } else {
                v = v.ln();
                if f64::from(k) < npq / 2.0 - 1.0 {
                    let amaxp =
                        (f64::from(k) / npq) * ((f64::from(k) * (f64::from(k) / 3.0 + 0.625) + 1.0 / 6.0) / npq + 0.5);
                    let ynorm = -f64::from(k * k) / (2.0 * npq);
                    if v < ynorm - amaxp {
                        break cand;
                    }
                    if v > ynorm + amaxp {
                        continue;
                    }
                }

                let x1 = f64::from(cand + 1);
                let w1 = f64::from(n as i32 - cand + 1);
                let f1 = fm + 1.0;
                let z1 = f64::from(n as i32 + 1 - m);

                let accept = xm * (f1 / x1).ln()
                    + (f64::from(n) - fm + 0.5) * (z1 / w1).ln()
                    + f64::from(cand - m) * (w1 * p / (x1 * q)).ln()
                    + stirling(f1)
                    + stirling(z1)
                    - stirling(x1)
                    - stirling(w1);

                if v <= accept {
                    break cand;
                }
            }
        }
    };

    if flipped {
        n - (ix as u32)
    } else {
        ix as u32
    }
}

/// Exact port of `gsl_ran_poisson` (`mu > 10` gamma-binomial reduction + Knuth tail).
pub(crate) fn poisson_gsl<G: RngCore>(rng: &mut G, mut mu: f64) -> u32 {
    let mut k: u32 = 0;

    while mu > 10.0 {
        let m = (mu * 0.875) as u32;
        let x = gamma_int(rng, m);

        if x >= mu {
            return k + binomial_tpe(rng, mu / x, m - 1);
        }
        k += m;
        mu -= x;
    }

    let emu = (-mu).exp();
    let mut prod = 1.0;
    loop {
        prod *= rng.gen_double();
        if prod <= emu {
            return k;
        }
        k += 1;
    }
}
