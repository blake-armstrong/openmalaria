//! Parsers for `<human><component>` intervention types (`interventions.xsd`).

use crate::error::SchemaError;
use crate::interventions::{
    GviAnophParams, GviDesc, HumanComponent, HumanComponentKind, IrsAnophParams, IrsDesc,
    IrsKillingEffect, ItnAnophParams, ItnDesc, ItnDeterrencyMode, ItnEffectLogit, ItnEffectMode,
    ItnEnteringMode, ItnKillingEffect, ScreenSpec, SubPopRemoval, VaccineDesc, VaccinePhenotype,
};
use crate::parser_clin::parse_decision_tree;
use crate::pharmacology::AlleleRestriction;
use crate::xml_helpers::{
    bool_or, child_double_val, children_named, opt_attr, opt_child, opt_child_double_val, opt_f64,
    parse_decay_spec, parse_sampled_ln, parse_sampled_n, req_attr, req_child, req_f64,
};
use roxmltree::Node;

/// Parses a `<component id="..." name="...">` inside `<human>`.
pub fn parse_human_component(node: Node<'_, '_>) -> Result<HumanComponent, SchemaError> {
    let kind = if let Some(sc) = opt_child(node, "screen") {
        let positive_components = children_named(sc, "positive")
            .map(|c| req_attr(c, "id"))
            .collect::<Result<Vec<_>, SchemaError>>()?;
        let negative_components = children_named(sc, "negative")
            .map(|c| req_attr(c, "id"))
            .collect::<Result<Vec<_>, SchemaError>>()?;
        HumanComponentKind::Screen(ScreenSpec {
            diagnostic: req_attr(sc, "diagnostic")?,
            positive_components,
            negative_components,
        })
    } else if let Some(ts) = opt_child(node, "treatSimple") {
        HumanComponentKind::TreatSimple {
            duration_liver: req_attr(ts, "durationLiver")?,
            duration_blood: req_attr(ts, "durationBlood")?,
        }
    } else if let Some(tp) = opt_child(node, "treatPKPD") {
        HumanComponentKind::TreatPkPd {
            schedule: req_attr(tp, "schedule")?,
            dosage: req_attr(tp, "dosage")?,
            delay_h: opt_f64(tp, "delay_h")?.unwrap_or(0.0),
        }
    } else if let Some(dt) = opt_child(node, "decisionTree") {
        HumanComponentKind::DecisionTree(parse_decision_tree(dt)?)
    } else if let Some(pev) = opt_child(node, "PEV") {
        HumanComponentKind::Pev(parse_vaccine(pev)?)
    } else if let Some(bsv) = opt_child(node, "BSV") {
        HumanComponentKind::Bsv(parse_vaccine(bsv)?)
    } else if let Some(tbv) = opt_child(node, "TBV") {
        HumanComponentKind::Tbv(parse_vaccine(tbv)?)
    } else if let Some(itn) = opt_child(node, "ITN") {
        HumanComponentKind::Itn(Box::new(parse_itn(itn)?))
    } else if let Some(irs) = opt_child(node, "IRS") {
        HumanComponentKind::Irs(Box::new(parse_irs(irs)?))
    } else if let Some(gvi) = opt_child(node, "GVI") {
        HumanComponentKind::Gvi(Box::new(parse_gvi(gvi)?))
    } else if opt_child(node, "clearImmunity").is_some() {
        HumanComponentKind::ClearImmunity
    } else {
        HumanComponentKind::RecruitmentOnly
    };

    let sub_pop_removal = opt_child(node, "subPopRemoval")
        .map(|spr| -> Result<SubPopRemoval, SchemaError> {
            Ok(SubPopRemoval {
                on_first_bout: bool_or(spr, "onFirstBout", false)?,
                on_first_treatment: bool_or(spr, "onFirstTreatment", false)?,
                on_first_infection: bool_or(spr, "onFirstInfection", false)?,
                after_years: opt_f64(spr, "afterYears")?,
            })
        })
        .transpose()?;

    Ok(HumanComponent {
        id: req_attr(node, "id")?,
        name: opt_attr(node, "name"),
        kind,
        sub_pop_removal,
    })
}

