//! Instance-owned simulation time (`SimTime`) and clock (`SimClock`), replacing static `OM::sim`.

use crate::error::CoreError;
use serde::{Deserialize, Serialize};

/// Simulation time measured in whole days (`typedef int SimTime` in OpenMalaria >= 43.1).
pub type SimTime = i32;

/// Number of days in an OpenMalaria year (leap years are not simulated).
pub const DAYS_IN_YEAR: SimTime = 365;

/// Sentinel time always strictly in the past (`-0x3FFF_FFFF`).
pub const SIM_NEVER: SimTime = -0x3fff_ffff;

/// Sentinel time always strictly in the future (`0x3FFF_FFFF`).
pub const SIM_FUTURE: SimTime = 0x3fff_ffff;

/// Non-negative integer modulo (`OM::util::mod_nn`), matching C++ assertions `a >= 0 && b > 0`.
#[inline]
#[must_use]
pub const fn mod_nn(a: i32, b: i32) -> i32 {
    debug_assert!(a >= 0 && b > 0);
    a % b
}

/// Rounds `x` toward `-infinity` as an `i32` (`OM::floorToInt`).
#[inline]
#[must_use]
pub fn floor_to_int(x: f64) -> i32 {
    x.floor() as i32
}

/// Converts `SimTime` (days) to fractional years (`d / 365.0`).
#[inline]
#[must_use]
pub fn in_years(d: SimTime) -> f64 {
    f64::from(d) * (1.0 / f64::from(DAYS_IN_YEAR))
}

/// Instance-owned simulation clock replacing all process-wide static fields in `OM::sim` and `SimData`.
///
/// # Examples
/// ```
/// use om_core::SimClock;
///
/// let mut clock = SimClock::new(5, 90.0).unwrap();
/// assert_eq!(clock.one_ts(), 5);
/// assert_eq!(clock.steps_per_year(), 73);
/// clock.reset_for_warmup();
/// clock.start_update();
/// assert_eq!((clock.ts0(), clock.ts1()), (0, 5));
/// clock.end_update();
/// assert_eq!(clock.now(), 5);
/// ```
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SimClock {
    interval: SimTime,
    steps_per_year: usize,
    years_per_step: f64,
    max_human_age: SimTime,
    start_date: SimTime,
    end_date: SimTime,
    t0: SimTime,
    t1: SimTime,
    interv: SimTime,
    in_update: bool,
}

impl SimClock {
    /// Creates a new `SimClock` for a `1`-day or `5`-day time step and maximum human age in years.
    ///
    /// Fixes legacy C++ Latent Bug #6 where missing `interval` left `SimData::interval == 0`
    /// and triggered integer division-by-zero UB in `365 / SimData::interval`.
    pub fn new(interval: SimTime, max_age_years: f64) -> Result<Self, CoreError> {
        if interval != 1 && interval != 5 {
            return Err(CoreError::Scenario(format!(
                "Time step set to {interval} days but only 1 and 5 days are supported."
            )));
        }
        let steps_per_year = (DAYS_IN_YEAR / interval) as usize;
        let years_per_step = 1.0 / (steps_per_year as f64);
        let max_human_age = interval * floor_to_int((steps_per_year as f64) * max_age_years);
        Ok(Self {
            interval,
            steps_per_year,
            years_per_step,
            max_human_age,
            start_date: 0,
            end_date: SIM_NEVER,
            t0: 0,
            t1: 0,
            interv: SIM_NEVER,
            in_update: false,
        })
    }

    /// Length of one time step in days (`1` or `5`).
    #[inline]
    #[must_use]
    pub const fn one_ts(&self) -> SimTime {
        self.interval
    }

    /// Number of time steps per 365-day year (`365` or `73`).
    #[inline]
    #[must_use]
    pub const fn steps_per_year(&self) -> usize {
        self.steps_per_year
    }

    /// Cached `1.0 / steps_per_year`.
    #[inline]
    #[must_use]
    pub const fn years_per_step(&self) -> f64 {
        self.years_per_step
    }

    /// Maximum simulated human age in days (rounded down to time steps).
    #[inline]
    #[must_use]
    pub const fn max_human_age(&self) -> SimTime {
        self.max_human_age
    }

    /// Monitoring start date (`0` when `monitoring/startDate` is omitted).
    #[inline]
    #[must_use]
    pub const fn start_date(&self) -> SimTime {
        self.start_date
    }

    /// Sets the monitoring start date.
    pub fn set_start_date(&mut self, start_date: SimTime) {
        self.start_date = start_date;
    }

    /// Simulation end date (relative to `start_date` or calendar origin).
    #[inline]
    #[must_use]
    pub const fn end_date(&self) -> SimTime {
        self.end_date
    }

