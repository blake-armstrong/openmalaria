//! Parsers for `<clinical>` and `<healthSystem>` (`healthSystem.xsd`).

use crate::error::SchemaError;
use crate::health_system::{
    ClearInfectionsSpec, ClinicalNmf, ClinicalOutcomesSpec, ClinicalSpec, DrugRegimen,
    HealthSystemSpec, HealthSystemVariant, HsDecisionTree5Day, HsEsNmf, HsEventScheduler,
    HsImmediateOutcomes, LiverStageDrugSpec, TreatmentActions, TreatmentDetails, TreatmentOption,
};
pub use crate::parser_dt::parse_decision_tree;
use crate::xml_helpers::{
    bool_or, child_double_val, children_named, opt_attr, opt_child, opt_child_double_val,
    parse_age_group_values, parse_triggered_deployments, req_attr, req_bool, req_child, text_f64,
    text_i32,
};
use roxmltree::Node;

/// Parses `<clinical>` inside `<model>`.
pub fn parse_clinical(node: Node<'_, '_>) -> Result<ClinicalSpec, SchemaError> {
    let neonatal_mortality_diagnostic = opt_child(node, "NeonatalMortality")
        .map(|nm| req_attr(nm, "diagnostic"))
        .transpose()?;
    let non_malaria_fevers = opt_child(node, "NonMalariaFevers")
        .map(|nmf| -> Result<ClinicalNmf, SchemaError> {
            Ok(ClinicalNmf {
                incidence: parse_age_group_values(req_child(
                    nmf,
                    "NonMalariaFevers",
                    "incidence",
                )?)?,
                pr_need_treatment_nmf: opt_child(nmf, "prNeedTreatmentNMF")
                    .map(parse_age_group_values)
                    .transpose()?,
                pr_need_treatment_mf: opt_child(nmf, "prNeedTreatmentMF")
                    .map(parse_age_group_values)
                    .transpose()?,
            })
        })
        .transpose()?;
    Ok(ClinicalSpec {
        health_system_memory: req_attr(node, "healthSystemMemory")?,
        neonatal_mortality_diagnostic,
        non_malaria_fevers,
    })
}

/// Parses `<healthSystem>` (or `<changeHS><timedDeployment>`).
pub fn parse_health_system(node: Node<'_, '_>) -> Result<HealthSystemSpec, SchemaError> {
    let variant = if let Some(io) = opt_child(node, "ImmediateOutcomes") {
        HealthSystemVariant::ImmediateOutcomes(Box::new(parse_hs_immediate_outcomes(io)?))
    } else if let Some(dt5) = opt_child(node, "DecisionTree5Day") {
        HealthSystemVariant::DecisionTree5Day(Box::new(parse_hs_dt_5day(dt5)?))
    } else if let Some(es) = opt_child(node, "EventScheduler") {
        HealthSystemVariant::EventScheduler(Box::new(parse_hs_event_scheduler(es)?))
    } else {
        return Err(SchemaError::MissingElement {
            parent: "healthSystem",
            child: "ImmediateOutcomes|DecisionTree5Day|EventScheduler",
        });
    };

    Ok(HealthSystemSpec {
        variant,
        cfr: parse_age_group_values(req_child(node, "healthSystem", "CFR")?)?,
        p_sequelae_inpatient: parse_age_group_values(req_child(
            node,
            "healthSystem",
            "pSequelaeInpatient",
        )?)?,
    })
}

fn parse_liver_stage_drug(node: Node<'_, '_>) -> Result<LiverStageDrugSpec, SchemaError> {
    let ignore_cannot_receive = opt_child(node, "ignoreCannotReceive")
        .map(|ic| req_bool(ic, "value"))
        .transpose()?;
    Ok(LiverStageDrugSpec {
        p_human_cannot_receive: child_double_val(node, "liverStageDrug", "pHumanCannotReceive")?,
        ignore_cannot_receive,
        p_use_uncomplicated: opt_child_double_val(node, "pUseUncomplicated")?,
        effectiveness_on_use: child_double_val(node, "liverStageDrug", "effectivenessOnUse")?,
    })
}

