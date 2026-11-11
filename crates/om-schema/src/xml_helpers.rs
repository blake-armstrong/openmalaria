//! Safe, concise `roxmltree::Node` parsing primitives and shared `util.xsd` parsers.

use crate::common::{
    AgeGroupPoint, AgeGroupValues, BetaMeanSample, DecaySpec, OptionElt, SampledValueCv,
    SampledValueLn, SampledValueN, TriggeredDeployGroup, WeibullSample,
};
use crate::error::SchemaError;
use roxmltree::Node;

/// Iterates over element children of `node` (ignoring comments, text, PIs).
pub fn elem_children<'a, 'input: 'a>(
    node: Node<'a, 'input>,
) -> impl Iterator<Item = Node<'a, 'input>> {
    node.children().filter(Node::is_element)
}

/// Finds the first element child matching `local_name`.
pub fn opt_child<'a, 'input: 'a>(
    node: Node<'a, 'input>,
    local_name: &str,
) -> Option<Node<'a, 'input>> {
    elem_children(node).find(|c| c.tag_name().name() == local_name)
}

/// Finds a required element child matching `child`.
pub fn req_child<'a, 'input: 'a>(
    node: Node<'a, 'input>,
    parent: &'static str,
    child: &'static str,
) -> Result<Node<'a, 'input>, SchemaError> {
    opt_child(node, child).ok_or(SchemaError::MissingElement { parent, child })
}

/// Iterates over all element children matching `local_name`.
pub fn children_named<'a, 'input: 'a>(
    node: Node<'a, 'input>,
    local_name: &'a str,
) -> impl Iterator<Item = Node<'a, 'input>> + 'a {
    elem_children(node).filter(move |c| c.tag_name().name() == local_name)
}

/// Reads an optional string attribute by local name.
pub fn opt_attr(node: Node<'_, '_>, attr: &str) -> Option<String> {
    node.attribute(attr).map(str::to_owned)
}

/// Reads a required string attribute by local name.
pub fn req_attr(node: Node<'_, '_>, attr: &'static str) -> Result<String, SchemaError> {
    node.attribute(attr)
        .map(str::to_owned)
        .ok_or_else(|| SchemaError::MissingAttr {
            element: node.tag_name().name().to_owned(),
            attr,
        })
}

/// Parses an XSD boolean (`"true"`, `"1"`, `"false"`, `"0"`).
pub fn parse_xsd_bool(raw: &str, element: &str, field: &'static str) -> Result<bool, SchemaError> {
    match raw.trim() {
        "true" | "1" => Ok(true),
        "false" | "0" => Ok(false),
        other => Err(SchemaError::InvalidValue {
            element: element.to_owned(),
            field,
            message: format!("expected XSD boolean (true/false/1/0), got '{other}'"),
        }),
    }
}

/// Reads an optional XSD boolean attribute.
pub fn opt_bool(node: Node<'_, '_>, attr: &'static str) -> Result<Option<bool>, SchemaError> {
    match node.attribute(attr) {
        Some(v) => Ok(Some(parse_xsd_bool(v, node.tag_name().name(), attr)?)),
        None => Ok(None),
    }
}

/// Reads an XSD boolean attribute with a default value when omitted.
pub fn bool_or(
    node: Node<'_, '_>,
    attr: &'static str,
    default: bool,
) -> Result<bool, SchemaError> {
    Ok(opt_bool(node, attr)?.unwrap_or(default))
}

/// Reads a required XSD boolean attribute.
pub fn req_bool(node: Node<'_, '_>, attr: &'static str) -> Result<bool, SchemaError> {
    opt_bool(node, attr)?.ok_or_else(|| SchemaError::MissingAttr {
        element: node.tag_name().name().to_owned(),
        attr,
    })
}

/// Reads an optional `f64` attribute.
pub fn opt_f64(node: Node<'_, '_>, attr: &'static str) -> Result<Option<f64>, SchemaError> {
    match node.attribute(attr) {
        Some(v) => v
            .trim()
            .parse::<f64>()
            .map(Some)
            .map_err(|e| SchemaError::InvalidValue {
                element: node.tag_name().name().to_owned(),
                field: attr,
                message: e.to_string(),
            }),
        None => Ok(None),
    }
}

