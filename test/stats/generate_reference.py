#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2,<3"]
# ///
"""Build a statistical reference from a directory of replicate runs made by run_replicates.py.

For every cell of output.txt and ctsout.txt it writes the mean, the sample SD, the min and
max, and the empirical 2.5% and 97.5% quantiles (Hyndman-Fan type 7, numpy's default): a
95% prediction interval for one new run. A single run is expected to miss about 5% of the
cells, so the file also records, for every replicate, the miss count it gets against the
interval built from the other N - 1 (leave one out); check_scenario.py turns a run's miss
count into a p-value against them. The key aggregates get intervals widened by Bonferroni
over the aggregates present, so that together they cover 99%. Their quantiles are Hyndman-Fan
type 6, whose expected coverage of one new run is the nominal one; type 7 is too narrow in
the far tails these intervals use.

Needs numpy; the uv shebang provides it, or run it with any python that has numpy.
"""

import argparse
import datetime
import hashlib
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import stats_io

ROOT = Path(__file__).resolve().parents[2]
CELL_INTERVAL = (0.025, 0.975)
AGGREGATE_ALPHA = 0.01
LINEAR = "linear"
WEIBULL = "weibull"
QUANTILE_METHOD_NAMES = {LINEAR: "hyndman-fan-7", WEIBULL: "hyndman-fan-6"}
CELL_QUANTILE = LINEAR
AGGREGATE_QUANTILE = WEIBULL
NOT_FIXED = -2
CONSTANT = -1
MIN_REPLICATES = 10
CHUNK_CELLS = 1 << 14
SCHEMA_VERSION = re.compile(rb"""\bschemaVersion\s*=\s*["'](\d+)["']""")


class ReferenceError(Exception):
    pass


def quantile_position(n, p, method):
    h = (n - 1) * p if method == LINEAR else n * p + p - 1
    return min(max(h, 0), n - 1)


def interpolate(element, n, p, method):
    h = quantile_position(n, p, method)
    j = math.floor(h)
    if j + 1 >= n:
        return element(n - 1)
    below = element(j)
    return below + (h - j) * (element(j + 1) - below)


def quantile_sorted(s, p, method=LINEAR):
    return interpolate(lambda t: s[t], s.shape[0], p, method)


def loo_quantile(s, rank, p, method=LINEAR):
    return interpolate(lambda t: np.where(rank > t, s[t], s[t + 1]), s.shape[0] - 1, p, method)


def ranks(x):
    order = np.argsort(x, axis=0, kind="stable")
    rank = np.empty_like(order)
    np.put_along_axis(rank, order, np.arange(x.shape[0])[:, None], axis=0)
    return rank


def check_nan(x, keys):
    nan = np.isnan(x)
    mixed = np.flatnonzero(nan.any(axis=0) & ~nan.all(axis=0))
    if mixed.size:
        shown = ", ".join(str(keys[i]) for i in mixed[:5])
        raise ReferenceError(f"{mixed.size} cells are NaN in some replicates but not all, e.g. {shown}")


@dataclass
class CellStatistics:
    stats: dict
    misses: np.ndarray
    outlier: np.ndarray
    common: np.ndarray


def block_misses(block, s, rank, p_lo, p_hi, method):
    lo = loo_quantile(s, rank, p_lo, method)
    hi = loo_quantile(s, rank, p_hi, method)
    return ((block < lo) | (block > hi)).sum(axis=1)


def block_outliers(s, rank):
    n = s.shape[0]
    constant = (s[0] == s[n - 1]) | np.isnan(s[0])
    low = ~constant & (s[1] == s[n - 1])
    high = ~constant & (s[0] == s[n - 2])
    outlier = np.full(s.shape[1], NOT_FIXED)
    outlier[constant] = CONSTANT
    outlier[low] = np.argmin(rank, axis=0)[low]
    outlier[high] = np.argmax(rank, axis=0)[high]
    return outlier, np.where(low, s[1], s[0])


def cell_statistics(x, p_lo, p_hi, chunk=CHUNK_CELLS, method=LINEAR):
    n, cells = x.shape
    result = CellStatistics({c: np.empty(cells) for c in stats_io.STAT_COLUMNS},
                            np.zeros(n, dtype=np.int64), np.empty(cells, dtype=np.int64), np.empty(cells))
    for start in range(0, cells, chunk):
        block = x[:, start:start + chunk]
        part = slice(start, start + block.shape[1])
        s = np.sort(block, axis=0)
        rank = ranks(block)
        with np.errstate(invalid="ignore"):
            result.stats["sd"][part] = np.where(s[0] == s[-1], 0.0, block.std(axis=0, ddof=1))
        result.stats["mean"][part] = block.mean(axis=0)
        result.stats["min"][part] = s[0]
        result.stats["max"][part] = s[-1]
        result.stats["lo"][part] = quantile_sorted(s, p_lo, method)
        result.stats["hi"][part] = quantile_sorted(s, p_hi, method)
        result.misses += block_misses(block, s, rank, p_lo, p_hi, method)
        result.outlier[part], result.common[part] = block_outliers(s, rank)
    return result


