import argparse
import concurrent.futures
import os
import re
import statistics
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST = os.path.join(REPO, "test")
VARIANTS = ("none", "copula_corr1", "copula_corr0", "plain")
DEPLOYMENT_START = '<deployment name="kill_repell_deploy">'
FIRST_SURVEY_AFTER_DEPLOYMENT = 367
DEPLOYMENT_ROUNDS = 5
MEASURES = {"nInfect": 1, "nUncomp": 14, "simulatedEIR": 36, "nMassGVI": 56}


def scenario_text(name, variant, seed):
    s = open(os.path.join(TEST, f"scenario{name}.xml")).read()
    if variant == "none":
        i = s.index(DEPLOYMENT_START)
        s = s[:i] + s[s.index("</deployment>", i) + len("</deployment>"):]
    elif variant == "copula_corr0":
        s = s.replace('coverageCorr="1.0"', 'coverageCorr="0.0"')
    elif variant == "plain":
        s = s.replace(' coverageCorr="1.0" coverageVar="0.15"', "")
    return re.sub(r'iseed\s*=\s*"\d+"', f'iseed="{seed}"', s)


def run(binary, out, name, variant, seed):
    d = os.path.join(out, name, variant, str(seed))
    os.makedirs(d)
    with open(os.path.join(d, "scenario.xml"), "w") as f:
        f.write(scenario_text(name, variant, seed))
    os.symlink(os.path.join(os.path.dirname(binary), "schema", "scenario_current.xsd"),
               os.path.join(d, "scenario_current.xsd"))
    p = subprocess.run([binary, "--resource-path", TEST, "--scenario", os.path.join(d, "scenario.xml")], cwd=d,
                       capture_output=True)
    if p.returncode != 0:
        raise RuntimeError(f"{name} {variant} seed {seed}: exit {p.returncode}")
    with open(os.path.join(d, "output.txt")) as f:
        return [(int(a), int(b), int(c), float(v)) for a, b, c, v in (line.split("\t") for line in f)]


def summarise(rows):
    after = [r for r in rows if r[0] >= FIRST_SURVEY_AFTER_DEPLOYMENT]
    eir = [r[3] for r in after if r[2] == MEASURES["simulatedEIR"]]
    return {
        "covered per round": sum(r[3] for r in after if r[2] == MEASURES["nMassGVI"]) / DEPLOYMENT_ROUNDS,
        "simulated EIR": sum(eir) / len(eir),
        "nInfect": sum(r[3] for r in after if r[2] == MEASURES["nInfect"]),
        "nUncomp": sum(r[3] for r in after if r[2] == MEASURES["nUncomp"]),
    }


def mean_se(values):
    return f"{statistics.mean(values):.4g} ± {statistics.stdev(values) / len(values) ** 0.5:.2g}"


def main():
    parser = argparse.ArgumentParser(
        description="Run a deployed Gaussian copula scenario (default: GaussianCopulaGammaDeployed) over several "
                    "seeds in four variants: no deployment, the copula with correlation 1 (the scenario as is), "
                    "the copula with correlation 0, and plain coverage. Prints the coverage per round and the "
                    "transmission after the first deployment, and checks that every variant's output before it "
                    "equals the no-deployment output (F73).")
    parser.add_argument("binary", help="openMalaria binary")
    parser.add_argument("scenarios", nargs="*", default=["GaussianCopulaGammaDeployed"],
                        help="scenario names in test/, without 'scenario' and '.xml'")
    parser.add_argument("-n", type=int, default=20, help="seeds 2001..2000+N (default 20)")
    parser.add_argument("-j", type=int, default=4, help="parallel runs (default 4)")
    opts = parser.parse_args()
    binary = os.path.abspath(opts.binary)
    out = tempfile.mkdtemp(prefix="copula_check-")
    seeds = range(2001, 2001 + opts.n)
    ok = True
    for name in opts.scenarios:
        jobs = [(v, s) for v in VARIANTS for s in seeds]
        with concurrent.futures.ThreadPoolExecutor(opts.j) as ex:
            rows = dict(zip(jobs, ex.map(lambda job: run(binary, out, name, *job), jobs)))
        before_equal = all(
            [r for r in rows[(v, s)] if r[0] < FIRST_SURVEY_AFTER_DEPLOYMENT]
            == [r for r in rows[("none", s)] if r[0] < FIRST_SURVEY_AFTER_DEPLOYMENT]
            for v in VARIANTS for s in seeds)
        ok = ok and before_equal
        print(f"{name}: {len(seeds)} seeds; output before the first deployment equals no deployment: {before_equal}")
        sums = {v: [summarise(rows[(v, s)]) for s in seeds] for v in VARIANTS}
        keys = list(sums["none"][0])
        print(f"  {'variant':14}" + "".join(f"{k:>22}" for k in keys))
        for v in VARIANTS:
            print(f"  {v:14}" + "".join(f"{mean_se([x[k] for x in sums[v]]):>22}" for k in keys))
        for v in VARIANTS[1:]:
            changes = []
            for k in keys[1:]:
                base = statistics.mean(x[k] for x in sums["none"])
                changes.append(f"{k} {(statistics.mean(x[k] for x in sums[v]) - base) / base * 100:+.1f}%")
            print(f"  {v} against none: " + ", ".join(changes))
    print(f"runs kept in {out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
