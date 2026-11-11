//! Extensible [`DrugCompartmentModel`] trait, checkpoint-serializable [`DrugInstance`] enum,
//! and per-human [`PatientPkPd`] manager (`model/PkPd/LSTMModel.h`, `LSTMModel.cpp`).

use crate::conversion::ConversionDrug;
use crate::dosage::MedicateData;
use crate::error::PkPdError;
use crate::one_comp::OneCompDrug;
use crate::pd::InfectionKnCache;
use crate::registry::DrugRegistry;
use crate::three_comp::ThreeCompDrug;
use om_rng::LocalRng;
use serde::{Deserialize, Serialize};

/// Common interface implemented by all PK/PD compartment drug models (`OM::PkPd::LSTMDrug`).
pub trait DrugCompartmentModel {
    /// Primary drug type index in [`DrugRegistry`].
    fn type_index(&self) -> usize;

    /// Schedules an administered dose of `qty` mg at `time` days past the start of the current day.
    fn medicate(&mut self, time: f64, qty: f64);

    /// Returns active blood serum concentration (`mg / l`) for `drug_type_idx` (`0.0` if non-matching).
    fn get_concentration(&self, drug_type_idx: usize) -> f64;

    /// Computes the daily parasite survival multiplier in `[0.0, 1.0]` without mutating blood concentrations.
    fn calculate_drug_factor(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        genotype_id: u32,
        kn_cache: &mut InfectionKnCache,
        body_mass: f64,
    ) -> Result<f64, PkPdError>;

    /// Advances blood concentrations to the end of the 1-day step, clears today's doses,
    /// and zeroes negligible concentrations.
    fn update_concentration(&mut self, registry: &DrugRegistry, body_mass: f64);
}

/// Concrete enum dispatch over all compartment models (`LSTMDrug` hierarchy),
/// enabling zero-overhead dispatch and direct `serde` checkpoint serialization.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum DrugInstance {
    /// Single-compartment drug (`LSTMDrugOneComp`).
    OneComp(OneCompDrug),
    /// Two- or three-compartment drug (`LSTMDrugThreeComp`).
    ThreeComp(ThreeCompDrug),
    /// Parent-to-metabolite conversion drug (`LSTMDrugConversion`).
    Conversion(ConversionDrug),
}

impl DrugCompartmentModel for DrugInstance {
    #[inline]
    fn type_index(&self) -> usize {
        match self {
            Self::OneComp(d) => d.type_index(),
            Self::ThreeComp(d) => d.type_index(),
            Self::Conversion(d) => d.type_index(),
        }
    }

    #[inline]
    fn medicate(&mut self, time: f64, qty: f64) {
        match self {
            Self::OneComp(d) => d.medicate(time, qty),
            Self::ThreeComp(d) => d.medicate(time, qty),
            Self::Conversion(d) => d.medicate(time, qty),
        }
    }

    #[inline]
    fn get_concentration(&self, drug_type_idx: usize) -> f64 {
        match self {
            Self::OneComp(d) => d.get_concentration(drug_type_idx),
            Self::ThreeComp(d) => d.get_concentration(drug_type_idx),
            Self::Conversion(d) => d.get_concentration(drug_type_idx),
        }
    }

    #[inline]
    fn calculate_drug_factor(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        genotype_id: u32,
        kn_cache: &mut InfectionKnCache,
        body_mass: f64,
    ) -> Result<f64, PkPdError> {
        match self {
            Self::OneComp(d) => {
                d.calculate_drug_factor(rng, registry, genotype_id, kn_cache, body_mass)
            }
            Self::ThreeComp(d) => {
                d.calculate_drug_factor(rng, registry, genotype_id, kn_cache, body_mass)
            }
            Self::Conversion(d) => {
                d.calculate_drug_factor(rng, registry, genotype_id, kn_cache, body_mass)
            }
        }
    }

    #[inline]
    fn update_concentration(&mut self, registry: &DrugRegistry, body_mass: f64) {
        match self {
            Self::OneComp(d) => d.update_concentration(registry, body_mass),
            Self::ThreeComp(d) => d.update_concentration(registry, body_mass),
            Self::Conversion(d) => d.update_concentration(registry, body_mass),
        }
    }
}

/// Per-human pharmacokinetic / pharmacodynamic state (`OM::PkPd::LSTMModel`).
///
/// Expected daily calling order in `CommonWithinHost`:
/// 1. [`PatientPkPd::prescribe`] (when clinical or MDA/MSAT interventions administer a course)
/// 2. [`PatientPkPd::medicate`] (moves pending doses with `time < 1.0` into active drug instances)
/// 3. [`PatientPkPd::get_drug_factor`] (once per patent/blood-stage infection)
/// 4. [`PatientPkPd::decay_drugs`] (advances concentrations by 1 day and clears negligible levels)
#[derive(Default, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PatientPkPd {
    drugs: Vec<DrugInstance>,
    medicate_queue: Vec<MedicateData>,
}

