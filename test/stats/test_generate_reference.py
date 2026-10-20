#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2,<3"]
# ///
"""Unit tests for generate_reference.py and stats_io.py on synthetic replicate data."""

import gzip
import hashlib
import io
import itertools
import json
import math
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

import generate_reference as gen
import stats_io

PROBS = (0.0, 0.001, 0.0025, 0.01, 0.025, 0.1, 0.5, 0.9, 0.975, 0.99, 0.9975, 0.999, 1.0)
METHODS = (gen.LINEAR, gen.WEIBULL)


def brute_loo_misses(x, p_lo, p_hi, method=gen.LINEAR):
    misses = np.zeros(x.shape[0], dtype=int)
    for i in range(x.shape[0]):
        others = np.delete(x, i, axis=0)
        lo = np.quantile(others, p_lo, axis=0, method=method)
        hi = np.quantile(others, p_hi, axis=0, method=method)
        misses[i] = int(((x[i] < lo) | (x[i] > hi)).sum())
    return misses


def brute_fixed_failures(keys, x):
    failing = np.zeros(x.shape[0], dtype=bool)
    for i in range(x.shape[0]):
        others = np.delete(x, i, axis=0)
        rows = {k: {"min": others[:, c].min(), "max": others[:, c].max()} for c, k in enumerate(keys)}
        fixed = stats_io.fixed_keys(rows)
        failing[i] = any(not stats_io.same_value(x[i, c], rows[k]["min"]) for c, k in enumerate(keys) if k in fixed)
    return failing


def loo_fixed_failures(keys, x, chunk=gen.CHUNK_CELLS):
    cells = gen.cell_statistics(x, 0.025, 0.975, chunk=chunk)
    return gen.fixed_failures(gen.fixed_cell_groups(keys), cells.outlier, cells.common, x.shape[0])


def misses(x, p_lo, p_hi):
    return gen.cell_statistics(x, p_lo, p_hi).misses


class QuantileTests(unittest.TestCase):
    def test_matches_numpy(self):
        rng = np.random.default_rng(1)
        for n in (1, 2, 3, 10, 49, 50, 51, 499, 500):
            x = rng.normal(size=(n, 7))
            s = np.sort(x, axis=0)
            for method, p in itertools.product(METHODS, PROBS):
                np.testing.assert_allclose(gen.quantile_sorted(s, p, method), np.quantile(x, p, axis=0, method=method),
                                           rtol=0, atol=1e-12, err_msg=f"n={n} p={p} {method}")

    def test_weibull_tail_is_an_order_statistic(self):
        s = np.arange(1.0, 500.0)[:, None]
        self.assertEqual(gen.quantile_sorted(s, 0.0025, gen.WEIBULL)[0], 1.25)
        self.assertEqual(gen.quantile_sorted(s, 0.001, gen.WEIBULL)[0], 1.0)
        self.assertEqual(gen.quantile_sorted(s, 0.999, gen.WEIBULL)[0], 499.0)

    def test_ordinal_data_gives_exact_order_statistics(self):
        s = np.arange(1.0, 51.0)[:, None]
        self.assertEqual(gen.quantile_sorted(s, 0.0)[0], 1.0)
        self.assertEqual(gen.quantile_sorted(s, 1.0)[0], 50.0)
        self.assertEqual(gen.quantile_sorted(s, 0.5)[0], 25.5)