fn parse_vaccine(node: Node<'_, '_>) -> Result<VaccineDesc, SchemaError> {
    let initial_efficacy = children_named(node, "initialEfficacy")
        .map(|ie| req_f64(ie, "value"))
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let phenotypes = children_named(node, "phenotype")
        .map(|ph| {
            let restrictions = children_named(ph, "restriction")
                .map(|r| {
                    Ok(AlleleRestriction {
                        on_locus: req_attr(r, "onLocus")?,
                        to_allele: req_attr(r, "toAllele")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            let ph_eff = children_named(ph, "initialEfficacy")
                .map(|ie| req_f64(ie, "value"))
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(VaccinePhenotype {
                name: opt_attr(ph, "name"),
                restrictions,
                initial_efficacy: ph_eff,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(VaccineDesc {
        decay: parse_decay_spec(req_child(node, "vaccine", "decay")?)?,
        efficacy_b: child_double_val(node, "vaccine", "efficacyB")?,
        initial_efficacy,
        phenotypes,
    })
}

fn parse_itn_exp_effect(node: Node<'_, '_>) -> Result<ItnKillingEffect, SchemaError> {
    Ok(ItnKillingEffect {
        base_factor: req_f64(node, "baseFactor")?,
        insecticide_factor: req_f64(node, "insecticideFactor")?,
        insecticide_scaling_factor: req_f64(node, "insecticideScalingFactor")?,
        hole_factor: req_f64(node, "holeFactor")?,
        interaction_factor: req_f64(node, "interactionFactor")?,
        hole_scaling_factor: req_f64(node, "holeScalingFactor")?,
    })
}

fn parse_itn_logit_effect(node: Node<'_, '_>) -> Result<ItnEffectLogit, SchemaError> {
    Ok(ItnEffectLogit {
        base_factor: req_f64(node, "baseFactor")?,
        insecticide_factor: req_f64(node, "insecticideFactor")?,
        hole_factor: req_f64(node, "holeFactor")?,
        interaction_factor: req_f64(node, "interactionFactor")?,
    })
}

fn parse_itn_effect_choice(
    parent: Node<'_, '_>,
    exp_tag: &'static str,
    logit_tag: &'static str,
) -> Result<Option<ItnEffectMode>, SchemaError> {
    if let Some(e) = opt_child(parent, exp_tag) {
        Ok(Some(ItnEffectMode::Exponential(parse_itn_exp_effect(e)?)))
    } else if let Some(l) = opt_child(parent, logit_tag) {
        Ok(Some(ItnEffectMode::Logit(parse_itn_logit_effect(l)?)))
    } else {
        Ok(None)
    }
}

fn parse_itn(node: Node<'_, '_>) -> Result<ItnDesc, SchemaError> {
    let anopheles_params = children_named(node, "anophelesParams")
        .map(|ap| {
            let deterrency = if let Some(det) = opt_child(ap, "deterrency") {
                ItnDeterrencyMode::SingleStage {
                    insecticide_factor: req_f64(det, "insecticideFactor")?,
                    insecticide_scaling_factor: req_f64(det, "insecticideScalingFactor")?,
                    hole_factor: req_f64(det, "holeFactor")?,
                    interaction_factor: req_f64(det, "interactionFactor")?,
                    hole_scaling_factor: req_f64(det, "holeScalingFactor")?,
                }
            } else if let Some(tsd) = opt_child(ap, "twoStageDeterrency") {
                let entering = if let Some(ent) = opt_child(tsd, "entering") {
                    ItnEnteringMode::Exponential {
                        insecticide_factor: req_f64(ent, "insecticideFactor")?,
                        insecticide_scaling_factor: req_f64(ent, "insecticideScalingFactor")?,
                    }
                } else if let Some(ent_l) = opt_child(tsd, "enteringLogit") {
                    ItnEnteringMode::Logit {
                        base_factor: req_f64(ent_l, "baseFactor")?,
                        insecticide_factor: req_f64(ent_l, "insecticideFactor")?,
                    }
                } else {
                    return Err(SchemaError::MissingElement {
                        parent: "twoStageDeterrency",
                        child: "entering|enteringLogit",
                    });
                };
                let attacking = parse_itn_effect_choice(tsd, "attacking", "attackingLogit")?
                    .ok_or(SchemaError::MissingElement {
                        parent: "twoStageDeterrency",
                        child: "attacking|attackingLogit",
                    })?;
                ItnDeterrencyMode::TwoStage {
                    entering,
                    attacking,
                }
            } else {
                return Err(SchemaError::MissingElement {
                    parent: "anophelesParams",
                    child: "deterrency|twoStageDeterrency",
                });
            };

            let preprandial_killing = parse_itn_effect_choice(
                ap,
                "preprandialKillingEffect",
                "preprandialKillingEffectLogit",
            )?
            .ok_or(SchemaError::MissingElement {
                parent: "anophelesParams",
                child: "preprandialKillingEffect|preprandialKillingEffectLogit",
            })?;
            let postprandial_killing = parse_itn_effect_choice(
                ap,
                "postprandialKillingEffect",
                "postprandialKillingEffectLogit",
            )?
            .ok_or(SchemaError::MissingElement {
                parent: "anophelesParams",
                child: "postprandialKillingEffect|postprandialKillingEffectLogit",
            })?;
            let fecundity_reduction =
                parse_itn_effect_choice(ap, "fecundityReduction", "fecundityReductionLogit")?;

            Ok(ItnAnophParams {
                mosquito: req_attr(ap, "mosquito")?,
                prop_active: opt_f64(ap, "propActive")?.unwrap_or(1.0),
                hole_index_max: opt_child_double_val(ap, "holeIndexMax")?,
                deterrency,
                preprandial_killing,
                postprandial_killing,
                fecundity_reduction,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(ItnDesc {
        usage: opt_child_double_val(node, "usage")?,
        hole_rate: parse_sampled_ln(req_child(node, "ITN", "holeRate")?)?,
        rip_rate: parse_sampled_ln(req_child(node, "ITN", "ripRate")?)?,
        rip_factor: child_double_val(node, "ITN", "ripFactor")?,
        initial_insecticide: parse_sampled_n(req_child(node, "ITN", "initialInsecticide")?)?,
        insecticide_decay: parse_decay_spec(req_child(node, "ITN", "insecticideDecay")?)?,
        attrition_of_nets: parse_decay_spec(req_child(node, "ITN", "attritionOfNets")?)?,
        anopheles_params,
    })
}

fn parse_irs_killing(node: Node<'_, '_>) -> Result<IrsKillingEffect, SchemaError> {
    Ok(IrsKillingEffect {
        base_factor: req_f64(node, "baseFactor")?,
        insecticide_factor: req_f64(node, "insecticideFactor")?,
        insecticide_scaling_factor: req_f64(node, "insecticideScalingFactor")?,
    })
}

fn parse_irs(node: Node<'_, '_>) -> Result<IrsDesc, SchemaError> {
    let anopheles_params = children_named(node, "anophelesParams")
        .map(|ap| {
            let det = req_child(ap, "anophelesParams", "deterrency")?;
            Ok(IrsAnophParams {
                mosquito: req_attr(ap, "mosquito")?,
                prop_active: opt_f64(ap, "propActive")?.unwrap_or(1.0),
                deterrency_insecticide_factor: req_f64(det, "insecticideFactor")?,
                deterrency_scaling_factor: req_f64(det, "insecticideScalingFactor")?,
                preprandial_killing: parse_irs_killing(req_child(
                    ap,
                    "anophelesParams",
                    "preprandialKillingEffect",
                )?)?,
                postprandial_killing: parse_irs_killing(req_child(
                    ap,
                    "anophelesParams",
                    "postprandialKillingEffect",
                )?)?,
                fecundity_reduction: opt_child(ap, "fecundityReduction")
                    .map(parse_irs_killing)
                    .transpose()?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(IrsDesc {
        usage: opt_child_double_val(node, "usage")?,
        initial_insecticide: parse_sampled_n(req_child(node, "IRS", "initialInsecticide")?)?,
        insecticide_decay: parse_decay_spec(req_child(node, "IRS", "insecticideDecay")?)?,
        anopheles_params,
    })
}

fn parse_gvi(node: Node<'_, '_>) -> Result<GviDesc, SchemaError> {
    let anopheles_params = children_named(node, "anophelesParams")
        .map(|ap| {
            Ok(GviAnophParams {
                mosquito: req_attr(ap, "mosquito")?,
                prop_active: opt_f64(ap, "propActive")?.unwrap_or(1.0),
                deterrency: opt_child_double_val(ap, "deterrency")?,
                preprandial_killing_effect: opt_child_double_val(ap, "preprandialKillingEffect")?,
                postprandial_killing_effect: opt_child_double_val(ap, "postprandialKillingEffect")?,
                fecundity_reduction: opt_child_double_val(ap, "fecundityReduction")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(GviDesc {
        usage: opt_child_double_val(node, "usage")?,
        decay: parse_decay_spec(req_child(node, "GVI", "decay")?)?,
        anopheles_params,
    })
}