impl PatientPkPd {
    /// Creates an empty [`PatientPkPd`] with no prescribed or active drugs.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            drugs: Vec::new(),
            medicate_queue: Vec::new(),
        }
    }

    /// Prescribes a treatment schedule scaled by a dosage table and delayed by `delay_days`
    /// (`LSTMModel::prescribe`).
    pub fn prescribe(
        &mut self,
        registry: &DrugRegistry,
        schedule_idx: usize,
        dosage_idx: usize,
        age_years: f64,
        body_mass: f64,
        delay_days: f64,
    ) -> Result<(), PkPdError> {
        let table = registry.dosage(dosage_idx)?;
        let schedule = registry.schedule(schedule_idx)?;
        let key = if table.uses_body_mass() {
            body_mass
        } else {
            age_years
        };
        let dose_mult = table.get_multiplier(key)?;
        for &med in schedule.medications() {
            let mut data = med.multiplied(dose_mult);
            data.time += delay_days;
            self.medicate_queue.push(data);
        }
        Ok(())
    }

    /// Processes the pending medication queue for the current day (`LSTMModel::medicate`):
    /// doses with `time < 1.0` are transferred to active drug instances, while later doses
    /// have their delay decremented by `1.0` day.
    pub fn medicate(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
    ) -> Result<(), PkPdError> {
        if self.medicate_queue.is_empty() {
            return Ok(());
        }
        let mut first_err: Option<PkPdError> = None;
        self.medicate_queue.retain_mut(|item| {
            if first_err.is_some() {
                return true;
            }
            if item.time < 1.0 {
                if let Err(e) = Self::medicate_drug_vec(
                    &mut self.drugs,
                    rng,
                    registry,
                    item.drug,
                    item.qty,
                    item.time,
                ) {
                    first_err = Some(e);
                    return true;
                }
                false
            } else {
                item.time -= 1.0;
                true
            }
        });
        match first_err {
            Some(e) => Err(e),
            None => Ok(()),
        }
    }

    /// Directly medicates `qty` mg of `type_index` at `time` days past the start of today,
    /// lazily instantiating `type_index` on first exposure (`LSTMModel::medicateDrug`).
    ///
    /// Instantiated drugs are never removed from `drugs`, ensuring per-human sampled PK
    /// parameters persist for the host's lifetime.
    pub fn medicate_drug(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        type_index: usize,
        qty: f64,
        time: f64,
    ) -> Result<(), PkPdError> {
        Self::medicate_drug_vec(&mut self.drugs, rng, registry, type_index, qty, time)
    }

    fn medicate_drug_vec(
        drugs: &mut Vec<DrugInstance>,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        type_index: usize,
        qty: f64,
        time: f64,
    ) -> Result<(), PkPdError> {
        for drug in drugs.iter_mut() {
            if drug.type_index() == type_index {
                drug.medicate(time, qty);
                return Ok(());
            }
        }
        let mut instance = registry.create_instance(rng, type_index)?;
        instance.medicate(time, qty);
        drugs.push(instance);
        Ok(())
    }

    /// Returns total blood serum concentration (`mg / l`) for `drug_type_idx` across all active
    /// drug instances, clamping negative rounding artifacts to `0.0` (`LSTMModel::getDrugConc`).
    #[must_use]
    pub fn drug_concentration(&self, drug_type_idx: usize) -> f64 {
        let mut total = 0.0;
        for drug in &self.drugs {
            let d = drug.get_concentration(drug_type_idx);
            if d > 0.0 {
                total += d;
            }
        }
        total
    }

    /// Computes the combined daily parasite survival factor across all active drugs for an infection
    /// with `genotype_id` and per-infection `kn_cache` (`LSTMModel::getDrugFactor`).
    pub fn get_drug_factor(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        genotype_id: u32,
        kn_cache: &mut InfectionKnCache,
        body_mass: f64,
    ) -> Result<f64, PkPdError> {
        let mut factor = 1.0;
        for drug in &mut self.drugs {
            factor *= drug.calculate_drug_factor(rng, registry, genotype_id, kn_cache, body_mass)?;
        }
        Ok(factor)
    }

    /// Advances all active drug concentrations to the end of the day (`LSTMModel::decayDrugs`).
    pub fn decay_drugs(&mut self, registry: &DrugRegistry, body_mass: f64) {
        for drug in &mut self.drugs {
            drug.update_concentration(registry, body_mass);
        }
    }

    /// Yields `(drug_type_idx, concentration)` pairs for every `(idx in drugs_in_use, drug in m_drugs)`
    /// with `concentration > 0.0`, matching `LSTMModel::summarize` (`nHostDrugConcNonZero` / `sumLogDrugConcNonZero`).
    pub fn non_zero_concentrations<'a>(
        &'a self,
        registry: &'a DrugRegistry,
    ) -> impl Iterator<Item = (usize, f64)> + 'a {
        registry.drugs_in_use().iter().copied().flat_map(move |idx| {
            self.drugs.iter().filter_map(move |drug| {
                let conc = drug.get_concentration(idx);
                (conc > 0.0).then_some((idx, conc))
            })
        })
    }

    /// Instantiated per-human drug models (`m_drugs`).
    #[must_use]
    pub fn drugs(&self) -> &[DrugInstance] {
        &self.drugs
    }

    /// Pending prescribed medications (`medicateQueue`).
    #[must_use]
    pub fn pending_medications(&self) -> &[MedicateData] {
        &self.medicate_queue
    }

    /// Clears all pending prescribed medications.
    pub fn clear_pending_medications(&mut self) {
        self.medicate_queue.clear();
    }
}