class LeaveOneOutTests(unittest.TestCase):
    def test_matches_brute_force_with_ties(self):
        rng = np.random.default_rng(2)
        for n in (10, 11, 40):
            x = rng.integers(0, 4, size=(n, 300)).astype(float)
            for method, (p_lo, p_hi) in itertools.product(METHODS, ((0.025, 0.975), (0.1, 0.9), (0.0, 1.0),
                                                                   (0.0025, 0.9975))):
                np.testing.assert_array_equal(gen.cell_statistics(x, p_lo, p_hi, chunk=64, method=method).misses,
                                              brute_loo_misses(x, p_lo, p_hi, method), err_msg=f"n={n} {method}")

    def test_matches_brute_force_continuous(self):
        x = np.random.default_rng(3).lognormal(size=(25, 200))
        np.testing.assert_array_equal(misses(x, 0.025, 0.975), brute_loo_misses(x, 0.025, 0.975))

    def test_loo_quantile_matches_numpy_per_replicate(self):
        x = np.random.default_rng(4).integers(0, 6, size=(12, 30)).astype(float)
        s = np.sort(x, axis=0)
        rank = gen.ranks(x)
        for method, p in itertools.product(METHODS, PROBS):
            fast = gen.loo_quantile(s, rank, p, method)
            for i in range(x.shape[0]):
                np.testing.assert_allclose(fast[i], np.quantile(np.delete(x, i, axis=0), p, axis=0, method=method),
                                           rtol=0, atol=1e-12)

    def test_constant_cells_have_sd_zero(self):
        x = np.full((7, 2), 3.0425)
        x[:, 1] = 0.1
        np.testing.assert_array_equal(gen.cell_statistics(x, 0.025, 0.975).stats["sd"], [0.0, 0.0])

    def test_constant_cells_never_miss(self):
        x = np.full((20, 5), 3.0)
        self.assertEqual(misses(x, 0.025, 0.975).sum(), 0)

    def test_all_nan_cells_never_miss(self):
        x = np.random.default_rng(5).normal(size=(20, 3))
        without_nan = misses(x[:, [0, 2]], 0.025, 0.975)
        x[:, 1] = math.nan
        np.testing.assert_array_equal(misses(x, 0.025, 0.975), without_nan)

class RunPValueTests(unittest.TestCase):
    def test_loo_p_values_match_brute_force_with_ties(self):
        misses = np.random.default_rng(6).integers(0, 8, size=40)
        for i, p in enumerate(gen.loo_p_values(misses)):
            others = np.delete(misses, i).tolist()
            self.assertEqual(p, stats_io.run_p_value(int(misses[i]), others))

    def test_run_p_value(self):
        self.assertEqual(stats_io.run_p_value(5, [1, 5, 5, 9]), 4 / 5)
        self.assertEqual(stats_io.run_p_value(10, [1, 5, 5, 9]), 1 / 5)
        self.assertEqual(stats_io.run_p_value(0, [1, 5, 5, 9]), 1.0)

    def test_max_passing_misses(self):
        misses = np.arange(1000)
        limit = gen.max_passing_misses(misses)
        self.assertGreater(stats_io.run_p_value(limit, misses.tolist()), stats_io.RUN_ALPHA)
        self.assertLessEqual(stats_io.run_p_value(limit + 1, misses.tolist()), stats_io.RUN_ALPHA)
        self.assertEqual(limit, 990)
        self.assertIsNone(gen.max_passing_misses(np.array([0, 0, 0, 7])))


def ctsout_key(time, column):
    return (stats_io.CTSOUT, str(time), "", column)


