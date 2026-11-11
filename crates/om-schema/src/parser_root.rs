//! Parser for `<scenario>`, `<demography>`, `<monitoring>`, `<parasiteGenetics>`, `<diagnostics>`, `<model>`, and `<vivax>`.

use crate::demog_mon::{
    CohortSubPop, DemogAgeGroup, DemogGroupBounds, Demography, MonAgeGroup, Monitoring,
    MonitoringContinuous, MonitoringOption, MonitoringOptions, SurveyTime, Surveys,
};
use crate::error::SchemaError;
use crate::parser_clin::{parse_clinical, parse_health_system};
use crate::parser_ento_pharm::{parse_entomology, parse_pharmacology};
use crate::parser_interv::parse_interventions;
use crate::scenario::{
    DiagnosticBody, DiagnosticSpec, HumanSpec, HumanWeightSpec, HypnozoiteReleaseDist,
    HypnozoiteReleaseSpec, ModelSpec, ParameterEntry, ParametersBlock, ParasiteAllele,
    ParasiteGeneticsSpec, ParasiteLocus, ProbVivaxEvent, Scenario, VivaxClinicalEvents, VivaxSpec,
};
use crate::xml_helpers::{
    bool_or, child_double_val, children_named, opt_attr, opt_bool, opt_child, opt_f64, opt_i32,
    parse_age_group_values, parse_option_set, parse_sampled_ln, parse_weibull, req_attr,
    req_child, req_f64, req_i32, req_i64, text_trimmed,
};
use roxmltree::{Document, Node};

/// Parses a complete OpenMalaria scenario XML string into [`Scenario`] (before `validate()`).
pub fn parse_scenario_xml(xml: &str) -> Result<Scenario, SchemaError> {
    let doc = Document::parse(xml)?;
    let root = doc.root_element();
    if root.tag_name().name() != "scenario" {
        return Err(SchemaError::InvalidValue {
            element: root.tag_name().name().to_owned(),
            field: "root",
            message: "expected root element <scenario> or <om:scenario>".into(),
        });
    }

    let diagnostics = opt_child(root, "diagnostics")
        .map(parse_diagnostics)
        .transpose()?
        .unwrap_or_default();

    Ok(Scenario {
        schema_version: req_i32(root, "schemaVersion")?,
        name: req_attr(root, "name")?,
        analysis_no: opt_i32(root, "analysisNo")?,
        wu_id: opt_i32(root, "wuID")?,
        demography: parse_demography(req_child(root, "scenario", "demography")?)?,
        monitoring: parse_monitoring(req_child(root, "scenario", "monitoring")?)?,
        interventions: parse_interventions(req_child(root, "scenario", "interventions")?)?,
        health_system: parse_health_system(req_child(root, "scenario", "healthSystem")?)?,
        entomology: parse_entomology(req_child(root, "scenario", "entomology")?)?,
        parasite_genetics: opt_child(root, "parasiteGenetics")
            .map(parse_parasite_genetics)
            .transpose()?,
        pharmacology: opt_child(root, "pharmacology")
            .map(parse_pharmacology)
            .transpose()?,
        diagnostics,
        model: parse_model(req_child(root, "scenario", "model")?)?,
    })
}

