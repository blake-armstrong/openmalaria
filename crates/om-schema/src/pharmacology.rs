//! Pharmacology PK/PD AST definitions (`schema/pharmacology.xsd`).

use crate::common::SampledValueLn;
use serde::{Deserialize, Serialize};

/// Top-level `<pharmacology>` library (`om:Pharmacology`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PharmacologySpec {
    pub schedules: Vec<PkPdSchedule>,
    pub dosages: Vec<PkPdDosages>,
    pub drugs: Vec<PkPdDrug>,
}

/// Named dosing schedule (`<schedule name="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkPdSchedule {
    pub name: String,
    pub medications: Vec<MedicateStep>,
}

/// Single `<medicate drug="..." mg="..." hour="..."/>` step.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct MedicateStep {
    pub drug: String,
    pub mg: f64,
    pub hour: f64,
}

/// Named dosage multiplier table (`<dosages name="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkPdDosages {
    pub name: String,
    pub mode: DosageMode,
}

/// Choice of `<age>`, `<bodymass>`, or `<multiply by="kg"/>` dosage scaling.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum DosageMode {
    Age(Vec<DoseStep>),
    BodyMass(Vec<DoseStep>),
    Multiply { by: String },
}

/// Bracket step `<age|bodymass lowerbound="..." dose_mult="..."/>`.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct DoseStep {
    pub lowerbound: f64,
    pub dose_mult: f64,
}

/// Single `<drug abbrev="...">` PK/PD definition (`om:PKPDDrug`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkPdDrug {
    pub abbrev: String,
    pub pd: PdSpec,
    pub pk: PkSpec,
}

/// Pharmacodynamics section (`<PD locus="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PdSpec {
    pub locus: Option<String>,
    pub phenotypes: Vec<PdPhenotype>,
}

/// Resistance/sensitivity `<phenotype name="...">` entry.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PdPhenotype {
    pub name: Option<String>,
    pub restrictions: Vec<AlleleRestriction>,
    pub max_killing_rate: f64,
    pub ic50: SampledValueLn,
    pub slope: f64,
}

/// `<restriction onLocus="..." toAllele="..."/>`.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct AlleleRestriction {
    pub on_locus: String,
    pub to_allele: String,
}

/// Pharmacokinetics section (`<PK>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkSpec {
    pub negligible_concentration: f64,
    pub elimination: PkElimination,
    pub k_a: Option<SampledValueLn>,
    pub conversion: Option<PkConversion>,
    pub vol_dist: SampledValueLn,
    pub compartment2: Option<PkCompartmentPair>,
    pub compartment3: Option<PkCompartmentPair>,
}

/// Choice between constant `half_life` (days) and allometric `(k, m_exponent)`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum PkElimination {
    HalfLife(f64),
    EliminationRate { k: SampledValueLn, m_exponent: f64 },
}

/// Parent-to-metabolite conversion (`<conversion>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkConversion {
    pub metabolite: String,
    pub rate: SampledValueLn,
    pub mol_ratio: f64,
    pub ic50_log_correlation: f64,
}

/// Peripheral compartment exchange rates (`<compartment2>` or `<compartment3>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct PkCompartmentPair {
    pub k_in: SampledValueLn,
    pub k_out: SampledValueLn,
}
