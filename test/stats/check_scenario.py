#!/usr/bin/env python3
"""Check that openMalaria still behaves statistically like a scenario's reference.

One run fails if any of these hold:
  (a) its count of cells outside their reference interval is improbable: p <= RUN_ALPHA,
      where p is the share of the reference's leave-one-out miss counts at least as large
  (b) an aggregate is outside its reference interval
  (c) a fixed cell, one that was the same non-zero value (or NaN) in every replicate, differs

Run 1 uses the scenario's own iseed. If it fails, the retry seeds run, and the check fails
if any of them also fails. With --replicates K, K seeds run up front instead, and the check
fails if that many failing runs is improbable at the expected per-run failure rate.

Exit codes: 0 pass, 1 statistical failure, 2 error (bad input, a crashed run, or a run
whose cells differ from the reference's).
"""

import argparse
import hashlib
import math
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import run_replicates
import stats_io

ROOT = Path(__file__).resolve().parents[2]
RETRY_SEEDS = (2001, 2002, 2003, 2004, 2005)
PER_RUN_RATE = 0.02
REPLICATES_ALPHA = 0.001
TOP_MISSES = 10


class CheckError(Exception):
    pass


@dataclass
class Cell:
    key: tuple
    value: float
    row: dict

    @property
    def z(self):
        sd = self.row["sd"]
        if not sd or math.isnan(sd) or math.isnan(self.value) or self.row["min"] == self.row["max"]:
            return math.inf
        return abs(self.value - self.row["mean"]) / sd


@dataclass
class RunResult:
    seed: str
    cells: int
    misses: list
    p: float
    aggregates: dict
    fixed_cells: int
    fixed_mismatches: list

    @property
    def outside_aggregates(self):
        return [name for name, cell in self.aggregates.items() if stats_io.is_miss(cell.value, cell.row)]

    @property
    def failed(self):
        return self.p <= stats_io.RUN_ALPHA or bool(self.outside_aggregates) or bool(self.fixed_mismatches)


def judge_run(ref, keys, values, seed):
    cells = dict(zip(keys, values, strict=True))
    if len(cells) != len(keys):
        raise CheckError(f"run {seed} has duplicate cells")
    expected = {k for k in ref.rows if k[0] != stats_io.AGGREGATE}
    if cells.keys() != expected:
        missing = sorted(expected - cells.keys())
        extra = sorted(cells.keys() - expected)
        raise CheckError(f"run {seed} has different cells from the reference: {len(missing)} missing "
                         f"{missing[:3]}, {len(extra)} extra {extra[:3]}")
    misses = []
    fixed_cells = 0
    fixed_mismatches = []
    for key, value in cells.items():
        row = ref.rows[key]
        if stats_io.is_miss(value, row):
            misses.append(Cell(key, value, row))
        if stats_io.is_fixed_cell(row):
            fixed_cells += 1
            if not stats_io.same_value(value, row["min"]):
                fixed_mismatches.append(Cell(key, value, row))
    found = stats_io.aggregates(cells)
    aggregates = {}
    for key, row in ref.rows.items():
        if key[0] == stats_io.AGGREGATE:
            if key[3] not in found:
                raise CheckError(f"run {seed} has no {key[3]} aggregate")
            aggregates[key[3]] = Cell(key, found[key[3]], row)
    p = stats_io.run_p_value(len(misses), ref.loo_counts)
    return RunResult(str(seed), len(cells), misses, p, aggregates, fixed_cells, fixed_mismatches)


def judge_dir(ref, directory):
    try:
        keys, values = stats_io.read_run(directory)
    except (stats_io.FormatError, OSError, ValueError) as e:
        raise CheckError(f"can't read run {directory.name}: {e}") from e
    return judge_run(ref, keys, values, directory.name)


def rerun_verdict(first, reruns):
    failing = sum(r.failed for r in reruns)
    if not first.failed:
        return False, "pass"
    if failing:
        return True, f"FAIL: run 1 failed and {failing} of {len(reruns)} reruns failed"
    return False, f"pass after rerun: run 1 failed, none of {len(reruns)} reruns failed"


