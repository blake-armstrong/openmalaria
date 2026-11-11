//! Human-specific intervention component and deployment AST types (`schema/interventions.xsd`).

use crate::common::{DecaySpec, SampledValueLn, SampledValueN};
use crate::decision_tree::DecisionTree;
use crate::pharmacology::AlleleRestriction;
use serde::{Deserialize, Serialize};

/// `<human>` interventions block (`om:HumanInterventions`).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct HumanInterventionsSpec {
    pub components: Vec<HumanComponent>,
    pub deployments: Vec<DeploymentRule>,
}

/// Single `<component id="..." name="...">` inside `<human>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct HumanComponent {
    pub id: String,
    pub name: Option<String>,
    pub kind: HumanComponentKind,
    pub sub_pop_removal: Option<SubPopRemoval>,
}

/// Polymorphic human intervention component action.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum HumanComponentKind {
    Screen(ScreenSpec),
    TreatSimple {
        duration_liver: String,
        duration_blood: String,
    },
    TreatPkPd {
        schedule: String,
        dosage: String,
        delay_h: f64,
    },
    DecisionTree(DecisionTree),
    Pev(VaccineDesc),
    Bsv(VaccineDesc),
    Tbv(VaccineDesc),
    Itn(Box<ItnDesc>),
    Irs(Box<IrsDesc>),
    Gvi(Box<GviDesc>),
    RecruitmentOnly,
    ClearImmunity,
}

/// Diagnostic screen-and-treat component (`<screen diagnostic="...">`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ScreenSpec {
    pub diagnostic: String,
    pub positive_components: Vec<String>,
    pub negative_components: Vec<String>,
}

/// Pre-erythrocytic, blood-stage, or transmission-blocking vaccine (`om:VaccineDescription`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VaccineDesc {
    pub decay: DecaySpec,
    pub efficacy_b: f64,
    pub initial_efficacy: Vec<f64>,
    pub phenotypes: Vec<VaccinePhenotype>,
}

/// Genotype-restricted vaccine efficacy (`<phenotype>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VaccinePhenotype {
    pub name: Option<String>,
    pub restrictions: Vec<AlleleRestriction>,
    pub initial_efficacy: Vec<f64>,
}

/// Insecticide-treated net parameterization (`om:ITNDescription`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ItnDesc {
    pub usage: Option<f64>,
    pub hole_rate: SampledValueLn,
    pub rip_rate: SampledValueLn,
    pub rip_factor: f64,
    pub initial_insecticide: SampledValueN,
    pub insecticide_decay: DecaySpec,
    pub attrition_of_nets: DecaySpec,
    pub anopheles_params: Vec<ItnAnophParams>,
}

/// Species-specific ITN deterrent and killing curves.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ItnAnophParams {
    pub mosquito: String,
    pub prop_active: f64,
    pub hole_index_max: Option<f64>,
    pub deterrency: ItnDeterrencyMode,
    pub preprandial_killing: ItnEffectMode,
    pub postprandial_killing: ItnEffectMode,
    pub fecundity_reduction: Option<ItnEffectMode>,
}

/// Single-stage vs two-stage ITN deterrency.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum ItnDeterrencyMode {
    SingleStage {
        insecticide_factor: f64,
        insecticide_scaling_factor: f64,
        hole_factor: f64,
        interaction_factor: f64,
        hole_scaling_factor: f64,
    },
    TwoStage {
        entering: ItnEnteringMode,
        attacking: ItnEffectMode,
    },
}

/// Entering deterrency inside `<twoStageDeterrency>`.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum ItnEnteringMode {
    Exponential {
        insecticide_factor: f64,
        insecticide_scaling_factor: f64,
    },
    Logit {
        base_factor: f64,
        insecticide_factor: f64,
    },
}

/// Standard exponential vs logit ITN effect curve.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum ItnEffectMode {
    Exponential(ItnKillingEffect),
    Logit(ItnEffectLogit),
}

/// 6-parameter ITN exponential killing/attacking effect (`om:ITNKillingEffect`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct ItnKillingEffect {
    pub base_factor: f64,
    pub insecticide_factor: f64,
    pub insecticide_scaling_factor: f64,
    pub hole_factor: f64,
    pub interaction_factor: f64,
    pub hole_scaling_factor: f64,
}

