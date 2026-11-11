//! Pure-Rust numerical solvers replacing GSL `nmsimplex`, `qag`, and `secant`/`bisection`.
#![allow(clippy::excessive_precision)]

use crate::error::CoreError;

/// 2D Nelder-Mead simplex minimizer matching `gsl_multimin_fminimizer_nmsimplex` (`multimin/simplex.c`).
pub fn nmsimplex_2d<F>(
    mut f: F,
    initial: [f64; 2],
    step: [f64; 2],
    tol_size: f64,
    max_iters: usize,
) -> Result<[f64; 2], CoreError>
where
    F: FnMut(f64, f64) -> f64,
{
    let mut x = [
        initial,
        [initial[0] + step[0], initial[1]],
        [initial[0], initial[1] + step[1]],
    ];
    let mut y = [f(x[0][0], x[0][1]), f(x[1][0], x[1][1]), f(x[2][0], x[2][1])];

    let simplex_size = |pts: &[[f64; 2]; 3]| -> f64 {
        let cx = (pts[0][0] + pts[1][0] + pts[2][0]) / 3.0;
        let cy = (pts[0][1] + pts[1][1] + pts[2][1]) / 3.0;
        pts.iter()
            .map(|p| (p[0] - cx).hypot(p[1] - cy))
            .sum::<f64>()
            / 3.0
    };

    for _ in 0..max_iters {
        let mut lo = 0;
        let mut hi = 0;
        for i in 1..3 {
            if y[i] < y[lo] {
                lo = i;
            }
            if y[i] > y[hi] {
                hi = i;
            }
        }
        let s_hi = (0..3).find(|&i| i != lo && i != hi).unwrap_or(lo);

        // Centroid of the 2 non-highest points (`n = 2`).
        let mut bar = [0.0; 2];
        for (i, pt) in x.iter().enumerate() {
            if i != hi {
                bar[0] += 0.5 * pt[0];
                bar[1] += 0.5 * pt[1];
            }
        }

        // Reflection: x_r = 2 * bar - x[hi]
        let xr = [2.0 * bar[0] - x[hi][0], 2.0 * bar[1] - x[hi][1]];
        let yr = f(xr[0], xr[1]);
        if !yr.is_finite() {
            return Err(CoreError::Numerical("nmsimplex: non-finite evaluation".into()));
        }

        if yr < y[lo] {
            // Expansion: x_e = 2 * x_r - bar = 3 * bar - 2 * x[hi]
            let xe = [2.0 * xr[0] - bar[0], 2.0 * xr[1] - bar[1]];
            let ye = f(xe[0], xe[1]);
            if ye < yr {
                x[hi] = xe;
                y[hi] = ye;
            } else {
                x[hi] = xr;
                y[hi] = yr;
            }
        } else if yr < y[s_hi] {
            x[hi] = xr;
            y[hi] = yr;
        } else {
            if yr < y[hi] {
                x[hi] = xr;
                y[hi] = yr;
            }
            let xc = [0.5 * (bar[0] + x[hi][0]), 0.5 * (bar[1] + x[hi][1])];
            let yc = f(xc[0], xc[1]);
            if yc < y[hi] {
                x[hi] = xc;
                y[hi] = yc;
            } else {
                let best = x[lo];
                for i in 0..3 {
                    if i != lo {
                        x[i] = [0.5 * (x[i][0] + best[0]), 0.5 * (x[i][1] + best[1])];
                        y[i] = f(x[i][0], x[i][1]);
                    }
                }
            }
        }

        if simplex_size(&x) < tol_size {
            let best_idx = (0..3)
                .min_by(|&a, &b| y[a].total_cmp(&y[b]))
                .unwrap_or(0);
            return Ok(x[best_idx]);
        }
    }

    let best_idx = (0..3)
        .min_by(|&a, &b| y[a].total_cmp(&y[b]))
        .unwrap_or(0);
    Ok(x[best_idx])
}