def binomial_tail(k, n, p):
    def log_term(i):
        return (math.lgamma(n + 1) - math.lgamma(i + 1) - math.lgamma(n - i + 1)
                + i * math.log(p) + (n - i) * math.log1p(-p))
    return min(1.0, math.fsum(math.exp(log_term(i)) for i in range(k, n + 1)))


def replicates_verdict(results):
    failing = sum(r.failed for r in results)
    tail = binomial_tail(failing, len(results), PER_RUN_RATE)
    summary = (f"{failing} of {len(results)} runs failed; P(at least {failing} | per-run rate {PER_RUN_RATE}) "
               f"= {tail:.3g}")
    if tail <= REPLICATES_ALPHA:
        return True, f"FAIL: {summary} <= {REPLICATES_ALPHA}"
    return False, f"pass: {summary}"


def format_key(key):
    file, survey, group, measure = key
    if file == stats_io.SURVEY:
        return f"survey {survey} group {group} measure {measure}"
    if file == stats_io.CTSOUT:
        return f"ctsout {survey} '{measure}'"
    return f"aggregate {measure}"


def format_cell(cell):
    row = cell.row
    return (f"{format_key(cell.key)} = {cell.value:.6g}; reference mean {row['mean']:.6g}, sd {row['sd']:.3g}, "
            f"interval [{row['lo']:.6g}, {row['hi']:.6g}], z {cell.z:.2f}")


def describe(result, details=False):
    lines = [f"run seed {result.seed}: {'FAIL' if result.failed else 'pass'}"]
    mark = " <- fails" if result.p <= stats_io.RUN_ALPHA else ""
    lines.append(f"  misses {len(result.misses)} of {result.cells} cells, p = {result.p:.3g}{mark}")
    for name, cell in result.aggregates.items():
        mark = " <- fails" if name in result.outside_aggregates else ""
        lines.append(f"  aggregate {name} {cell.value:.6g}, interval [{cell.row['lo']:.6g}, "
                     f"{cell.row['hi']:.6g}]{mark}")
    mark = " <- fails" if result.fixed_mismatches else ""
    lines.append(f"  fixed cells: {len(result.fixed_mismatches)} of {result.fixed_cells} differ{mark}")
    if not details:
        return "\n".join(lines)
    lines += [f"    {format_cell(c)}" for c in result.fixed_mismatches[:TOP_MISSES]]
    fixed = {c.key for c in result.fixed_mismatches}
    misses = [c for c in result.misses if c.key not in fixed]
    ranked = sorted((c for c in misses if math.isfinite(c.z)), key=lambda c: -c.z)
    if ranked:
        lines.append(f"  largest misses by |z| ({min(len(ranked), TOP_MISSES)} of {len(ranked)}):")
        lines += [f"    {format_cell(c)}" for c in ranked[:TOP_MISSES]]
    flat = [c for c in misses if not math.isfinite(c.z)]
    if flat:
        lines.append(f"  other misses in cells with sd 0 ({min(len(flat), TOP_MISSES)} of {len(flat)}):")
        lines += [f"    {format_cell(c)}" for c in flat[:TOP_MISSES]]
    return "\n".join(lines)


def parse_seeds(text):
    seeds = set()
    for part in text.split():
        first, _, last = part.partition("-")
        seeds.update(range(int(first), int(last or first) + 1))
    return seeds


def own_seed(scenario_bytes):
    match = run_replicates.ISEED.search(scenario_bytes)
    if match is None:
        raise CheckError("the scenario has no <computationParameters iseed>")
    return int(match[3])


def default_reference(scenario):
    path = Path(scenario)
    stem = path.stem if path.suffix == ".xml" else f"scenario{scenario}"
    return ROOT / "test" / "reference" / f"{stem}{stats_io.REFERENCE_SUFFIX}"


def run_and_judge(ref, job, seeds, jobs):
    failed = run_replicates.run_seeds(job, seeds, jobs, quiet=True)
    if failed:
        raise CheckError(f"openMalaria failed for seeds {failed}")
    return [judge_dir(ref, job.out / str(seed)) for seed in seeds]


