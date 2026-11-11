//! Root `Scenario`, `ModelSpec`, `ParasiteGeneticsSpec`, `DiagnosticSpec`, and `VivaxSpec` (`schema/scenario.xsd`, `schema/vivax.xsd`).

use crate::common::{AgeGroupValues, OptionElt, SampledValueLn, WeibullSample};
use crate::demog_mon::{Demography, Monitoring};
use crate::entomology::Entomology;
use crate::error::SchemaError;
use crate::health_system::{ClinicalSpec, HealthSystemSpec};
use crate::interventions::InterventionsSpec;
use crate::pharmacology::PharmacologySpec;
use serde::{Deserialize, Serialize};
use std::path::Path;

/// Complete OpenMalaria Schema v50 scenario AST (`<om:scenario>` / `<scenario>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Scenario {
    pub schema_version: i32,
    pub name: String,
    pub analysis_no: Option<i32>,
    pub wu_id: Option<i32>,
    pub demography: Demography,
    pub monitoring: Monitoring,
    pub interventions: InterventionsSpec,
    pub health_system: HealthSystemSpec,
    pub entomology: Entomology,
    pub parasite_genetics: Option<ParasiteGeneticsSpec>,
    pub pharmacology: Option<PharmacologySpec>,
    pub diagnostics: Vec<DiagnosticSpec>,
    pub model: ModelSpec,
}

impl Scenario {
    /// Parses and post-validates an OpenMalaria scenario XML string.
    pub fn from_xml_str(xml: &str) -> Result<Self, SchemaError> {
        let scenario = crate::parser_root::parse_scenario_xml(xml)?;
        scenario.validate()?;
        Ok(scenario)
    }

    /// Loads, parses, and post-validates an OpenMalaria scenario XML file from disk.
    pub fn from_xml_file(path: impl AsRef<Path>) -> Result<Self, SchemaError> {
        let xml = std::fs::read_to_string(path)?;
        Self::from_xml_str(&xml)
    }

    /// Runs all Schema v50 post-validation checks (`XMLChecker`, `schemaVersion`, `ModelName`, `ModelOptions`, `Parameters`).
    ///
    /// Returns `Ok(Some(warning))` if `schema_version < 50` (matching `DocumentLoader.cpp` deprecation warning).
    pub fn validate(&self) -> Result<Option<String>, SchemaError> {
        crate::validate::validate_scenario(self)
    }

    /// Overrides the master PRNG seed (`<computationParameters iseed="..."/>`).
    #[must_use]
    pub fn with_seed(mut self, seed: i32) -> Self {
        self.model.iseed = seed;
        self
    }

    /// Overrides the simulated human population size (`<demography popSize="..."/>`).
    #[must_use]
    pub fn with_pop_size(mut self, pop_size: i32) -> Self {
        self.demography.pop_size = pop_size;
        self
    }

    /// Overrides or inserts a model parameter (`<parameter number="..." value="..."/>`).
    #[must_use]
    pub fn with_parameter(mut self, number: i32, value: f64) -> Self {
        let block = self.model.parameters.get_or_insert_with(|| ParametersBlock {
            interval: 5,
            latentp: "3t".into(),
            entries: Vec::new(),
        });
        if let Some(existing) = block.entries.iter_mut().find(|e| e.number == number) {
            existing.value = value;
        } else {
            block.entries.push(ParameterEntry {
                name: None,
                number,
                value,
            });
        }
        self
    }

    /// Overrides or inserts a boolean `<ModelOptions>` flag.
    #[must_use]
    pub fn with_option(mut self, name: impl Into<String>, value: bool) -> Self {
        let name = name.into();
        let opts = self.model.model_options.get_or_insert_with(Vec::new);
        if let Some(existing) = opts.iter_mut().find(|o| o.name == name) {
            existing.value = value;
        } else {
            opts.push(OptionElt { name, value });
        }
        self
    }
}

/// `<parasiteGenetics samplingMode="initial|tracking">` configuration.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ParasiteGeneticsSpec {
    pub sampling_mode: String,
    pub loci: Vec<ParasiteLocus>,
}

/// Genetic `<locus name="...">`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ParasiteLocus {
    pub name: String,
    pub alleles: Vec<ParasiteAllele>,
}

/// Genetic `<allele name="..." initialFrequency="..." fitness="..." hrp2_deletion="..."/>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ParasiteAllele {
    pub name: String,
    pub initial_frequency: f64,
    pub fitness: f64,
    pub hrp2_deletion: bool,
}

/// `<diagnostic name="..." units="..." mechanism="...">`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DiagnosticSpec {
    pub name: String,
    pub units: Option<String>,
    pub mechanism: String,
    pub body: DiagnosticBody,
}

/// Deterministic threshold vs stochastic Hill curve diagnostic test.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum DiagnosticBody {
    Deterministic { min_density: f64 },
    Stochastic { dens_50: f64, specificity: f64 },
}

/// `<model>` block containing options, clinical, human, vivax, seed, and parameters.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ModelSpec {
    pub model_name: Option<String>,
    /// `None` when `<ModelOptions>` tag is absent; `Some(vec)` when present (even if empty!).
    pub model_options: Option<Vec<OptionElt>>,
    pub clinical: ClinicalSpec,
    pub human: HumanSpec,
    pub vivax: Option<VivaxSpec>,
    pub iseed: i32,
    pub parameters: Option<ParametersBlock>,
}

/// `<human>` availability-by-age and optional body-mass-by-age curves.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HumanSpec {
    pub availability_to_mosquitoes: AgeGroupValues,
    pub weight: Option<HumanWeightSpec>,
}

/// `<weight multStdDev="...">` table.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HumanWeightSpec {
    pub mult_std_dev: f64,
    pub values: AgeGroupValues,
}

/// `<parameters interval="..." latentp="...">` block.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ParametersBlock {
    pub interval: i32,
    pub latentp: String,
    pub entries: Vec<ParameterEntry>,
}

/// Single `<parameter number="..." value="..." name="..."/>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ParameterEntry {
    pub name: Option<String>,
    pub number: i32,
    pub value: f64,
}

/// *Plasmodium vivax* parameters (`om:Vivax`, `schema/vivax.xsd`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VivaxSpec {
    pub prob_blood_stage_infectious_to_mosq: f64,
    pub hypnozoite_release: HypnozoiteReleaseSpec,
    pub blood_stage_protection_latency: f64,
    pub blood_stage_length_days: WeibullSample,
    pub clinical_events: VivaxClinicalEvents,
}

/// `<hypnozoiteRelease pSecondRelease="...">`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HypnozoiteReleaseSpec {
    pub p_second_release: f64,
    pub max_hypnozoites: i32,
    pub base_hypnozoites: f64,
    pub first_release_days: HypnozoiteReleaseDist,
    pub second_release_days: Option<HypnozoiteReleaseDist>,
}

/// `<firstReleaseDays>` / `<secondReleaseDays>` (`SampledValueLN` + `latentRelapse`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HypnozoiteReleaseDist {
    pub latent_relapse: f64,
    pub dist: SampledValueLn,
}

/// `<clinicalEvents>` for *P. vivax* primary and relapse clinical event probabilities.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VivaxClinicalEvents {
    pub p_primary_infection: ProbVivaxEvent,
    pub p_relapse_one: ProbVivaxEvent,
    pub p_relapse_two_plus: ProbVivaxEvent,
    pub p_event_is_severe: f64,
    pub vivax_clin_option: String,
}

/// `<pPrimaryInfection a="..." b="..."/>` pair.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct ProbVivaxEvent {
    pub a: f64,
    pub b: f64,
}
