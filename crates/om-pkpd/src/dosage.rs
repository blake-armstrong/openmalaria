//! Medication steps, treatment schedules, dosage multiplier tables, and sorted per-day dose queues
//! (`model/PkPd/LSTMMedicate.h`, `LSTMTreatments.cpp`, `LSTMDrug.cpp`).

use crate::error::PkPdError;
use om_schema::pharmacology::{DosageMode, DoseStep, PkPdDosages};
use serde::{Deserialize, Serialize};

/// Single drug dose scheduled on or after the current simulation day (`LSTMDrug::DoseVec` element).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct Dose {
    /// Time in days relative to the start of the current 1-day step (`0.0 <= time < 1.0` acts today).
    pub time: f64,
    /// Quantity of active drug ingredient in milligrams (`mg`).
    pub qty: f64,
}

/// Inserts `(time, qty)` into `doses` at `std::lower_bound` (`lhs.time < rhs.time`),
/// matching `LSTMDrug::medicate` (`model/PkPd/Drug/LSTMDrug.cpp:54`) bit-for-bit.
///
/// Note that `std::lower_bound` finds the first element with `!(elem.time < time)` (`elem.time >= time`),
/// so equal-timestamp doses are inserted *before* existing doses at the same timestamp.
pub fn insert_dose_sorted(doses: &mut Vec<Dose>, time: f64, qty: f64) {
    let pos = doses.partition_point(|d| d.time < time);
    doses.insert(pos, Dose { time, qty });
}

/// Prescribed medication item in a [`Schedule`] or a patient's pending medication queue (`OM::PkPd::MedicateData`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct MedicateData {
    /// Registered drug type index (`DrugType::index`).
    pub drug: usize,
    /// Quantity of drug in milligrams (`mg`).
    pub qty: f64,
    /// Administration delay in days from the start of the current time step (`hour / 24.0 + delay_d`).
    pub time: f64,
}

impl MedicateData {
    /// Creates a medication entry from a resolved drug index, base `mg`, and `hour` offset (`time = hour / 24.0`).
    #[must_use]
    pub fn from_hours(drug: usize, mg: f64, hour: f64) -> Self {
        Self {
            drug,
            qty: mg,
            time: hour / 24.0,
        }
    }

    /// Returns a copy with `qty` scaled by `dose_mult` (`MedicateData::multiplied`).
    #[must_use]
    pub fn multiplied(mut self, dose_mult: f64) -> Self {
        self.qty *= dose_mult;
        self
    }
}

/// Named course of treatment (`OM::PkPd::Schedule`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Schedule {
    name: String,
    medications: Vec<MedicateData>,
}

impl Schedule {
    /// Constructs a named [`Schedule`] from resolved [`MedicateData`] steps.
    #[must_use]
    pub fn new(name: impl Into<String>, medications: Vec<MedicateData>) -> Self {
        Self {
            name: name.into(),
            medications,
        }
    }

    /// Schedule identifier (`<schedule name="...">`).
    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Sequence of base medication steps in this schedule.
    #[must_use]
    pub fn medications(&self) -> &[MedicateData] {
        &self.medications
    }
}

/// Dosage multiplier lookup strategy (`OM::PkPd::DosageTable`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DosageTable {
    name: String,
    use_mass: bool,
    mult_mass_kg: bool,
    /// Strictly increasing finite `(exclusive_upper_bound, dose_mult)` pairs,
    /// matching `std::map<double, double>::upper_bound(key)` in `LSTMTreatments.cpp:50-67`.
    table: Vec<(f64, f64)>,
    /// Multiplier for the unbounded top bracket `[last_lb, +inf)`.
    last_mult: f64,
}

impl DosageTable {
    /// Builds and validates a [`DosageTable`] from a Schema v50 [`PkPdDosages`] AST node.
    pub fn from_spec(spec: &PkPdDosages) -> Result<Self, PkPdError> {
        match &spec.mode {
            DosageMode::Multiply { by } => {
                if by != "kg" {
                    return Err(PkPdError::Scenario(
                        "dosages table \"multiply by\" option is not \"kg\"".into(),
                    ));
                }
                Ok(Self {
                    name: spec.name.clone(),
                    use_mass: true,
                    mult_mass_kg: true,
                    table: Vec::new(),
                    last_mult: 1.0,
                })
            }
            DosageMode::Age(steps) => Self::from_steps(&spec.name, steps, false),
            DosageMode::BodyMass(steps) => Self::from_steps(&spec.name, steps, true),
        }
    }

    fn from_steps(name: &str, steps: &[DoseStep], use_mass: bool) -> Result<Self, PkPdError> {
        if steps.is_empty() {
            return Err(PkPdError::Scenario(
                "dosages table does not include age, bodymass or multiply element".into(),
            ));
        }
        let mut table = Vec::with_capacity(steps.len().saturating_sub(1));
        let mut last_mult = 0.0;
        let mut last_lb = f64::NAN;
        for step in steps {
            if last_lb.is_nan() {
                if step.lowerbound != 0.0 {
                    return Err(PkPdError::Scenario(
                        "dosage table must have first lower bound equal 0".into(),
                    ));
                }
            } else {
                if step.lowerbound <= last_lb || step.lowerbound.is_nan() {
                    return Err(PkPdError::Scenario(
                        "dosage table must list age groups in increasing order".into(),
                    ));
                }
                table.push((step.lowerbound, last_mult));
            }
            last_mult = step.dose_mult;
            last_lb = step.lowerbound;
        }
        Ok(Self {
            name: name.to_owned(),
            use_mass,
            mult_mass_kg: false,
            table,
            last_mult,
        })
    }

    /// Dosage table identifier (`<dosages name="...">`).
    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Returns `true` when keyed by patient body mass (kg); `false` when keyed by patient age (years).
    #[must_use]
    pub const fn uses_body_mass(&self) -> bool {
        self.use_mass
    }

    /// Returns `true` when multiplying base doses directly by body mass in kg (`<multiply by="kg"/>`).
    #[must_use]
    pub const fn multiplies_by_kg(&self) -> bool {
        self.mult_mass_kg
    }

    /// Evaluates the dose multiplier for `key` (age in years or body mass in kg),
    /// using `[lb_i, lb_{i+1})` half-open intervals (`upper_bound` semantics).
    pub fn get_multiplier(&self, key: f64) -> Result<f64, PkPdError> {
        if self.mult_mass_kg {
            return Ok(key);
        }
        if let Some(&(_, mult)) = self.table.iter().find(|&&(ub, _)| ub > key) {
            return Ok(mult);
        }
        if key < f64::INFINITY {
            Ok(self.last_mult)
        } else {
            Err(PkPdError::DosageLookup("bad age/dosage table".into()))
        }
    }
}