def check_outputs(ref, directory, verbose):
    runs = sorted((p for p in directory.iterdir() if re.fullmatch(r"-?\d+", p.name) and p.is_dir()),
                  key=lambda p: int(p.name))
    if not runs:
        raise CheckError(f"no run directories in {directory}")
    results = [judge_dir(ref, run) for run in runs]
    for result in results:
        print(describe(result, verbose or result.failed))
    failing = sum(r.failed for r in results)
    print(f"{failing} of {len(results)} runs failed")
    return 1 if failing else 0


def check(ref, args, work):
    job = run_replicates.prepare(args.build_dir, args.scenario, work)
    if hashlib.sha256(job.scenario_bytes).hexdigest() != ref.meta.get("scenario_sha256"):
        print("warning: the scenario differs from the one the reference was built from", file=sys.stderr)
    first = own_seed(job.scenario_bytes)
    seeds = list(range(RETRY_SEEDS[0], RETRY_SEEDS[0] + args.replicates)) if args.replicates else [first]
    reruns = [] if args.replicates else args.retry_seeds
    overlap = sorted(parse_seeds(ref.meta.get("seeds", "")) & {*seeds, *reruns})
    if overlap:
        raise CheckError(f"seeds {overlap} are reference seeds, so they can't test it")

    results = run_and_judge(ref, job, seeds, args.jobs)
    for result in results:
        print(describe(result, args.verbose or (result.failed and not args.replicates)))
    if args.replicates:
        failed, summary = replicates_verdict(results)
    else:
        rerun_results = []
        if results[0].failed:
            print(f"run 1 failed; rerunning with seeds {' '.join(map(str, reruns))}")
            rerun_results = run_and_judge(ref, job, reruns, args.jobs)
            for result in rerun_results:
                print(describe(result, args.verbose))
        failed, summary = rerun_verdict(results[0], rerun_results)
    print(f"scenario {args.scenario}: {summary}")
    return 1 if failed else 0


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("scenario", help="NAME for test/scenarioNAME.xml, or a path to a scenario file")
    parser.add_argument("--reference", type=Path,
                        help="reference CSV, gzipped if it ends in .gz (default: test/reference/<scenario file stem>.csv.gz)")
    parser.add_argument("--retry-seeds", type=int, nargs="+", default=list(RETRY_SEEDS),
                        help=f"seeds to rerun when run 1 fails (default: {' '.join(map(str, RETRY_SEEDS))})")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--replicates", type=int, metavar="K",
                      help=f"run K seeds from {RETRY_SEEDS[0]} up front and judge the number that fail")
    mode.add_argument("--outputs", type=Path, metavar="DIR",
                      help="judge the run directories DIR/<seed> (e.g. from run_replicates.py) without running")
    parser.add_argument("--work-dir", type=Path,
                        help="keep run outputs here and reuse those already present (default: a temporary dir)")
    parser.add_argument("--build-dir", type=Path, default=Path(os.environ.get("OM_BUILD_DIR", "build")),
                        help="CMake build dir, relative to the repo root unless absolute "
                             "(default: $OM_BUILD_DIR or build)")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count(), help="parallel runs (default: all cores)")
    parser.add_argument("-v", "--verbose", action="store_true", help="list the largest misses of passing runs too")
    args = parser.parse_args(argv)
    if args.replicates is not None and args.replicates < 1:
        parser.error("--replicates must be at least 1")
    if args.jobs < 1:
        parser.error("-j must be at least 1")
    if not args.build_dir.is_absolute():
        args.build_dir = ROOT / args.build_dir
    args.reference = args.reference or default_reference(args.scenario)
    return args


def main(argv):
    args = parse_args(argv)
    try:
        ref = stats_io.read_reference(args.reference)
        print(f"reference {args.reference.name}: N = {ref.meta.get('replicates')}, seeds {ref.meta.get('seeds')}, "
              f"commit {str(ref.meta.get('commit'))[:8]}")
        if args.outputs:
            return check_outputs(ref, args.outputs, args.verbose)
        if args.work_dir:
            return check(ref, args, args.work_dir)
        with tempfile.TemporaryDirectory(prefix="om-check-") as tmp:
            return check(ref, args, Path(tmp))
    except (CheckError, run_replicates.ReplicateError, stats_io.FormatError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
