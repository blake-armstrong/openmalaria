//! Parent-to-metabolite 1-compartment conversion PK/PD model with correlated `IC50` draws
//! and adaptive 15-point Gauss-Kronrod quadrature (`model/PkPd/Drug/LSTMDrugConversion.*`).

use crate::dosage::{Dose, insert_dose_sorted};
use crate::drug_type::{DrugModelKind, DrugType};
use crate::error::PkPdError;
use crate::patient_model::DrugCompartmentModel;
use crate::pd::InfectionKnCache;
use crate::registry::DrugRegistry;
use om_core::NormalSample;
use om_core::solvers::qag_integrate;
use om_rng::LocalRng;
use serde::{Deserialize, Serialize};

const QAG_CONV_MAX_ITER: usize = 1000;
const QAG_CONV_ABS_EPS: f64 = 1e-5;
// GSL `gsl_integration_qag` rescales raw `|K15 - G7|` via `(200 * err / r_asc)^1.5` before
// comparing against `rel_eps = 1e-2`, equivalent to `(1e-2)^(2/3) / 200 ≈ 2.3208e-4` on raw `|K15 - G7|`.
const QAG_CONV_REL_EPS: f64 = 2.320_794_416_806_389_5e-4;

/// Per-human instance of a parent-to-metabolite conversion drug (`OM::PkPd::LSTMDrugConversion`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ConversionDrug {
    parent_index: usize,
    metabolite_index: usize,
    vol_dist: f64,
    qty_g: f64,
    qty_p: f64,
    qty_m: f64,
    nka: f64,
    nk_p_sample: f64,
    nconv_sample: f64,
    nk_m_sample: f64,
    vol_dist_metabolite: f64,
    last_bm: f64,
    doses: Vec<Dose>,
}

#[derive(Clone, Copy)]
struct ParamsConv {
    qty_g: f64,
    qty_p: f64,
    qty_m: f64,
    nka: f64,
    nk_m: f64,
    nl: f64,
    f: f64,
    g: f64,
    h: f64,
    i: f64,
    j: f64,
    inv_vd_p: f64,
    inv_vd_m: f64,
    n_p: f64,
    n_m: f64,
    v_p: f64,
    v_m: f64,
    kn_p: f64,
    kn_m: f64,
}

#[inline]
fn calc_parent_qty(p: &ParamsConv, exp_absorb: f64, exp_p_loss: f64) -> f64 {
    p.f * p.qty_g * exp_absorb + (p.qty_p - p.f * p.qty_g) * exp_p_loss
}

#[inline]
fn calc_metabolite_qty(p: &ParamsConv, exp_absorb: f64, exp_p_loss: f64, t: f64) -> f64 {
    p.g * p.qty_g * exp_absorb
        + (p.h * p.qty_g - p.i * p.qty_p) * exp_p_loss
        + (p.j * p.qty_g + p.i * p.qty_p + p.qty_m) * (p.nk_m * t).exp()
}

#[inline]
fn func_conv_factor(t: f64, p: &ParamsConv) -> f64 {
    let exp_absorb = (p.nka * t).exp();
    let exp_p_loss = (p.nl * t).exp();

    let cp = calc_parent_qty(p, exp_absorb, exp_p_loss) * p.inv_vd_p;
    let f_cp = if cp <= 0.0 {
        0.0
    } else {
        let cn_p = cp.powf(p.n_p);
        p.v_p * cn_p / (cn_p + p.kn_p)
    };

    let cm = calc_metabolite_qty(p, exp_absorb, exp_p_loss, t) * p.inv_vd_m;
    let f_cm = if cm <= 0.0 {
        0.0
    } else {
        let cn_m = cm.powf(p.n_m);
        p.v_m * cn_m / (cn_m + p.kn_m)
    };

    f_cp.max(f_cm)
}

