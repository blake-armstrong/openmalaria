//! Two- and three-compartment PK/PD model using Monolix Viète trigonometric cubic roots
//! and adaptive 15-point Gauss-Kronrod quadrature (`model/PkPd/Drug/LSTMDrugThreeComp.*`).

use crate::dosage::{Dose, insert_dose_sorted};
use crate::drug_type::{DrugModelKind, DrugType};
use crate::error::PkPdError;
use crate::patient_model::DrugCompartmentModel;
use crate::pd::InfectionKnCache;
use crate::registry::DrugRegistry;
use om_core::solvers::qag_integrate;
use om_rng::LocalRng;
use serde::{Deserialize, Serialize};
use std::f64::consts::PI;

const QAG_MAX_ITER: usize = 1000;
const QAG_ABS_EPS: f64 = 1e-4;
const QAG_REL_EPS: f64 = 1e-4;

/// Per-human instance of a 2- or 3-compartment drug (`OM::PkPd::LSTMDrugThreeComp`).
///
/// Follows Julie Bertrand & France Mentré, *"Mathematical Expressions of the Pharmacokinetic and
/// Pharmacodynamic Models implemented in the Monolix software"* (Sept 2008, eq. 1.72, p. 44).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ThreeCompDrug {
    type_index: usize,
    vol_dist: f64,
    conc_a: f64,
    conc_b: f64,
    conc_c: f64,
    conc_abc: f64,
    elim_sample: f64,
    k12: f64,
    k21: f64,
    k13: f64,
    k31: f64,
    nka: f64,
    last_bm: f64,
    na: f64,
    nb: f64,
    ng: f64,
    a_coeff: f64,
    b_coeff: f64,
    c_coeff: f64,
    doses: Vec<Dose>,
}

#[derive(Clone, Copy)]
struct ParamsFc {
    ca: f64,
    cb: f64,
    cc: f64,
    cabc: f64,
    na: f64,
    nb: f64,
    ng: f64,
    nka: f64,
    n: f64,
    v: f64,
    kn: f64,
}

#[inline]
fn func_fc(t: f64, p: &ParamsFc) -> f64 {
    let conc_a = p.ca * (p.na * t).exp();
    let conc_b = p.cb * (p.nb * t).exp();
    let conc_c = p.cc * (p.ng * t).exp();
    let conc_abc = p.cabc * (p.nka * t).exp();
    let conc = conc_a + conc_b + conc_c - conc_abc;
    if conc <= 0.0 {
        return 0.0;
    }
    let cn = conc.powf(p.n);
    p.v * cn / (cn + p.kn)
}

impl ThreeCompDrug {
    /// Creates a new 2- or 3-compartment drug instance, sampling parameters in exact C++ order:
    /// `vol_dist`, `elim_sample`, `k12`, `k21`, `k13`, `k31`, `nka = -sample_ka(rng)`.
    pub fn new(drug_type: &DrugType, rng: &mut LocalRng) -> Self {
        let vol_dist = drug_type.sample_vd(rng);
        let elim_sample = drug_type.sample_elim_rate(rng);
        let (k12, k21, k13, k31, nka) = match drug_type.kind() {
            DrugModelKind::ThreeComp {
                k12,
                k21,
                k13,
                k31,
                absorption_rate,
            } => (
                k12.sample(rng),
                k21.sample(rng),
                k13.sample(rng),
                k31.sample(rng),
                -absorption_rate.sample(rng),
            ),
            _ => (0.0, 0.0, 0.0, 0.0, 0.0),
        };
        Self {
            type_index: drug_type.index(),
            vol_dist,
            conc_a: 0.0,
            conc_b: 0.0,
            conc_c: 0.0,
            conc_abc: 0.0,
            elim_sample,
            k12,
            k21,
            k13,
            k31,
            nka,
            last_bm: -1.0,
            na: 0.0,
            nb: 0.0,
            ng: 0.0,
            a_coeff: 0.0,
            b_coeff: 0.0,
            c_coeff: 0.0,
            doses: Vec::new(),
        }
    }

    #[inline]
    fn conc(&self) -> f64 {
        self.conc_a + self.conc_b + self.conc_c - self.conc_abc
    }

