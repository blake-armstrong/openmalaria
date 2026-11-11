//! Entomology AST definitions (`schema/entomology.xsd`).

use crate::common::{BetaMeanSample, SampledValueCv};
use serde::{Deserialize, Serialize};

/// Transmission mode (`mode="forced"` vs `mode="dynamic"`).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum EntomologyMode {
    Forced,
    Dynamic,
}

/// Top-level `<entomology>` block (`om:Entomology`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Entomology {
    pub name: String,
    pub mode: EntomologyMode,
    pub scaled_annual_eir: Option<f64>,
    pub backend: EntomologyBackend,
}

/// Choice between `<nonVector>` and `<vector>` transmission models.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum EntomologyBackend {
    NonVector(NonVectorSpec),
    Vector(VectorSpec),
}

/// Non-vector daily EIR schedule (`om:NonVector`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct NonVectorSpec {
    pub eip_duration: i32,
    pub eir_daily: Vec<EirDailyItem>,
}

/// Single `<EIRDaily origin="...">value</EIRDaily>` entry.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct EirDailyItem {
    pub value: f64,
    pub origin: Option<String>,
}

/// Vector transmission specification (`<vector>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct VectorSpec {
    pub anopheles: Vec<AnophelesParams>,
    pub non_human_hosts: Vec<NonHumanHostCount>,
}

/// Population size of a non-human host species (`<nonHumanHosts name="..." number="..."/>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct NonHumanHostCount {
    pub name: String,
    pub number: f64,
}

/// Per-mosquito-species bionomics and seasonality (`om:AnophelesParams`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AnophelesParams {
    pub mosquito: String,
    pub seasonality: SeasonalitySpec,
    pub mosq: MosqParams,
    pub life_cycle: Option<LifeCycleSpec>,
    pub simple_mpd: Option<SimpleMpdSpec>,
    pub non_human_hosts: Vec<AnophelesNonHumanParams>,
}

/// Annual seasonality specification (`<seasonality input="EIR" annualEIR="...">`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct SeasonalitySpec {
    pub input: String,
    pub annual_eir: Option<f64>,
    pub body: SeasonalityBody,
}

/// Polymorphic seasonality curve (`fourierSeries | monthlyValues | dailyValues`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum SeasonalityBody {
    FourierSeries {
        eir_rotate_angle: Option<f64>,
        coeffs: Vec<FourierCoeffic>,
    },
    MonthlyValues {
        smoothing: String,
        eir_rotate_angle: Option<f64>,
        values: Vec<f64>,
    },
    DailyValues {
        values: Vec<f64>,
    },
}

/// Harmonic pair `<coeffic a="..." b="..."/>`.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct FourierCoeffic {
    pub a: f64,
    pub b: f64,
}

/// Adult female mosquito feeding-cycle parameters (`<mosq>`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct MosqParams {
    pub min_infected_threshold: f64,
    pub mosq_rest_duration: i32,
    pub extrinsic_incubation_period: i32,
    pub mosq_laid_eggs_same_day_proportion: f64,
    pub mosq_seeking_duration: f64,
    pub mosq_survival_feeding_cycle_probability: f64,
    pub availability: SampledValueCv,
    pub mosq_prob_biting: BetaMeanSample,
    pub mosq_prob_find_rest_site: BetaMeanSample,
    pub mosq_prob_resting: BetaMeanSample,
    pub mosq_prob_ovipositing: f64,
    pub mosq_human_blood_index: f64,
}

/// Full aquatic mosquito life-cycle parameters (`om:LifeCycle`).
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct LifeCycleSpec {
    pub estimated_larval_resources: f64,
    pub egg_stage: MosqStage,
    pub larval_stage: MosqStage,
    pub larval_daily: Vec<LarvalDaily>,
    pub pupal_stage: MosqStage,
    pub female_eggs_laid_by_oviposit: f64,
}

/// Aquatic stage duration and survival (`om:MosqStage`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct MosqStage {
    pub duration: i32,
    pub survival: f64,
}

/// Daily larval resource consumption and competition (`<daily resourceUsage="..." effectCompetition="..."/>`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct LarvalDaily {
    pub resource_usage: f64,
    pub effect_competition: f64,
}

/// Simplified mosquito population dynamics (`om:SimpleMPD`).
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct SimpleMpdSpec {
    pub development_duration: i32,
    pub development_survival: f64,
    pub female_eggs_laid_by_oviposit: f64,
}

/// Species-specific bionomics on a non-human host category.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AnophelesNonHumanParams {
    pub name: String,
    pub mosq_relative_ento_availability: f64,
    pub mosq_prob_biting: f64,
    pub mosq_prob_find_rest_site: f64,
    pub mosq_prob_resting: f64,
    pub host_fecundity_factor: Option<f64>,
}
