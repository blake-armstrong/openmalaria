//! Age-group piecewise-constant and piecewise-linear interpolators (`model/util/AgeGroupInterpolation.cpp`).

use crate::error::CoreError;
use crate::time::in_years;
use crate::time::SimTime;
use serde::{Deserialize, Serialize};

/// Interpolation mode for age-indexed XML tables (`interpolation="linear"` or `"none"`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum InterpKind {
    /// Piecewise linear between group midpoints, clamped flat at `0` and `max_human_age` (default).
    #[default]
    Linear,
    /// Discontinuous step function (`interpolation="none"`).
    PiecewiseConstant,
}

impl InterpKind {
    /// Parses an optional XML `interpolation` attribute (`None` or `"linear"` -> `Linear`, `"none"` -> `PiecewiseConstant`).
    pub fn from_xml(raw: Option<&str>) -> Result<Self, CoreError> {
        match raw {
            None | Some("linear") => Ok(Self::Linear),
            Some("none") => Ok(Self::PiecewiseConstant),
            Some(other) => Err(CoreError::Scenario(format!(
                "age group interpolation {other} not implemented"
            ))),
        }
    }
}

/// Cache-friendly contiguous age-group interpolator replacing heap-allocated `std::map<double, double>`.
///
/// # Examples
/// ```
/// use om_core::{AgeGroupInterpolator, InterpKind};
///
/// let interp = AgeGroupInterpolator::from_groups(
///     &[(0.0, 2.0), (5.0, 10.0)],
///     InterpKind::PiecewiseConstant,
///     32850,
///     "weight",
/// ).unwrap();
/// assert_eq!(interp.eval(3.0), 2.0);
/// assert_eq!(interp.eval(5.0), 10.0);
/// ```
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AgeGroupInterpolator {
    kind: InterpKind,
    knots: Vec<(f64, f64)>,
}

impl AgeGroupInterpolator {
    /// Builds an `AgeGroupInterpolator` from `(lowerbound_years, value)` pairs and `max_human_age` (in days).
    pub fn from_groups(
        groups: &[(f64, f64)],
        kind: InterpKind,
        max_human_age: SimTime,
        elt_name: &str,
    ) -> Result<Self, CoreError> {
        if groups.is_empty() {
            return Err(CoreError::Scenario(format!(
                "{elt_name}: at least one age group required"
            )));
        }
        if groups[0].0 != 0.0 {
            return Err(CoreError::Scenario(format!(
                "{elt_name}: first lower-bound must be 0"
            )));
        }

        let mut knots = Vec::with_capacity(groups.len() + 3);
        match kind {
            InterpKind::PiecewiseConstant => {
                let mut greatest = -1.0_f64;
                for &(lbound, val) in groups {
                    if lbound < greatest {
                        return Err(CoreError::Scenario(format!(
                            "{elt_name}: lower bound {lbound} not greater than previous {greatest}"
                        )));
                    }
                    if knots.last().is_none_or(|&(k, _)| k < lbound) {
                        knots.push((lbound, val));
                    }
                    greatest = lbound;
                }
            }
            InterpKind::Linear => {
                let mut greatest = 0.0_f64;
                let mut last_val = groups[0].1;
                for &(lbound, val) in groups {
                    if lbound < greatest {
                        return Err(CoreError::Scenario(format!(
                            "{elt_name}: lower bound {lbound} less than previous {greatest}"
                        )));
                    }
                    let mid = 0.5 * (greatest + lbound);
                    if knots.last().is_none_or(|&(k, _)| k < mid) {
                        knots.push((mid, last_val));
                    }
                    greatest = lbound;
                    last_val = val;
                }
                let end_mid = 0.5 * (greatest + in_years(max_human_age));
                if let Some(last) = knots.last_mut()
                    && (last.0 - end_mid).abs() == 0.0
                {
                    last.1 = last_val;
                } else {
                    knots.push((end_mid, last_val));
                }
                let first_val = knots[0].1;
                if knots[0].0 == 0.0 {
                    knots[0].1 = first_val;
                } else {
                    knots.insert(0, (0.0, first_val));
                }
                let tail_val = knots.last().map_or(last_val, |&(_, v)| v);
                knots.push((f64::INFINITY, tail_val));
            }
        }
        Ok(Self { kind, knots })
    }

    /// Evaluates the interpolated value at `age_years >= 0.0`.
    #[inline]
    #[must_use]
    pub fn eval(&self, age_years: f64) -> f64 {
        debug_assert!(age_years >= 0.0);
        // `upper` is the first index with `knot_age > age_years` (`std::map::upper_bound`).
        let upper = self.knots.partition_point(|&(a, _)| a <= age_years);
        match self.kind {
            InterpKind::PiecewiseConstant => {
                let idx = upper.saturating_sub(1);
                self.knots[idx].1
            }
            InterpKind::Linear => {
                let idx1 = upper.min(self.knots.len() - 1);
                let idx0 = idx1.saturating_sub(1);
                let (a0, f0) = self.knots[idx0];
                let (a1, f1) = self.knots[idx1];
                if !a1.is_finite() || a1 == a0 {
                    f0
                } else {
                    (age_years - a0) / (a1 - a0) * (f1 - f0) + f0
                }
            }
        }
    }

    /// Scales all knot values in place by `factor`.
    pub fn scale(&mut self, factor: f64) {
        for (_, v) in &mut self.knots {
            *v *= factor;
        }
    }

    /// Returns the age knot at which the maximum value is first attained (`firstGlobalMaximum`).
    #[must_use]
    pub fn first_global_maximum(&self) -> f64 {
        let mut best_age = 0.0;
        let mut best_val = f64::MIN_POSITIVE;
        for &(age, val) in &self.knots {
            if val > best_val {
                best_val = val;
                best_age = age;
            }
        }
        best_age
    }
}