    fn update_cached(&mut self, neg_m_exp: f64, body_mass: f64) {
        if self.last_bm == body_mass {
            return;
        }
        let k = self.elim_sample * body_mass.powf(neg_m_exp);
        let a0 = k * self.k21 * self.k31;
        let a1 = k * self.k31
            + self.k21 * self.k31
            + self.k21 * self.k13
            + k * self.k21
            + self.k31 * self.k12;
        let a2 = k + self.k12 + self.k13 + self.k21 + self.k31;

        let third = 1.0 / 3.0;
        let p = a1 - third * a2 * a2;
        let q = (2.0 / 27.0) * a2 * a2 * a2 - third * a1 * a2 + a0;

        let at = third * a2;
        let rt = -third * p;
        let r2 = 2.0 * rt.max(0.0).sqrt();

        let cos_arg = if r2 == 0.0 {
            0.0
        } else {
            (-q / (rt * r2)).clamp(-1.0, 1.0)
        };
        let phi = cos_arg.acos() * third;
        let pi23 = (2.0 / 3.0) * PI;

        self.na = r2 * phi.cos() - at;
        self.nb = r2 * (phi + pi23).cos() - at;
        self.ng = r2 * (phi + 2.0 * pi23).cos() - at;

        let ka_ov = -self.nka / (self.vol_dist * body_mass);
        self.a_coeff = ka_ov * (self.k21 + self.na) * (self.k31 + self.na)
            / ((self.na - self.nka) * (self.nb - self.na) * (self.ng - self.na));
        self.b_coeff = ka_ov * (self.k21 + self.nb) * (self.k31 + self.nb)
            / ((self.nb - self.nka) * (self.na - self.nb) * (self.ng - self.nb));
        self.c_coeff = ka_ov * (self.k21 + self.ng) * (self.k31 + self.ng)
            / ((self.ng - self.nka) * (self.nb - self.ng) * (self.na - self.ng));

        self.last_bm = body_mass;
    }

    fn integrate_factor(p: &ParamsFc, duration: f64) -> Result<f64, PkPdError> {
        let intf_c = qag_integrate(
            |t| func_fc(t, p),
            0.0,
            duration,
            QAG_ABS_EPS,
            QAG_REL_EPS,
            QAG_MAX_ITER,
        )?;
        Ok(1.0 / intf_c.exp())
    }
}

impl DrugCompartmentModel for ThreeCompDrug {
    fn type_index(&self) -> usize {
        self.type_index
    }

    fn medicate(&mut self, time: f64, qty: f64) {
        insert_dose_sorted(&mut self.doses, time, qty);
    }

    fn get_concentration(&self, drug_type_idx: usize) -> f64 {
        if drug_type_idx == self.type_index {
            self.conc()
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
        if self.conc() == 0.0 && self.doses.is_empty() {
            return Ok(1.0);
        }
        let dt = registry.drug(self.type_index)?;
        self.update_cached(dt.neg_m_exponent(), body_mass);

        let pd = dt.get_pd(genotype_id)?;
        let kn = pd.ic50_pow_slope_cached(rng, self.type_index, kn_cache);

        let mut p = ParamsFc {
            ca: self.conc_a,
            cb: self.conc_b,
            cc: self.conc_c,
            cabc: self.conc_abc,
            na: self.na,
            nb: self.nb,
            ng: self.ng,
            nka: self.nka,
            n: pd.slope(),
            v: pd.max_killing_rate(),
            kn,
        };

        let mut time = 0.0;
        let mut total_factor = 1.0;
        let a_c = self.a_coeff;
        let b_c = self.b_coeff;
        let c_c = self.c_coeff;

        for &Dose {
            time: dose_time,
            qty,
        } in &self.doses
        {
            if dose_time < 1.0 {
                if time < dose_time {
                    let duration = dose_time - time;
                    total_factor *= Self::integrate_factor(&p, duration)?;
                    p.ca *= (p.na * duration).exp();
                    p.cb *= (p.nb * duration).exp();
                    p.cc *= (p.ng * duration).exp();
                    p.cabc *= (p.nka * duration).exp();
                    time = dose_time;
                } else {
                    debug_assert_eq!(time, dose_time);
                }
                p.ca += a_c * qty;
                p.cb += b_c * qty;
                p.cc += c_c * qty;
                p.cabc += (a_c + b_c + c_c) * qty;
            }
        }
        if time < 1.0 {
            total_factor *= Self::integrate_factor(&p, 1.0 - time)?;
        }

        Ok(total_factor)
    }

    fn update_concentration(&mut self, registry: &DrugRegistry, body_mass: f64) {
        if self.conc() == 0.0 && self.doses.is_empty() {
            return;
        }
        let Some(dt) = registry.get_drug(self.type_index) else {
            return;
        };
        self.update_cached(dt.neg_m_exponent(), body_mass);

        self.conc_a *= self.na.exp();
        self.conc_b *= self.nb.exp();
        self.conc_c *= self.ng.exp();
        self.conc_abc *= self.nka.exp();

        let mut doses_taken = 0;
        let a_c = self.a_coeff;
        let b_c = self.b_coeff;
        let c_c = self.c_coeff;
        let na = self.na;
        let nb = self.nb;
        let ng = self.ng;
        let nka = self.nka;

        for dose in &mut self.doses {
            if dose.time < 1.0 {
                let rem = 1.0 - dose.time;
                self.conc_a += a_c * dose.qty * (na * rem).exp();
                self.conc_b += b_c * dose.qty * (nb * rem).exp();
                self.conc_c += c_c * dose.qty * (ng * rem).exp();
                self.conc_abc += (a_c + b_c + c_c) * dose.qty * (nka * rem).exp();
                doses_taken += 1;
            } else {
                dose.time -= 1.0;
            }
        }
        self.doses.drain(0..doses_taken);

        if self.conc() < dt.negligible_concentration() {
            self.conc_a = 0.0;
            self.conc_b = 0.0;
            self.conc_c = 0.0;
            self.conc_abc = 0.0;
        }
    }
}