fn parse_demography(node: Node<'_, '_>) -> Result<Demography, SchemaError> {
    let ag = req_child(node, "demography", "ageGroup")?;
    let groups = children_named(ag, "group")
        .map(|g| {
            Ok(DemogGroupBounds {
                poppercent: req_f64(g, "poppercent")?,
                upperbound: req_f64(g, "upperbound")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(Demography {
        name: req_attr(node, "name")?,
        pop_size: req_i32(node, "popSize")?,
        maximum_age_yrs: req_f64(node, "maximumAgeYrs")?,
        age_group: DemogAgeGroup {
            lowerbound: req_f64(ag, "lowerbound")?,
            groups,
        },
    })
}

fn parse_monitoring(node: Node<'_, '_>) -> Result<Monitoring, SchemaError> {
    let continuous = opt_child(node, "continuous")
        .map(|c| -> Result<MonitoringContinuous, SchemaError> {
            Ok(MonitoringContinuous {
                period: req_attr(c, "period")?,
                during_init: bool_or(c, "duringInit", false)?,
                options: parse_option_set(c)?,
            })
        })
        .transpose()?;

    let so_node = req_child(node, "monitoring", "SurveyOptions")?;
    let so_opts = children_named(so_node, "option")
        .map(|o| {
            Ok(MonitoringOption {
                name: req_attr(o, "name")?,
                value: bool_or(o, "value", true)?,
                output_number: opt_i32(o, "outputNumber")?,
                by_age: opt_bool(o, "byAge")?,
                by_cohort: opt_bool(o, "byCohort")?,
                by_species: opt_bool(o, "bySpecies")?,
                by_genotype: opt_bool(o, "byGenotype")?,
                by_drug_type: opt_bool(o, "byDrugType")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let surv_node = req_child(node, "monitoring", "surveys")?;
    let survey_times = children_named(surv_node, "surveyTime")
        .map(|st| {
            Ok(SurveyTime {
                time: text_trimmed(st),
                repeat_step: opt_attr(st, "repeatStep"),
                repeat_end: opt_attr(st, "repeatEnd"),
                reported: bool_or(st, "reported", true)?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let ag_node = req_child(node, "monitoring", "ageGroup")?;
    let upperbounds = children_named(ag_node, "group")
        .map(|g| req_f64(g, "upperbound"))
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let cohorts = match opt_child(node, "cohorts") {
        Some(c_node) => children_named(c_node, "subPop")
            .map(|sp| {
                Ok(CohortSubPop {
                    id: req_attr(sp, "id")?,
                    number: req_i64(sp, "number")?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?,
        None => Vec::new(),
    };

    Ok(Monitoring {
        name: req_attr(node, "name")?,
        start_date: opt_attr(node, "startDate"),
        continuous,
        survey_options: MonitoringOptions {
            only_new_episode: bool_or(so_node, "onlyNewEpisode", false)?,
            options: so_opts,
        },
        surveys: Surveys {
            detection_limit: opt_f64(surv_node, "detectionLimit")?,
            diagnostic: opt_attr(surv_node, "diagnostic"),
            survey_times,
        },
        age_group: MonAgeGroup {
            lowerbound: req_f64(ag_node, "lowerbound")?,
            upperbounds,
        },
        cohorts,
    })
}

fn parse_parasite_genetics(node: Node<'_, '_>) -> Result<ParasiteGeneticsSpec, SchemaError> {
    let loci = children_named(node, "locus")
        .map(|l| {
            let alleles = children_named(l, "allele")
                .map(|a| {
                    Ok(ParasiteAllele {
                        name: req_attr(a, "name")?,
                        initial_frequency: req_f64(a, "initialFrequency")?,
                        fitness: req_f64(a, "fitness")?,
                        hrp2_deletion: bool_or(a, "hrp2_deletion", false)?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(ParasiteLocus {
                name: req_attr(l, "name")?,
                alleles,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(ParasiteGeneticsSpec {
        sampling_mode: req_attr(node, "samplingMode")?,
        loci,
    })
}

fn parse_diagnostics(node: Node<'_, '_>) -> Result<Vec<DiagnosticSpec>, SchemaError> {
    children_named(node, "diagnostic")
        .map(|d| {
            let body = if let Some(det) = opt_child(d, "deterministic") {
                DiagnosticBody::Deterministic {
                    min_density: req_f64(det, "minDensity")?,
                }
            } else if let Some(stoch) = opt_child(d, "stochastic") {
                DiagnosticBody::Stochastic {
                    dens_50: req_f64(stoch, "dens_50")?,
                    specificity: req_f64(stoch, "specificity")?,
                }
            } else {
                return Err(SchemaError::MissingElement {
                    parent: "diagnostic",
                    child: "deterministic|stochastic",
                });
            };
            Ok(DiagnosticSpec {
                name: req_attr(d, "name")?,
                units: opt_attr(d, "units"),
                mechanism: opt_attr(d, "mechanism").unwrap_or_else(|| "Other".into()),
                body,
            })
        })
        .collect()
}

fn parse_model(node: Node<'_, '_>) -> Result<ModelSpec, SchemaError> {
    let model_name = opt_child(node, "ModelName")
        .map(|m| req_attr(m, "name"))
        .transpose()?;
    let model_options = opt_child(node, "ModelOptions")
        .map(parse_option_set)
        .transpose()?;
    let clinical = parse_clinical(req_child(node, "model", "clinical")?)?;
    let human_node = req_child(node, "model", "human")?;
    let avail = parse_age_group_values(req_child(
        human_node,
        "human",
        "availabilityToMosquitoes",
    )?)?;
    let weight = opt_child(human_node, "weight")
        .map(|w| -> Result<HumanWeightSpec, SchemaError> {
            Ok(HumanWeightSpec {
                mult_std_dev: req_f64(w, "multStdDev")?,
                values: parse_age_group_values(w)?,
            })
        })
        .transpose()?;
    let vivax = opt_child(node, "vivax").map(parse_vivax).transpose()?;
    let comp = req_child(node, "model", "computationParameters")?;
    let parameters = opt_child(node, "parameters")
        .map(|p_node| -> Result<ParametersBlock, SchemaError> {
            let entries = children_named(p_node, "parameter")
                .map(|pe| {
                    Ok(ParameterEntry {
                        name: opt_attr(pe, "name"),
                        number: req_i32(pe, "number")?,
                        value: req_f64(pe, "value")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(ParametersBlock {
                interval: req_i32(p_node, "interval")?,
                latentp: req_attr(p_node, "latentp")?,
                entries,
            })
        })
        .transpose()?;

    Ok(ModelSpec {
        model_name,
        model_options,
        clinical,
        human: HumanSpec {
            availability_to_mosquitoes: avail,
            weight,
        },
        vivax,
        iseed: req_i32(comp, "iseed")?,
        parameters,
    })
}

fn parse_vivax(node: Node<'_, '_>) -> Result<VivaxSpec, SchemaError> {
    let hr = req_child(node, "vivax", "hypnozoiteRelease")?;
    let num_h = req_child(hr, "hypnozoiteRelease", "numberHypnozoites")?;
    let parse_rel = |n: Node<'_, '_>| -> Result<HypnozoiteReleaseDist, SchemaError> {
        Ok(HypnozoiteReleaseDist {
            latent_relapse: req_f64(n, "latentRelapse")?,
            dist: parse_sampled_ln(n)?,
        })
    };
    let ce = req_child(node, "vivax", "clinicalEvents")?;
    let parse_prob = |parent: &'static str, child: &'static str| -> Result<ProbVivaxEvent, SchemaError> {
        let c = req_child(ce, parent, child)?;
        Ok(ProbVivaxEvent {
            a: req_f64(c, "a")?,
            b: req_f64(c, "b")?,
        })
    };
    Ok(VivaxSpec {
        prob_blood_stage_infectious_to_mosq: child_double_val(
            node,
            "vivax",
            "probBloodStageInfectiousToMosq",
        )?,
        hypnozoite_release: HypnozoiteReleaseSpec {
            p_second_release: opt_f64(hr, "pSecondRelease")?.unwrap_or(0.0),
            max_hypnozoites: req_i32(num_h, "max")?,
            base_hypnozoites: req_f64(num_h, "base")?,
            first_release_days: parse_rel(req_child(hr, "hypnozoiteRelease", "firstReleaseDays")?)?,
            second_release_days: opt_child(hr, "secondReleaseDays")
                .map(parse_rel)
                .transpose()?,
        },
        blood_stage_protection_latency: child_double_val(
            node,
            "vivax",
            "bloodStageProtectionLatency",
        )?,
        blood_stage_length_days: parse_weibull(req_child(
            node,
            "vivax",
            "bloodStageLengthDays",
        )?)?,
        clinical_events: VivaxClinicalEvents {
            p_primary_infection: parse_prob("clinicalEvents", "pPrimaryInfection")?,
            p_relapse_one: parse_prob("clinicalEvents", "pRelapseOne")?,
            p_relapse_two_plus: parse_prob("clinicalEvents", "pRelapseTwoPlus")?,
            p_event_is_severe: child_double_val(ce, "clinicalEvents", "pEventIsSevere")?,
            vivax_clin_option: text_trimmed(req_child(ce, "clinicalEvents", "vivaxClinOption")?),
        },
    })
}
