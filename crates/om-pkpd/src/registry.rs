//! Instance-owned [`DrugRegistry`] replacing global `LSTMDrugType` and `LSTMTreatments` tables
//! (`model/PkPd/Drug/LSTMDrugType.cpp`, `model/PkPd/LSTMTreatments.cpp`).

use crate::conversion::ConversionDrug;
use crate::dosage::{DosageTable, MedicateData, Schedule};
use crate::drug_type::{DrugModelKind, DrugType};
use crate::error::PkPdError;
use crate::one_comp::OneCompDrug;
use crate::patient_model::DrugInstance;
use crate::three_comp::ThreeCompDrug;
use om_rng::LocalRng;
use om_schema::Scenario;
use om_schema::pharmacology::PharmacologySpec;
use om_schema::scenario::ParasiteGeneticsSpec;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

/// Lookup table mapping `(locus_name, allele_name)` pairs to sequential allele codes.
pub type AlleleCodeMap = BTreeMap<(String, String), u32>;

/// Builds the Cartesian-product genotype allele sets and `(locus, allele) -> code` map
/// matching `OM::WithinHost::Genotypes::init` (`model/Host/WithinHost/Genotypes.cpp`).
#[must_use]
pub fn build_genotype_alleles(
    genetics: Option<&ParasiteGeneticsSpec>,
) -> (Vec<BTreeSet<u32>>, AlleleCodeMap) {
    let Some(spec) = genetics else {
        return (vec![BTreeSet::from([0_u32])], BTreeMap::new());
    };
    if spec.loci.is_empty() {
        return (vec![BTreeSet::from([0_u32])], BTreeMap::new());
    }
    let mut allele_codes = BTreeMap::new();
    let mut next_code = 0_u32;
    let mut genotypes: Vec<BTreeSet<u32>> = Vec::new();

    for (locus_idx, locus) in spec.loci.iter().enumerate() {
        let mut locus_sets = Vec::with_capacity(locus.alleles.len());
        for allele in &locus.alleles {
            let code = next_code;
            next_code += 1;
            allele_codes.insert((locus.name.clone(), allele.name.clone()), code);
            locus_sets.push(BTreeSet::from([code]));
        }
        if locus_idx == 0 {
            genotypes = locus_sets;
        } else {
            let mut crossed = Vec::with_capacity(genotypes.len() * locus_sets.len());
            for existing in &genotypes {
                for added in &locus_sets {
                    let mut combined = existing.clone();
                    combined.extend(added.iter().copied());
                    crossed.push(combined);
                }
            }
            genotypes = crossed;
        }
    }
    (genotypes, allele_codes)
}

/// Instance-owned library of drug types, schedules, dosage tables, and active monitoring indices.
#[derive(Default, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DrugRegistry {
    drugs: Vec<DrugType>,
    drug_names: BTreeMap<String, usize>,
    drugs_in_use: Vec<usize>,
    schedules: Vec<Schedule>,
    schedule_names: BTreeMap<String, usize>,
    dosages: Vec<DosageTable>,
    dosage_names: BTreeMap<String, usize>,
}

impl DrugRegistry {
    /// Creates an empty [`DrugRegistry`] (used when `<pharmacology>` is absent from a scenario).
    #[must_use]
    pub fn empty() -> Self {
        Self::default()
    }

    /// Builds a [`DrugRegistry`] directly from a full Schema v50 [`Scenario`].
    pub fn from_scenario(scenario: &Scenario) -> Result<Self, PkPdError> {
        let (genotypes, allele_map) = build_genotype_alleles(scenario.parasite_genetics.as_ref());
        Self::from_map(scenario.pharmacology.as_ref(), &genotypes, &allele_map)
    }

    /// Builds a [`DrugRegistry`] using a `(locus, allele) -> code` [`BTreeMap`].
    pub fn from_map(
        pharmacology: Option<&PharmacologySpec>,
        genotypes: &[BTreeSet<u32>],
        allele_map: &BTreeMap<(String, String), u32>,
    ) -> Result<Self, PkPdError> {
        Self::new(pharmacology, genotypes, |locus, allele| {
            allele_map
                .get(&(locus.to_owned(), allele.to_owned()))
                .copied()
        })
    }

    /// Builds and validates a [`DrugRegistry`] from an optional [`PharmacologySpec`],
    /// parasite genotype allele sets, and a `(locus, allele) -> Option<u32>` resolver.
    pub fn new<F>(
        pharmacology: Option<&PharmacologySpec>,
        genotypes: &[BTreeSet<u32>],
        allele_lookup: F,
    ) -> Result<Self, PkPdError>
    where
        F: Fn(&str, &str) -> Option<u32>,
    {
        let Some(pharm) = pharmacology else {
            return Ok(Self::empty());
        };
        let mut reg = Self::empty();
        reg.drugs.reserve(pharm.drugs.len());

        for drug_spec in &pharm.drugs {
            if reg.drug_names.contains_key(&drug_spec.abbrev) {
                return Err(PkPdError::Scenario(format!(
                    "Drug added twice: {}",
                    drug_spec.abbrev
                )));
            }
            let idx = reg.drugs.len();
            let dt = DrugType::from_spec(
                idx,
                drug_spec,
                genotypes,
                |met_abbrev| reg.use_drug(met_abbrev),
                &allele_lookup,
            )?;
            reg.drug_names.insert(drug_spec.abbrev.clone(), idx);
            reg.drugs.push(dt);
        }

        reg.schedules.reserve(pharm.schedules.len());
        for sched_spec in &pharm.schedules {
            let mut meds = Vec::with_capacity(sched_spec.medications.len());
            for m in &sched_spec.medications {
                let drug_idx = reg.use_drug(&m.drug)?;
                meds.push(MedicateData::from_hours(drug_idx, m.mg, m.hour));
            }
            let idx = reg.schedules.len();
            reg.schedule_names.insert(sched_spec.name.clone(), idx);
            reg.schedules.push(Schedule::new(sched_spec.name.clone(), meds));
        }

        reg.dosages.reserve(pharm.dosages.len());
        for dos_spec in &pharm.dosages {
            let table = DosageTable::from_spec(dos_spec)?;
            let idx = reg.dosages.len();
            reg.dosage_names.insert(dos_spec.name.clone(), idx);
            reg.dosages.push(table);
        }

        Ok(reg)
    }