impl ConversionDrug {
    /// Creates a new conversion drug instance, drawing PK samples in exact C++ order:
    /// `vol_dist`, `nka`, `nkP_sample`, `nconv_sample`, `nkM_sample`, `vol_dist_metabolite`.
    pub fn new(parent: &DrugType, metabolite: &DrugType, rng: &mut LocalRng) -> Self {
        let vol_dist = parent.sample_vd(rng);
        let (nka, nk_p_sample, nconv_sample) = match parent.kind() {
            DrugModelKind::Conversion {
                conversion_rate,
                absorption_rate,
                ..
            } => (
                -absorption_rate.sample(rng),
                -parent.sample_elim_rate(rng),
                -conversion_rate.sample(rng),
            ),
            _ => (0.0, -parent.sample_elim_rate(rng), 0.0),
        };
        let nk_m_sample = -metabolite.sample_elim_rate(rng);
        let vol_dist_metabolite = metabolite.sample_vd(rng);
        Self {
            parent_index: parent.index(),
            metabolite_index: metabolite.index(),
            vol_dist,
            qty_g: 0.0,
            qty_p: 0.0,
            qty_m: 0.0,
            nka,
            nk_p_sample,
            nconv_sample,
            nk_m_sample,
            vol_dist_metabolite,
            last_bm: 1.0,
            doses: Vec::new(),
        }
    }

    /// Parent drug blood serum concentration (`mg / l`).
    #[must_use]
    pub fn parent_concentration(&self) -> f64 {
        if self.qty_p == 0.0 {
            0.0
        } else {
            self.qty_p / (self.vol_dist * self.last_bm)
        }
    }

    /// Metabolite drug blood serum concentration (`mg / l`).
    #[must_use]
    pub fn metabolite_concentration(&self) -> f64 {
        if self.qty_m == 0.0 {
            0.0
        } else {
            self.qty_m / (self.vol_dist_metabolite * self.last_bm)
        }
    }

    fn build_conv_params(&self, parent: &DrugType, metabolite: &DrugType, body_mass: f64) -> ParamsConv {
        let mwr = match parent.kind() {
            DrugModelKind::Conversion { mwr, .. } => *mwr,
            _ => 1.0,
        };
        let p_scale = body_mass.powf(parent.neg_m_exponent());
        let nk_p = self.nk_p_sample * p_scale;
        let nconv = self.nconv_sample * p_scale;
        let nka = self.nka;
        let nk_m = self.nk_m_sample * body_mass.powf(metabolite.neg_m_exponent());
        let nl = nk_p + nconv;

        let f = nka / (nl - nka);
        let rz = mwr * nconv;
        let g = rz * nka / ((nka - nl) * (nka - nk_m));
        let h = rz * nka / ((nka - nl) * (nk_m - nl));
        let i = rz / (nl - nk_m);
        let j = rz * nka / ((nk_m - nl) * (nk_m - nka));

        ParamsConv {
            qty_g: self.qty_g,
            qty_p: self.qty_p,
            qty_m: self.qty_m,
            nka,
            nk_m,
            nl,
            f,
            g,
            h,
            i,
            j,
            inv_vd_p: 1.0 / (self.vol_dist * body_mass),
            inv_vd_m: 1.0 / (self.vol_dist_metabolite * body_mass),
            n_p: 0.0,
            n_m: 0.0,
            v_p: 0.0,
            v_m: 0.0,
            kn_p: 0.0,
            kn_m: 0.0,
        }
    }

    fn set_killing_params(
        &self,
        rng: &mut LocalRng,
        parent: &DrugType,
        metabolite: &DrugType,
        genotype_id: u32,
        kn_cache: &mut InfectionKnCache,
        p: &mut ParamsConv,
    ) -> Result<(), PkPdError> {
        let pd_p = parent.get_pd(genotype_id)?;
        let pd_m = metabolite.get_pd(genotype_id)?;
        p.n_p = pd_p.slope();
        p.v_p = pd_p.max_killing_rate();
        p.n_m = pd_m.slope();
        p.v_m = pd_m.max_killing_rate();

        if let (Some(kn_p), Some(kn_m)) = (
            kn_cache.get(self.parent_index),
            kn_cache.get(self.metabolite_index),
        ) {
            p.kn_p = kn_p;
            p.kn_m = kn_m;
        } else {
            let (corr, corr_factor) = match parent.kind() {
                DrugModelKind::Conversion {
                    ic50_log_corr,
                    ic50_corr_factor,
                    ..
                } => (*ic50_log_corr, *ic50_corr_factor),
                _ => (1.0, 0.0),
            };
            let zscore = NormalSample::generate(rng);
            p.kn_p = pd_p.ic50_pow_slope_normal(zscore);
            kn_cache.insert(self.parent_index, p.kn_p);

            let metab_zscore = NormalSample::generate_correlated(zscore, corr, corr_factor, rng);
            p.kn_m = pd_m.ic50_pow_slope_normal(metab_zscore);
            kn_cache.insert(self.metabolite_index, p.kn_m);
        }
        Ok(())
    }

