#!/usr/bin/env python3
"""Validate the statistical check: false-failure rates and power, from directories of runs.

Each DIR holds one run_replicates.py output dir per scenario (DIR/<NAME>/<seed>/output.txt).
Every run is judged against test/reference/scenario<NAME>.csv.gz exactly as check_scenario.py
judges it. The default rerun rule fails with probability r * (1 - (1 - r)^R), for R retry
seeds, where r is the share of failing runs. The group test of --replicates is applied to
groups of K runs (K from the reference): for fresh runs, disjoint groups in seed order; for
runs of a perturbed model, --resamples random groups. For fresh runs of unchanged code these
give false-failure rates; for runs of a perturbed model, the power. The minimum detectable
effect is the smallest perturbation whose power reaches --power-target.

Writes a Markdown report to stdout.
"""

import argparse
import math
import random
import re
import statistics
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


@dataclass
class Judged:
    results: list

    @property
    def rates(self):
        results = self.results
        return Rates(len(results), sum(r.failed for r in results), sum(r.p <= stats_io.RUN_ALPHA for r in results),
                     sum(bool(r.outside_aggregates) for r in results), sum(bool(r.fixed_mismatches) for r in results))


def judge(ref, directory):
    runs = sorted((p for p in directory.iterdir() if re.fullmatch(r"-?\d+", p.name) and (p / "output.txt").is_file()),
                  key=lambda p: int(p.name))
    return Judged([check_scenario.judge_dir(ref, run) for run in runs])


RETRIES = len(check_scenario.RETRY_SEEDS)


def rerun_fail(r):
    return r * (1 - (1 - r) ** RETRIES)


def largest_z(ref, group):
    scores = check_scenario.group_scores(ref, group)
    return abs(scores[0].z) if scores else 0.0


def group_fails(ref, group, threshold):
    return largest_z(ref, group) > threshold or any(r.fixed_mismatches for r in group)


def group_quantiles(ref):
    items = ref.meta["group_quantiles"].split()
    return {float(a): float(q) for a, q in zip(items[0::2], items[1::2], strict=True)}


def disjoint_groups(results, k):
    return [results[i:i + k] for i in range(0, len(results) - k + 1, k)]


def resampled_power(ref, results, k, threshold, resamples, rng):
    if len(results) < k:
        return math.nan
    return sum(group_fails(ref, rng.sample(results, k), threshold) for _ in range(resamples)) / resamples


def largest_shift(ref, results):
    shifts = [(statistics.fmean(r.group[key] for r in results) - mean) / sd
              for key, (mean, sd) in ref.group_stats.items()]
    return max(shifts, key=abs, default=math.nan)


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
    parser.add_argument("--resamples", type=int, default=1000,
                        help="random groups per perturbation for the group test's power (default 1000)")
    parser.add_argument("--seed", type=int, default=1, help="seed for drawing those groups (default 1)")
    parser.add_argument("--power-target", type=float, default=0.8, help="power for the MDE (default 0.8)")
    return parser.parse_args(argv)


def scenario_names(fresh):
    return sorted(p.name for p in fresh.iterdir() if p.is_dir())


