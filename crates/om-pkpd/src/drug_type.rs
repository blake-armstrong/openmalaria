//! Drug type pharmacokinetic definitions (`DrugModelKind`, `DrugType`; `LSTMDrugType.*`).

use crate::error::PkPdError;
use crate::pd::{DrugPd, build_phenotypes, create_lognormal_sampler};
use om_core::LognormalSampler;
use om_rng::LocalRng;
use om_schema::pharmacology::{PkElimination, PkPdDrug};
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;

/// Compartment structure and model-specific PK samplers of a [`DrugType`].
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum DrugModelKind {
    /// Instantaneous-absorption single-compartment model (`LSTMDrugOneComp`).
    OneComp,
    /// Two- or three-compartment model with gut absorption (`LSTMDrugThreeComp`).
    /// For 2-compartment drugs, `k13` and `k31` are degenerate zero samplers (`mean = 0, cv = 0`).
    ThreeComp {
        /// Central-to-peripheral-2 rate `k12` (`1 / day`).
        k12: LognormalSampler,
        /// Peripheral-2-to-central rate `k21` (`1 / day`).
        k21: LognormalSampler,
        /// Central-to-peripheral-3 rate `k13` (`1 / day`; `0` for 2-compartment).
        k13: LognormalSampler,
        /// Peripheral-3-to-central rate `k31` (`1 / day`; `0` for 2-compartment).
        k31: LognormalSampler,
        /// Gut absorption rate `k_a` (`1 / day`).
        absorption_rate: LognormalSampler,
    },
    /// Parent-to-metabolite 1-compartment conversion model (`LSTMDrugConversion`).
    Conversion {
        /// Index of the previously defined metabolite [`DrugType`].
        metabolite: usize,
        /// Parent-to-metabolite conversion rate (`1 / day`).
        conversion_rate: LognormalSampler,
        /// Gut absorption rate `k_a` (`1 / day`).
        absorption_rate: LognormalSampler,
        /// Molecular weight ratio (`M_metabolite / M_parent`).
        mwr: f64,
        /// Log-scale `IC50` correlation in `[0, 1]`.
        ic50_log_corr: f64,
        /// Cached `sqrt(1 - corr^2)`.
        ic50_corr_factor: f64,
    },
}

/// Immutable PK/PD definition for a single drug compound (`LSTMDrugType`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DrugType {
    index: usize,
    abbrev: String,
    genotype_mapping: Vec<usize>,
    phenotypes: Vec<DrugPd>,
    negligible_concentration: f64,
    neg_m_exp: f64,
    vol_dist: LognormalSampler,
    elimination_rate: LognormalSampler,
    kind: DrugModelKind,
}

