//! Parsers for `<entomology>` (`entomology.xsd`) and `<pharmacology>` (`pharmacology.xsd`).

use crate::entomology::{
    AnophelesNonHumanParams, AnophelesParams, EirDailyItem, Entomology, EntomologyBackend,
    EntomologyMode, FourierCoeffic, LarvalDaily, LifeCycleSpec, MosqParams, MosqStage,
    NonHumanHostCount, NonVectorSpec, SeasonalityBody, SeasonalitySpec, SimpleMpdSpec, VectorSpec,
};
use crate::error::SchemaError;
use crate::pharmacology::{
    AlleleRestriction, DosageMode, DoseStep, MedicateStep, PdPhenotype, PdSpec, PharmacologySpec,
    PkCompartmentPair, PkConversion, PkElimination, PkPdDosages, PkPdDrug, PkPdSchedule, PkSpec,
};
use crate::xml_helpers::{
    child_double_val, child_int_val, children_named, opt_attr, opt_child, opt_child_double_val,
    opt_f64, parse_beta_mean, parse_sampled_cv, parse_sampled_ln, req_attr, req_child, req_f64,
    req_i32, text_f64, text_trimmed,
};
use roxmltree::Node;

/// Parses `<entomology>`.
pub fn parse_entomology(node: Node<'_, '_>) -> Result<Entomology, SchemaError> {
    let mode_str = req_attr(node, "mode")?;
    let mode = match mode_str.as_str() {
        "forced" => EntomologyMode::Forced,
        "dynamic" => EntomologyMode::Dynamic,
        other => {
            return Err(SchemaError::InvalidValue {
                element: "entomology".into(),
                field: "mode",
                message: format!("expected 'forced' or 'dynamic', got '{other}'"),
            });
        }
    };
    let backend = if let Some(nv) = opt_child(node, "nonVector") {
        EntomologyBackend::NonVector(parse_non_vector(nv)?)
    } else if let Some(vec_node) = opt_child(node, "vector") {
        let anopheles = children_named(vec_node, "anopheles")
            .map(parse_anopheles)
            .collect::<Result<Vec<_>, SchemaError>>()?;
        let non_human_hosts = children_named(vec_node, "nonHumanHosts")
            .map(|nh| {
                Ok(NonHumanHostCount {
                    name: req_attr(nh, "name")?,
                    number: req_f64(nh, "number")?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?;
        EntomologyBackend::Vector(VectorSpec {
            anopheles,
            non_human_hosts,
        })
    } else {
        return Err(SchemaError::MissingElement {
            parent: "entomology",
            child: "nonVector|vector",
        });
    };

    Ok(Entomology {
        name: req_attr(node, "name")?,
        mode,
        scaled_annual_eir: opt_f64(node, "scaledAnnualEIR")?,
        backend,
    })
}

/// Parses an `om:NonVector` element (`<nonVector>` or `<changeEIR><timedDeployment>`).
pub fn parse_non_vector(node: Node<'_, '_>) -> Result<NonVectorSpec, SchemaError> {
    let eir_daily = children_named(node, "EIRDaily")
        .map(|item| {
            Ok(EirDailyItem {
                value: text_f64(item, "EIRDaily")?,
                origin: opt_attr(item, "origin"),
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;
    Ok(NonVectorSpec {
        eip_duration: req_i32(node, "eipDuration")?,
        eir_daily,
    })
}

fn parse_anopheles(node: Node<'_, '_>) -> Result<AnophelesParams, SchemaError> {
    let seas_node = req_child(node, "anopheles", "seasonality")?;
    let seas_body = if let Some(fs) = opt_child(seas_node, "fourierSeries") {
        let coeffs = children_named(fs, "coeffic")
            .map(|c| {
                Ok(FourierCoeffic {
                    a: req_f64(c, "a")?,
                    b: req_f64(c, "b")?,
                })
            })
            .collect::<Result<Vec<_>, SchemaError>>()?;
        SeasonalityBody::FourierSeries {
            eir_rotate_angle: opt_f64(fs, "EIRRotateAngle")?,
            coeffs,
        }
    } else if let Some(mv) = opt_child(seas_node, "monthlyValues") {
        let values = children_named(mv, "value")
            .map(|v| text_f64(v, "value"))
            .collect::<Result<Vec<_>, SchemaError>>()?;
        SeasonalityBody::MonthlyValues {
            smoothing: req_attr(mv, "smoothing")?,
            eir_rotate_angle: opt_f64(mv, "EIRRotateAngle")?,
            values,
        }
    } else if let Some(dv) = opt_child(seas_node, "dailyValues") {
        let values = children_named(dv, "value")
            .map(|v| text_f64(v, "value"))
            .collect::<Result<Vec<_>, SchemaError>>()?;
        SeasonalityBody::DailyValues { values }
    } else {
        return Err(SchemaError::MissingElement {
            parent: "seasonality",
            child: "fourierSeries|monthlyValues|dailyValues",
        });
    };

    let m = req_child(node, "anopheles", "mosq")?;
    let mosq = MosqParams {
        min_infected_threshold: opt_f64(m, "minInfectedThreshold")?.unwrap_or(0.0),
        mosq_rest_duration: child_int_val(m, "mosq", "mosqRestDuration")?,
        extrinsic_incubation_period: child_int_val(m, "mosq", "extrinsicIncubationPeriod")?,
        mosq_laid_eggs_same_day_proportion: child_double_val(
            m,
            "mosq",
            "mosqLaidEggsSameDayProportion",
        )?,
        mosq_seeking_duration: child_double_val(m, "mosq", "mosqSeekingDuration")?,
        mosq_survival_feeding_cycle_probability: child_double_val(
            m,
            "mosq",
            "mosqSurvivalFeedingCycleProbability",
        )?,
        availability: parse_sampled_cv(req_child(m, "mosq", "availability")?)?,
        mosq_prob_biting: parse_beta_mean(req_child(m, "mosq", "mosqProbBiting")?)?,
        mosq_prob_find_rest_site: parse_beta_mean(req_child(m, "mosq", "mosqProbFindRestSite")?)?,
        mosq_prob_resting: parse_beta_mean(req_child(m, "mosq", "mosqProbResting")?)?,
        mosq_prob_ovipositing: child_double_val(m, "mosq", "mosqProbOvipositing")?,
        mosq_human_blood_index: child_double_val(m, "mosq", "mosqHumanBloodIndex")?,
    };

    let life_cycle = opt_child(node, "lifeCycle")
        .map(|lc| -> Result<LifeCycleSpec, SchemaError> {
            let parse_stage = |n: Node<'_, '_>| -> Result<MosqStage, SchemaError> {
                Ok(MosqStage {
                    duration: req_i32(n, "duration")?,
                    survival: req_f64(n, "survival")?,
                })
            };
            let ls = req_child(lc, "lifeCycle", "larvalStage")?;
            let larval_daily = children_named(ls, "daily")
                .map(|d| {
                    Ok(LarvalDaily {
                        resource_usage: req_f64(d, "resourceUsage")?,
                        effect_competition: req_f64(d, "effectCompetition")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(LifeCycleSpec {
                estimated_larval_resources: opt_f64(lc, "estimatedLarvalResources")?
                    .unwrap_or(1e8),
                egg_stage: parse_stage(req_child(lc, "lifeCycle", "eggStage")?)?,
                larval_stage: parse_stage(ls)?,
                larval_daily,
                pupal_stage: parse_stage(req_child(lc, "lifeCycle", "pupalStage")?)?,
                female_eggs_laid_by_oviposit: child_double_val(
                    lc,
                    "lifeCycle",
                    "femaleEggsLaidByOviposit",
                )?,
            })
        })
        .transpose()?;

    let simple_mpd = opt_child(node, "simpleMPD")
        .map(|sm| -> Result<SimpleMpdSpec, SchemaError> {
            Ok(SimpleMpdSpec {
                development_duration: child_int_val(sm, "simpleMPD", "developmentDuration")?,
                development_survival: child_double_val(sm, "simpleMPD", "developmentSurvival")?,
                female_eggs_laid_by_oviposit: child_double_val(
                    sm,
                    "simpleMPD",
                    "femaleEggsLaidByOviposit",
                )?,
            })
        })
        .transpose()?;

    let non_human_hosts = children_named(node, "nonHumanHosts")
        .map(|nh| {
            Ok(AnophelesNonHumanParams {
                name: req_attr(nh, "name")?,
                mosq_relative_ento_availability: child_double_val(
                    nh,
                    "nonHumanHosts",
                    "mosqRelativeEntoAvailability",
                )?,
                mosq_prob_biting: child_double_val(nh, "nonHumanHosts", "mosqProbBiting")?,
                mosq_prob_find_rest_site: child_double_val(
                    nh,
                    "nonHumanHosts",
                    "mosqProbFindRestSite",
                )?,
                mosq_prob_resting: child_double_val(nh, "nonHumanHosts", "mosqProbResting")?,
                host_fecundity_factor: opt_child_double_val(nh, "hostFecundityFactor")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(AnophelesParams {
        mosquito: req_attr(node, "mosquito")?,
        seasonality: SeasonalitySpec {
            input: req_attr(seas_node, "input")?,
            annual_eir: opt_f64(seas_node, "annualEIR")?,
            body: seas_body,
        },
        mosq,
        life_cycle,
        simple_mpd,
        non_human_hosts,
    })
}

/// Parses `<pharmacology>`.
pub fn parse_pharmacology(node: Node<'_, '_>) -> Result<PharmacologySpec, SchemaError> {
    let tr_node = req_child(node, "pharmacology", "treatments")?;
    let schedules = children_named(tr_node, "schedule")
        .map(|s| {
            let medications = children_named(s, "medicate")
                .map(|m| {
                    Ok(MedicateStep {
                        drug: req_attr(m, "drug")?,
                        mg: req_f64(m, "mg")?,
                        hour: req_f64(m, "hour")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(PkPdSchedule {
                name: req_attr(s, "name")?,
                medications,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let dosages = children_named(tr_node, "dosages")
        .map(|d| {
            let mode = if let Some(mult) = opt_child(d, "multiply") {
                DosageMode::Multiply {
                    by: req_attr(mult, "by")?,
                }
            } else {
                let ages = children_named(d, "age")
                    .map(|a| {
                        Ok(DoseStep {
                            lowerbound: req_f64(a, "lowerbound")?,
                            dose_mult: req_f64(a, "dose_mult")?,
                        })
                    })
                    .collect::<Result<Vec<_>, SchemaError>>()?;
                if !ages.is_empty() {
                    DosageMode::Age(ages)
                } else {
                    let masses = children_named(d, "bodymass")
                        .map(|b| {
                            Ok(DoseStep {
                                lowerbound: req_f64(b, "lowerbound")?,
                                dose_mult: req_f64(b, "dose_mult")?,
                            })
                        })
                        .collect::<Result<Vec<_>, SchemaError>>()?;
                    DosageMode::BodyMass(masses)
                }
            };
            Ok(PkPdDosages {
                name: req_attr(d, "name")?,
                mode,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let dr_node = req_child(node, "pharmacology", "drugs")?;
    let drugs = children_named(dr_node, "drug")
        .map(parse_drug)
        .collect::<Result<Vec<_>, SchemaError>>()?;

    Ok(PharmacologySpec {
        schedules,
        dosages,
        drugs,
    })
}

fn parse_drug(node: Node<'_, '_>) -> Result<PkPdDrug, SchemaError> {
    let pd_node = req_child(node, "drug", "PD")?;
    let phenotypes = children_named(pd_node, "phenotype")
        .map(|ph| {
            let restrictions = children_named(ph, "restriction")
                .map(|r| {
                    Ok(AlleleRestriction {
                        on_locus: req_attr(r, "onLocus")?,
                        to_allele: req_attr(r, "toAllele")?,
                    })
                })
                .collect::<Result<Vec<_>, SchemaError>>()?;
            Ok(PdPhenotype {
                name: opt_attr(ph, "name"),
                restrictions,
                max_killing_rate: text_f64(
                    req_child(ph, "phenotype", "max_killing_rate")?,
                    "max_killing_rate",
                )?,
                ic50: parse_sampled_ln(req_child(ph, "phenotype", "IC50")?)?,
                slope: text_f64(req_child(ph, "phenotype", "slope")?, "slope")?,
            })
        })
        .collect::<Result<Vec<_>, SchemaError>>()?;

    let pk_node = req_child(node, "drug", "PK")?;
    let elimination = if let Some(hl) = opt_child(pk_node, "half_life") {
        PkElimination::HalfLife(text_f64(hl, "half_life")?)
    } else {
        PkElimination::EliminationRate {
            k: parse_sampled_ln(req_child(pk_node, "PK", "k")?)?,
            m_exponent: text_f64(req_child(pk_node, "PK", "m_exponent")?, "m_exponent")?,
        }
    };
    let conversion = opt_child(pk_node, "conversion")
        .map(|cv| -> Result<PkConversion, SchemaError> {
            Ok(PkConversion {
                metabolite: text_trimmed(req_child(cv, "conversion", "metabolite")?),
                rate: parse_sampled_ln(req_child(cv, "conversion", "rate")?)?,
                mol_ratio: text_f64(req_child(cv, "conversion", "molRatio")?, "molRatio")?,
                ic50_log_correlation: text_f64(
                    req_child(cv, "conversion", "IC50_log_correlation")?,
                    "IC50_log_correlation",
                )?,
            })
        })
        .transpose()?;
    let compartment2 = opt_child(pk_node, "compartment2")
        .map(|c2| -> Result<PkCompartmentPair, SchemaError> {
            Ok(PkCompartmentPair {
                k_in: parse_sampled_ln(req_child(c2, "compartment2", "k12")?)?,
                k_out: parse_sampled_ln(req_child(c2, "compartment2", "k21")?)?,
            })
        })
        .transpose()?;
    let compartment3 = opt_child(pk_node, "compartment3")
        .map(|c3| -> Result<PkCompartmentPair, SchemaError> {
            Ok(PkCompartmentPair {
                k_in: parse_sampled_ln(req_child(c3, "compartment3", "k13")?)?,
                k_out: parse_sampled_ln(req_child(c3, "compartment3", "k31")?)?,
            })
        })
        .transpose()?;

    Ok(PkPdDrug {
        abbrev: req_attr(node, "abbrev")?,
        pd: PdSpec {
            locus: opt_attr(pd_node, "locus"),
            phenotypes,
        },
        pk: PkSpec {
            negligible_concentration: text_f64(
                req_child(pk_node, "PK", "negligible_concentration")?,
                "negligible_concentration",
            )?,
            elimination,
            k_a: opt_child(pk_node, "k_a").map(parse_sampled_ln).transpose()?,
            conversion,
            vol_dist: parse_sampled_ln(req_child(pk_node, "PK", "vol_dist")?)?,
            compartment2,
            compartment3,
        },
    })
}
