import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST = os.path.join(REPO, "test")
SCHEMA = os.path.join(REPO, "schema")


def read(name):
    with open(os.path.join(TEST, name)) as f:
        return f.read()


def rep(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"expected exactly one {old!r}")
    return text.replace(old, new)


def drop_element(text, start, end):
    i = text.index(start)
    j = text.index(end, i) + len(end)
    return text[:i] + text[j:]


def drop_parameter(text, number):
    return re.sub(rf'\s*<parameter [^>]*number="{number}"[^>]*/>', "", text, count=1)


class Variant:
    def __init__(self, name, scenario, transform=lambda s: s, args=(), omit_resource=None, runs=1):
        self.name = name
        self.scenario = scenario
        self.transform = transform
        self.args = list(args)
        self.omit_resource = omit_resource
        self.runs = runs


def vivax_lsd(p_no="0.3", ignore=None, puse=None, eff="0.8"):
    x = f'<liverStageDrug><pHumanCannotReceive value="{p_no}"/>'
    if ignore is not None:
        x += f'<ignoreCannotReceive value="{ignore}"/>'
    if puse is not None:
        x += f'<pUseUncomplicated value="{puse}"/>'
    return x + f'<effectivenessOnUse value="{eff}"/></liverStageDrug>'


def with_lsd(drug, liver="0"):
    def transform(s):
        s = rep(s, "<treeUCOfficial>", drug + "\n  <treeUCOfficial>")
        s = rep(s, '<treatSimple durationLiver="0"', f'<treatSimple durationLiver="{liver}"')
        return rep(s, '<option name="nLiverStageTreatments" value="false"/>',
                   '<option name="nLiverStageTreatments" value="true"/>')
    return transform


def with_change_hs(drug, first=lambda s: s):
    def transform(s):
        body = s[s.index("<healthSystem>") + len("<healthSystem>"):s.index("</healthSystem>")]
        body = rep(body, "<treeUCOfficial>", drug + "\n  <treeUCOfficial>")
        change = f'<changeHS><timedDeployment time="1360-1-1">{body}</timedDeployment></changeHS>'
        return rep(first(s), '<interventions name="no interventions">',
                   '<interventions name="no interventions">' + change)
    return transform


def second_release(p):
    def transform(s):
        s = rep(s, "<hypnozoiteRelease>", f'<hypnozoiteRelease pSecondRelease="{p}">')
        return rep(s, 'latentRelapse="15"/>',
                   'latentRelapse="15"/><secondReleaseDays mean="300" CV="0.5" distr="lognormal" latentRelapse="20"/>')
    return transform


def clin_option(option):
    return lambda s: rep(s, "<vivaxClinOption>A2j</vivaxClinOption>", f"<vivaxClinOption>{option}</vivaxClinOption>")


def rep_first(text, old, new):
    if old not in text:
        raise ValueError(f"expected {old!r}")
    return text.replace(old, new, 1)


def model_option(name):
    return lambda s: rep(s, "<ModelOptions>", f'<ModelOptions>\n      <option name="{name}" value="true"/>')


TRACKING = """<parasiteGenetics samplingMode="tracking">
    <locus name="mdr">
      <allele name="sensitive" initialFrequency="0.5" fitness="1"/>
      <allele name="resistant" initialFrequency="0.5" fitness="1"/>
    </locus>
  </parasiteGenetics>"""
SECOND_LOCUS = """</locus>
    <locus name="hrp2">
      <allele name="present" initialFrequency="0.7" fitness="1"/>
      <allele name="deleted" initialFrequency="0.2" fitness="0.9" hrp2_deletion="true"/>
      <allele name="partial" initialFrequency="0.1" fitness="0.95"/>
    </locus>"""


PHENOTYPE = "<max_killing_rate>{v}</max_killing_rate><IC50 mean=\"{ic50}\"/><slope>{slope}</slope>"
DHA = ('<drug abbrev="DHA"><PD><phenotype>' + PHENOTYPE.format(v=27.6, ic50=0.009, slope=4) + "</phenotype></PD>"
       '<PK><negligible_concentration>1e-17</negligible_concentration><k mean="25.4"/><m_exponent>0</m_exponent>'
       '<vol_dist mean="1.49"/></PK></drug>')


def artesunate(correlation="1.0", compartment2=False):
    return ('<drug abbrev="AS"><PD><phenotype>' + PHENOTYPE.format(v=27.6, ic50=0.0016, slope=4) + "</phenotype></PD>"
            '<PK><negligible_concentration>1e-45</negligible_concentration><k mean="0"/><m_exponent>0</m_exponent>'
            '<k_a mean="252"/><conversion><metabolite>DHA</metabolite><rate mean="30.96" CV="0.2" distr="lognormal"/>'
            f"<molRatio>0.741155</molRatio><IC50_log_correlation>{correlation}</IC50_log_correlation></conversion>"
            '<vol_dist mean="7.1" CV="0.3" distr="lognormal"/>' + ("<compartment2><k12 mean=\"1\"/><k21 mean=\"1\"/></compartment2>"
                                                 if compartment2 else "") + "</PK></drug>")