fn parse_treatment_details(node: Node<'_, '_>) -> Result<TreatmentDetails, SchemaError> {
    Ok(TreatmentDetails {
        cq: opt_child_double_val(node, "CQ")?,
        sp: opt_child_double_val(node, "SP")?,
        aq: opt_child_double_val(node, "AQ")?,
        spaq: opt_child_double_val(node, "SPAQ")?,
        act: opt_child_double_val(node, "ACT")?,
        qn: opt_child_double_val(node, "QN")?,
        self_treatment: child_double_val(node, "TreatmentDetails", "selfTreatment")?,
    })
}

/// Parses an `om:TreatmentOption` element.
pub fn parse_treatment_option(node: Node<'_, '_>) -> Result<TreatmentOption, SchemaError> {
    let clear_infections = children_named(node, "clearInfections")
        .map(|ci| {
            Ok(ClearInfectionsSpec {
                timesteps: req_attr(ci, "timesteps")?,
                stage: req_attr(ci, "stage")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(TreatmentOption {
        name: opt_attr(node, "name"),
        deployments: parse_triggered_deployments(node)?,
        clear_infections,
    })
}

fn parse_hs_immediate_outcomes(node: Node<'_, '_>) -> Result<HsImmediateOutcomes, SchemaError> {
    let dr = req_child(node, "ImmediateOutcomes", "drugRegimen")?;
    let ta = req_child(node, "ImmediateOutcomes", "treatmentActions")?;
    Ok(HsImmediateOutcomes {
        name: opt_attr(node, "name"),
        use_diagnostic_uc: bool_or(node, "useDiagnosticUC", false)?,
        drug_regimen: DrugRegimen {
            first_line: req_attr(dr, "firstLine")?,
            second_line: req_attr(dr, "secondLine")?,
            inpatient: req_attr(dr, "inpatient")?,
        },
        initial_acr: parse_treatment_details(req_child(node, "ImmediateOutcomes", "initialACR")?)?,
        compliance: parse_treatment_details(req_child(node, "ImmediateOutcomes", "compliance")?)?,
        non_compliers_effective: parse_treatment_details(req_child(
            node,
            "ImmediateOutcomes",
            "nonCompliersEffective",
        )?)?,
        treatment_actions: TreatmentActions {
            cq: opt_child(ta, "CQ").map(parse_treatment_option).transpose()?,
            sp: opt_child(ta, "SP").map(parse_treatment_option).transpose()?,
            aq: opt_child(ta, "AQ").map(parse_treatment_option).transpose()?,
            spaq: opt_child(ta, "SPAQ").map(parse_treatment_option).transpose()?,
            act: opt_child(ta, "ACT").map(parse_treatment_option).transpose()?,
            qn: opt_child(ta, "QN").map(parse_treatment_option).transpose()?,
        },
        p_seek_official_care_uncomplicated_1: child_double_val(
            node,
            "ImmediateOutcomes",
            "pSeekOfficialCareUncomplicated1",
        )?,
        p_self_treat_uncomplicated: child_double_val(
            node,
            "ImmediateOutcomes",
            "pSelfTreatUncomplicated",
        )?,
        p_seek_official_care_uncomplicated_2: child_double_val(
            node,
            "ImmediateOutcomes",
            "pSeekOfficialCareUncomplicated2",
        )?,
        p_seek_official_care_severe: child_double_val(
            node,
            "ImmediateOutcomes",
            "pSeekOfficialCareSevere",
        )?,
        liver_stage_drug: opt_child(node, "liverStageDrug")
            .map(parse_liver_stage_drug)
            .transpose()?,
    })
}

fn parse_hs_dt_5day(node: Node<'_, '_>) -> Result<HsDecisionTree5Day, SchemaError> {
    Ok(HsDecisionTree5Day {
        name: opt_attr(node, "name"),
        p_seek_official_care_uncomplicated_1: child_double_val(
            node,
            "DecisionTree5Day",
            "pSeekOfficialCareUncomplicated1",
        )?,
        p_self_treat_uncomplicated: child_double_val(
            node,
            "DecisionTree5Day",
            "pSelfTreatUncomplicated",
        )?,
        p_seek_official_care_uncomplicated_2: child_double_val(
            node,
            "DecisionTree5Day",
            "pSeekOfficialCareUncomplicated2",
        )?,
        p_seek_official_care_severe: child_double_val(
            node,
            "DecisionTree5Day",
            "pSeekOfficialCareSevere",
        )?,
        liver_stage_drug: opt_child(node, "liverStageDrug")
            .map(parse_liver_stage_drug)
            .transpose()?,
        tree_uc_official: parse_decision_tree(req_child(
            node,
            "DecisionTree5Day",
            "treeUCOfficial",
        )?)?,
        tree_uc_self_treat: parse_decision_tree(req_child(
            node,
            "DecisionTree5Day",
            "treeUCSelfTreat",
        )?)?,
        cure_rate_severe: child_double_val(node, "DecisionTree5Day", "cureRateSevere")?,
        treatment_severe: parse_treatment_option(req_child(
            node,
            "DecisionTree5Day",
            "treatmentSevere",
        )?)?,
    })
}

fn parse_hs_event_scheduler(node: Node<'_, '_>) -> Result<HsEventScheduler, SchemaError> {
    let co = req_child(node, "EventScheduler", "ClinicalOutcomes")?;
    let daily_pr_imm_ucts = children_named(co, "dailyPrImmUCTS")
        .map(|d| text_f64(d, "dailyPrImmUCTS"))
        .collect::<Result<Vec<_>, SchemaError>>()?;
    let non_malaria_fevers = opt_child(node, "NonMalariaFevers")
        .map(|nmf| -> Result<HsEsNmf, SchemaError> {
            Ok(HsEsNmf {
                pr_treatment: text_f64(
                    req_child(nmf, "NonMalariaFevers", "prTreatment")?,
                    "prTreatment",
                )?,
                effect_negative_test: text_f64(
                    req_child(nmf, "NonMalariaFevers", "effectNegativeTest")?,
                    "effectNegativeTest",
                )?,
                effect_positive_test: text_f64(
                    req_child(nmf, "NonMalariaFevers", "effectPositiveTest")?,
                    "effectPositiveTest",
                )?,
                effect_need: text_f64(
                    req_child(nmf, "NonMalariaFevers", "effectNeed")?,
                    "effectNeed",
                )?,
                effect_informal: text_f64(
                    req_child(nmf, "NonMalariaFevers", "effectInformal")?,
                    "effectInformal",
                )?,
                cfr: parse_age_group_values(req_child(nmf, "NonMalariaFevers", "CFR")?)?,
                treatment_efficacy: text_f64(
                    req_child(nmf, "NonMalariaFevers", "TreatmentEfficacy")?,
                    "TreatmentEfficacy",
                )?,
            })
        })
        .transpose()?;

    Ok(HsEventScheduler {
        uncomplicated: parse_decision_tree(req_child(node, "EventScheduler", "uncomplicated")?)?,
        complicated: parse_decision_tree(req_child(node, "EventScheduler", "complicated")?)?,
        clinical_outcomes: ClinicalOutcomesSpec {
            max_uc_seeking_memory: text_i32(
                req_child(co, "ClinicalOutcomes", "maxUCSeekingMemory")?,
                "maxUCSeekingMemory",
            )?,
            uncomplicated_case_duration: text_i32(
                req_child(co, "ClinicalOutcomes", "uncomplicatedCaseDuration")?,
                "uncomplicatedCaseDuration",
            )?,
            complicated_case_duration: text_i32(
                req_child(co, "ClinicalOutcomes", "complicatedCaseDuration")?,
                "complicatedCaseDuration",
            )?,
            complicated_risk_duration: text_i32(
                req_child(co, "ClinicalOutcomes", "complicatedRiskDuration")?,
                "complicatedRiskDuration",
            )?,
            daily_pr_imm_ucts,
        },
        non_malaria_fevers,
    })
}