    fn mark_drug_used(&mut self, index: usize) {
        if !self.drugs_in_use.contains(&index) {
            self.drugs_in_use.push(index);
        }
        if let Some(DrugModelKind::Conversion { metabolite, .. }) =
            self.drugs.get(index).map(DrugType::kind)
        {
            let met = *metabolite;
            if !self.drugs_in_use.contains(&met) {
                self.drugs_in_use.push(met);
            }
        }
    }

    /// Resolves `abbrev` to its drug index and marks it (and any metabolite) in `drugs_in_use`
    /// (`LSTMDrugType::findDrug`).
    pub fn use_drug(&mut self, abbrev: &str) -> Result<usize, PkPdError> {
        let idx = self.find_drug(abbrev)?;
        self.mark_drug_used(idx);
        Ok(idx)
    }

    /// Pure read-only lookup of a drug index by abbreviation (`<drug abbrev="...">`).
    pub fn find_drug(&self, abbrev: &str) -> Result<usize, PkPdError> {
        self.drug_names.get(abbrev).copied().ok_or_else(|| {
            PkPdError::Scenario(format!("attempt to use drug without description: {abbrev}"))
        })
    }

    /// Total number of registered drug types.
    #[must_use]
    pub fn num_drug_types(&self) -> usize {
        self.drugs.len()
    }

    /// Borrows a [`DrugType`] by index, returning [`PkPdError::InvalidDrugIndex`] if out of bounds.
    pub fn drug(&self, index: usize) -> Result<&DrugType, PkPdError> {
        self.drugs.get(index).ok_or(PkPdError::InvalidDrugIndex {
            index,
            count: self.drugs.len(),
        })
    }

    /// Optional non-throwing lookup of a [`DrugType`] by index.
    #[must_use]
    pub fn get_drug(&self, index: usize) -> Option<&DrugType> {
        self.drugs.get(index)
    }

    /// Ordered, deduplicated list of drug indices referenced by schedules or conversions (`getDrugsInUse`).
    #[must_use]
    pub fn drugs_in_use(&self) -> &[usize] {
        &self.drugs_in_use
    }

    /// Resolves a treatment [`Schedule`] index by name (`LSTMTreatments::findSchedule`).
    pub fn find_schedule(&self, name: &str) -> Result<usize, PkPdError> {
        if self.schedule_names.is_empty() {
            return Err(PkPdError::Scenario(
                "use of PK/PD treatment requires use of a compatible infection model and specification of pharmacology/treatments"
                    .into(),
            ));
        }
        self.schedule_names.get(name).copied().ok_or_else(|| {
            PkPdError::Scenario(format!("no treatment schedule with this name: {name}"))
        })
    }

    /// Number of registered treatment schedules.
    #[must_use]
    pub fn num_schedules(&self) -> usize {
        self.schedules.len()
    }

    /// Borrows a [`Schedule`] by index.
    pub fn schedule(&self, index: usize) -> Result<&Schedule, PkPdError> {
        self.schedules
            .get(index)
            .ok_or(PkPdError::InvalidScheduleIndex {
                index,
                count: self.schedules.len(),
            })
    }

    /// Number of registered dosage tables.
    #[must_use]
    pub fn num_dosages(&self) -> usize {
        self.dosages.len()
    }

    /// Resolves a [`DosageTable`] index by name (`LSTMTreatments::findDosages`).
    pub fn find_dosages(&self, name: &str) -> Result<usize, PkPdError> {
        if self.dosage_names.is_empty() {
            return Err(PkPdError::Scenario(
                "use of PK/PD treatment requires use of a compatible infection model and specification of pharmacology/treatments"
                    .into(),
            ));
        }
        self.dosage_names
            .get(name)
            .copied()
            .ok_or_else(|| PkPdError::Scenario(format!("no dosage table with this name: {name}")))
    }

    /// Borrows a [`DosageTable`] by index.
    pub fn dosage(&self, index: usize) -> Result<&DosageTable, PkPdError> {
        self.dosages.get(index).ok_or(PkPdError::InvalidDosageIndex {
            index,
            count: self.dosages.len(),
        })
    }

    /// Instantiates a per-human [`DrugInstance`] for `type_index`, consuming RNG draws in exact C++ order
    /// (`LSTMDrugType::createInstance`).
    pub fn create_instance(
        &self,
        rng: &mut LocalRng,
        type_index: usize,
    ) -> Result<DrugInstance, PkPdError> {
        let dt = self.drug(type_index)?;
        match dt.kind() {
            DrugModelKind::OneComp => Ok(DrugInstance::OneComp(OneCompDrug::new(dt, rng))),
            DrugModelKind::ThreeComp { .. } => {
                Ok(DrugInstance::ThreeComp(ThreeCompDrug::new(dt, rng)))
            }
            DrugModelKind::Conversion { metabolite, .. } => {
                let met_dt = self.drug(*metabolite)?;
                Ok(DrugInstance::Conversion(ConversionDrug::new(dt, met_dt, rng)))
            }
        }
    }
}
