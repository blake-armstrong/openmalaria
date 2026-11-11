//! Top-level `<interventions>` and system/vector intervention AST types (`schema/interventions.xsd`).

use crate::common::DecaySpec;
use crate::entomology::NonVectorSpec;
use crate::health_system::HealthSystemSpec;
pub use crate::interventions_human::*;
use serde::{Deserialize, Serialize};

/// Top-level `<interventions name="...">` specification (`om:Interventions`).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct InterventionsSpec {
    pub name: String,
    pub change_hs: Vec<TimedHealthSystem>,
    pub change_eir: Vec<TimedNonVector>,
    pub imported_infections: Option<ImportedInfectionsSpec>,
    pub insert_r0_case: Vec<String>,
    pub uninfect_vectors: Vec<String>,
    pub vector_pop: Vec<VectorPopInterv>,
    pub vector_trap: Vec<VectorTrapInterv>,
    pub non_human_hosts_modifications: Vec<NonHumanModInterv>,
    pub add_non_human_hosts: Vec<AddNonHumanInterv>,
    pub human: Option<HumanInterventionsSpec>,
}

/// Timed replacement health system (`<changeHS><timedDeployment time="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct TimedHealthSystem {
    pub time: String,
    pub health_system: HealthSystemSpec,
}

/// Timed replacement non-vector EIR schedule (`<changeEIR><timedDeployment time="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct TimedNonVector {
    pub time: String,
    pub non_vector: NonVectorSpec,
}

/// Imported infections schedule (`<importedInfections>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ImportedInfectionsSpec {
    pub name: Option<String>,
    pub period: String,
    pub rates: Vec<ImportedRate>,
}

/// Single `<rate time="..." value="..."/>` entry inside `<importedInfections>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ImportedRate {
    pub time: String,
    pub value: f64,
}

/// `<vectorPop><intervention name="...">` (larviciding, ATSB, adulticiding).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorPopInterv {
    pub name: String,
    pub anopheles: Vec<VectorSpeciesInterv>,
    pub timed_deployments: Vec<String>,
}

/// Species-specific `<anopheles mosquito="...">` inside `<vectorPop>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorSpeciesInterv {
    pub mosquito: String,
    pub seeking_death_rate_increase: Option<DecayEffect>,
    pub prob_death_ovipositing: Option<DecayEffect>,
    pub emergence_reduction: Option<DecayEffect>,
    pub prob_additional_death_sugar_feeding: Option<DecayEffect>,
}

/// Initial scalar effect paired with a `<decay>` curve (`@initial` + `<decay .../>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DecayEffect {
    pub initial: f64,
    pub decay: DecaySpec,
}

/// `<vectorTrap><intervention>` baited trap intervention.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorTrapInterv {
    pub name: Option<String>,
    pub descriptions: Vec<VectorTrapDesc>,
    pub deployments: Vec<VectorTrapDeploy>,
}

/// Species availability and decay for `<vectorTrap>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorTrapDesc {
    pub mosquito: String,
    pub relative_availability: f64,
    pub decay_of_availability: DecaySpec,
}

/// Timed deployment of vector traps (`<deploy time="..." ratioToHumans="..." lifespan="..."/>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorTrapDeploy {
    pub time: String,
    pub ratio_to_humans: f64,
    pub lifespan: String,
}

/// `<nonHumanHostsModifications><intervention name="..." nonHumanHostsName="...">`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct NonHumanModInterv {
    pub name: String,
    pub non_human_hosts_name: String,
    pub decay: DecaySpec,
    pub anopheles: Vec<NonHumanModAnoph>,
    pub timed_deployments: Vec<String>,
}

/// Species effects on modified non-human hosts.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct NonHumanModAnoph {
    pub mosquito: String,
    pub availability_reduction: Option<f64>,
    pub preprandial_killing_effect: Option<f64>,
    pub postprandial_killing_effect: Option<f64>,
    pub resting_killing_effect: Option<f64>,
    pub fecundity_reduction: Option<f64>,
}

/// `<addNonHumanHosts><nonHumanHosts name="...">` dynamic introduction of alternative hosts.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AddNonHumanInterv {
    pub name: String,
    pub anopheles: Vec<AddNonHumanAnoph>,
    pub deployments: Vec<AddNonHumanDeploy>,
}

/// Species bionomics on newly introduced non-human hosts.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AddNonHumanAnoph {
    pub mosquito: String,
    pub mosq_relative_availability_human: f64,
    pub mosq_prob_biting: f64,
    pub mosq_prob_find_rest_site: f64,
    pub mosq_prob_resting: f64,
    pub host_fecundity_factor: f64,
}

/// Timed deployment of additional non-human hosts (`<deploy time="..." lifespan="..."/>`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct AddNonHumanDeploy {
    pub time: String,
    pub lifespan: String,
}