def piperaquine(k_a=True, compartment2=True):
    return ('<drug abbrev="PPQ3"><PD><phenotype>' + PHENOTYPE.format(v=3.45, ic50=0.020831339, slope=6)
            + "</phenotype></PD><PK><negligible_concentration>0.005</negligible_concentration>"
            '<k mean="16.314788273615637"/><m_exponent>1</m_exponent>' + ('<k_a mean="3.4825"/>' if k_a else "")
            + '<vol_dist mean="57.5625" CV="0.2" distr="lognormal"/>'
            + ('<compartment2><k12 mean="1.854166666666667"/><k21 mean="1.1545945945945946"/></compartment2>'
               if compartment2 else "")
            + '<compartment3><k13 mean="0.9027777777777778"/><k31 mean="0.07948639559767655"/></compartment3>'
            "</PK></drug>")


def with_drugs(*drugs, schedule=('<medicate drug="AS" hour="0" mg="4"/>', '<medicate drug="PPQ3" hour="0" mg="18"/>')):
    def transform(s):
        s = rep(s, "</drugs>", "".join(drugs) + "</drugs>")
        return rep(s, '<medicate drug="CQ" hour="0" mg="10"/>', '<medicate drug="CQ" hour="0" mg="10"/>'
                   + "".join(schedule))
    return transform


def duplicate_cq(s):
    i = s.index('<drug abbrev="CQ">')
    j = s.index("</drug>", i) + len("</drug>")
    return s[:j] + s[i:j] + s[j:]


SENSITIVE = '<restriction onLocus="mdr" toAllele="sensitive"/>'
RESISTANT = '<restriction onLocus="mdr" toAllele="resistant"/>'


NMF = """<NonMalariaFevers>
        <incidence>
          <group lowerbound="0" value="0.32"/>
          <group lowerbound="5" value="0.25"/>
          <group lowerbound="20" value="0.18"/>
        </incidence>
      </NonMalariaFevers>"""


def nmf_on(s):
    return rep(s, '<option name="NON_MALARIA_FEVERS" value="false"/>', '<option name="NON_MALARIA_FEVERS" value="true"/>')


def vivax_nmf(s):
    return rep(nmf_on(s), '<clinical healthSystemMemory="6t">', '<clinical healthSystemMemory="6t">\n      ' + NMF)


PYROGEN_READ_ORDER = (18, 21, 24, 30, 28, 25, 13, 14, 26)


ES = "scenarioESTS.xml"
AGES = '<group lowerbound="0" value="{}"/><group lowerbound="5" value="{}"/>'
CLINICAL_NMF = ("<NonMalariaFevers><incidence>" + AGES.format(0.32, 0.18) + "</incidence>"
                "<prNeedTreatmentNMF>" + AGES.format(0.1, 0.05) + "</prNeedTreatmentNMF>"
                "<prNeedTreatmentMF>" + AGES.format(0.05, 0.02) + "</prNeedTreatmentMF></NonMalariaFevers>")
HS_NMF = ("<NonMalariaFevers><prTreatment>0.5</prTreatment><effectNegativeTest>1.2</effectNegativeTest>"
          "<effectPositiveTest>0.8</effectPositiveTest><effectNeed>1.5</effectNeed><effectInformal>0.9</effectInformal>"
          "<CFR>" + AGES.format(0.3, 0.1) + "</CFR><TreatmentEfficacy>0.6</TreatmentEfficacy></NonMalariaFevers>")


def es_nmf(clinical=CLINICAL_NMF, hs=HS_NMF):
    def transform(s):
        s = model_option("NON_MALARIA_FEVERS")(s)
        s = rep(s, '<clinical healthSystemMemory="14d"/>', f'<clinical healthSystemMemory="14d">{clinical}</clinical>')
        return rep(s, "</ClinicalOutcomes>", "</ClinicalOutcomes>" + hs)
    return transform


def es_change_hs(transform_body=lambda b: b):
    def transform(s):
        body = s[s.index("<healthSystem>") + len("<healthSystem>"):s.index("</healthSystem>")]
        change = f'<changeHS><timedDeployment time="0">{transform_body(body)}</timedDeployment></changeHS>'
        return rep(s, '<interventions name="No intervention"/>',
                   f'<interventions name="No intervention">{change}</interventions>')
    return transform


def section(name, start, end):
    text = read(name)
    i = text.index(start)
    return text[i:text.index(end, i) + len(end)]


def replace_element(start, end, new):
    def transform(s):
        i = s.index(start)
        j = s.index(end, i) + len(end)
        return s[:i] + new() + s[j:]
    return transform


def dt5_section():
    return section("scenario1.xml", "<DecisionTree5Day", "</DecisionTree5Day>")


def severe_mda(s):
    s = rep_first(s, "<decisionTree>", '<decisionTree><severe><positive><treatPKPD schedule="AL_treat" '
                                       'dosage="AL_dosages"/></positive><negative>')
    s = rep_first(s, "</decisionTree>", "</negative></severe></decisionTree>")
    return rep(s, '<deploy coverage="0.85" time="1484"/>',
               '<deploy coverage="1" time="1484" repeatStep="1t" repeatEnd="1849t"/>')


DIELMO = "scenarioDecisionTree5DayDielmo.xml"
REPORT_80 = '<report outputNumber="80"/>'
DIELMO_TREAT = '<treatSimple durationLiver="0" durationBlood="1t"/>'
NO_TREATMENT = "<noTreatment/>"


