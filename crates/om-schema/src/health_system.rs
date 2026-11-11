//! Clinical and HealthSystem AST definitions (`schema/healthSystem.xsd`).

use crate::common::{AgeGroupValues, TriggeredDeployGroup};
use crate::decision_tree::DecisionTree;
use serde::{Deserialize, Serialize};

/// Clinical model settings under `<model><clinical>` (`om:Clinical`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ClinicalSpec {
    pub health_system_memory: String,
    pub neonatal_mortality_diagnostic: Option<String>,
    pub non_malaria_fevers: Option<ClinicalNmf>,
}

/// Non-malaria fever incidence and treatment-need curves (`om:ClinicalNMF`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ClinicalNmf {
    pub incidence: AgeGroupValues,
    pub pr_need_treatment_nmf: Option<AgeGroupValues>,
    pub pr_need_treatment_mf: Option<AgeGroupValues>,
}

/// Top-level `<healthSystem>` specification (`om:HealthSystem`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HealthSystemSpec {
    pub variant: HealthSystemVariant,
    pub cfr: AgeGroupValues,
    pub p_sequelae_inpatient: AgeGroupValues,
}

/// Polymorphic case-management submodel inside `<healthSystem>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum HealthSystemVariant {
    ImmediateOutcomes(Box<HsImmediateOutcomes>),
    DecisionTree5Day(Box<HsDecisionTree5Day>),
    EventScheduler(Box<HsEventScheduler>),
}

/// Vivax primaquine/tafenoquine radical-cure policy (`om:LiverStageDrug`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct LiverStageDrugSpec {
    pub p_human_cannot_receive: f64,
    pub ignore_cannot_receive: Option<bool>,
    pub p_use_uncomplicated: Option<f64>,
    pub effectiveness_on_use: f64,
}

/// 5-day `<ImmediateOutcomes>` health system (`om:HSImmediateOutcomes`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HsImmediateOutcomes {
    pub name: Option<String>,
    pub use_diagnostic_uc: bool,
    pub drug_regimen: DrugRegimen,
    pub initial_acr: TreatmentDetails,
    pub compliance: TreatmentDetails,
    pub non_compliers_effective: TreatmentDetails,
    pub treatment_actions: TreatmentActions,
    pub p_seek_official_care_uncomplicated_1: f64,
    pub p_self_treat_uncomplicated: f64,
    pub p_seek_official_care_uncomplicated_2: f64,
    pub p_seek_official_care_severe: f64,
    pub liver_stage_drug: Option<LiverStageDrugSpec>,
}

/// Named drug regimens (`<drugRegimen firstLine="..." secondLine="..." inpatient="..."/>`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct DrugRegimen {
    pub first_line: String,
    pub second_line: String,
    pub inpatient: String,
}

/// Per-drug probabilities (`CQ`, `SP`, `AQ`, `SPAQ`, `ACT`, `QN`, `selfTreatment`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct TreatmentDetails {
    pub cq: Option<f64>,
    pub sp: Option<f64>,
    pub aq: Option<f64>,
    pub spaq: Option<f64>,
    pub act: Option<f64>,
    pub qn: Option<f64>,
    pub self_treatment: f64,
}

/// Per-drug treatment actions (`<treatmentActions>`).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct TreatmentActions {
    pub cq: Option<TreatmentOption>,
    pub sp: Option<TreatmentOption>,
    pub aq: Option<TreatmentOption>,
    pub spaq: Option<TreatmentOption>,
    pub act: Option<TreatmentOption>,
    pub qn: Option<TreatmentOption>,
}

/// Triggered deployments + deterministic clearance actions (`om:TreatmentOption`).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct TreatmentOption {
    pub name: Option<String>,
    pub deployments: Vec<TriggeredDeployGroup>,
    pub clear_infections: Vec<ClearInfectionsSpec>,
}

/// `<clearInfections timesteps="..." stage="liver|blood|both"/>`.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ClearInfectionsSpec {
    pub timesteps: String,
    pub stage: String,
}

/// 5-day `<DecisionTree5Day>` health system (`om:HSDT5Day`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HsDecisionTree5Day {
    pub name: Option<String>,
    pub p_seek_official_care_uncomplicated_1: f64,
    pub p_self_treat_uncomplicated: f64,
    pub p_seek_official_care_uncomplicated_2: f64,
    pub p_seek_official_care_severe: f64,
    pub liver_stage_drug: Option<LiverStageDrugSpec>,
    pub tree_uc_official: DecisionTree,
    pub tree_uc_self_treat: DecisionTree,
    pub cure_rate_severe: f64,
    pub treatment_severe: TreatmentOption,
}

/// 1-day `<EventScheduler>` health system (`om:HSEventScheduler`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HsEventScheduler {
    pub uncomplicated: DecisionTree,
    pub complicated: DecisionTree,
    pub clinical_outcomes: ClinicalOutcomesSpec,
    pub non_malaria_fevers: Option<HsEsNmf>,
}

/// `<ClinicalOutcomes>` durations and daily treatment-seeking probabilities (`om:ClinicalOutcomes`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ClinicalOutcomesSpec {
    pub max_uc_seeking_memory: i32,
    pub uncomplicated_case_duration: i32,
    pub complicated_case_duration: i32,
    pub complicated_risk_duration: i32,
    pub daily_pr_imm_ucts: Vec<f64>,
}

/// Non-malaria fever management parameters in `<EventScheduler>` (`om:HSESNMF`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HsEsNmf {
    pub pr_treatment: f64,
    pub effect_negative_test: f64,
    pub effect_positive_test: f64,
    pub effect_need: f64,
    pub effect_informal: f64,
    pub cfr: AgeGroupValues,
    pub treatment_efficacy: f64,
}
