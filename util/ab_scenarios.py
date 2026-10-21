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


V = "scenarioVivax.xml"
GROUPS = {
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
        results.append((p.returncode, p.stdout.replace(work, "WORK"), p.stderr.replace(work, "WORK")))
    files = {}
    for f in sorted(os.listdir(work)):
        if f.startswith(("output", "ctsout", "checkpoint")):
            with open(os.path.join(work, f), "rb") as fh:
                files[f] = fh.read()
    return results, files


def main():
    parser = argparse.ArgumentParser(
        description="Run crafted scenario variants with a base and a candidate openMalaria and compare their "
                    "stdout, stderr, exit codes, outputs and checkpoint files.")
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