def branches(name, attrs, positive, negative):
    return f"<{name} {attrs}><positive>{positive}</positive><negative>{negative}</negative></{name}>"


def age_switch(*groups):
    return "<age>" + "".join(f'<age lb="{lb}">{tree}</age>' for lb, tree in groups) + "</age>"


def report_outputs(*numbers):
    options = "".join(f'<option name="nCMDTReport" outputNumber="{n}" value="true"/>' for n in numbers)
    return lambda s: rep(s, "<SurveyOptions>", "<SurveyOptions>" + options)


def dielmo_extra(tree, *numbers):
    return lambda s: report_outputs(*numbers)(rep(s, REPORT_80, REPORT_80 + tree))


def dielmo_rare_outcome(tree):
    def transform(s):
        i = s.index('<outcome p="0.128">')
        j = s.index(NO_TREATMENT, i)
        return s[:j] + tree + s[j + len(NO_TREATMENT):]
    return transform


def uncomplicated_pair(first, second):
    return dielmo_extra("".join(branches("uncomplicated", f'memory="{m}"', '<report outputNumber="90"/>', NO_TREATMENT)
                                for m in (first, second)), 90)


def cohort_pair(first, second):
    def transform(s):
        pair = "".join(branches("cohort", f'component="{c}"', '<report outputNumber="90"/>', NO_TREATMENT)
                       for c in (first, second))
        s = rep(report_outputs(90)(s), "<caseType>", "<multiple>" + pair + "<caseType>")
        return rep_first(s, "</caseType>", "</caseType></multiple>")
    return transform


def wrap_cohort(component):
    def transform(s):
        s = rep(s, "<caseType>", f'<cohort component="{component}"><positive>{NO_TREATMENT}</positive><negative><caseType>')
        return rep_first(s, "</caseType>", "</caseType></negative></cohort>")
    return transform


def wrap_es_complicated(before, after):
    def transform(s):
        s = rep(s, "<complicated>", "<complicated>" + before)
        return rep(s, "</complicated>", after + "</complicated>")
    return transform


DAILY_PR_TWO_DAYS = "<dailyPrImmUCTS>0.2</dailyPrImmUCTS><dailyPrImmUCTS>0.8</dailyPrImmUCTS>"
CLINICAL_READ_ORDER = (17, 19, 38, 33)


AF = "scenarioAvailabilityFilter.xml"
COPULA = "scenarioGaussianCopulaGamma.xml"


def second_species(s):
    i = s.index('<anopheles mosquito="gambiae_ss">')
    j = s.index("</anopheles>", i) + len("</anopheles>")
    block = s[i:j].replace('mosquito="gambiae_ss"', 'mosquito="funestus"').replace(
        'annualEIR="24.826144381650714"', 'annualEIR="10"')
    s = s[:j] + block + s[j:]
    i = s.index('<anophelesParams mosquito="gambiae_ss"')
    j = s.index("</anophelesParams>", i) + len("</anophelesParams>")
    return s[:j] + s[i:j].replace('mosquito="gambiae_ss"', 'mosquito="funestus"') + s[j:]


def copula_in_run(s):
    return rep(s, 'time="2030-01-01" repeatStep="3y"', 'time="2005-01-01" repeatStep="3y"')


def const_first_species_availability(s):
    return rep(s, '<availability distr="gamma" CV="3"/>', '<availability distr="const"/>')

