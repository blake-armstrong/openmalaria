//! Per-infection `Kn` cache, `SampledValueLN` construction, pharmacodynamic Hill parameters
//! (`DrugPd`), and genotype-to-phenotype partition builder (`model/PkPd/Drug/LSTMDrugType.*`).

use crate::error::PkPdError;
use om_core::{LognormalSampler, NormalSample};
use om_rng::LocalRng;
use om_schema::common::SampledValueLn;
use om_schema::pharmacology::PdPhenotype;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

/// Per-infection cache mapping `drug_type_index -> Kn` (`IC50^slope`), replacing `CommonInfection::Kn`.
///
/// Stored as a sorted inline `(usize, f64)` vector for zero B-tree node allocation overhead,
/// canonical equality, and deterministic `serde` checkpoint serialization.
#[derive(Default, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct InfectionKnCache {
    entries: Vec<(usize, f64)>,
}

impl InfectionKnCache {
    /// Creates an empty per-infection `Kn` cache.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            entries: Vec::new(),
        }
    }

    /// Looks up a previously sampled `Kn = IC50^slope` for `drug_type_index`.
    #[must_use]
    pub fn get(&self, drug_type_index: usize) -> Option<f64> {
        self.entries
            .binary_search_by_key(&drug_type_index, |&(idx, _)| idx)
            .ok()
            .map(|pos| self.entries[pos].1)
    }

    /// Inserts or updates `Kn = IC50^slope` for `drug_type_index`.
    pub fn insert(&mut self, drug_type_index: usize, kn: f64) {
        match self
            .entries
            .binary_search_by_key(&drug_type_index, |&(idx, _)| idx)
        {
            Ok(pos) => self.entries[pos].1 = kn,
            Err(pos) => self.entries.insert(pos, (drug_type_index, kn)),
        }
    }

    /// Returns `true` if `drug_type_index` has already been sampled for this infection.
    #[must_use]
    pub fn contains(&self, drug_type_index: usize) -> bool {
        self.entries
            .binary_search_by_key(&drug_type_index, |&(idx, _)| idx)
            .is_ok()
    }

    /// Number of cached drug `Kn` values.
    #[must_use]
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Returns `true` if no drug `Kn` values have been cached yet.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// Clears all cached `Kn` values.
    pub fn clear(&mut self) {
        self.entries.clear();
    }
}

/// Builds a [`LognormalSampler`] from a Schema v50 [`SampledValueLn`] element,
/// matching `OM::util::createSampler<LognormalSampler>` (`model/util/sampler.h:279-348`).
pub fn create_lognormal_sampler(elt: &SampledValueLn) -> Result<LognormalSampler, PkPdError> {
    if elt.distr == "const" {
        if elt.cv.is_some_and(|v| v != 0.0) {
            return Err(PkPdError::Scenario(
                "attribute \"CV\" must be zero or omitted when distr=\"const\" or is omitted"
                    .into(),
            ));
        }
        if elt.variance.is_some_and(|v| v != 0.0) {
            return Err(PkPdError::Scenario(
                "attribute \"variance\" must be zero or omitted when distr=\"const\" or is omitted"
                    .into(),
            ));
        }
        return Ok(LognormalSampler::from_mean_cv(elt.mean, 0.0, None)?);
    }
    if elt.distr != "lognormal" {
        return Err(PkPdError::Scenario(format!(
            "\"distr={}\": expected \"lognormal\" or \"const\"",
            elt.distr
        )));
    }
    if elt.cv.is_some() == elt.variance.is_some() {
        return Err(PkPdError::Scenario(
            "\"distr=lognormal\": exactly one of attributes \"CV\" or \"variance\" must be specified"
                .into(),
        ));
    }
    if let Some(cv) = elt.cv {
        Ok(LognormalSampler::from_mean_cv(elt.mean, cv, elt.truncate)?)
    } else {
        Ok(LognormalSampler::from_mean_variance(
            elt.mean,
            elt.variance.unwrap_or(0.0),
            elt.truncate,
        )?)
    }
}

/// Pharmacodynamic Hill dose-response parameters for one drug resistance phenotype (`LSTMDrugPD`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct DrugPd {
    slope: f64,
    ic50: LognormalSampler,
    max_killing_rate: f64,
}

impl DrugPd {
    /// Builds [`DrugPd`] from a Schema v50 [`PdPhenotype`].
    pub fn from_phenotype(ph: &PdPhenotype) -> Result<Self, PkPdError> {
        Ok(Self {
            slope: ph.slope,
            ic50: create_lognormal_sampler(&ph.ic50)?,
            max_killing_rate: ph.max_killing_rate,
        })
    }

    /// Hill curve slope `n` (dimensionless).
    #[must_use]
    pub const fn slope(&self) -> f64 {
        self.slope
    }

    /// Maximal parasite killing rate `V` (`1 / days`).
    #[must_use]
    pub const fn max_killing_rate(&self) -> f64 {
        self.max_killing_rate
    }

    /// Underlying `IC50` sampler (`mg / l`).
    #[must_use]
    pub const fn ic50(&self) -> &LognormalSampler {
        &self.ic50
    }

