#!/usr/bin/env python3
"""Validate the statistical check: false-failure rates and power, from directories of runs.

Each DIR holds one run_replicates.py output dir per scenario (DIR/<NAME>/<seed>/output.txt).
Every run is judged against test/reference/scenario<NAME>.csv.gz exactly as check_scenario.py
judges it. From the share r of failing runs:
- the default rerun rule fails with probability r * (1 - (1 - r)^R), for R retry seeds
- --replicates K fails with probability P(Binomial(K, r) >= k), where k is the smallest count
  check_scenario.py calls improbable
For fresh runs of unchanged code these are false-failure rates; for runs of a perturbed model
they are the power. The minimum detectable effect is the smallest perturbation whose power
reaches --power-target.

Writes a Markdown report to stdout.
"""

import argparse
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import check_scenario
import stats_io

ROOT = Path(__file__).resolve().parents[2]


@dataclass
class Rates:
    runs: int
    failed: int
    misses: int
    aggregates: int
    fixed: int

    @property
    def rate(self):
        return self.failed / self.runs if self.runs else math.nan


def judge(ref, directory):
    runs = [p for p in directory.iterdir() if re.fullmatch(r"-?\d+", p.name) and (p / "output.txt").is_file()]
    results = [check_scenario.judge_dir(ref, run) for run in runs]
    return Rates(len(results), sum(r.failed for r in results), sum(r.p <= stats_io.RUN_ALPHA for r in results),
                 sum(bool(r.outside_aggregates) for r in results), sum(bool(r.fixed_mismatches) for r in results))


RETRIES = len(check_scenario.RETRY_SEEDS)


def rerun_fail(r):
    return r * (1 - (1 - r) ** RETRIES)


def critical_count(k):
    return next(c for c in range(k + 1)
                if check_scenario.binomial_tail(c, k, check_scenario.PER_RUN_RATE) <= check_scenario.REPLICATES_ALPHA)


def replicates_fail(r, k):
    if r <= 0 or r >= 1:
        return float(r >= 1)
    return check_scenario.binomial_tail(critical_count(k), k, r)


def percent(x, digits=1):
    return "–" if math.isnan(x) else f"{100 * x:.{digits}f}"


def parse_label(text):
    label, sep, path = text.partition("=")
    if not sep:
        raise argparse.ArgumentTypeError(f"expected LABEL=DIR, got {text!r}")
    return label, Path(path)


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fresh", type=Path, required=True, help="runs of unchanged code at fresh seeds")
    parser.add_argument("--effect", type=parse_label, action="append", default=[], metavar="LABEL=DIR",
                        help="runs of a perturbed model, in increasing size; repeat for each perturbation")
    parser.add_argument("--other", type=parse_label, action="append", default=[], metavar="LABEL=DIR",
                        help="runs of another change, reported but not used for the minimum detectable effect")
    parser.add_argument("-k", "--replicates", type=int, default=20, help="K for the --replicates mode (default 20)")
    parser.add_argument("--power-target", type=float, default=0.8, help="power for the MDE (default 0.8)")
    return parser.parse_args(argv)


def scenario_names(fresh):
    return sorted(p.name for p in fresh.iterdir() if p.is_dir())


def main(argv):
    args = parse_args(argv)
    k = args.replicates
    changes = args.effect + args.other
    print(f"Per-run failure rate r; rerun rule P(fail) = r(1 - (1 - r)^{len(check_scenario.RETRY_SEEDS)}); "
          f"--replicates {k} fails at {critical_count(k)} or more failing runs. All values in %.\n")
    print("## False failures (unchanged code)\n")
    print("| scenario | runs | r | (a) misses | (b) aggregates | (c) fixed | rerun rule | "
          f"--replicates {k} | reference LOO r | fresh + LOO r | rerun rule, fresh + LOO |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    fresh_rates = {}
    power = {}
    for name in scenario_names(args.fresh):
        ref = stats_io.read_reference(ROOT / "test" / "reference" / f"scenario{name}{stats_io.REFERENCE_SUFFIX}")
        f = fresh_rates[name] = judge(ref, args.fresh / name)
        loo = ref.meta.get("loo_run_failures", "").split(" (")[0]
        loo_failed, _, loo_runs = loo.partition(" of ")
        loo_rate = int(loo_failed) / int(loo_runs) if loo_runs else math.nan
        pooled = (f.failed + int(loo_failed)) / (f.runs + int(loo_runs)) if loo_runs else math.nan
        print(f"| {name} | {f.runs} | {percent(f.rate)} | {f.misses} | {f.aggregates} | {f.fixed} | "
              f"{percent(rerun_fail(f.rate), 2)} | {percent(replicates_fail(f.rate, k), 2)} | {percent(loo_rate)} | "
              f"{percent(pooled)} | {percent(rerun_fail(pooled), 2)} |")
        power[name] = {label: judge(ref, d / name) for label, d in changes if (d / name).is_dir()}

    total = Rates(*(sum(getattr(f, a) for f in fresh_rates.values())
                    for a in ("runs", "failed", "misses", "aggregates", "fixed")))
    suite = 1 - math.prod(1 - rerun_fail(f.rate) for f in fresh_rates.values())
    print(f"| **all** | {total.runs} | {percent(total.rate)} | {total.misses} | {total.aggregates} | {total.fixed} | "
          f"{percent(rerun_fail(total.rate), 2)} | {percent(replicates_fail(total.rate, k), 2)} | | | |\n")
    worst = max(fresh_rates, key=lambda n: fresh_rates[n].rate)
    print(f"Largest per-scenario rerun-rule false failure: {worst}, "
          f"{percent(rerun_fail(fresh_rates[worst].rate), 2)}%. Chance that at least one of the "
          f"{len(fresh_rates)} stat tests fails falsely on one correct build: {percent(suite)}%.\n")

    if not changes:
        return 0
    print("## Power\n")
    print("Each cell: per-run failure rate q / power of the rerun rule / power of "
          f"--replicates {k}. MDE: the smallest effect with power ≥ {percent(args.power_target, 0)}%.\n")
    labels = [label for label, _ in changes]
    print("| scenario | runs | " + " | ".join(labels) + f" | MDE rerun | MDE --replicates {k} |")
    print("|---|---|" + "---|" * len(labels) + "---|---|")
    effects = [label for label, _ in args.effect]
    for name in scenario_names(args.fresh):
        cells = []
        mde = {"rerun": f">{effects[-1]}" if effects else "–", "replicates": f">{effects[-1]}" if effects else "–"}
        runs = max((p.runs for p in power[name].values()), default=0)
        for label in labels:
            p = power[name].get(label)
            if p is None or not p.runs:
                cells.append("–")
                continue
            rerun, reps = rerun_fail(p.rate), replicates_fail(p.rate, k)
            cells.append(f"{percent(p.rate, 0)} / {percent(rerun, 0)} / {percent(reps, 0)}")
            if label in effects:
                if rerun >= args.power_target and mde["rerun"].startswith(">"):
                    mde["rerun"] = label
                if reps >= args.power_target and mde["replicates"].startswith(">"):
                    mde["replicates"] = label
        print(f"| {name} | {runs} | " + " | ".join(cells) + f" | {mde['rerun']} | {mde['replicates']} |")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
