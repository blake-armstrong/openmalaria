//! Discrete log-Fourier transforms and vector comparison helpers (`model/util/vectors.h`).

use crate::error::CoreError;

/// Returns `true` when `|a - b| <= max(|a|, |b|) * lim_fact` (`OM::util::vectors::approxEqual`).
#[inline]
#[must_use]
pub fn approx_equal(a: f64, b: f64, lim_fact: f64) -> bool {
    (a - b).abs() <= a.abs().max(b.abs()) * lim_fact
}

/// Returns `true` when `xs` and `ys` have equal length and all elements satisfy `approx_equal`.
#[must_use]
pub fn approx_equal_slice(xs: &[f64], ys: &[f64], lim_fact: f64) -> bool {
    xs.len() == ys.len()
        && xs
            .iter()
            .zip(ys)
            .all(|(&a, &b)| approx_equal(a, b, lim_fact))
}

/// Discrete Fourier Transform of `ln(i_array[t])` (`OM::util::vectors::logDFT`).
///
/// `fc` must have odd length `2 * N - 1` with `fc.len() <= 2 * i_array.len()`.
pub fn log_dft(i_array: &[f64], fc: &mut [f64]) -> Result<(), CoreError> {
    if fc.len() > 2 * i_array.len() || fc.len().is_multiple_of(2) {
        return Err(CoreError::Numerical(
            "Require DFT fc.len() to be 2*n - 1 for n <= i_array.len()".into(),
        ));
    }
    let t_len = i_array.len();
    let n_harm = fc.len().div_ceil(2);
    let w = 2.0 * std::f64::consts::PI / (t_len as f64);

    fc.fill(0.0);
    for (t, &inp) in i_array.iter().enumerate() {
        let val = inp.ln();
        fc[0] += val;
        for n in 1..n_harm {
            let angle = w * (n as f64) * (t as f64);
            fc[2 * n - 1] += val * angle.cos();
            fc[2 * n] += val * angle.sin();
        }
    }
    let inv_t = 1.0 / (t_len as f64);
    for coeff in fc {
        *coeff *= inv_t;
    }
    Ok(())
}

/// Computes Fourier series coefficients of `ln(i_array)` (`logDFT` with `fc[1..] *= 2.0`).
pub fn log_fourier_coefficients(i_array: &[f64], fc: &mut [f64]) -> Result<(), CoreError> {
    log_dft(i_array, fc)?;
    for coeff in &mut fc[1..] {
        *coeff *= 2.0;
    }
    Ok(())
}

/// Evaluates the exponentiated inverse Fourier series rotated by `r_angle` radians (`OM::util::vectors::expIDFT`).
pub fn exp_idft(t_array: &mut [f64], fc: &[f64], r_angle: f64) -> Result<(), CoreError> {
    if fc.len().is_multiple_of(2) {
        return Err(CoreError::Numerical(
            "The number of Fourier coefficients should be odd.".into(),
        ));
    }
    let t2 = t_array.len();
    let n_harm = fc.len().div_ceil(2);
    let w = 2.0 * std::f64::consts::PI / (t2 as f64);

    for (t, out) in t_array.iter_mut().enumerate() {
        let mut temp = fc[0];
        let wt = w * (t as f64) - r_angle;
        for n in 1..n_harm {
            let n_wt = (n as f64) * wt;
            temp += fc[2 * n - 1] * n_wt.cos() + fc[2 * n] * n_wt.sin();
        }
        *out = temp.exp();
    }
    Ok(())
}