/// Reads a required `f64` attribute.
pub fn req_f64(node: Node<'_, '_>, attr: &'static str) -> Result<f64, SchemaError> {
    opt_f64(node, attr)?.ok_or_else(|| SchemaError::MissingAttr {
        element: node.tag_name().name().to_owned(),
        attr,
    })
}

/// Reads an optional `i32` attribute.
pub fn opt_i32(node: Node<'_, '_>, attr: &'static str) -> Result<Option<i32>, SchemaError> {
    match node.attribute(attr) {
        Some(v) => v
            .trim()
            .parse::<i32>()
            .map(Some)
            .map_err(|e| SchemaError::InvalidValue {
                element: node.tag_name().name().to_owned(),
                field: attr,
                message: e.to_string(),
            }),
        None => Ok(None),
    }
}

/// Reads a required `i32` attribute.
pub fn req_i32(node: Node<'_, '_>, attr: &'static str) -> Result<i32, SchemaError> {
    opt_i32(node, attr)?.ok_or_else(|| SchemaError::MissingAttr {
        element: node.tag_name().name().to_owned(),
        attr,
    })
}

/// Reads a required `i64` attribute.
pub fn req_i64(node: Node<'_, '_>, attr: &'static str) -> Result<i64, SchemaError> {
    let raw = req_attr(node, attr)?;
    raw.trim()
        .parse::<i64>()
        .map_err(|e| SchemaError::InvalidValue {
            element: node.tag_name().name().to_owned(),
            field: attr,
            message: e.to_string(),
        })
}

/// Reads trimmed text content inside `node`.
pub fn text_trimmed(node: Node<'_, '_>) -> String {
    node.text().unwrap_or("").trim().to_owned()
}

/// Parses trimmed text content inside `node` as `f64`.
pub fn text_f64(node: Node<'_, '_>, field: &'static str) -> Result<f64, SchemaError> {
    let s = text_trimmed(node);
    s.parse::<f64>().map_err(|e| SchemaError::InvalidValue {
        element: node.tag_name().name().to_owned(),
        field,
        message: format!("'{s}': {e}"),
    })
}

/// Parses trimmed text content inside `node` as `i32`.
pub fn text_i32(node: Node<'_, '_>, field: &'static str) -> Result<i32, SchemaError> {
    let s = text_trimmed(node);
    s.parse::<i32>().map_err(|e| SchemaError::InvalidValue {
        element: node.tag_name().name().to_owned(),
        field,
        message: format!("'{s}': {e}"),
    })
}

/// Reads `<child value="..."/>` as `f64`.
pub fn child_double_val(
    node: Node<'_, '_>,
    parent: &'static str,
    child: &'static str,
) -> Result<f64, SchemaError> {
    req_f64(req_child(node, parent, child)?, "value")
}

/// Reads optional `<child value="..."/>` as `Option<f64>`.
pub fn opt_child_double_val(node: Node<'_, '_>, child: &str) -> Result<Option<f64>, SchemaError> {
    opt_child(node, child)
        .map(|c| req_f64(c, "value"))
        .transpose()
}

/// Reads `<child value="..."/>` as `i32`.
pub fn child_int_val(
    node: Node<'_, '_>,
    parent: &'static str,
    child: &'static str,
) -> Result<i32, SchemaError> {
    req_i32(req_child(node, parent, child)?, "value")
}

/// Parses an `OptionSet` sequence of `<option name="..." value="..."/>`.
pub fn parse_option_set(node: Node<'_, '_>) -> Result<Vec<OptionElt>, SchemaError> {
    children_named(node, "option")
        .map(|opt| {
            Ok(OptionElt {
                name: req_attr(opt, "name")?,
                value: bool_or(opt, "value", true)?,
            })
        })
        .collect()
}