    /// Analytic 1-compartment survival factor over `duration` days (Hastings & Winter 2011),
    /// updating `*conc` in-place from `C0` to `C1 = C0 * exp(neg_elim_rate * duration)`.
    pub fn calc_factor(
        &self,
        kn: f64,
        neg_elim_rate: f64,
        conc: &mut f64,
        duration: f64,
    ) -> f64 {
        let c0 = *conc;
        let c1 = c0 * (neg_elim_rate * duration).exp();
        let numerator = kn + c1.powf(self.slope);
        let denominator = kn + c0.powf(self.slope);
        let power = self.max_killing_rate / (-neg_elim_rate * self.slope);
        *conc = c1;
        (numerator / denominator).powf(power)
    }

    /// Returns cached `Kn = IC50^slope` for `drug_index`, sampling once on first encounter.
    pub fn ic50_pow_slope_cached(
        &self,
        rng: &mut LocalRng,
        drug_index: usize,
        kn_cache: &mut InfectionKnCache,
    ) -> f64 {
        if let Some(kn) = kn_cache.get(drug_index) {
            kn
        } else {
            let kn = self.ic50.sample(rng).powf(self.slope);
            kn_cache.insert(drug_index, kn);
            kn
        }
    }

    /// Evaluates `IC50(normal)^slope` from a pre-drawn [`NormalSample`] (used for parent/metabolite correlation).
    #[must_use]
    pub fn ic50_pow_slope_normal(&self, normal: NormalSample) -> f64 {
        self.ic50.sample_normal(normal).powf(self.slope)
    }
}

/// Validates `<PD>` phenotypes and constructs the `genotype_id -> phenotype_idx` partition map.
pub(crate) fn build_phenotypes<FA>(
    pd_specs: &[PdPhenotype],
    genotypes: &[BTreeSet<u32>],
    allele_lookup: &FA,
) -> Result<(Vec<DrugPd>, Vec<usize>), PkPdError>
where
    FA: Fn(&str, &str) -> Option<u32>,
{
    if pd_specs.is_empty() {
        return Err(PkPdError::Scenario(
            "drug PD must specify at least one phenotype".into(),
        ));
    }
    let first_name = pd_specs[0]
        .name
        .clone()
        .unwrap_or_else(|| "0".to_owned());
    let first_loci: BTreeSet<&str> = pd_specs[0]
        .restrictions
        .iter()
        .map(|r| r.on_locus.as_str())
        .collect();

    let mut phenotypes = Vec::with_capacity(pd_specs.len());
    let mut restrictions_by_phen: Vec<Vec<Vec<u32>>> = Vec::with_capacity(pd_specs.len());

    for (i, ph) in pd_specs.iter().enumerate() {
        if i > 0 {
            let this_name = ph.name.clone().unwrap_or_else(|| i.to_string());
            let this_loci: BTreeSet<&str> =
                ph.restrictions.iter().map(|r| r.on_locus.as_str()).collect();
            if let Some(&extra) = this_loci.difference(&first_loci).next() {
                return Err(PkPdError::Scenario(format!(
                    "locus {extra} included in restriction of phenotype {this_name} but not {first_name}"
                )));
            }
            if let Some(&missing) = first_loci.difference(&this_loci).next() {
                return Err(PkPdError::Scenario(format!(
                    "locus {missing} included in restriction of phenotype {first_name} but not {this_name}"
                )));
            }
        }
        let mut locus_idx: BTreeMap<&str, usize> = BTreeMap::new();
        let mut loc_alleles: Vec<Vec<u32>> = Vec::new();
        for r in &ph.restrictions {
            let allele = allele_lookup(&r.on_locus, &r.to_allele).ok_or_else(|| {
                PkPdError::Scenario(format!(
                    "phenotype has restriction on locus {} allele {} but this locus/allele has not been defined in parasiteGenetics section",
                    r.on_locus, r.to_allele
                ))
            })?;
            if let Some(&idx) = locus_idx.get(r.on_locus.as_str()) {
                loc_alleles[idx].push(allele);
            } else {
                locus_idx.insert(&r.on_locus, loc_alleles.len());
                loc_alleles.push(vec![allele]);
            }
        }
        restrictions_by_phen.push(loc_alleles);
        phenotypes.push(DrugPd::from_phenotype(ph)?);
    }

    if first_loci.is_empty() {
        if pd_specs.len() > 1 {
            return Err(PkPdError::Scenario(
                "pharmacology/drugs/drug/pd/phenotype: restrictions required when num. phenotypes > 1"
                    .into(),
            ));
        }
        return Ok((phenotypes, vec![0; genotypes.len().max(1)]));
    }
    if genotypes.is_empty() {
        return Err(PkPdError::Scenario(
            "phenotype restrictions too restrictive: no phenotype matching some genotype".into(),
        ));
    }

    let mut mapping = Vec::with_capacity(genotypes.len());
    for g_alleles in genotypes {
        let mut matched: Option<usize> = None;
        for (i, rules) in restrictions_by_phen.iter().enumerate() {
            if rules
                .iter()
                .all(|loc| loc.iter().any(|a| g_alleles.contains(a)))
            {
                if matched.is_some() {
                    return Err(PkPdError::Scenario(
                        "phenotype restrictions not restrictive enough: multiple phenotypes match some genotype"
                            .into(),
                    ));
                }
                matched = Some(i);
            }
        }
        mapping.push(matched.ok_or_else(|| {
            PkPdError::Scenario(
                "phenotype restrictions too restrictive: no phenotype matching some genotype"
                    .into(),
            )
        })?);
    }
    Ok((phenotypes, mapping))
}