class FixedCellTests(unittest.TestCase):
    def test_single_outlier_of_a_constant_group_is_reported(self):
        keys = [ctsout_key(0, "a"), ctsout_key(1, "a"), ctsout_key(0, "b"), ctsout_key(1, "b"),
                ctsout_key(0, "zero"), ctsout_key(0, "c"), ctsout_key(1, "c"), ctsout_key(0, "d"),
                ctsout_key(1, "d")]
        x = np.full((10, len(keys)), 2.0)
        x[3, 1] = 1.0
        x[7, 2] = -2.0
        x[2, 3] = 5.0
        x[:, 4] = 0.0
        x[5, 4] = 1.0
        x[6, 5] = 3.0
        x[1, 6] = x[4, 6] = 1.0
        x[:, [7, 8]] = 3.0
        x[8, [7, 8]] = 4.0
        failing, failure_cells = loo_fixed_failures(keys, x, chunk=4)
        self.assertEqual(sorted(np.flatnonzero(failing)), [3, 8])
        self.assertEqual(failure_cells, 3)
        np.testing.assert_array_equal(failing, brute_fixed_failures(keys, x))

    def test_matches_brute_force(self):
        rng = np.random.default_rng(8)
        keys = [ctsout_key(t, f"m{t % 7}") for t in range(60)] + [("survey", "1", str(g), str(g % 5))
                                                                  for g in range(40)]
        for trial in range(20):
            x = np.repeat(rng.integers(0, 3, size=(1, len(keys))).astype(float), 12, axis=0)
            x[:, rng.integers(0, len(keys), size=3)] = math.nan
            deviants = rng.integers(0, 6)
            x[rng.integers(0, 12, size=deviants), rng.integers(0, len(keys), size=deviants)] += 1.0
            np.testing.assert_array_equal(loo_fixed_failures(keys, x, chunk=17)[0], brute_fixed_failures(keys, x),
                                          err_msg=f"trial {trial}")

    def test_fixed_keys(self):
        def row(lo, hi):
            return {"min": lo, "max": hi}
        rows = {ctsout_key(0, "input EIR"): row(2.5, 2.5), ctsout_key(1, "input EIR"): row(0.0, 0.0),
                ctsout_key(0, "births"): row(2.0, 2.0), ctsout_key(1, "births"): row(2.0, 3.0),
                ctsout_key(0, "simulated EIR"): row(math.nan, math.nan),
                ctsout_key(0, "zero"): row(0.0, 0.0),
                (stats_io.SURVEY, "1", "1", "35"): row(7.0, 7.0), (stats_io.SURVEY, "2", "1", "35"): row(7.0, 7.0),
                (stats_io.AGGREGATE, "", "", "mean_eir"): row(1.0, 1.0)}
        self.assertEqual(stats_io.fixed_keys(rows),
                         {ctsout_key(0, "input EIR"), ctsout_key(0, "simulated EIR"),
                          (stats_io.SURVEY, "1", "1", "35"), (stats_io.SURVEY, "2", "1", "35")})

    def test_is_miss(self):
        row = {"lo": 1.0, "hi": 3.0}
        self.assertEqual([stats_io.is_miss(v, row) for v in (0.5, 1.0, 3.0, 3.5, math.nan)],
                         [True, False, False, True, True])
        nan_row = {"lo": math.nan, "hi": math.nan}
        self.assertEqual([stats_io.is_miss(v, nan_row) for v in (math.nan, 0.0)], [False, True])

    def test_partly_nan_measures(self):
        keys = [("survey", "1", "0", "0"), ("survey", "1", "0", "36"), ("survey", "2", "0", "36"),
                ctsout_key(0, "simulated EIR")]
        x = np.zeros((10, 4))
        x[:, 3] = math.nan
        self.assertEqual(gen.partly_nan_measures(x, keys), frozenset())
        x[0, 1] = math.nan
        measures = gen.partly_nan_measures(x, keys)
        self.assertEqual(measures, {("survey", "36")})
        kept, y = gen.drop_measures(x, keys, measures)
        self.assertEqual(kept, [keys[0], keys[3]])
        self.assertEqual(y.shape, (10, 2))

    def test_measure_list_round_trip(self):
        measures = frozenset({("survey", "36"), ("ctsout", "simulated EIR")})
        text = stats_io.format_measures(measures)
        self.assertEqual(text, "ctsout:simulated EIR | survey:36")
        self.assertEqual(stats_io.parse_measures(text), measures)
        self.assertEqual(stats_io.format_measures(frozenset()), "none")
        self.assertEqual(stats_io.parse_measures("none"), frozenset())


class ParserTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_survey(self):
        (self.dir / "o.txt").write_text("1\t1\t0\t6\n1\t1001\t3\t2.5\n")
        keys, values = stats_io.read_survey(self.dir / "o.txt")
        self.assertEqual(keys, [("survey", "1", "1", "0"), ("survey", "1", "1001", "3")])
        self.assertEqual(values, [6.0, 2.5])

    def test_survey_bad_row(self):
        (self.dir / "o.txt").write_text("1\t1\t0\n")
        with self.assertRaises(stats_io.FormatError):
            stats_io.read_survey(self.dir / "o.txt")

    def test_ctsout_nan_and_duplicate_headers(self):
        (self.dir / "c.txt").write_text("##\t##\nt\tEIR\tx\tx\n0\tnan\t1\t2\n1\t-nan\t3\t4\n")
        keys, values = stats_io.read_ctsout(self.dir / "c.txt")
        self.assertEqual(keys[:3], [("ctsout", "0", "", "EIR"), ("ctsout", "0", "", "x"), ("ctsout", "0", "", "x#2")])
        self.assertTrue(math.isnan(values[0]) and math.isnan(values[3]))
        self.assertEqual(values[4:], [3.0, 4.0])

    def test_ctsout_during_init_keyed_by_timestep(self):
        (self.dir / "c.txt").write_text(
            "##\t##\nsimulation time\ttimestep\tEIR\n0\tnan\t9\n5\tnan\t8\n10\t0\t1\n15\t1\t2\n"
        )
        keys, values = stats_io.read_ctsout(self.dir / "c.txt")
        self.assertEqual(keys, [("ctsout", "0", "", "EIR"), ("ctsout", "1", "", "EIR")])
        self.assertEqual(values, [1.0, 2.0])

    def test_ctsout_timestep_first_column_unchanged(self):
        (self.dir / "c.txt").write_text("##\t##\ntimestep\tsimulation time\n0\tnan\n")
        keys, values = stats_io.read_ctsout(self.dir / "c.txt")
        self.assertEqual(keys, [("ctsout", "0", "", "simulation time")])
        self.assertTrue(math.isnan(values[0]))


class AggregateTests(unittest.TestCase):
    def cells(self, rows, cts=()):
        cells = {("survey", str(s), str(g), str(m)): v for s, g, m, v in rows}
        cells.update({("ctsout", str(t), "", name): v for t, name, v in cts})
        return cells

    def test_sums_whole_population_groups_only(self):
        cells = self.cells([(1, 1, 14, 2), (1, 2, 14, 3), (2, 1, 14, 1), (1, 1001, 14, 100),
                            (1, 1, 18, 1), (1, 1, 19, 2), (1, 1, 0, 10), (1, 2, 0, 10), (1, 1, 3, 5)])
        agg = stats_io.aggregates(cells)
        self.assertEqual(agg["clinical_episodes"], 6)
        self.assertEqual(agg["deaths"], 3)
        self.assertEqual(agg["mean_prevalence"], 0.25)
        self.assertNotIn("severe_episodes", agg)
        self.assertNotIn("mean_eir", agg)

    def test_eir_prefers_survey_measure_then_ctsout(self):
        cts = [(0, "simulated EIR", math.nan), (1, "simulated EIR", 2.0), (2, "simulated EIR", 4.0)]
        self.assertEqual(stats_io.aggregates(self.cells([], cts))["mean_eir"], 3.0)
        self.assertEqual(stats_io.aggregates(self.cells([(1, 0, 36, 7), (2, 0, 36, 9)], cts))["mean_eir"], 8.0)

    def test_eir_skips_nan_surveys(self):
        rows = [(1, 0, 36, 7), (2, 0, 36, math.nan), (3, 0, 36, 9)]
        self.assertEqual(stats_io.aggregates(self.cells(rows))["mean_eir"], 8.0)

    def test_aggregate_inputs_cover_every_aggregate(self):
        cells = self.cells([(1, 1, m, 1.0) for m in (0, 3, 14, 15, 18, 19, 36)] + [(1, 1, 11, 4.0)],
                           [(0, "simulated EIR", 1.0), (0, "input EIR", 1.0)])
        subset = {k: v for k, v in cells.items() if stats_io.is_aggregate_input(k)}
        self.assertEqual(len(subset), 8)
        self.assertEqual(stats_io.aggregates(subset), stats_io.aggregates(cells))