V = "scenarioVivax.xml"
G = "scenarioGenotypes.xml"
M = "scenarioMSAT_HRP2.xml"
GROUPS = {
    "registries": [
        Variant("genotypes_print", G, args=["--print-genotypes"]),
        Variant("genotypes_print_two_loci", G, lambda s: rep_first(s, "</locus>", SECOND_LOCUS),
                ["--print-genotypes"]),
        Variant("genotypes_print_molpairwise", "scenarioMolPairwise.xml", args=["--print-genotypes"]),
        Variant("genotypes_print_none", "scenario5.xml", args=["--print-genotypes"]),
        Variant("genotypes_tracking_checkpoint", G, args=["--checkpoint"], runs=2),
        Variant("genotypes_initial_checkpoint", G, lambda s: rep(s, 'samplingMode="tracking"', 'samplingMode="initial"'),
                ["--checkpoint"], runs=2),
        Variant("genotypes_tracking_non_vector", "scenario5.xml",
                lambda s: rep(s, "</entomology>", "</entomology>\n  " + TRACKING)),
        Variant("genotypes_tracking_forced", G, lambda s: rep(s, '<entomology mode="dynamic"', '<entomology mode="forced"')),
        Variant("genotypes_freq_sum", G, lambda s: rep(s, 'name="resistant" initialFrequency="0.5"',
                                                         'name="resistant" initialFrequency="0.6"')),
        Variant("diagnostics_duplicate", G, lambda s: rep(s, '<diagnostic name="neonatal"', '<diagnostic name="standard"')),
        Variant("diagnostics_specificity", M, lambda s: rep(s, 'specificity=".942"', 'specificity="1.5"')),
        Variant("diagnostics_negative_density", G, lambda s: rep_first(s, 'minDensity="40"', 'minDensity="-1"')),
        Variant("diagnostics_garki_no_units", M, model_option("GARKI_DENSITY_BIAS")),
        Variant("diagnostics_garki_survey_diagnostic", G, model_option("GARKI_DENSITY_BIAS")),
        Variant("diagnostics_garki_detection_limit", "scenario5.xml", model_option("GARKI_DENSITY_BIAS")),
        Variant("diagnostics_limit_and_diagnostic", "scenario5.xml",
                lambda s: rep(s, '<surveys detectionLimit="40">', '<surveys detectionLimit="40" diagnostic="x">')),
        Variant("diagnostics_neither", G, lambda s: rep(s, '<surveys diagnostic="standard">', "<surveys>")),
        Variant("diagnostics_hrp2_monitoring", M, lambda s: rep(s, '<surveys diagnostic="standard">',
                                                              '<surveys diagnostic="RDT">')),
        Variant("diagnostics_unknown_survey", G, lambda s: rep(s, '<surveys diagnostic="standard">',
                                                             '<surveys diagnostic="nope">')),
        Variant("diagnostics_unknown_intervention", M,
                lambda s: rep_first(s, '<diagnostic diagnostic="RDT">', '<diagnostic diagnostic="nope">')),
        Variant("diagnostics_unknown_neonatal", G, lambda s: rep(s, '<NeonatalMortality diagnostic="neonatal"/>',
                                                               '<NeonatalMortality diagnostic="nope"/>')),
        Variant("diagnostics_detection_limit", "scenario5.xml"),
        Variant("diagnostics_detection_limit_nodeprec", "scenario5.xml", args=["--no-deprecation-warnings"]),
        Variant("diagnostics_analysis_no_22", "scenario5.xml", lambda s: rep(s, 'analysisNo="99"', 'analysisNo="22"')),
        Variant("diagnostics_hrp2_msat", M),
        Variant("treatments_zero_steps", "scenario5.xml",
                lambda s: rep_first(s, '<clearInfections stage="both" timesteps="-1"/>',
                                    '<clearInfections stage="both" timesteps="0"/>')),
        Variant("treatments_liver_twice", "scenario5.xml",
                lambda s: rep_first(s, '<clearInfections stage="both" timesteps="-1"/>',
                                    '<clearInfections stage="both" timesteps="-1"/>'
                                    '<clearInfections stage="liver" timesteps="1"/>')),
    ],
    "vivax": [
        Variant("plain", V),
        Variant("lsd_neg", V, with_lsd(vivax_lsd(), "-1t")),
        Variant("lsd_pos", V, with_lsd(vivax_lsd(ignore="true"), "1t")),
        Variant("lsd_eff1", V, with_lsd(vivax_lsd(eff="1"), "5d")),
        Variant("lsd_puse", V, with_lsd(vivax_lsd(puse="0.5"))),
        Variant("lsd_puse_nodeprec", V, with_lsd(vivax_lsd(puse="0.5")), ["--no-deprecation-warnings"]),
        Variant("lsd_puse_conflict", V, with_lsd(vivax_lsd(puse="0.5"), "-1t")),
        Variant("changehs_same", V, with_change_hs(vivax_lsd(puse="0.7"), with_lsd(vivax_lsd(), "-1t"))),
        Variant("changehs_same_checkpoint", V, with_change_hs(vivax_lsd(puse="0.7"), with_lsd(vivax_lsd(), "-1t")),
                ["--checkpoint"], runs=2),
        Variant("changehs_diff", V, with_change_hs(vivax_lsd(p_no="0.4"), with_lsd(vivax_lsd(), "-1t"))),
        Variant("changehs_from_none", V, with_change_hs(vivax_lsd(p_no="0.4"))),
        Variant("clin_b1j", V, clin_option("B1j")),
        Variant("clin_bad", V, clin_option("C1j")),
        Variant("second_release", V, second_release("0.4")),
        Variant("second_release_all", V, second_release("1")),
        Variant("latentp_bad", V, lambda s: rep(s, 'latentp="10d"', 'latentp="abc"')),
        Variant("latentp_steps", V, lambda s: rep(s, 'latentp="10d"', 'latentp="3t"')),
        Variant("no_vivax", V, lambda s: drop_element(s, "<vivax>", "</vivax>")),
    ],
    "pkpd": [
        Variant("conversion_three_comp", G, with_drugs(DHA, artesunate(), piperaquine())),
        Variant("conversion_three_comp_checkpoint", G, with_drugs(DHA, artesunate(), piperaquine()), ["--checkpoint"],
                runs=2),
        Variant("conversion_three_comp_print", G, with_drugs(DHA, artesunate(), piperaquine()), ["--print-genotypes"]),
        Variant("print_two_loci", G, lambda s: rep_first(s, "</locus>", SECOND_LOCUS), ["--print-genotypes"]),
        Variant("drug_twice", G, duplicate_cq),
        Variant("metabolite_after_parent", G, with_drugs(artesunate(), DHA, piperaquine())),
        Variant("metabolite_missing", G, with_drugs(artesunate(), piperaquine())),
        Variant("conversion_with_compartment2", G, with_drugs(DHA, artesunate(compartment2=True), piperaquine())),
        Variant("ic50_correlation_bad", G, with_drugs(DHA, artesunate("1.5"), piperaquine())),
        Variant("compartment2_without_k_a", G, with_drugs(DHA, artesunate(), piperaquine(k_a=False))),
        Variant("compartment3_without_compartment2", G, with_drugs(DHA, artesunate(), piperaquine(compartment2=False))),
        Variant("k_a_without_compartment2", G, lambda s: rep(s, "<half_life>30.006</half_life>",
                                                            '<half_life>30.006</half_life><k_a mean="1"/>')),
        Variant("phenotype_unknown_allele", G, lambda s: rep(s, SENSITIVE, SENSITIVE.replace("sensitive", "nope"))),
        Variant("phenotype_locus_only_second", G, lambda s: rep(s, RESISTANT, RESISTANT.replace('"mdr"', '"x"'))),
        Variant("phenotype_locus_only_first", G, lambda s: rep(rep_first(s, "</locus>", SECOND_LOCUS), SENSITIVE,
                                                              SENSITIVE + '<restriction onLocus="hrp2" '
                                                                          'toAllele="present"/>')),
        Variant("phenotype_ambiguous", G, lambda s: rep(s, RESISTANT, SENSITIVE)),
        Variant("phenotype_too_restrictive", G, lambda s: rep(
            rep(rep_first(s, "</locus>", SECOND_LOCUS), SENSITIVE,
                SENSITIVE + '<restriction onLocus="hrp2" toAllele="present"/>'),
            RESISTANT, RESISTANT + '<restriction onLocus="hrp2" toAllele="present"/>')),
        Variant("phenotypes_without_restrictions", G, lambda s: rep(rep(s, SENSITIVE, ""), RESISTANT, "")),
        Variant("dosage_first_bound", G, lambda s: rep(s, '<bodymass lowerbound="0" dose_mult="5"/>',
                                                      '<bodymass lowerbound="1" dose_mult="5"/>')),
        Variant("dosage_order", G, lambda s: rep(s, '<bodymass lowerbound="10" dose_mult="20"/>',
                                                '<bodymass lowerbound="4" dose_mult="20"/>')),
        Variant("unknown_drug", G, lambda s: rep(s, '<medicate drug="CQ" hour="0" mg="10"/>',
                                                '<medicate drug="XX" hour="0" mg="10"/>')),
        Variant("unknown_schedule", G, lambda s: rep(s, '<treatPKPD schedule="CQ_3_dose"',
                                                    '<treatPKPD schedule="nope"')),
        Variant("unknown_dosage", G, lambda s: rep(s, 'dosage="CQ"/>', 'dosage="nope"/>')),
        Variant("no_pharmacology", G, lambda s: drop_element(s, "<pharmacology>", "</pharmacology>")),
        Variant("intervention_unknown_schedule", "scenarioCohort.xml",
                lambda s: rep(s, '<component id="MDA">\n        <treatPKPD schedule="MQ1"',
                              '<component id="MDA">\n        <treatPKPD schedule="nope"')),
        Variant("intervention_checkpoint", "scenarioCohort.xml", args=["--checkpoint"], runs=2),
        Variant("models_checkpoint", "scenarioPkPdModels.xml", args=["--checkpoint"], runs=2),
    ],
    "pathogenesis": [
        Variant("vivax_nmf", V, vivax_nmf),
        Variant("vivax_nmf_checkpoint", V, vivax_nmf, ["--checkpoint"], runs=2),
        Variant("vivax_nmf_no_element", V, nmf_on),
        Variant("falciparum_nmf_no_element", "scenario5.xml", model_option("NON_MALARIA_FEVERS")),
        Variant("falciparum_nmf_no_element_latentp_bad", "scenario5.xml",
                lambda s: rep(model_option("NON_MALARIA_FEVERS")(s), 'latentp="3t"', 'latentp="abc"')),
        Variant("mueller_checkpoint", "scenario10.xml", args=["--checkpoint"], runs=2),
        Variant("predetermined_checkpoint", "scenario9.xml", args=["--checkpoint"], runs=2),
        Variant("pyrogen_nmf_checkpoint", "scenarioNonHumanHosts.xml", args=["--checkpoint"], runs=2),
    ] + [
        Variant(f"pyrogen_no_param_{n}", "scenario5.xml", lambda s, n=n: drop_parameter(s, n))
        for n in PYROGEN_READ_ORDER
    ] + [
        Variant(f"pyrogen_no_params_{m}_{n}", "scenario5.xml", lambda s, m=m, n=n: drop_parameter(drop_parameter(s, m), n))
        for m, n in zip(PYROGEN_READ_ORDER, PYROGEN_READ_ORDER[1:])
    ] + [
        Variant(f"mueller_no_param_{n}", "scenario10.xml", lambda s, n=n: drop_parameter(s, n)) for n in (31, 32)
    ] + [
        Variant("mueller_no_params_31_32", "scenario10.xml", lambda s: drop_parameter(drop_parameter(s, 31), 32)),
    ],
    "clinical": [
        Variant("io_plain", "scenario5.xml"),
        Variant("io_nodeprec", "scenario5.xml", args=["--no-deprecation-warnings"]),
        Variant("io_checkpoint", "scenario5.xml", args=["--checkpoint"], runs=2),
        Variant("dt5_checkpoint", "scenario1.xml", args=["--checkpoint"], runs=2),
        Variant("es_checkpoint", ES, args=["--checkpoint"], runs=2),
        Variant("io_cfr_pf_use_hospital", "scenario5.xml", model_option("CFR_PF_USE_HOSPITAL")),
        Variant("es_nmf", ES, es_nmf()),
        Variant("es_nmf_checkpoint", ES, es_nmf(), ["--checkpoint"], runs=2),
        Variant("es_nmf_no_need_treatment", ES, es_nmf(clinical=CLINICAL_NMF[:CLINICAL_NMF.index("<prNeed")]
                                                       + "</NonMalariaFevers>")),
        Variant("es_nmf_no_hs_element", ES, es_nmf(hs="")),
        Variant("es_changehs_daily_pr", ES, es_change_hs(lambda b: rep(b, "<dailyPrImmUCTS>1</dailyPrImmUCTS>",
                                                                       DAILY_PR_TWO_DAYS))),
        Variant("es_changehs_daily_pr_checkpoint", ES,
                es_change_hs(lambda b: rep(b, "<dailyPrImmUCTS>1</dailyPrImmUCTS>", DAILY_PR_TWO_DAYS)),
                ["--checkpoint"], runs=2),
        Variant("es_changehs_same", ES, es_change_hs()),
        Variant("es_changehs_case_duration", ES, es_change_hs(
            lambda b: rep(b, "<uncomplicatedCaseDuration>3</uncomplicatedCaseDuration>",
                          "<uncomplicatedCaseDuration>9</uncomplicatedCaseDuration>"))),
        Variant("es_changehs_wrong_section", ES,
                es_change_hs(replace_element("<EventScheduler>", "</EventScheduler>", dt5_section))),
        Variant("es_wrong_section", ES, replace_element("<EventScheduler>", "</EventScheduler>", dt5_section)),
        Variant("es_5_day", "scenario5.xml", model_option("CLINICAL_EVENT_SCHEDULER")),
        Variant("es_treat_het", ES, model_option("TREAT_HET")),
        Variant("es_daily_pr_sum", ES, lambda s: rep(s, "<dailyPrImmUCTS>1</dailyPrImmUCTS>",
                                                    "<dailyPrImmUCTS>0.5</dailyPrImmUCTS>")),
        Variant("es_risk_duration", ES, lambda s: rep(s, "<complicatedRiskDuration>6</complicatedRiskDuration>",
                                                     "<complicatedRiskDuration>7</complicatedRiskDuration>")),
        Variant("io_changehs_wrong_section", "scenario4.xml",
                replace_element('<ImmediateOutcomes name="Test HS">', "</ImmediateOutcomes>", dt5_section)),
        Variant("dt5_wrong_section", "scenario1.xml",
                replace_element("<DecisionTree5Day", "</DecisionTree5Day>",
                                lambda: section(ES, "<EventScheduler>", "</EventScheduler>"))),
        Variant("dt_intervention_severe_node", "scenarioMSAT.xml", severe_mda),
        Variant("io_bad_drug", "scenario5.xml", lambda s: rep_first(s, 'firstLine="', 'firstLine="XX')),
        Variant("dt5_uncomplicated_memory", "scenarioDecisionTree5DayDielmo.xml",
                lambda s: rep_first(s, "<uncomplicated>", '<uncomplicated memory="2t">')),
        Variant("hs_memory_bad", "scenario5.xml", lambda s: rep(s, 'healthSystemMemory="6"', 'healthSystemMemory="abc"')),
        Variant("hs_memory_bad_no_param_17", "scenario5.xml",
                lambda s: drop_parameter(rep(s, 'healthSystemMemory="6"', 'healthSystemMemory="abc"'), 17)),
    ] + [
        Variant(f"io_no_param_{n}", "scenario5.xml", lambda s, n=n: drop_parameter(s, n)) for n in (17, 19)
    ] + [
        Variant("io_no_params_17_19", "scenario5.xml", lambda s: drop_parameter(drop_parameter(s, 17), 19)),
    ] + [
        Variant(f"es_no_param_{n}", ES, lambda s, n=n: drop_parameter(s, n)) for n in CLINICAL_READ_ORDER
    ] + [
        Variant(f"es_no_params_{m}_{n}", ES, lambda s, m=m, n=n: drop_parameter(drop_parameter(s, m), n))
        for m, n in zip(CLINICAL_READ_ORDER, CLINICAL_READ_ORDER[1:])
    ],
    "decision-trees": [
        Variant("dielmo_plain", DIELMO),
        Variant("dielmo_checkpoint", DIELMO, args=["--checkpoint"], runs=2),
        Variant("infection_origin", "scenarioInfectionOrigin.xml",
                lambda s: rep_first(s, '<treatSimple durationLiver="0" durationBlood="1t"/>',
                                    "<infectionOrigin><imported><noTreatment/></imported><introduced>"
                                    '<treatSimple durationLiver="0" durationBlood="1t"/></introduced><indigenous>'
                                    '<treatSimple durationLiver="0" durationBlood="1t"/></indigenous></infectionOrigin>')),
        Variant("age", DIELMO, dielmo_extra(age_switch((0, '<report outputNumber="81"/>'),
                                                       (5, '<report outputNumber="82"/>')), 81, 82)),
        Variant("age_first_bound", DIELMO, dielmo_extra(age_switch((1, NO_TREATMENT), (5, NO_TREATMENT)))),
        Variant("age_order", DIELMO, dielmo_extra(age_switch((0, NO_TREATMENT), (5, NO_TREATMENT), (5, NO_TREATMENT)))),
        Variant("cohort", "scenarioCohort.xml", wrap_cohort("cohort")),
        Variant("cohort_unknown_component", "scenarioCohort.xml", wrap_cohort("nope")),
        Variant("es_severe", ES, wrap_es_complicated(f"<severe><positive>{NO_TREATMENT}</positive><negative>",
                                                     "</negative></severe>")),
        Variant("es_case_type_under_severe", ES, wrap_es_complicated(
            "<severe><positive><caseType><firstLine>", f"</firstLine><secondLine>{NO_TREATMENT}</secondLine></caseType>"
                                                       f"</positive><negative>{NO_TREATMENT}</negative></severe>")),
        Variant("es_case_type_complicated", ES, wrap_es_complicated(
            "<caseType><firstLine>", f"</firstLine><secondLine>{NO_TREATMENT}</secondLine></caseType>")),
        Variant("case_type_intervention", DIELMO, dielmo_extra(
            f"<caseType><firstLine>{NO_TREATMENT}</firstLine><secondLine>{NO_TREATMENT}</secondLine></caseType>")),
        Variant("random_sum", DIELMO, lambda s: rep(s, '<outcome p="0.128">', '<outcome p="0.028">')),
        Variant("unknown_diagnostic", DIELMO, lambda s: rep(s, '<diagnostic diagnostic="standard">',
                                                           '<diagnostic diagnostic="nope">')),
        Variant("unknown_diagnostic_and_random_sum", DIELMO, lambda s: rep(
            rep(s, '<outcome p="0.128">', '<outcome p="0.028">'), '<diagnostic diagnostic="standard">',
            '<diagnostic diagnostic="nope">')),
        Variant("unknown_diagnostic_and_bad_child", DIELMO, lambda s: rep(
            rep(s, '<report outputNumber="3"/>', '<random><outcome p="0.5"><noTreatment/></outcome></random>'),
            '<diagnostic diagnostic="standard">', '<diagnostic diagnostic="nope">')),
        Variant("age_order_and_bad_child", DIELMO, dielmo_extra(age_switch(
            (0, NO_TREATMENT), (5, '<random><outcome p="0.5"><noTreatment/></outcome></random>'), (5, NO_TREATMENT)))),
        Variant("treat_simple_negative", DIELMO, lambda s: rep(s, DIELMO_TREAT, DIELMO_TREAT.replace('"1t"', '"-2t"'))),
        Variant("treat_simple_format", DIELMO, lambda s: rep(s, DIELMO_TREAT, DIELMO_TREAT.replace('"1t"', '"abc"'))),
        Variant("dedupe_uncomplicated_memory_0_1", DIELMO, uncomplicated_pair("0t", "1t")),
        Variant("dedupe_uncomplicated_memory_0_0", DIELMO, uncomplicated_pair("0t", "0t")),
        Variant("dedupe_uncomplicated_memory_1_1", DIELMO, uncomplicated_pair("1t", "1t")),
        Variant("dedupe_treat_simple_blood_1_5", DIELMO,
                dielmo_rare_outcome(DIELMO_TREAT.replace('"1t"', '"5t"'))),
        Variant("dedupe_treat_simple_blood_5_5", DIELMO, lambda s: dielmo_rare_outcome(
            DIELMO_TREAT.replace('"1t"', '"5t"'))(rep(s, DIELMO_TREAT, DIELMO_TREAT.replace('"1t"', '"5t"')))),
        Variant("dedupe_treat_simple_blood_1_1", DIELMO, dielmo_rare_outcome(DIELMO_TREAT)),
        Variant("dedupe_cohort_cohort_mda", "scenarioCohort.xml", cohort_pair("cohort", "MDA")),
        Variant("dedupe_cohort_cohort_cohort", "scenarioCohort.xml", cohort_pair("cohort", "cohort")),
        Variant("dedupe_cohort_mda_mda", "scenarioCohort.xml", cohort_pair("MDA", "MDA")),
    ],
    "transmission": [
        Variant("vector_plain", AF),
        Variant("vector_checkpoint", AF, args=["--checkpoint"], runs=2),
        Variant("vector_print_eir", AF, args=["--print-EIR"]),
        Variant("non_vector_print_eir", "scenario5.xml", args=["--print-EIR"]),
        Variant("life_cycle", AF, model_option("VECTOR_LIFE_CYCLE_MODEL")),
        Variant("non_vector_simple_mpd", "scenario5.xml", model_option("VECTOR_SIMPLE_MPD_MODEL")),
        Variant("mass_action_het_first", AF, model_option("NEGATIVE_BINOMIAL_MASS_ACTION")),
        Variant("mass_action_het_second", AF,
                lambda s: const_first_species_availability(model_option("NEGATIVE_BINOMIAL_MASS_ACTION")(s))),
        Variant("mass_action_het_and_life_cycle", AF,
                lambda s: model_option("VECTOR_LIFE_CYCLE_MODEL")(model_option("NEGATIVE_BINOMIAL_MASS_ACTION")(s))),
        Variant("filter_percentile_101", AF, lambda s: rep_first(s, 'maxAvailability="100" time="2005-01-01"/>',
                                                                'maxAvailability="101" time="2005-01-01"/>')),
        Variant("copula_deployed", COPULA, copula_in_run),
        Variant("copula_deployed_checkpoint", COPULA, copula_in_run, ["--checkpoint"], runs=2),
        Variant("copula_deployed_two_species", COPULA, lambda s: second_species(copula_in_run(s))),
        Variant("copula_two_species_not_deployed", COPULA, second_species),
    ],
    "falciparum-init": [
        Variant("molineaux_no_weight", "scenarioMolineaux.xml", lambda s: drop_element(s, "<weight ", "</weight>")),
        Variant("molineaux_no_param_34", "scenarioMolineaux.xml", lambda s: drop_parameter(s, 34)),
        Variant("descriptive_latentp_bad", "scenario5.xml", lambda s: rep(s, 'latentp="3t"', 'latentp="abc"')),
        Variant("descriptive_1_day", "scenario5.xml", lambda s: rep(s, 'interval="5"', 'interval="1"')),
        Variant("descriptive_no_densities", "scenario5.xml", omit_resource="densities.csv"),
        Variant("empirical_no_autoregression", "scenarioEmpirical.xml", omit_resource="autoRegressionParameters.csv"),
    ] + [
        Variant(f"descriptive_no_param_{n}", "scenario5.xml", lambda s, n=n: drop_parameter(s, n))
        for n in (2, 6, 22, 23, 27)
    ] + [
        Variant("descriptive_no_params_23_27", "scenario5.xml", lambda s: drop_parameter(drop_parameter(s, 23), 27)),
    ],
}