impl DrugType {
    /// Builds and validates a [`DrugType`] from XML [`PkPdDrug`], genotype allele sets, and resolvers.
    pub(crate) fn from_spec<FM, FA>(
        index: usize,
        spec: &PkPdDrug,
        genotypes: &[BTreeSet<u32>],
        mut resolve_metabolite: FM,
        allele_lookup: &FA,
    ) -> Result<Self, PkPdError>
    where
        FM: FnMut(&str) -> Result<usize, PkPdError>,
        FA: Fn(&str, &str) -> Option<u32>,
    {
        let pk = &spec.pk;
        let (elimination_rate, neg_m_exp) = match &pk.elimination {
            PkElimination::HalfLife(hl) => (
                LognormalSampler::from_mean_cv(2.0_f64.ln() / *hl, 0.0, None)?,
                0.0,
            ),
            PkElimination::EliminationRate { k, m_exponent } => {
                (create_lognormal_sampler(k)?, -*m_exponent)
            }
        };
        let vol_dist = create_lognormal_sampler(&pk.vol_dist)?;
        if pk.compartment2.is_none() && pk.compartment3.is_some() {
            return Err(PkPdError::Scenario(
                "PK model specifies parameters for compartment3 without compartment2".into(),
            ));
        }
        if pk.compartment2.is_some() && pk.conversion.is_some() {
            return Err(PkPdError::Scenario(
                "PK conversion model is incompatible with 2/3-compartment model".into(),
            ));
        }
        let kind = if let Some(c2) = &pk.compartment2 {
            let k12 = create_lognormal_sampler(&c2.k_in)?;
            let k21 = create_lognormal_sampler(&c2.k_out)?;
            let (k13, k31) = if let Some(c3) = &pk.compartment3 {
                (
                    create_lognormal_sampler(&c3.k_in)?,
                    create_lognormal_sampler(&c3.k_out)?,
                )
            } else {
                let zero = LognormalSampler::from_mean_cv(0.0, 0.0, None)?;
                (zero, zero)
            };
            let ka_spec = pk.k_a.as_ref().ok_or_else(|| {
                PkPdError::Scenario(
                    "PK models require an absorption rate parameter (k_a) when compartment2 is present"
                        .into(),
                )
            })?;
            DrugModelKind::ThreeComp {
                k12,
                k21,
                k13,
                k31,
                absorption_rate: create_lognormal_sampler(ka_spec)?,
            }
        } else if let Some(conv) = &pk.conversion {
            let metabolite = resolve_metabolite(&conv.metabolite).map_err(|_| {
                PkPdError::Scenario(
                    "PK: metabolite drug not found; metabolite must be defined *before* parent drug!"
                        .into(),
                )
            })?;
            let corr = conv.ic50_log_correlation;
            if !(0.0..=1.0).contains(&corr) {
                return Err(PkPdError::Scenario(
                    "PK conversion model: IC50 correlation must be between 0 and 1".into(),
                ));
            }
            let ka_spec = pk.k_a.as_ref().ok_or_else(|| {
                PkPdError::Scenario(
                    "PK models require an absorption rate parameter (k_a) when conversion is present"
                        .into(),
                )
            })?;
            DrugModelKind::Conversion {
                metabolite,
                conversion_rate: create_lognormal_sampler(&conv.rate)?,
                absorption_rate: create_lognormal_sampler(ka_spec)?,
                mwr: conv.mol_ratio,
                ic50_log_corr: corr,
                ic50_corr_factor: (1.0 - corr * corr).sqrt(),
            }
        } else {
            if pk.k_a.is_some() {
                return Err(PkPdError::Scenario(
                    "PK models only allow an absorption rate parameter (k_a) when compartment2 or conversion parameters are present"
                        .into(),
                ));
            }
            DrugModelKind::OneComp
        };

        let (phenotypes, genotype_mapping) =
            build_phenotypes(&spec.pd.phenotypes, genotypes, allele_lookup)?;

        Ok(Self {
            index,
            abbrev: spec.abbrev.clone(),
            genotype_mapping,
            phenotypes,
            negligible_concentration: pk.negligible_concentration,
            neg_m_exp,
            vol_dist,
            elimination_rate,
            kind,
        })
    }

    /// Drug index in [`crate::DrugRegistry`].
    #[must_use]
    pub const fn index(&self) -> usize {
        self.index
    }

    /// Drug abbreviation (`<drug abbrev="...">`).
    #[must_use]
    pub fn abbrev(&self) -> &str {
        &self.abbrev
    }

    /// Concentration threshold below which blood levels are zeroed (`mg / l`).
    #[must_use]
    pub const fn negligible_concentration(&self) -> f64 {
        self.negligible_concentration
    }

    /// Negated allometric body-mass exponent `-m_exponent` (`0.0` when `half_life` is specified).
    #[must_use]
    pub const fn neg_m_exponent(&self) -> f64 {
        self.neg_m_exp
    }

    /// Compartment model configuration for this drug.
    #[must_use]
    pub const fn kind(&self) -> &DrugModelKind {
        &self.kind
    }

    /// Samples volume of distribution `Vd` (`l / kg`).
    pub fn sample_vd(&self, rng: &mut LocalRng) -> f64 {
        self.vol_dist.sample(rng)
    }

    /// Samples terminal elimination rate constant `k` (`1 / day`).
    pub fn sample_elim_rate(&self, rng: &mut LocalRng) -> f64 {
        self.elimination_rate.sample(rng)
    }

    /// Resolves the [`DrugPd`] phenotype corresponding to parasite `genotype_id`.
    pub fn get_pd(&self, genotype_id: u32) -> Result<&DrugPd, PkPdError> {
        let phen_idx = *self
            .genotype_mapping
            .get(genotype_id as usize)
            .ok_or(PkPdError::InvalidGenotype {
                genotype_id,
                count: self.genotype_mapping.len(),
            })?;
        Ok(&self.phenotypes[phen_idx])
    }
}