def fixed_cell_groups(keys):
    index = {}
    return np.array([index.setdefault(stats_io.fixed_cell_group(k), len(index)) for k in keys], dtype=np.int64)


def fixed_failures(group, outlier, common, n):
    groups = int(group.max()) + 1 if group.size else 0
    fixable = np.ones(groups, dtype=bool)
    fixable[group[outlier == NOT_FIXED]] = False
    deviant = outlier >= 0
    first = np.full(groups, n)
    last = np.full(groups, -1)
    np.minimum.at(first, group[deviant], outlier[deviant])
    np.maximum.at(last, group[deviant], outlier[deviant])
    single = fixable & (first == last)
    failing_cells = deviant & single[group] & (common != 0)
    failing = np.zeros(n, dtype=bool)
    failing[outlier[failing_cells]] = True
    return failing, int(failing_cells.sum())


def loo_p_values(misses):
    s = np.sort(misses)
    at_least = s.size - np.searchsorted(s, misses, side="left")
    return at_least / s.size


def max_passing_misses(misses):
    counts = misses.tolist()
    if stats_io.run_p_value(int(misses.max()) + 1, counts) > stats_io.RUN_ALPHA:
        return None
    return max(m for m in range(int(misses.max()) + 1) if stats_io.run_p_value(m, counts) > stats_io.RUN_ALPHA)


def seed_dirs(directory):
    seeds = sorted(int(p.name) for p in directory.iterdir()
                   if re.fullmatch(r"-?\d+", p.name) and (p / "output.txt").is_file())
    return seeds


def load_replicates(directory, seeds):
    keys = index = x = None
    for row, seed in enumerate(seeds):
        run_keys, values = stats_io.read_run(directory / str(seed))
        if keys is None:
            keys = run_keys
            if len(set(keys)) != len(keys):
                raise ReferenceError(f"seed {seed} has duplicate cells")
            x = np.empty((len(seeds), len(keys)))
        elif run_keys != keys:
            if index is None:
                index = {k: i for i, k in enumerate(keys)}
            if len(run_keys) != len(keys) or set(run_keys) != index.keys():
                differing = set(keys) ^ set(run_keys)
                raise ReferenceError(
                    f"seed {seed} has different cells from seed {seeds[0]}, e.g. {sorted(differing)[:3]}")
            reordered = [0.0] * len(keys)
            for k, v in zip(run_keys, values, strict=True):
                reordered[index[k]] = v
            values = reordered
        x[row] = values
    return keys, x


def seed_ranges(seeds):
    ranges = []
    start = prev = seeds[0]
    for seed in seeds[1:] + [None]:
        if seed is not None and seed == prev + 1:
            prev = seed
            continue
        ranges.append(str(start) if start == prev else f"{start}-{prev}")
        if seed is not None:
            start = prev = seed
    return " ".join(ranges)


def schema_version(run_meta):
    scenario = Path(run_meta.get("scenario", ""))
    if not scenario.is_file():
        return "unknown"
    text = scenario.read_bytes()
    if hashlib.sha256(text).hexdigest() != run_meta.get("scenario_sha256"):
        return "unknown"
    match = SCHEMA_VERSION.search(text)
    return match[1].decode() if match else "unknown"