    #[inline]
    fn advance_quantities(p: &mut ParamsConv, duration: f64) {
        let exp_absorb = (p.nka * duration).exp();
        let exp_p_loss = (p.nl * duration).exp();
        let next_m = calc_metabolite_qty(p, exp_absorb, exp_p_loss, duration);
        let next_p = calc_parent_qty(p, exp_absorb, exp_p_loss);
        p.qty_m = next_m;
        p.qty_p = next_p;
        p.qty_g *= exp_absorb;
    }

    fn integrate_factor(p: &ParamsConv, duration: f64) -> Result<f64, PkPdError> {
        let intf_c = qag_integrate(
            |t| func_conv_factor(t, p),
            0.0,
            duration,
            QAG_CONV_ABS_EPS,
            QAG_CONV_REL_EPS,
            QAG_CONV_MAX_ITER,
        )?;
        Ok((-intf_c).exp())
    }
}

impl DrugCompartmentModel for ConversionDrug {
    fn type_index(&self) -> usize {
        self.parent_index
    }

    fn medicate(&mut self, time: f64, qty: f64) {
        insert_dose_sorted(&mut self.doses, time, qty);
    }

    fn get_concentration(&self, drug_type_idx: usize) -> f64 {
        if drug_type_idx == self.parent_index {
            self.parent_concentration()
        } else if drug_type_idx == self.metabolite_index {
            self.metabolite_concentration()
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
        if self.qty_g == 0.0 && self.qty_p == 0.0 && self.qty_m == 0.0 && self.doses.is_empty() {
            return Ok(1.0);
        }
        let parent = registry.drug(self.parent_index)?;
        let metabolite = registry.drug(self.metabolite_index)?;

        let mut p = self.build_conv_params(parent, metabolite, body_mass);
        self.set_killing_params(rng, parent, metabolite, genotype_id, kn_cache, &mut p)?;

        let mut time = 0.0;
        let mut total_factor = 1.0;
        for &Dose {
            time: dose_time,
            qty,
        } in &self.doses
        {
            if dose_time < 1.0 {
                if time < dose_time {
                    let duration = dose_time - time;
                    total_factor *= Self::integrate_factor(&p, duration)?;
                    Self::advance_quantities(&mut p, duration);
                    time = dose_time;
                } else {
                    debug_assert_eq!(time, dose_time);
                }
                p.qty_g += qty;
            }
        }
        if time < 1.0 {
            total_factor *= Self::integrate_factor(&p, 1.0 - time)?;
        }

        Ok(total_factor)
    }

    fn update_concentration(&mut self, registry: &DrugRegistry, body_mass: f64) {
        if self.qty_g == 0.0 && self.qty_p == 0.0 && self.qty_m == 0.0 && self.doses.is_empty() {
            return;
        }
        let (Some(parent), Some(metabolite)) = (
            registry.get_drug(self.parent_index),
            registry.get_drug(self.metabolite_index),
        ) else {
            return;
        };
        self.last_bm = body_mass;
        let mut p = self.build_conv_params(parent, metabolite, body_mass);

        let mut time = 0.0;
        let mut doses_taken = 0;
        for dose in &mut self.doses {
            if dose.time < 1.0 {
                let duration = dose.time - time;
                if duration > 0.0 {
                    Self::advance_quantities(&mut p, duration);
                    time = dose.time;
                } else {
                    debug_assert_eq!(time, dose.time);
                }
                p.qty_g += dose.qty;
                doses_taken += 1;
            } else {
                dose.time -= 1.0;
            }
        }
        if time < 1.0 {
            Self::advance_quantities(&mut p, 1.0 - time);
        }
        self.doses.drain(0..doses_taken);

        self.qty_m = p.qty_m;
        self.qty_p = p.qty_p;
        self.qty_g = p.qty_g;

        if self.qty_p < parent.negligible_concentration() * body_mass * self.vol_dist
            && self.qty_m
                < metabolite.negligible_concentration() * body_mass * self.vol_dist_metabolite
        {
            self.qty_g = 0.0;
            self.qty_p = 0.0;
            self.qty_m = 0.0;
        }
    }
}
