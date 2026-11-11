//! Recursive clinical/intervention `DecisionTree` AST (`schema/healthSystem.xsd`).

use serde::{Deserialize, Serialize};

/// A recursive decision-tree container (`om:DecisionTree`) holding an ordered sequence of child operations.
///
/// In Schema v50, a `DecisionTree` element (or inline `<outcome>` / `<age>` extension) may contain
/// a single branching switch, `<multiple>` heterogeneous nodes, `1..unbounded` leaf actions
/// (`report`, `treatPKPD`, `treatSimple`), or `0..unbounded` `deploy` actions (including empty!).
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct DecisionTree {
    pub name: Option<String>,
    pub items: Vec<DecisionTreeNode>,
}

/// Individual branching switch or leaf action inside a [`DecisionTree`].
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum DecisionTreeNode {
    /// `<multiple>` sequential block of decision-tree nodes.
    Multiple {
        name: Option<String>,
        children: Vec<DecisionTreeNode>,
    },
    /// `<caseType>` switch (`firstLine` vs `secondLine`).
    CaseType {
        name: Option<String>,
        first_line: Box<DecisionTree>,
        second_line: Box<DecisionTree>,
    },
    /// `<infectionOrigin>` switch (`imported`, `introduced`, `indigenous`).
    InfectionOrigin {
        name: Option<String>,
        imported: Box<DecisionTree>,
        introduced: Box<DecisionTree>,
        indigenous: Box<DecisionTree>,
    },
    /// `<diagnostic diagnostic="...">` switch (`positive` vs `negative`).
    Diagnostic {
        name: Option<String>,
        diagnostic: String,
        positive: Box<DecisionTree>,
        negative: Box<DecisionTree>,
    },
    /// `<uncomplicated memory="...">` recent-episode switch.
    Uncomplicated {
        name: Option<String>,
        memory: String,
        positive: Box<DecisionTree>,
        negative: Box<DecisionTree>,
    },
    /// `<severe memory="...">` severe-episode switch.
    Severe {
        name: Option<String>,
        memory: Option<String>,
        positive: Box<DecisionTree>,
        negative: Box<DecisionTree>,
    },
    /// `<cohort component="...">` sub-population membership switch.
    Cohort {
        name: Option<String>,
        component: String,
        positive: Box<DecisionTree>,
        negative: Box<DecisionTree>,
    },
    /// `<random>` stochastic multi-way switch (`<outcome p="...">`).
    Random {
        name: Option<String>,
        outcomes: Vec<RandomOutcome>,
    },
    /// `<age>` deterministic age-bracket switch (`<age lb="...">`).
    Age {
        name: Option<String>,
        branches: Vec<AgeBranch>,
    },
    /// `<noTreatment/>` explicit no-op leaf.
    NoTreatment { name: Option<String> },
    /// `<treatFailure/>` explicit treatment-failure marker leaf.
    TreatFailure { name: Option<String> },
    /// `<report outputNumber="..."/>` survey counter increment (`nCMDTReport`).
    Report {
        name: Option<String>,
        output_number: i32,
    },
    /// `<treatPKPD schedule="..." dosage="..." delay_h="..."/>` PK/PD drug prescription.
    TreatPkPd {
        schedule: String,
        dosage: String,
        delay_h: f64,
    },
    /// `<treatSimple durationLiver="..." durationBlood="..."/>` deterministic clearance prescription.
    TreatSimple {
        duration_liver: String,
        duration_blood: String,
    },
    /// `<deploy component="..."/>` triggered human intervention deployment.
    Deploy { component: String },
}

/// Stochastic branch `<outcome p="...">` inside `<random>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct RandomOutcome {
    pub p: f64,
    pub tree: DecisionTree,
}

/// Age-gated branch `<age lb="...">` inside `<age>`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AgeBranch {
    pub lb: f64,
    pub tree: DecisionTree,
}