    /// Sets the simulation end date.
    pub fn set_end_date(&mut self, end_date: SimTime) {
        self.end_date = end_date;
    }

    /// Converts days to time steps (truncating integer division `d / interval`).
    #[inline]
    #[must_use]
    pub const fn in_steps(&self, d: SimTime) -> i32 {
        d / self.interval
    }

    /// Converts time steps to days (`ts * interval`).
    #[inline]
    #[must_use]
    pub const fn from_ts(&self, ts: i32) -> SimTime {
        ts * self.interval
    }

    /// Converts whole years to days (`365 * years`).
    #[inline]
    #[must_use]
    pub const fn from_years_i(years: i32) -> SimTime {
        DAYS_IN_YEAR * years
    }

    /// Rounds fractional days to the nearest time step (`interval * floor(days / interval + 0.5)`).
    #[inline]
    #[must_use]
    pub fn round_to_ts_from_days(&self, days: f64) -> SimTime {
        self.from_ts(floor_to_int(days / f64::from(self.interval) + 0.5))
    }

    /// Converts fractional years to the nearest time step (`roundToTSFromDays(365 * years)`).
    #[inline]
    #[must_use]
    pub fn from_years_n(&self, years: f64) -> SimTime {
        self.round_to_ts_from_days(f64::from(DAYS_IN_YEAR) * years)
    }

    /// Converts fractional years to days rounded down to a time step.
    #[inline]
    #[must_use]
    pub fn from_years_d(&self, years: f64) -> SimTime {
        self.from_ts(floor_to_int((self.steps_per_year as f64) * years))
    }

    /// Returns `(d / interval) % denominator` for non-negative `d` and positive `denominator`.
    #[inline]
    #[must_use]
    pub const fn modulo_steps(&self, d: SimTime, denominator: i32) -> i32 {
        mod_nn(d / self.interval, denominator)
    }

    /// Returns `(d / interval) % steps_per_year`.
    #[inline]
    #[must_use]
    pub const fn modulo_year_steps(&self, d: SimTime) -> usize {
        mod_nn(d / self.interval, self.steps_per_year as i32) as usize
    }

    /// Resets `t0 = 0`, `t1 = 0`, `interv = SIM_NEVER` at the start of the one-life warmup.
    pub fn reset_for_warmup(&mut self) {
        self.t0 = 0;
        self.t1 = 0;
        self.interv = SIM_NEVER;
        self.in_update = false;
    }

    /// Starts the main intervention/monitoring phase by setting `interv = 0`.
    pub fn begin_intervention_period(&mut self) {
        self.interv = 0;
    }

    /// Enters a time-step update (`t1 += interval`).
    #[inline]
    pub fn start_update(&mut self) {
        self.t1 += self.interval;
        self.in_update = true;
    }

    /// Finishes a time-step update (`t0 = t1`, `interv += interval`).
    #[inline]
    pub fn end_update(&mut self) {
        self.in_update = false;
        self.t0 = self.t1;
        self.interv += self.interval;
    }

    /// Beginning-of-step time `ts0` (valid during updates).
    #[inline]
    #[must_use]
    pub const fn ts0(&self) -> SimTime {
        debug_assert!(self.in_update);
        self.t0
    }

    /// End-of-step time `ts1` (valid during updates).
    #[inline]
    #[must_use]
    pub const fn ts1(&self) -> SimTime {
        debug_assert!(self.in_update);
        self.t1
    }

    /// Mid-day time between updates (`ts0 == ts1`, valid outside updates).
    #[inline]
    #[must_use]
    pub const fn now(&self) -> SimTime {
        debug_assert!(!self.in_update);
        self.t0
    }

    /// Returns `ts0` during updates and `now` between updates.
    #[inline]
    #[must_use]
    pub const fn now_or_ts0(&self) -> SimTime {
        self.t0
    }

    /// Returns `ts1` during updates and `now` between updates.
    #[inline]
    #[must_use]
    pub const fn now_or_ts1(&self) -> SimTime {
        self.t1
    }

    /// Returns `ts0` during updates and `now - interval` between updates.
    #[inline]
    #[must_use]
    pub const fn latest_ts0(&self) -> SimTime {
        self.t1 - self.interval
    }

    /// Time elapsed since the start of the main intervention/monitoring phase.
    #[inline]
    #[must_use]
    pub const fn interv_time(&self) -> SimTime {
        self.interv
    }

    /// Calendar date `start_date + interv_time`.
    #[inline]
    #[must_use]
    pub const fn interv_date(&self) -> SimTime {
        self.start_date + self.interv
    }
}