def build(directory, allow_dirty=False):
    meta_path = directory / "meta.json"
    if not meta_path.is_file():
        raise ReferenceError(f"{meta_path} not found; is {directory} a run_replicates.py output dir?")
    run_meta = json.loads(meta_path.read_text())
    if run_meta.get("dirty") and not allow_dirty:
        raise ReferenceError("the replicates were built from a dirty tree; pass --allow-dirty to use them anyway")
    seeds = seed_dirs(directory)
    if len(seeds) < MIN_REPLICATES:
        raise ReferenceError(f"found {len(seeds)} replicates in {directory}, need at least {MIN_REPLICATES}")

    keys, x = load_replicates(directory, seeds)
    check_nan(x, keys)
    cells = cell_statistics(x, *CELL_INTERVAL)
    zero_width = int(np.sum((cells.stats["min"] == cells.stats["max"]) | np.isnan(cells.stats["min"])))

    inputs = [i for i, k in enumerate(keys) if stats_io.is_aggregate_input(k)]
    agg_values = [stats_io.aggregates({keys[i]: v for i, v in zip(inputs, row, strict=True)})
                  for row in x[:, inputs].tolist()]
    agg_names = [n for n in stats_io.AGGREGATES if all(n in a for a in agg_values)]
    tail = AGGREGATE_ALPHA / 2 / max(len(agg_names), 1)
    agg = np.array([[a[n] for n in agg_names] for a in agg_values], dtype=np.float64).reshape(len(seeds), -1)
    agg_cells = cell_statistics(agg, tail, 1 - tail, method=AGGREGATE_QUANTILE)
    agg_stats = agg_cells.stats

    rows = [(k, {c: cells.stats[c][i] for c in stats_io.STAT_COLUMNS}) for i, k in enumerate(keys)]
    rows += [((stats_io.AGGREGATE, "", "", n), {c: agg_stats[c][i] for c in stats_io.STAT_COLUMNS})
             for i, n in enumerate(agg_names)]

    n_survey = sum(1 for k in keys if k[0] == stats_io.SURVEY)
    misses = cells.misses
    sorted_misses = np.sort(misses)
    miss_failing = loo_p_values(misses) <= stats_io.RUN_ALPHA
    agg_failing = agg_cells.misses > 0
    fixed_failing, fixed_failure_cells = fixed_failures(fixed_cell_groups(keys), cells.outlier, cells.common,
                                                        len(seeds))
    run_failing = miss_failing | agg_failing | fixed_failing
    limit = max_passing_misses(misses)
    if limit is None:
        print(f"warning: with {len(seeds)} replicates no miss count is improbable at "
              f"{stats_io.RUN_ALPHA}; use at least {math.ceil(1 / stats_io.RUN_ALPHA)}", file=sys.stderr)
    meta = {
        "format_version": stats_io.FORMAT_VERSION,
        "scenario": Path(run_meta.get("scenario", "")).stem,
        "scenario_sha256": run_meta.get("scenario_sha256"),
        "schema": run_meta.get("schema"),
        "schema_version": schema_version(run_meta),
        "commit": run_meta.get("commit"),
        "dirty": str(bool(run_meta.get("dirty"))).lower(),
        "binary_sha256": run_meta.get("binary_sha256"),
        "output_format": run_meta.get("output_format"),
        "replicates": len(seeds),
        "seeds": seed_ranges(seeds),
        "replicates_created": run_meta.get("created"),
        "generated": datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds"),
        "quantile_method": QUANTILE_METHOD_NAMES[CELL_QUANTILE],
        "aggregate_quantile_method": QUANTILE_METHOD_NAMES[AGGREGATE_QUANTILE],
        "cells": f"{len(keys)} (survey {n_survey}, ctsout {len(keys) - n_survey})",
        "zero_width_cells": zero_width,
        "cell_interval": f"{CELL_INTERVAL[0]} {CELL_INTERVAL[1]}",
        "loo_misses": (f"min {int(sorted_misses[0])} median {np.median(sorted_misses):g} "
                       f"mean {misses.mean():.2f} max {int(sorted_misses[-1])}"),
        "run_alpha": stats_io.RUN_ALPHA,
        "max_passing_misses": "unlimited" if limit is None else limit,
        "aggregates": " ".join(agg_names) or "none",
        "aggregate_interval": f"{format(tail, '.6g')} {format(1 - tail, '.6g')}",
        "loo_run_failures": (f"{int(run_failing.sum())} of {len(seeds)} (misses {int(miss_failing.sum())}, "
                             f"aggregates {int(agg_failing.sum())}, fixed cells {int(fixed_failing.sum())} "
                             f"in {fixed_failure_cells} cells)"),
        stats_io.LOO_COUNTS: " ".join(str(int(m)) for m in misses),
    }
    return meta, rows


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("replicates", type=Path, help="output dir of run_replicates.py")
    parser.add_argument("-o", "--out", type=Path,
                        help="reference CSV to write, gzipped if it ends in .gz (default: test/reference/<scenario file stem>.csv.gz)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="accept replicates built from a source tree with uncommitted changes")
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    try:
        meta, rows = build(args.replicates, args.allow_dirty)
        out = args.out or ROOT / "test" / "reference" / f"{meta['scenario']}{stats_io.REFERENCE_SUFFIX}"
        out.parent.mkdir(parents=True, exist_ok=True)
        stats_io.write_reference(out, meta, rows)
    except (ReferenceError, stats_io.FormatError, OSError, json.JSONDecodeError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    print(f"wrote {out}: {meta['replicates']} replicates, {meta['cells']} cells, "
          f"leave-one-out run failures {meta['loo_run_failures']}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
