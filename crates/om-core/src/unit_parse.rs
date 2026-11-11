//! Pure-Rust port of `OM::UnitParse` duration and `YYYY-MM-DD` date parsing (`model/util/UnitParse.cpp`).

use crate::error::CoreError;
use crate::time::{DAYS_IN_YEAR, SIM_NEVER, SimClock, SimTime};

/// Default unit policy when an XML duration string omits its trailing unit suffix.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DefaultUnit {
    /// Unit suffix is mandatory (except for literal `0`).
    None,
    /// Assume days (`d`).
    Days,
    /// Assume 365-day years (`y`).
    Years,
    /// Assume simulation time steps (`t`).
    Steps,
}

/// Days per calendar month (leap years are not simulated in OpenMalaria).
pub const MONTH_LEN: [i32; 12] = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];

/// Cumulative day offsets at the start of each calendar month (`0..=365`).
pub const MONTH_START: [i32; 13] = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365];

fn cast_to_i32_exact(x: f64) -> Result<i32, CoreError> {
    let y = x as i32;
    if f64::from(y) != x {
        return Err(CoreError::Format("underflow/overflow".into()));
    }
    Ok(y)
}

fn split_number_and_unit(raw: &str) -> (&str, Option<char>) {
    let s = raw.trim();
    if let Some((prefix, ch)) = s.char_indices().next_back()
        && ch.is_ascii_alphabetic()
    {
        return (&s[..prefix], Some(ch));
    }
    (s, None)
}

/// Parses an integer duration with `d`/`t` suffix, rounding to the nearest time step (`UnitParse::readShortDuration`).
pub fn read_short_duration(
    clock: &SimClock,
    raw: &str,
    def_unit: DefaultUnit,
) -> Result<SimTime, CoreError> {
    let (num_str, unit_ch) = split_number_and_unit(raw);
    let v: i64 = num_str
        .parse()
        .map_err(|_| CoreError::Format(format!("bad format: '{raw}' (try e.g. 1 or 2d or 3t)")))?;

    match unit_ch {
        None => {
            if v == 0 {
                Ok(0)
            } else {
                match def_unit {
                    DefaultUnit::None => Err(CoreError::Format(
                        "unit required but not given (try e.g. 5d or 12t)".into(),
                    )),
                    DefaultUnit::Days => Ok(clock.round_to_ts_from_days(v as f64)),
                    DefaultUnit::Steps => {
                        let ts = i32::try_from(v)
                            .map_err(|_| CoreError::Format("underflow/overflow".into()))?;
                        Ok(clock.from_ts(ts))
                    }
                    DefaultUnit::Years => Err(CoreError::Format(format!(
                        "bad format: '{raw}' (short duration does not accept years)"
                    ))),
                }
            }
        }
        Some('d' | 'D') => Ok(clock.round_to_ts_from_days(v as f64)),
        Some('t' | 'T') => {
            let ts =
                i32::try_from(v).map_err(|_| CoreError::Format("underflow/overflow".into()))?;
            Ok(clock.from_ts(ts))
        }
        Some(_) => Err(CoreError::Format(format!(
            "bad format: '{raw}' (try e.g. 1 or 2d or 3t)"
        ))),
    }
}

fn parse_duration_and_unit(raw: &str, mut unit: DefaultUnit) -> Result<(f64, DefaultUnit), CoreError> {
    let (num_str, unit_ch) = split_number_and_unit(raw);
    if num_str.is_empty() {
        return Err(CoreError::Format(format!(
            "bad format: '{raw}' (try e.g. 1 or 2d or 3t or 4y)"
        )));
    }
    let v: f64 = num_str.parse().map_err(|_| {
        CoreError::Format(format!(
            "bad format: '{raw}' (try e.g. 1 or 2d or 3t or 4y)"
        ))
    })?;
    if !v.is_finite() {
        return Err(CoreError::Format(format!("non-finite duration: '{raw}'")));
    }

    match unit_ch {
        Some('y' | 'Y') => unit = DefaultUnit::Years,
        Some('d' | 'D') => unit = DefaultUnit::Days,
        Some('t' | 'T') => unit = DefaultUnit::Steps,
        Some(_) => {
            return Err(CoreError::Format(format!(
                "unknown unit: '{raw}' (try e.g. 1 or 2d or 3t or 4y)"
            )));
        }
        None => {
            if v == 0.0 {
                unit = DefaultUnit::Days;
            } else if unit == DefaultUnit::None {
                return Err(CoreError::Format(
                    "unit required but not given (try e.g. 5d or 12t or 2.3y)".into(),
                ));
            }
        }
    }
    Ok((v, unit))
}