TRACE = re.compile(r"(?<=Call stack, starting from )\S+:\d+(?=:)")


def schema_name(text):
    m = re.search(r'schemaLocation="\S+ (\S+)"', text)
    return m.group(1)


def resource_dir(work, omit):
    if omit is None:
        return TEST
    d = os.path.join(work, "resources")
    os.makedirs(d)
    for f in os.listdir(TEST):
        if f != omit and os.path.isfile(os.path.join(TEST, f)):
            os.symlink(os.path.join(TEST, f), os.path.join(d, f))
    return d


def run(binary, variant, text, work):
    os.makedirs(work)
    scenario = os.path.join(work, "scenario.xml")
    with open(scenario, "w") as f:
        f.write(text)
    xsd = schema_name(text)
    built = os.path.join(os.path.dirname(binary), "schema", xsd)
    os.symlink(built if os.path.isfile(built) else os.path.join(SCHEMA, xsd), os.path.join(work, xsd))
    resources = resource_dir(work, variant.omit_resource)
    results = []
    for _ in range(variant.runs):
        p = subprocess.run([binary, "--resource-path", resources, "--scenario", scenario] + variant.args,
                           cwd=work, capture_output=True, text=True)
        results.append((p.returncode, p.stdout.replace(work, "WORK"), TRACE.sub("SOURCE", p.stderr.replace(work, "WORK"))))
    files = {}
    for f in sorted(os.listdir(work)):
        if f.startswith(("output", "ctsout", "checkpoint")):
            with open(os.path.join(work, f), "rb") as fh:
                files[f] = fh.read()
    return results, files


