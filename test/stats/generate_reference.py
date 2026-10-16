#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2,<3"]
# ///
"""Build a statistical reference from a directory of replicate runs made by run_replicates.py.

For every cell of output.txt and ctsout.txt it writes the mean, the sample SD, the min and
max, and the empirical 2.5% and 97.5% quantiles (Hyndman-Fan type 7, numpy's default): a
95% prediction interval for one new run. A single run is expected to miss about 5% of the
cells, so the file also records the miss-count threshold: the 99.9th percentile of the miss
counts each replicate gets against the interval built from the other N - 1 (leave one out).
The key aggregates get intervals widened by Bonferroni over the aggregates present.

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
AGGREGATE_ALPHA = 0.05
LOO_MISS_PERCENTILE = 0.999
MIN_REPLICATES = 10
CHUNK_CELLS = 1 << 14
SCHEMA_VERSION = re.compile(rb"""\bschemaVersion\s*=\s*["'](\d+)["']""")


class ReferenceError(Exception):
    pass


def quantile_sorted(s, p):
    n = s.shape[0]
    h = (n - 1) * p
    j = math.floor(h)
    if j + 1 >= n:
        return s[n - 1]
    return s[j] + (h - j) * (s[j + 1] - s[j])


def loo_quantile(s, rank, p):
    n = s.shape[0] - 1
    h = (n - 1) * p
    j = math.floor(h)
    below = np.where(rank > j, s[j], s[j + 1])
    if j + 1 >= n:
        return below
    above = np.where(rank > j + 1, s[j + 1], s[j + 2])
    return below + (h - j) * (above - below)


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
    zero_width_failing: np.ndarray
    zero_width_failure_cells: int


def block_misses(block, s, rank, p_lo, p_hi):
    lo = loo_quantile(s, rank, p_lo)
    hi = loo_quantile(s, rank, p_hi)
    return ((block < lo) | (block > hi)).sum(axis=1)


def block_zero_width_failures(s, rank):
    n = s.shape[0]
    low_outlier = (s[1] == s[n - 1]) & (s[0] != s[1])
    high_outlier = (s[0] == s[n - 2]) & (s[n - 2] != s[n - 1])
    failing = (low_outlier & (rank == 0)) | (high_outlier & (rank == n - 1))
    return failing.any(axis=1), int((low_outlier | high_outlier).sum())


def cell_statistics(x, p_lo, p_hi, chunk=CHUNK_CELLS):
    n, cells = x.shape
    result = CellStatistics({c: np.empty(cells) for c in stats_io.STAT_COLUMNS},
                            np.zeros(n, dtype=np.int64), np.zeros(n, dtype=bool), 0)
    for start in range(0, cells, chunk):
        block = x[:, start:start + chunk]
        part = slice(start, start + block.shape[1])
        s = np.sort(block, axis=0)
        rank = ranks(block)
        with np.errstate(invalid="ignore"):
            result.stats["sd"][part] = block.std(axis=0, ddof=1)
        result.stats["mean"][part] = block.mean(axis=0)
        result.stats["min"][part] = s[0]
        result.stats["max"][part] = s[-1]
        result.stats["lo"][part] = quantile_sorted(s, p_lo)
        result.stats["hi"][part] = quantile_sorted(s, p_hi)
        result.misses += block_misses(block, s, rank, p_lo, p_hi)
        failing, failure_cells = block_zero_width_failures(s, rank)
        result.zero_width_failing |= failing
        result.zero_width_failure_cells += failure_cells
    return result


def miss_threshold(misses):
    return math.ceil(quantile_sorted(np.sort(misses.astype(float)), LOO_MISS_PERCENTILE))


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
    agg_stats = cell_statistics(agg, tail, 1 - tail).stats

    rows = [(k, {c: cells.stats[c][i] for c in stats_io.STAT_COLUMNS}) for i, k in enumerate(keys)]
    rows += [((stats_io.AGGREGATE, "", "", n), {c: agg_stats[c][i] for c in stats_io.STAT_COLUMNS})
             for i, n in enumerate(agg_names)]

    n_survey = sum(1 for k in keys if k[0] == stats_io.SURVEY)
    misses = cells.misses
    sorted_misses = np.sort(misses)
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
        "quantile_method": "hyndman-fan-7",
        "cells": f"{len(keys)} (survey {n_survey}, ctsout {len(keys) - n_survey})",
        "zero_width_cells": zero_width,
        "cell_interval": f"{CELL_INTERVAL[0]} {CELL_INTERVAL[1]}",
        "loo_misses": (f"min {int(sorted_misses[0])} median {np.median(sorted_misses):g} "
                       f"mean {misses.mean():.2f} max {int(sorted_misses[-1])}"),
        "loo_miss_percentile": LOO_MISS_PERCENTILE,
        "miss_threshold": miss_threshold(misses),
        "loo_zero_width_failures": (f"{int(cells.zero_width_failing.sum())} of {len(seeds)} replicates, "
                                    f"{cells.zero_width_failure_cells} cells"),
        "aggregates": " ".join(agg_names) or "none",
        "aggregate_interval": f"{format(tail, '.6g')} {format(1 - tail, '.6g')}",
    }
    return meta, rows


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("replicates", type=Path, help="output dir of run_replicates.py")
    parser.add_argument("-o", "--out", type=Path,
                        help="reference CSV to write (default: test/reference/<scenario file stem>.csv)")
    parser.add_argument("--allow-dirty", action="store_true",
                        help="accept replicates built from a source tree with uncommitted changes")
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    try:
        meta, rows = build(args.replicates, args.allow_dirty)
        out = args.out or ROOT / "test" / "reference" / f"{meta['scenario']}.csv"
        out.parent.mkdir(parents=True, exist_ok=True)
        stats_io.write_reference(out, meta, rows)
    except (ReferenceError, stats_io.FormatError, OSError, json.JSONDecodeError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    print(f"wrote {out}: {meta['replicates']} replicates, {meta['cells']} cells, "
          f"miss threshold {meta['miss_threshold']}, aggregates {meta['aggregates']}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