/// 4-parameter ITN logit effect (`om:ITNEffectLogit`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct ItnEffectLogit {
    pub base_factor: f64,
    pub insecticide_factor: f64,
    pub hole_factor: f64,
    pub interaction_factor: f64,
}

/// Indoor residual spraying parameterization (`om:IRSDescription`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct IrsDesc {
    pub usage: Option<f64>,
    pub initial_insecticide: SampledValueN,
    pub insecticide_decay: DecaySpec,
    pub anopheles_params: Vec<IrsAnophParams>,
}

/// Species-specific IRS deterrent and killing parameters.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct IrsAnophParams {
    pub mosquito: String,
    pub prop_active: f64,
    pub deterrency_insecticide_factor: f64,
    pub deterrency_scaling_factor: f64,
    pub preprandial_killing: IrsKillingEffect,
    pub postprandial_killing: IrsKillingEffect,
    pub fecundity_reduction: Option<IrsKillingEffect>,
}

/// 3-parameter IRS killing effect (`om:IRSKillingEffect`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct IrsKillingEffect {
    pub base_factor: f64,
    pub insecticide_factor: f64,
    pub insecticide_scaling_factor: f64,
}

/// Generic vector intervention (`om:GVIDescription`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct GviDesc {
    pub usage: Option<f64>,
    pub decay: DecaySpec,
    pub anopheles_params: Vec<GviAnophParams>,
}

/// Species-specific GVI effects.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct GviAnophParams {
    pub mosquito: String,
    pub prop_active: f64,
    pub deterrency: Option<f64>,
    pub preprandial_killing_effect: Option<f64>,
    pub postprandial_killing_effect: Option<f64>,
    pub fecundity_reduction: Option<f64>,
}

/// Sub-population expiry rules (`<subPopRemoval .../>`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct SubPopRemoval {
    pub on_first_bout: bool,
    pub on_first_treatment: bool,
    pub on_first_infection: bool,
    pub after_years: Option<f64>,
}

/// `<deployment name="...">` rule inside `<human>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DeploymentRule {
    pub name: Option<String>,
    pub components: Vec<String>,
    pub conditions: Vec<DeployCondition>,
    pub continuous: Vec<ContinuousDeployBlock>,
    pub timed: Vec<TimedDeployBlock>,
}

/// Dynamic threshold trigger (`<condition measure="..." minValue="..." maxValue="..." initialState="..."/>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DeployCondition {
    pub measure: String,
    pub min_value: Option<f64>,
    pub max_value: Option<f64>,
    pub initial_state: bool,
}

/// Cohort eligibility filter (`<restrictToSubPop id="..." complement="..."/>`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct SubPopRestriction {
    pub id: String,
    pub complement: bool,
}

/// Shared coverage/dose/copula attributes (`om:DeploymentBase`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct DeploymentBase {
    pub coverage: f64,
    pub vacc_min_prev_doses: Option<i32>,
    pub vacc_max_cum_doses: Option<i32>,
    pub min_availability: Option<i32>,
    pub max_availability: Option<i32>,
    pub coverage_corr: Option<f64>,
    pub coverage_var: Option<f64>,
}

/// `<continuous>` age-targeted deployment block.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ContinuousDeployBlock {
    pub restrict_to_sub_pop: Option<SubPopRestriction>,
    pub deploys: Vec<ContinuousDeploy>,
}

/// Single `<deploy targetAgeYrs="..." begin="..." end="..."/>` inside `<continuous>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ContinuousDeploy {
    pub base: DeploymentBase,
    pub target_age_yrs: f64,
    pub begin: Option<String>,
    pub end: Option<String>,
}

/// `<timed>` mass deployment block.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct TimedDeployBlock {
    pub restrict_to_sub_pop: Option<SubPopRestriction>,
    pub cumulative_coverage_component: Option<String>,
    pub deploys: Vec<MassDeploy>,
}

/// Single `<deploy time="..." minAge="..." maxAge="..." repeatStep="..." repeatEnd="..."/>` inside `<timed>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct MassDeploy {
    pub base: DeploymentBase,
    pub time: String,
    pub min_age: f64,
    pub max_age: Option<f64>,
    pub repeat_step: Option<String>,
    pub repeat_end: Option<String>,
}