def main():
    parser = argparse.ArgumentParser(
        description="Run crafted scenario variants with a base and a candidate openMalaria and compare their "
                    "stdout, stderr, exit codes, outputs and checkpoint files. The source file and line that a "
                    "traced exception reports in stderr are not compared.")
    parser.add_argument("base", help="base openMalaria binary")
    parser.add_argument("candidate", help="candidate openMalaria binary")
    parser.add_argument("groups", nargs="*", default=sorted(GROUPS), help=f"variant groups (default: all of "
                                                                          f"{', '.join(sorted(GROUPS))})")
    parser.add_argument("--keep", metavar="DIR", help="run in DIR and keep it, instead of a temporary directory")
    opts = parser.parse_args()
    for g in opts.groups:
        if g not in GROUPS:
            parser.error(f"unknown group {g}")

    root = opts.keep or tempfile.mkdtemp(prefix="ab_scenarios-")
    differing = 0
    try:
        for g in opts.groups:
            for v in GROUPS[g]:
                text = v.transform(read(v.scenario))
                base = run(os.path.abspath(opts.base), v, text, os.path.join(root, g, v.name, "base"))
                cand = run(os.path.abspath(opts.candidate), v, text, os.path.join(root, g, v.name, "candidate"))
                same = base == cand
                differing += not same
                codes = [r[0] for r in base[0]]
                stderr = base[0][-1][2].strip().splitlines()
                errors = [line for line in stderr if "rror" in line]
                shown = (errors or stderr or [""])[0]
                print(f"{'same' if same else 'DIFF'}  {g}/{v.name}  exit {codes}  files {sorted(base[1])}  "
                      f"stderr: {shown}")
                if not same:
                    print(f"      runs equal: {base[0] == cand[0]}, files equal: {base[1] == cand[1]}")
    finally:
        if not opts.keep:
            shutil.rmtree(root)
    print(f"{differing} of {sum(len(GROUPS[g]) for g in opts.groups)} variants differ")
    return 1 if differing else 0


if __name__ == "__main__":
    sys.exit(main())