class GroupQuantityTests(unittest.TestCase):
    def test_measure_totals_split_whole_population_and_subgroups(self):
        cells = AggregateTests().cells([(1, 1, 14, 2), (1, 2, 14, 3), (2, 1, 14, 1), (1, 1001, 14, 100),
                                        (2, 1000000, 14, 7), (1, 1, 3, 0.1), (2, 1, 3, 0.2)],
                                       [(0, "simulated EIR", 4.0)])
        self.assertEqual(stats_io.measure_totals(cells), {
            ("total", "", "whole", "14"): 6, ("total", "", "split", "14"): 107,
            ("total", "", "whole", "3"): math.fsum([0.1, 0.2])})
        quantities = stats_io.group_quantities(cells)
        self.assertEqual(quantities[("aggregate", "", "", "clinical_episodes")], 6)
        self.assertEqual(quantities[("aggregate", "", "", "mean_eir")], 4.0)
        self.assertEqual(len(quantities), 5)

    def test_replicate_totals_match_measure_totals(self):
        rng = np.random.default_rng(3)
        keys = [("survey", str(s), str(g), str(m)) for s in (1, 2) for g in (1, 2, 1001) for m in (0, 14)]
        keys.append(("ctsout", "0", "", "simulated EIR"))
        x = rng.random((4, len(keys))) * 1e3
        totals = gen.replicate_totals(x, keys)
        for r in range(4):
            expected = stats_io.measure_totals(dict(zip(keys, x[r].tolist(), strict=True)))
            self.assertEqual({k: v[r] for k, v in totals.items()}, expected)

    def test_varies_enough(self):
        self.assertFalse(gen.varies_enough([0.0] * 491 + [1.0] * 9))
        self.assertTrue(gen.varies_enough([0.0] * 490 + [1.0] * 10))
        self.assertTrue(gen.varies_enough(list(range(20))))
        self.assertFalse(gen.varies_enough([math.nan] * 20 + list(range(20))))

    def test_largest_group_z_matches_brute_force(self):
        rng = np.random.default_rng(4)
        n, k = 40, 6
        values = np.column_stack([rng.normal(5, 2, n), rng.poisson(3, n), np.r_[np.zeros(n - 3), 1, 2, 3]])
        centred = values - values.mean(axis=0)
        chosen = np.array([rng.permutation(n)[:k] for _ in range(50)])
        chosen[0] = [n - 3, n - 2, n - 1, 0, 1, 2]
        maxima = gen.largest_group_z(centred, chosen)
        for row, indices in zip(maxima, chosen, strict=True):
            rest = np.delete(values, indices, axis=0)
            group = values[indices]
            z = [abs(group[:, q].mean() - rest[:, q].mean()) / (rest[:, q].std(ddof=1) * math.sqrt(1 / k + 1 / (n - k)))
                 if rest[:, q].std() else (math.inf if group[:, q].mean() != rest[:, q].mean() else 0.0)
                 for q in range(values.shape[1])]
            self.assertAlmostEqual(row, max(z), places=9)
        self.assertEqual(maxima[0], math.inf)

    def test_calibration_of_one_normal_quantity(self):
        values = np.random.default_rng(5).normal(size=(500, 1))
        maxima = gen.leave_k_out_maxima(values, 20, 20000)
        self.assertAlmostEqual(float(np.quantile(maxima, 0.95)), 1.96, delta=0.15)
        np.testing.assert_array_equal(gen.leave_k_out_maxima(values, 20, 20000), maxima)

    def test_calibrate_group_skips_rare_quantities(self):
        rng = np.random.default_rng(6)
        candidates = {("total", "", "whole", "14"): rng.poisson(30, 100).tolist(),
                      ("total", "", "whole", "15"): [0.0] * 95 + [1.0] * 5,
                      ("total", "", "whole", "0"): [100.0] * 100}
        group = gen.calibrate_group(candidates, 100, 10, 0.001, 5000)
        self.assertEqual(list(group.values), [("total", "", "whole", "14")])
        self.assertEqual(group.skipped, 2)
        self.assertEqual(list(group.quantiles), [0.05, 0.01, 0.001])
        self.assertLess(group.quantiles[0.05], group.quantiles[0.001])
        self.assertFalse(gen.calibrate_group(candidates, 100, 60, 0.001, 5000).quantiles)


