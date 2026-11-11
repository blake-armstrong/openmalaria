//! Parsers for `<interventions>` system/vector rules and `<human><deployment>` schedules.

use crate::error::SchemaError;
use crate::interventions::{
    AddNonHumanAnoph, AddNonHumanDeploy, AddNonHumanInterv, ContinuousDeploy,
    ContinuousDeployBlock, DecayEffect, DeployCondition, DeploymentBase, DeploymentRule,
    HumanInterventionsSpec, ImportedInfectionsSpec, ImportedRate, InterventionsSpec, MassDeploy,
    NonHumanModAnoph, NonHumanModInterv, SubPopRestriction, TimedDeployBlock, TimedHealthSystem,
    TimedNonVector, VectorPopInterv, VectorSpeciesInterv, VectorTrapDeploy, VectorTrapDesc,
    VectorTrapInterv,
};
use crate::parser_clin::parse_health_system;
use crate::parser_ento_pharm::parse_non_vector;
use crate::parser_interv_comp::parse_human_component;
use crate::xml_helpers::{
    bool_or, child_double_val, children_named, opt_attr, opt_child, opt_f64, opt_i32,
    parse_decay_spec, req_attr, req_bool, req_child, req_f64,
};
use roxmltree::Node;

/// Parses `<interventions name="...">`.
pub fn parse_interventions(node: Node<'_, '_>) -> Result<InterventionsSpec, SchemaError> {
    let change_hs = match opt_child(node, "changeHS") {
        Some(ch) => children_named(ch, "timedDeployment")
            .map(|td| {
                Ok(TimedHealthSystem {
                    time: req_attr(td, "time")?,
                    health_system: parse_health_system(td)?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let change_eir = match opt_child(node, "changeEIR") {
        Some(ce) => children_named(ce, "timedDeployment")
            .map(|td| {
                Ok(TimedNonVector {
                    time: req_attr(td, "time")?,
                    non_vector: parse_non_vector(td)?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let imported_infections = opt_child(node, "importedInfections")
        .map(|ii| -> Result<ImportedInfectionsSpec, SchemaError> {
            let timed = req_child(ii, "importedInfections", "timed")?;
            let rates = children_named(timed, "rate")
                .map(|r| {
                    Ok(ImportedRate {
                        time: req_attr(r, "time")?,
                        value: req_f64(r, "value")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(ImportedInfectionsSpec {
                name: opt_attr(ii, "name"),
                period: opt_attr(timed, "period").unwrap_or_else(|| "0".into()),
                rates,
            })
        })
        .transpose()?;

    let read_timed_times = |tag: &str| -> Result<Vec<String>, SchemaError> {
        match opt_child(node, tag) {
            Some(parent) => children_named(parent, "timedDeployment")
                .map(|td| req_attr(td, "time"))
                .collect(),
            None => Ok(Vec::new()),
        }
    };

    let vector_pop = match opt_child(node, "vectorPop") {
        Some(vp) => children_named(vp, "intervention")
            .map(parse_vector_pop)
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let vector_trap = match opt_child(node, "vectorTrap") {
        Some(vt) => children_named(vt, "intervention")
            .map(parse_vector_trap)
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let non_human_hosts_modifications = match opt_child(node, "nonHumanHostsModifications") {
        Some(nhm) => children_named(nhm, "intervention")
            .map(parse_non_human_mod)
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let add_non_human_hosts = match opt_child(node, "addNonHumanHosts") {
        Some(anh) => children_named(anh, "nonHumanHosts")
            .map(parse_add_non_human)
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    let human = opt_child(node, "human").map(parse_human_block).transpose()?;

    Ok(InterventionsSpec {
        name: req_attr(node, "name")?,
        change_hs,
        change_eir,
        imported_infections,
        insert_r0_case: read_timed_times("insertR_0Case")?,
        uninfect_vectors: read_timed_times("uninfectVectors")?,
        vector_pop,
        vector_trap,
        non_human_hosts_modifications,
        add_non_human_hosts,
        human,
    })
}

fn opt_decay_effect(parent: Node<'_, '_>, tag: &'static str) -> Result<Option<DecayEffect>, SchemaError> {
    opt_child(parent, tag)
        .map(|n| {
            Ok(DecayEffect {
                initial: req_f64(n, "initial")?,
                decay: parse_decay_spec(req_child(n, tag, "decay")?)?,
            })
        })
        .transpose()
}

fn parse_vector_pop(node: Node<'_, '_>) -> Result<VectorPopInterv, SchemaError> {
    let desc = req_child(node, "intervention", "description")?;
    let anopheles = children_named(desc, "anopheles")
        .map(|an| {
            Ok(VectorSpeciesInterv {
                mosquito: req_attr(an, "mosquito")?,
                seeking_death_rate_increase: opt_decay_effect(an, "seekingDeathRateIncrease")?,
                prob_death_ovipositing: opt_decay_effect(an, "probDeathOvipositing")?,
                emergence_reduction: opt_decay_effect(an, "emergenceReduction")?,
                prob_additional_death_sugar_feeding: opt_decay_effect(
                    an,
                    "probAdditionalDeathSugarFeeding",
                )?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let timed_deployments = match opt_child(node, "timed") {
        Some(t) => children_named(t, "deploy")
            .map(|d| req_attr(d, "time"))
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };
    Ok(VectorPopInterv {
        name: req_attr(node, "name")?,
        anopheles,
        timed_deployments,
    })
}

fn parse_vector_trap(node: Node<'_, '_>) -> Result<VectorTrapInterv, SchemaError> {
    let descriptions = children_named(node, "description")
        .map(|d| {
            Ok(VectorTrapDesc {
                mosquito: req_attr(d, "mosquito")?,
                relative_availability: child_double_val(d, "description", "relativeAvailability")?,
                decay_of_availability: parse_decay_spec(req_child(
                    d,
                    "description",
                    "decayOfAvailability",
                )?)?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let deployments = match opt_child(node, "timed") {
        Some(t) => children_named(t, "deploy")
            .map(|d| {
                Ok(VectorTrapDeploy {
                    time: req_attr(d, "time")?,
                    ratio_to_humans: req_f64(d, "ratioToHumans")?,
                    lifespan: req_attr(d, "lifespan")?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };
    Ok(VectorTrapInterv {
        name: opt_attr(node, "name"),
        descriptions,
        deployments,
    })
}

fn parse_non_human_mod(node: Node<'_, '_>) -> Result<NonHumanModInterv, SchemaError> {
    let desc = req_child(node, "intervention", "description")?;
    let opt_initial = |n: Node<'_, '_>, tag: &str| -> Result<Option<f64>, SchemaError> {
        opt_child(n, tag).map(|c| req_f64(c, "initial")).transpose()
    };
    let anopheles = children_named(desc, "anopheles")
        .map(|an| {
            Ok(NonHumanModAnoph {
                mosquito: req_attr(an, "mosquito")?,
                availability_reduction: opt_initial(an, "availabilityReduction")?,
                preprandial_killing_effect: opt_initial(an, "preprandialKillingEffect")?,
                postprandial_killing_effect: opt_initial(an, "postprandialKillingEffect")?,
                resting_killing_effect: opt_initial(an, "restingKillingEffect")?,
                fecundity_reduction: opt_initial(an, "fecundityReduction")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let timed_deployments = match opt_child(node, "timed") {
        Some(t) => children_named(t, "deploy")
            .map(|d| req_attr(d, "time"))
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };
    Ok(NonHumanModInterv {
        name: req_attr(node, "name")?,
        non_human_hosts_name: req_attr(node, "nonHumanHostsName")?,
        decay: parse_decay_spec(req_child(node, "intervention", "decay")?)?,
        anopheles,
        timed_deployments,
    })
}

fn parse_add_non_human(node: Node<'_, '_>) -> Result<AddNonHumanInterv, SchemaError> {
    let desc = req_child(node, "nonHumanHosts", "description")?;
    let anopheles = children_named(desc, "anopheles")
        .map(|an| {
            Ok(AddNonHumanAnoph {
                mosquito: req_attr(an, "mosquito")?,
                mosq_relative_availability_human: child_double_val(
                    an,
                    "anopheles",
                    "mosqRelativeAvailabilityHuman",
                )?,
                mosq_prob_biting: child_double_val(an, "anopheles", "mosqProbBiting")?,
                mosq_prob_find_rest_site: child_double_val(
                    an,
                    "anopheles",
                    "mosqProbFindRestSite",
                )?,
                mosq_prob_resting: child_double_val(an, "anopheles", "mosqProbResting")?,
                host_fecundity_factor: child_double_val(an, "anopheles", "hostFecundityFactor")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let deployments = match opt_child(node, "timed") {
        Some(t) => children_named(t, "deploy")
            .map(|d| {
                Ok(AddNonHumanDeploy {
                    time: req_attr(d, "time")?,
                    lifespan: req_attr(d, "lifespan")?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };
    Ok(AddNonHumanInterv {
        name: req_attr(node, "name")?,
        anopheles,
        deployments,
    })
}

fn parse_human_block(node: Node<'_, '_>) -> Result<HumanInterventionsSpec, SchemaError> {
    let components = children_named(node, "component")
        .map(parse_human_component)
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let deployments = children_named(node, "deployment")
        .map(parse_deployment_rule)
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(HumanInterventionsSpec {
        components,
        deployments,
    })
}

fn parse_deploy_base(node: Node<'_, '_>) -> Result<DeploymentBase, SchemaError> {
    Ok(DeploymentBase {
        coverage: req_f64(node, "coverage")?,
        vacc_min_prev_doses: opt_i32(node, "vaccMinPrevDoses")?,
        vacc_max_cum_doses: opt_i32(node, "vaccMaxCumDoses")?,
        min_availability: opt_i32(node, "minAvailability")?,
        max_availability: opt_i32(node, "maxAvailability")?,
        coverage_corr: opt_f64(node, "coverageCorr")?,
        coverage_var: opt_f64(node, "coverageVar")?,
    })
}

fn parse_sub_pop_restriction(node: Node<'_, '_>) -> Result<Option<SubPopRestriction>, SchemaError> {
    opt_child(node, "restrictToSubPop")
        .map(|r| {
            Ok(SubPopRestriction {
                id: req_attr(r, "id")?,
                complement: bool_or(r, "complement", false)?,
            })
        })
        .transpose()
}

fn parse_deployment_rule(node: Node<'_, '_>) -> Result<DeploymentRule, SchemaError> {
    let components = children_named(node, "component")
        .map(|c| req_attr(c, "id"))
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let conditions = children_named(node, "condition")
        .map(|c| {
            Ok(DeployCondition {
                measure: req_attr(c, "measure")?,
                min_value: opt_f64(c, "minValue")?,
                max_value: opt_f64(c, "maxValue")?,
                initial_state: req_bool(c, "initialState")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let continuous = children_named(node, "continuous")
        .map(|cb| {
            let deploys = children_named(cb, "deploy")
                .map(|d| {
                    Ok(ContinuousDeploy {
                        base: parse_deploy_base(d)?,
                        target_age_yrs: req_f64(d, "targetAgeYrs")?,
                        begin: opt_attr(d, "begin"),
                        end: opt_attr(d, "end"),
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(ContinuousDeployBlock {
                restrict_to_sub_pop: parse_sub_pop_restriction(cb)?,
                deploys,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let timed = children_named(node, "timed")
        .map(|tb| {
            let cumulative_coverage_component = opt_child(tb, "cumulativeCoverage")
                .map(|cc| req_attr(cc, "component"))
                .transpose()?;
            let deploys = children_named(tb, "deploy")
                .map(|d| {
                    Ok(MassDeploy {
                        base: parse_deploy_base(d)?,
                        time: req_attr(d, "time")?,
                        min_age: opt_f64(d, "minAge")?.unwrap_or(0.0),
                        max_age: opt_f64(d, "maxAge")?,
                        repeat_step: opt_attr(d, "repeatStep"),
                        repeat_end: opt_attr(d, "repeatEnd"),
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(TimedDeployBlock {
                restrict_to_sub_pop: parse_sub_pop_restriction(tb)?,
                cumulative_coverage_component,
                deploys,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(DeploymentRule {
        name: opt_attr(node, "name"),
        components,
        conditions,
        continuous,
        timed,
    })
}