/// Parses a duration (`d`, `t`, or fractional `y`) rounded to time steps (`UnitParse::readDuration`).
pub fn read_duration(
    clock: &SimClock,
    raw: &str,
    def_unit: DefaultUnit,
) -> Result<SimTime, CoreError> {
    let (v, unit) = parse_duration_and_unit(raw, def_unit)?;
    match unit {
        DefaultUnit::Years => Ok(clock.from_years_n(v)),
        DefaultUnit::Days => {
            if v != v.floor() {
                return Err(CoreError::Format(
                    "fractional values are only allowed when the unit is years (e.g. 0.25y)".into(),
                ));
            }
            Ok(clock.round_to_ts_from_days(v))
        }
        DefaultUnit::Steps => {
            if v != v.floor() {
                return Err(CoreError::Format(
                    "fractional values are only allowed when the unit is years (e.g. 0.25y)".into(),
                ));
            }
            Ok(clock.from_ts(cast_to_i32_exact(v)?))
        }
        DefaultUnit::None => unreachable!(),
    }
}

/// Parses a duration into exact unrounded days (`UnitParse::durationToDays`).
///
/// When `clock` is `None` (e.g. parsing `L` in decay functions with `y` or `d`), `Steps` requires `Some(clock)`.
pub fn duration_to_days(
    clock: Option<&SimClock>,
    raw: &str,
    def_unit: DefaultUnit,
) -> Result<f64, CoreError> {
    let (v, unit) = parse_duration_and_unit(raw, def_unit)?;
    match unit {
        DefaultUnit::Years => Ok(v * f64::from(DAYS_IN_YEAR)),
        DefaultUnit::Days => Ok(v),
        DefaultUnit::Steps => {
            let clk = clock.ok_or_else(|| {
                CoreError::Format("step-based duration requires initialized SimClock".into())
            })?;
            Ok(v * f64::from(clk.one_ts()))
        }
        DefaultUnit::None => unreachable!(),
    }
}

/// Parses `YYYY-MM-DD` to `SimTime`, returning `Ok(SIM_NEVER)` if `raw` does not match date syntax (`UnitParse::parseDate`).
pub fn parse_date(clock: &SimClock, raw: &str) -> Result<SimTime, CoreError> {
    let s = raw.trim();
    let mut parts = s.split('-');
    let (Some(y_str), Some(m_str), Some(d_str), None) =
        (parts.next(), parts.next(), parts.next(), parts.next())
    else {
        return Ok(SIM_NEVER);
    };

    if y_str.len() != 4
        || !(1..=2).contains(&m_str.len())
        || !(1..=2).contains(&d_str.len())
        || !y_str.bytes().all(|b| b.is_ascii_digit())
        || !m_str.bytes().all(|b| b.is_ascii_digit())
        || !d_str.bytes().all(|b| b.is_ascii_digit())
    {
        return Ok(SIM_NEVER);
    }

    let year: i32 = y_str.parse().unwrap_or(-1);
    let month: i32 = m_str.parse().unwrap_or(-1);
    let day: i32 = d_str.parse().unwrap_or(-1);

    if !(1..=12).contains(&month) || day < 1 || day > MONTH_LEN[(month - 1) as usize] {
        return Err(CoreError::Format(format!(
            "{s} does not look like a date (expected YYYY-MM-DD with 1<=MM<=12 and 1<=DD<=days_in_month)"
        )));
    }

    Ok(SimClock::from_years_i(year)
        + clock.round_to_ts_from_days(f64::from(MONTH_START[(month - 1) as usize] + day - 1)))
}

/// Parses either a `YYYY-MM-DD` calendar date or a duration offset from `clock.start_date()` (`UnitParse::readDate`).
pub fn read_date(
    clock: &SimClock,
    raw: &str,
    def_unit: DefaultUnit,
) -> Result<SimTime, CoreError> {
    let date = parse_date(clock, raw)?;
    if date != SIM_NEVER {
        if date < clock.start_date() {
            return Err(CoreError::Format(
                "date of event is before the start of monitoring".into(),
            ));
        }
        Ok(date)
    } else {
        Ok(clock.start_date() + read_duration(clock, raw, def_unit)?)
    }
}