/// Parses an `om:AgeGroupValues` element.
pub fn parse_age_group_values(node: Node<'_, '_>) -> Result<AgeGroupValues, SchemaError> {
    let groups = children_named(node, "group")
        .map(|g| {
            Ok(AgeGroupPoint {
                lowerbound: req_f64(g, "lowerbound")?,
                value: req_f64(g, "value")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(AgeGroupValues {
        interpolation: opt_attr(node, "interpolation"),
        groups,
    })
}

/// Parses a recursive `om:DecayFunction` element into `DecaySpec`.
pub fn parse_decay_spec(node: Node<'_, '_>) -> Result<DecaySpec, SchemaError> {
    let children = children_named(node, "decay")
        .map(parse_decay_spec)
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(DecaySpec {
        function: req_attr(node, "function")?,
        l: opt_attr(node, "L"),
        k: opt_f64(node, "k")?.unwrap_or(1.0),
        cv: opt_f64(node, "CV")?.unwrap_or(0.0),
        increasing: bool_or(node, "increasing", false)?,
        initial_efficacy: opt_f64(node, "initialEfficacy")?.unwrap_or(1.0),
        emax: opt_f64(node, "Emax")?,
        ic50: opt_f64(node, "IC50")?,
        slope: opt_f64(node, "slope")?,
        initial_concentration: opt_f64(node, "initialConcentration")?,
        children,
    })
}

/// Parses `om:SampledValueN`.
pub fn parse_sampled_n(node: Node<'_, '_>) -> Result<SampledValueN, SchemaError> {
    Ok(SampledValueN {
        mean: req_f64(node, "mean")?,
        sd: opt_f64(node, "SD")?,
        distr: opt_attr(node, "distr").unwrap_or_else(|| "const".into()),
    })
}

/// Parses `om:SampledValueCV`.
pub fn parse_sampled_cv(node: Node<'_, '_>) -> Result<SampledValueCv, SchemaError> {
    Ok(SampledValueCv {
        cv: opt_f64(node, "CV")?,
        variance: opt_f64(node, "variance")?,
        truncate: opt_f64(node, "truncate")?,
        distr: opt_attr(node, "distr").unwrap_or_else(|| "const".into()),
    })
}

/// Parses `om:SampledValueLN`.
pub fn parse_sampled_ln(node: Node<'_, '_>) -> Result<SampledValueLn, SchemaError> {
    Ok(SampledValueLn {
        mean: req_f64(node, "mean")?,
        cv: opt_f64(node, "CV")?,
        variance: opt_f64(node, "variance")?,
        truncate: opt_f64(node, "truncate")?,
        distr: opt_attr(node, "distr").unwrap_or_else(|| "const".into()),
    })
}

/// Parses `om:BetaMeanSample`.
pub fn parse_beta_mean(node: Node<'_, '_>) -> Result<BetaMeanSample, SchemaError> {
    Ok(BetaMeanSample {
        mean: req_f64(node, "mean")?,
        variance: req_f64(node, "variance")?,
    })
}

/// Parses `om:WeibullSample`.
pub fn parse_weibull(node: Node<'_, '_>) -> Result<WeibullSample, SchemaError> {
    Ok(WeibullSample {
        scale: req_f64(node, "scale")?,
        shape: req_f64(node, "shape")?,
        distr: req_attr(node, "distr")?,
    })
}

/// Parses `deploy*` groups inside `om:TriggeredDeployments`.
pub fn parse_triggered_deployments(
    node: Node<'_, '_>,
) -> Result<Vec<TriggeredDeployGroup>, SchemaError> {
    children_named(node, "deploy")
        .map(|d| {
            let components = children_named(d, "component")
                .map(|c| req_attr(c, "id"))
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(TriggeredDeployGroup {
                min_age: opt_f64(d, "minAge")?.unwrap_or(0.0),
                max_age: opt_f64(d, "maxAge")?,
                p: opt_f64(d, "p")?.unwrap_or(1.0),
                components,
            })
        })
        .collect()
}
