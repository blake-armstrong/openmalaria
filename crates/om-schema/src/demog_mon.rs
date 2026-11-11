//! Demography and Monitoring AST definitions (`schema/demography.xsd`, `schema/monitoring.xsd`).

use crate::common::OptionElt;
use serde::{Deserialize, Serialize};

/// Human age structure and population size (`om:Demography`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Demography {
    pub name: String,
    pub pop_size: i32,
    pub maximum_age_yrs: f64,
    pub age_group: DemogAgeGroup,
}

/// Demographic age brackets (`om:DemogAgeGroup`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct DemogAgeGroup {
    pub lowerbound: f64,
    pub groups: Vec<DemogGroupBounds>,
}

/// Single demographic bracket (`<group poppercent="..." upperbound="..."/>`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct DemogGroupBounds {
    pub poppercent: f64,
    pub upperbound: f64,
}

/// Top-level `<monitoring>` configuration (`om:Monitoring`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Monitoring {
    pub name: String,
    pub start_date: Option<String>,
    pub continuous: Option<MonitoringContinuous>,
    pub survey_options: MonitoringOptions,
    pub surveys: Surveys,
    pub age_group: MonAgeGroup,
    pub cohorts: Vec<CohortSubPop>,
}

/// Continuous time-series output settings (`<continuous period="..." duringInit="...">`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MonitoringContinuous {
    pub period: String,
    pub during_init: bool,
    pub options: Vec<OptionElt>,
}

/// `<SurveyOptions onlyNewEpisode="...">` measure list (`om:MonitoringOptions`).
///
/// Stored as a `Vec` rather than a `HashMap` because measures such as `nCMDTReport`
/// may appear multiple times with distinct `output_number` values.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MonitoringOptions {
    pub only_new_episode: bool,
    pub options: Vec<MonitoringOption>,
}

/// Single `<option>` inside `<SurveyOptions>` (`om:MonitoringOption`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MonitoringOption {
    pub name: String,
    pub value: bool,
    pub output_number: Option<i32>,
    pub by_age: Option<bool>,
    pub by_cohort: Option<bool>,
    pub by_species: Option<bool>,
    pub by_genotype: Option<bool>,
    pub by_drug_type: Option<bool>,
}

/// `<surveys>` schedule and patent-detection configuration.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Surveys {
    pub detection_limit: Option<f64>,
    pub diagnostic: Option<String>,
    pub survey_times: Vec<SurveyTime>,
}

/// Single `<surveyTime repeatStep="..." repeatEnd="..." reported="...">...</surveyTime>` entry.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct SurveyTime {
    pub time: String,
    pub repeat_step: Option<String>,
    pub repeat_end: Option<String>,
    pub reported: bool,
}

/// Survey reporting age brackets (`om:MonAgeGroup`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct MonAgeGroup {
    pub lowerbound: f64,
    pub upperbounds: Vec<f64>,
}

/// Sub-population bitmask entry (`<subPop id="..." number="..."/>`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct CohortSubPop {
    pub id: String,
    pub number: i64,
}