def main(argv):
    args = parse_args(argv)
    rng = random.Random(args.seed)
    changes = args.effect + args.other
    print(f"Per-run failure rate r; rerun rule P(fail) = r(1 - (1 - r)^{RETRIES}). Group test: the largest |z| "
          "of the K-run means against the reference's leave-K-out threshold, or a fixed cell differing in any "
          "run. All rates in %.\n")
    print("## False failures (unchanged code)\n")
    print("Groups: disjoint groups of K fresh runs in seed order. Largest |z|: over those groups.\n")
    print("| scenario | runs | r | (a) misses | (b) aggregates | (c) fixed | rerun rule | reference LOO r | "
          "fresh + LOO r | rerun rule, fresh + LOO | K | groups | group fails | largest \\|z\\| | threshold |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    refs = {}
    fresh = {}
    calibration = {}
    power = {}
    group_totals = [0, 0]
    for name in scenario_names(args.fresh):
        ref = refs[name] = stats_io.read_reference(
            ROOT / "test" / "reference" / f"scenario{name}{stats_io.REFERENCE_SUFFIX}")
        judged = fresh[name] = judge(ref, args.fresh / name)
        f = judged.rates
        loo = ref.meta.get("loo_run_failures", "").split(" (")[0]
        loo_failed, _, loo_runs = loo.partition(" of ")
        loo_rate = int(loo_failed) / int(loo_runs) if loo_runs else math.nan
        pooled = (f.failed + int(loo_failed)) / (f.runs + int(loo_runs)) if loo_runs else math.nan
        k, threshold = check_scenario.group_threshold(ref)
        groups = disjoint_groups(judged.results, k)
        largest = [largest_z(ref, g) for g in groups]
        failing = sum(group_fails(ref, g, threshold) for g in groups)
        group_totals[0] += len(groups)
        group_totals[1] += failing
        for alpha, q in group_quantiles(ref).items():
            seen, exceeding = calibration.get(alpha, (0, 0))
            calibration[alpha] = (seen + len(groups), exceeding + sum(z > q for z in largest))
        print(f"| {name} | {f.runs} | {percent(f.rate)} | {f.misses} | {f.aggregates} | {f.fixed} | "
              f"{percent(rerun_fail(f.rate), 2)} | {percent(loo_rate)} | {percent(pooled)} | "
              f"{percent(rerun_fail(pooled), 2)} | {k} | {len(groups)} | {failing} | "
              f"{max(largest, default=math.nan):.2f} | {threshold:g} |")
        power[name] = {label: judge(ref, d / name) for label, d in changes if (d / name).is_dir()}

    rates = [j.rates for j in fresh.values()]
    total = Rates(*(sum(getattr(f, a) for f in rates) for a in ("runs", "failed", "misses", "aggregates", "fixed")))
    suite = 1 - math.prod(1 - rerun_fail(f.rate) for f in rates)
    print(f"| **all** | {total.runs} | {percent(total.rate)} | {total.misses} | {total.aggregates} | {total.fixed} | "
          f"{percent(rerun_fail(total.rate), 2)} | | | | | {group_totals[0]} | {group_totals[1]} | | |\n")
    worst = max(fresh, key=lambda n: fresh[n].rates.rate)
    print(f"Largest per-scenario rerun-rule false failure: {worst}, "
          f"{percent(rerun_fail(fresh[worst].rates.rate), 2)}%. Chance that at least one of the "
          f"{len(fresh)} stat tests fails falsely on one correct build: {percent(suite)}%.\n")
    print("Calibration of the group test: fresh groups whose largest |z| exceeds the reference's leave-K-out "
          "quantile at each level, pooled over scenarios.\n")
    print("| alpha | groups | expected | exceeding |")
    print("|---|---|---|---|")
    for alpha, (seen, exceeding) in sorted(calibration.items(), reverse=True):
        print(f"| {alpha:g} | {seen} | {alpha * seen:.2f} | {exceeding} |")
    print()

    if not changes:
        return 0
    print("## Power\n")
    print(f"Each cell: per-run failure rate q / power of the rerun rule / power of the group test, from "
          f"{args.resamples} random groups of K runs. MDE: the smallest effect with power ≥ "
          f"{percent(args.power_target, 0)}%. Shift: the mean over all runs of the largest effect, minus the "
          "reference mean, in single-run SDs, for the group quantity that moved most.\n")
    labels = [label for label, _ in changes]
    effects = [label for label, _ in args.effect]
    print("| scenario | runs | " + " | ".join(labels) + " | MDE rerun | MDE group | shift at "
          f"{effects[-1] if effects else '–'} |")
    print("|---|---|" + "---|" * len(labels) + "---|---|---|")
    for name in scenario_names(args.fresh):
        ref = refs[name]
        k, threshold = check_scenario.group_threshold(ref)
        cells = []
        mde = {"rerun": f">{effects[-1]}" if effects else "–", "group": f">{effects[-1]}" if effects else "–"}
        runs = max((len(j.results) for j in power[name].values()), default=0)
        for label in labels:
            judged = power[name].get(label)
            if judged is None or not judged.results:
                cells.append("–")
                continue
            q = judged.rates.rate
            rerun = rerun_fail(q)
            group = resampled_power(ref, judged.results, k, threshold, args.resamples, rng)
            cells.append(f"{percent(q, 0)} / {percent(rerun, 0)} / {percent(group, 0)}")
            if label in effects:
                if rerun >= args.power_target and mde["rerun"].startswith(">"):
                    mde["rerun"] = label
                if group >= args.power_target and mde["group"].startswith(">"):
                    mde["group"] = label
        last = power[name].get(effects[-1]) if effects else None
        shift = f"{largest_shift(ref, last.results):+.1f}" if last and last.results else "–"
        print(f"| {name} | {runs} | " + " | ".join(cells) + f" | {mde['rerun']} | {mde['group']} | {shift} |")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
