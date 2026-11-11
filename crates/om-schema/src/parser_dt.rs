//! Recursive `<DecisionTree>` parser (`schema/healthSystem.xsd`).

use crate::decision_tree::{AgeBranch, DecisionTree, DecisionTreeNode, RandomOutcome};
use crate::error::SchemaError;
use crate::xml_helpers::{
    children_named, elem_children, opt_attr, opt_f64, opt_i32, req_attr, req_child, req_f64,
};
use roxmltree::Node;

/// Parses an `om:DecisionTree` container (including inline `<outcome>` and `<age>` extensions).
pub fn parse_decision_tree(node: Node<'_, '_>) -> Result<DecisionTree, SchemaError> {
    let items = elem_children(node)
        .map(parse_dt_node)
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(DecisionTree {
        name: opt_attr(node, "name"),
        items,
    })
}

fn parse_dt_node(node: Node<'_, '_>) -> Result<DecisionTreeNode, SchemaError> {
    let name = opt_attr(node, "name");
    match node.tag_name().name() {
        "multiple" => {
            let children = elem_children(node)
                .map(parse_dt_node)
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(DecisionTreeNode::Multiple { name, children })
        }
        "caseType" => Ok(DecisionTreeNode::CaseType {
            name,
            first_line: Box::new(parse_decision_tree(req_child(
                node,
                "caseType",
                "firstLine",
            )?)?),
            second_line: Box::new(parse_decision_tree(req_child(
                node,
                "caseType",
                "secondLine",
            )?)?),
        }),
        "infectionOrigin" => Ok(DecisionTreeNode::InfectionOrigin {
            name,
            imported: Box::new(parse_decision_tree(req_child(
                node,
                "infectionOrigin",
                "imported",
            )?)?),
            introduced: Box::new(parse_decision_tree(req_child(
                node,
                "infectionOrigin",
                "introduced",
            )?)?),
            indigenous: Box::new(parse_decision_tree(req_child(
                node,
                "infectionOrigin",
                "indigenous",
            )?)?),
        }),
        "diagnostic" => Ok(DecisionTreeNode::Diagnostic {
            name,
            diagnostic: req_attr(node, "diagnostic")?,
            positive: Box::new(parse_decision_tree(req_child(
                node,
                "diagnostic",
                "positive",
            )?)?),
            negative: Box::new(parse_decision_tree(req_child(
                node,
                "diagnostic",
                "negative",
            )?)?),
        }),
        "uncomplicated" => Ok(DecisionTreeNode::Uncomplicated {
            name,
            memory: opt_attr(node, "memory").unwrap_or_else(|| "1".into()),
            positive: Box::new(parse_decision_tree(req_child(
                node,
                "uncomplicated",
                "positive",
            )?)?),
            negative: Box::new(parse_decision_tree(req_child(
                node,
                "uncomplicated",
                "negative",
            )?)?),
        }),
        "severe" => Ok(DecisionTreeNode::Severe {
            name,
            memory: opt_attr(node, "memory"),
            positive: Box::new(parse_decision_tree(req_child(
                node, "severe", "positive",
            )?)?),
            negative: Box::new(parse_decision_tree(req_child(
                node, "severe", "negative",
            )?)?),
        }),
        "cohort" => Ok(DecisionTreeNode::Cohort {
            name,
            component: req_attr(node, "component")?,
            positive: Box::new(parse_decision_tree(req_child(
                node, "cohort", "positive",
            )?)?),
            negative: Box::new(parse_decision_tree(req_child(
                node, "cohort", "negative",
            )?)?),
        }),
        "random" => {
            let outcomes = children_named(node, "outcome")
                .map(|o| {
                    Ok(RandomOutcome {
                        p: req_f64(o, "p")?,
                        tree: parse_decision_tree(o)?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(DecisionTreeNode::Random { name, outcomes })
        }
        "age" => {
            let branches = children_named(node, "age")
                .map(|a| {
                    Ok(AgeBranch {
                        lb: req_f64(a, "lb")?,
                        tree: parse_decision_tree(a)?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(DecisionTreeNode::Age { name, branches })
        }
        "noTreatment" => Ok(DecisionTreeNode::NoTreatment { name }),
        "treatFailure" => Ok(DecisionTreeNode::TreatFailure { name }),
        "report" => Ok(DecisionTreeNode::Report {
            name,
            output_number: opt_i32(node, "outputNumber")?.unwrap_or(0),
        }),
        "treatPKPD" => Ok(DecisionTreeNode::TreatPkPd {
            schedule: req_attr(node, "schedule")?,
            dosage: req_attr(node, "dosage")?,
            delay_h: opt_f64(node, "delay_h")?.unwrap_or(0.0),
        }),
        "treatSimple" => Ok(DecisionTreeNode::TreatSimple {
            duration_liver: req_attr(node, "durationLiver")?,
            duration_blood: req_attr(node, "durationBlood")?,
        }),
        "deploy" => Ok(DecisionTreeNode::Deploy {
            component: req_attr(node, "component")?,
        }),
        other => Err(SchemaError::InvalidValue {
            element: other.to_owned(),
            field: "DecisionTree",
            message: format!("unrecognized DecisionTree child element <{other}>"),
        }),
    }
}