fn gk15_panel<F: FnMut(f64) -> f64>(f: &mut F, a: f64, b: f64) -> (f64, f64) {
    const XGK: [f64; 8] = [
        0.9914553711208126,
        0.9491079123427585,
        0.8648644233597691,
        0.7415311855993945,
        0.5860872354676911,
        0.4058451513773972,
        0.2077849550078985,
        0.0,
    ];
    const WGK: [f64; 8] = [
        0.02293532201052922,
        0.06309209262997856,
        0.1047900103222502,
        0.1406532597155259,
        0.1690047266392679,
        0.1903505780647854,
        0.2044329400752989,
        0.2094821410847278,
    ];
    const WG: [f64; 4] = [
        0.1294849661688697,
        0.2797053914892767,
        0.3818300505051189,
        0.4179591836734694,
    ];

    let center = 0.5 * (a + b);
    let half = 0.5 * (b - a);
    let f_center = f(center);

    let mut res_gauss = WG[3] * f_center;
    let mut res_kronrod = WGK[7] * f_center;

    for j in 0..7 {
        let absc = half * XGK[j];
        let fsum = f(center - absc) + f(center + absc);
        res_kronrod += WGK[j] * fsum;
        if j % 2 == 1 {
            res_gauss += WG[j / 2] * fsum;
        }
    }
    let result = res_kronrod * half;
    let err = ((res_kronrod - res_gauss) * half).abs();
    (result, err)
}

/// Adaptive Gauss-Kronrod quadrature matching `gsl_integration_qag` (`PK/PD` drug kill factors).
pub fn qag_integrate<F>(
    mut f: F,
    a: f64,
    b: f64,
    eps_abs: f64,
    eps_rel: f64,
    max_subintervals: usize,
) -> Result<f64, CoreError>
where
    F: FnMut(f64) -> f64,
{
    if a == b {
        return Ok(0.0);
    }
    let (r0, e0) = gk15_panel(&mut f, a, b);
    if !r0.is_finite() {
        return Err(CoreError::Numerical(
            "calculateFactor: non-finite integrand in qag".into(),
        ));
    }
    if e0 <= eps_abs.max(eps_rel * r0.abs()) {
        return Ok(r0);
    }

    let mut panels = Vec::with_capacity(max_subintervals.min(64));
    panels.push((a, b, r0, e0));
    let mut total_res = r0;
    let mut total_err = e0;

    while panels.len() < max_subintervals {
        let worst_idx = panels
            .iter()
            .enumerate()
            .max_by(|(_, x), (_, y)| x.3.total_cmp(&y.3))
            .map_or(0, |(idx, _)| idx);
        let (pa, pb, pr, pe) = panels.swap_remove(worst_idx);
        let mid = 0.5 * (pa + pb);
        let (r1, e1) = gk15_panel(&mut f, pa, mid);
        let (r2, e2) = gk15_panel(&mut f, mid, pb);
        if !r1.is_finite() || !r2.is_finite() {
            return Err(CoreError::Numerical(
                "calculateFactor: non-finite integrand in qag".into(),
            ));
        }

        total_res += (r1 + r2) - pr;
        total_err += (e1 + e2) - pe;
        panels.push((pa, mid, r1, e1));
        panels.push((mid, pb, r2, e2));

        if total_err <= eps_abs.max(eps_rel * total_res.abs()) {
            return Ok(total_res);
        }
    }
    Err(CoreError::Numerical(
        "calculateFactor: error from gsl_integration_qag (max subdivisions)".into(),
    ))
}

/// 1D Secant solver matching `gsl_root_fdfsolver_secant` + `gsl_root_test_delta` (`roots/secant.c`).
pub fn secant_root<FDF>(
    mut fdf: FDF,
    x_init: f64,
    eps_abs: f64,
    eps_rel: f64,
    max_iters: usize,
) -> Result<f64, CoreError>
where
    FDF: FnMut(f64) -> (f64, f64),
{
    let mut x = x_init;
    let (mut fx, mut dfx) = fdf(x);
    for _ in 0..max_iters {
        let x_prev = x;
        let f_prev = fx;
        if dfx == 0.0 || !dfx.is_finite() {
            return Err(CoreError::Numerical("secant derivative is zero".into()));
        }
        let x_new = x - fx / dfx;
        let (f_new, _) = fdf(x_new);
        x = x_new;
        fx = f_new;
        if (x - x_prev).abs() <= eps_abs + eps_rel * x.abs() {
            return Ok(x);
        }
        let dx = x - x_prev;
        if dx != 0.0 {
            dfx = (fx - f_prev) / dx;
        }
    }
    Ok(x)
}
