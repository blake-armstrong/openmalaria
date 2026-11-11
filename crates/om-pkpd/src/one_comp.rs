//! Single-compartment PK/PD model with instantaneous absorption and analytic Hill integration
//! (`model/PkPd/Drug/LSTMDrugOneComp.h`, `LSTMDrugOneComp.cpp`; Hastings & Winter 2011).

use crate::dosage::{Dose, insert_dose_sorted};
use crate::drug_type::DrugType;
use crate::error::PkPdError;
use crate::patient_model::DrugCompartmentModel;
use crate::pd::InfectionKnCache;
use crate::registry::DrugRegistry;
use om_rng::LocalRng;
use serde::{Deserialize, Serialize};

/// Per-human instance of a 1-compartment drug (`OM::PkPd::LSTMDrugOneComp`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct OneCompDrug {
    type_index: usize,
    vol_dist: f64,
    neg_elim_sample: f64,
    concentration: f64,
    doses: Vec<Dose>,
}

impl OneCompDrug {
    /// Creates a new 1-compartment drug instance, drawing `Vd` first and `-k` second
    /// to preserve bit-exact PRNG parity with `LSTMDrugOneComp::LSTMDrugOneComp`.
    pub fn new(drug_type: &DrugType, rng: &mut LocalRng) -> Self {
        let vol_dist = drug_type.sample_vd(rng);
        let neg_elim_sample = -drug_type.sample_elim_rate(rng);
        Self {
            type_index: drug_type.index(),
            vol_dist,
            neg_elim_sample,
            concentration: 0.0,
            doses: Vec::new(),
        }
    }

    /// Sampled volume of distribution (`l / kg`).
    #[must_use]
    pub const fn vol_dist(&self) -> f64 {
        self.vol_dist
    }

    /// Current blood serum concentration at the start of the day (`mg / l`).
    #[must_use]
    pub const fn concentration(&self) -> f64 {
        self.concentration
    }

    /// Pending sorted doses (`(time_days, qty_mg)`).
    #[must_use]
    pub fn doses(&self) -> &[Dose] {
        &self.doses
    }
}

impl DrugCompartmentModel for OneCompDrug {
    fn type_index(&self) -> usize {
        self.type_index
    }

    fn medicate(&mut self, time: f64, qty: f64) {
        insert_dose_sorted(&mut self.doses, time, qty);
    }

    fn get_concentration(&self, drug_type_idx: usize) -> f64 {
        if drug_type_idx == self.type_index {
            self.concentration
        } else {
            0.0
        }
    }

    fn calculate_drug_factor(
        &mut self,
        rng: &mut LocalRng,
        registry: &DrugRegistry,
        genotype_id: u32,
        kn_cache: &mut InfectionKnCache,
        body_mass: f64,
    ) -> Result<f64, PkPdError> {
        if self.concentration == 0.0 && self.doses.is_empty() {
            return Ok(1.0);
        }

        let dt = registry.drug(self.type_index)?;
        let mut total_factor = 1.0;
        let mut concentration_today = self.concentration;
        let neg_elim_rate = self.neg_elim_sample * body_mass.powf(dt.neg_m_exponent());

        let pd = dt.get_pd(genotype_id)?;
        let kn = pd.ic50_pow_slope_cached(rng, self.type_index, kn_cache);

        let mut time = 0.0;
        for &Dose {
            time: dose_time,
            qty,
        } in &self.doses
        {
            if dose_time < 1.0 {
                if time < dose_time {
                    total_factor *= pd.calc_factor(
                        kn,
                        neg_elim_rate,
                        &mut concentration_today,
                        dose_time - time,
                    );
                    time = dose_time;
                } else {
                    debug_assert_eq!(time, dose_time);
                }
                concentration_today += qty / (self.vol_dist * body_mass);
            } else {
                break;
            }
        }
        if time < 1.0 {
            total_factor *=
                pd.calc_factor(kn, neg_elim_rate, &mut concentration_today, 1.0 - time);
        }

        Ok(total_factor)
    }

    fn update_concentration(&mut self, registry: &DrugRegistry, body_mass: f64) {
        if self.concentration == 0.0 && self.doses.is_empty() {
            return;
        }
        let Some(dt) = registry.get_drug(self.type_index) else {
            return;
        };

        let neg_elim_rate = self.neg_elim_sample * body_mass.powf(dt.neg_m_exponent());
        self.concentration *= neg_elim_rate.exp();

        let mut doses_taken = 0;
        for dose in &mut self.doses {
            if dose.time < 1.0 {
                self.concentration += dose.qty / (self.vol_dist * body_mass)
                    * (neg_elim_rate * (1.0 - dose.time)).exp();
                doses_taken += 1;
            } else {
                dose.time -= 1.0;
            }
        }
        self.doses.drain(0..doses_taken);

        if self.concentration < dt.negligible_concentration() {
            self.concentration = 0.0;
        }
    }
}