class EndToEndTests(unittest.TestCase):
    N = 30

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.reps = self.dir / "reps"
        self.reps.mkdir()
        self.scenario = self.dir / "scenarioSynthetic.xml"
        self.scenario.write_bytes(b'<om:scenario schemaVersion="50" name="s"/>')
        self.write_meta()
        for i in range(self.N):
            self.write_run(1001 + i, i)

    def tearDown(self):
        self.tmp.cleanup()

    def write_meta(self, dirty=False):
        (self.reps / "meta.json").write_text(json.dumps({
            "scenario": str(self.scenario),
            "scenario_sha256": hashlib.sha256(self.scenario.read_bytes()).hexdigest(),
            "schema": "scenario_current.xsd", "commit": "abc", "dirty": dirty,
            "binary_sha256": "def", "output_format": "txt", "created": "2026-01-01T00:00:00+00:00",
        }))

    def write_run(self, seed, i, reverse=False):
        rows = [
            (1, 1, 0, 100), (1, 1, 3, 10 + i % 7), (1, 1, 14, i + 1),
            (2, 1, 0, 100), (2, 1, 3, 20 + i % 3), (2, 1, 14, 2 * i),
            (2, 1001, 14, 0),
        ]
        lines = [f"{s}\t{g}\t{m}\t{v}\n" for s, g, m, v in rows]
        if reverse:
            lines.reverse()
        d = self.reps / str(seed)
        d.mkdir(exist_ok=True)
        (d / "output.txt").write_text("".join(lines))
        (d / "ctsout.txt").write_text(
            f"##\t##\ntimestep\tinput EIR\tsimulated EIR\n0\t3\tnan\n1\t3\t{1 + i / 10}\n")

    def run_main(self, *extra):
        out = self.dir / "ref.csv"
        with redirect_stderr(io.StringIO()) as err:
            code = gen.main([str(self.reps), "-o", str(out), *extra])
        return code, out, err.getvalue()

    def test_reference_contents(self):
        self.write_run(1001 + self.N - 1, self.N - 1, reverse=True)
        (self.reps / "2000.partial").mkdir()
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        meta = ref.meta
        self.assertEqual(meta["scenario"], "scenarioSynthetic")
        self.assertEqual(meta["schema_version"], "50")
        self.assertEqual(meta["replicates"], str(self.N))
        self.assertEqual(meta["seeds"], f"1001-{1000 + self.N}")
        self.assertEqual(meta["cells"], "11 (survey 7, ctsout 4)")
        self.assertEqual(meta["zero_width_cells"], "6")
        self.assertEqual(meta["aggregates"], "clinical_episodes mean_prevalence mean_eir")
        self.assertEqual(meta["aggregate_interval"], "0.00166667 0.998333")
        self.assertEqual(len(ref.loo_counts), self.N)
        self.assertEqual(meta["loo_run_failures"].split(" (")[0].split(" of ")[1], str(self.N))
        self.assertEqual(len(ref.rows), 11 + 3)

        episodes = ref.rows[("survey", "1", "1", "14")]
        values = np.arange(1.0, self.N + 1)
        self.assertEqual(episodes["mean"], values.mean())
        self.assertEqual(episodes["sd"], values.std(ddof=1))
        self.assertEqual((episodes["min"], episodes["max"]), (1.0, float(self.N)))
        self.assertEqual(episodes["lo"], np.quantile(values, 0.025))
        self.assertEqual(episodes["hi"], np.quantile(values, 0.975))

        hosts = ref.rows[("survey", "2", "1", "0")]
        self.assertEqual((hosts["min"], hosts["max"], hosts["lo"], hosts["hi"], hosts["sd"]), (100.0,) * 4 + (0.0,))
        self.assertTrue(math.isnan(ref.rows[("ctsout", "0", "", "simulated EIR")]["min"]))

        total = np.arange(self.N) * 3 + 1.0
        clinical = ref.rows[("aggregate", "", "", "clinical_episodes")]
        self.assertEqual(clinical["lo"], np.quantile(total, 0.005 / 3, method="weibull"))
        self.assertEqual(clinical["hi"], np.quantile(total, 1 - 0.005 / 3, method="weibull"))
        self.assertEqual((meta["quantile_method"], meta["aggregate_quantile_method"]),
                         ("hyndman-fan-7", "hyndman-fan-6"))

    def test_group_values_and_threshold(self):
        code, out, err = self.run_main("--group-k", "5", "--group-alpha", "0.01", "--group-resamples", "2000")
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        self.assertEqual(set(ref.group_values), {
            ("aggregate", "", "", "clinical_episodes"), ("aggregate", "", "", "mean_prevalence"),
            ("aggregate", "", "", "mean_eir"), ("total", "", "whole", "3"), ("total", "", "whole", "14")})
        for i, seed in enumerate(range(1001, 1001 + self.N)):
            keys, values = stats_io.read_run(self.reps / str(seed))
            quantities = stats_io.group_quantities(dict(zip(keys, values, strict=True)))
            self.assertEqual({k: v[i] for k, v in ref.group_values.items()},
                             {k: quantities[k] for k in ref.group_values})
        meta = ref.meta
        self.assertEqual((meta["group_k"], meta["group_alpha"], meta["group_resamples"]), ("5", "0.01", "2000"))
        quantiles = meta["group_quantiles"].split()
        self.assertEqual(quantiles[0::2], ["0.05", "0.01", "0.001"])
        self.assertEqual(meta["group_threshold"], quantiles[3])
        self.assertTrue(meta["group_quantities"].startswith("5 (skipped 2 "))

    def test_no_group_threshold_with_too_few_replicates(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        self.assertIn("no group test", err)
        self.assertEqual(stats_io.read_reference(out).meta["group_threshold"], "none")

    def test_round_trip_is_exact(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        copy = self.dir / "copy.csv"
        stats_io.write_reference(copy, ref.meta, list(ref.rows.items()), ref.group_values)
        self.assertEqual(copy.read_text(), out.read_text())

    def test_gzip_round_trip_is_exact_and_reproducible(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        packed = self.dir / "ref.csv.gz"
        stats_io.write_reference(packed, ref.meta, list(ref.rows.items()), ref.group_values)
        self.assertEqual(gzip.decompress(packed.read_bytes()), out.read_bytes())
        first = packed.read_bytes()
        stats_io.write_reference(packed, ref.meta, list(ref.rows.items()), ref.group_values)
        self.assertEqual(packed.read_bytes(), first)
        unpacked = stats_io.read_reference(packed)
        self.assertEqual(unpacked.meta, ref.meta)
        self.assertEqual(unpacked.loo_counts, ref.loo_counts)
        self.assertEqual(unpacked.rows.keys(), ref.rows.keys())
        self.assertEqual(unpacked.group_values, ref.group_values)

    def test_partly_nan_measure_has_no_cells(self):
        for seed in (1003, 1007):
            path = self.reps / str(seed) / "output.txt"
            path.write_text(path.read_text() + "1\t0\t36\tnan\n2\t0\t36\t5\n")
        for i in range(self.N):
            path = self.reps / str(1001 + i) / "output.txt"
            if 1001 + i not in (1003, 1007):
                path.write_text(path.read_text() + f"1\t0\t36\t{4 + i % 3}\n2\t0\t36\t5\n")
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        self.assertEqual(ref.meta[stats_io.NAN_MEASURES], "survey:36")
        self.assertEqual(ref.nan_measures, {("survey", "36")})
        self.assertFalse([k for k in ref.rows if k[3] == "36"])
        self.assertEqual(ref.meta["cells"], "11 (survey 7, ctsout 4)")
        self.assertIn(("aggregate", "", "", "mean_eir"), ref.rows)
        self.assertNotIn(("total", "", "whole", "36"), ref.group_values)

    def test_no_nan_measures(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        self.assertEqual(ref.meta[stats_io.NAN_MEASURES], "none")
        self.assertEqual(ref.nan_measures, frozenset())

    def test_differing_cells_are_an_error(self):
        (self.reps / "1005" / "output.txt").write_text("1\t1\t0\t100\n")
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("different cells", err)

    def test_dirty_replicates_need_flag(self):
        self.write_meta(dirty=True)
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("dirty", err)
        code, out, err = self.run_main("--allow-dirty")
        self.assertEqual(code, 0, err)
        self.assertEqual(stats_io.read_reference(out).meta["dirty"], "true")

    def test_too_few_replicates(self):
        for i in range(gen.MIN_REPLICATES - 1, self.N):
            for f in (self.reps / str(1001 + i)).iterdir():
                f.unlink()
            (self.reps / str(1001 + i)).rmdir()
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("need at least", err)

    def test_changed_scenario_gives_unknown_schema_version(self):
        self.scenario.write_bytes(b'<om:scenario schemaVersion="51"/>')
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        self.assertEqual(stats_io.read_reference(out).meta["schema_version"], "unknown")


if __name__ == "__main__":
    unittest.main()
