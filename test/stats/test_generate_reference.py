#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2,<3"]
# ///
"""Unit tests for generate_reference.py and stats_io.py on synthetic replicate data."""

import gzip
import hashlib
import io
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

PROBS = (0.0, 0.01, 0.025, 0.1, 0.5, 0.9, 0.975, 0.99, 1.0)


def brute_loo_misses(x, p_lo, p_hi):
    misses = np.zeros(x.shape[0], dtype=int)
    for i in range(x.shape[0]):
        others = np.delete(x, i, axis=0)
        lo = np.quantile(others, p_lo, axis=0)
        hi = np.quantile(others, p_hi, axis=0)
        misses[i] = int(((x[i] < lo) | (x[i] > hi)).sum())
    return misses


def misses(x, p_lo, p_hi):
    return gen.cell_statistics(x, p_lo, p_hi).misses


class QuantileTests(unittest.TestCase):
    def test_matches_numpy_default(self):
        rng = np.random.default_rng(1)
        for n in (1, 2, 3, 10, 49, 50, 51):
            x = rng.normal(size=(n, 7))
            s = np.sort(x, axis=0)
            for p in PROBS:
                np.testing.assert_allclose(gen.quantile_sorted(s, p), np.quantile(x, p, axis=0),
                                           rtol=0, atol=1e-12, err_msg=f"n={n} p={p}")

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
            for p_lo, p_hi in ((0.025, 0.975), (0.1, 0.9), (0.0, 1.0)):
                np.testing.assert_array_equal(gen.cell_statistics(x, p_lo, p_hi, chunk=64).misses,
                                              brute_loo_misses(x, p_lo, p_hi), err_msg=f"n={n}")

    def test_matches_brute_force_continuous(self):
        x = np.random.default_rng(3).lognormal(size=(25, 200))
        np.testing.assert_array_equal(misses(x, 0.025, 0.975), brute_loo_misses(x, 0.025, 0.975))

    def test_loo_quantile_matches_numpy_per_replicate(self):
        x = np.random.default_rng(4).integers(0, 6, size=(12, 30)).astype(float)
        s = np.sort(x, axis=0)
        rank = gen.ranks(x)
        for p in PROBS:
            fast = gen.loo_quantile(s, rank, p)
            for i in range(x.shape[0]):
                np.testing.assert_allclose(fast[i], np.quantile(np.delete(x, i, axis=0), p, axis=0),
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


class FixedCellTests(unittest.TestCase):
    def test_single_outlier_from_non_zero_constant_is_reported(self):
        x = np.zeros((10, 5))
        x[:, [3, 4]] = 2.0
        x[3, 0] = 1.0
        x[7, 3] = -2.0
        x[2, 4] = 5.0
        x[2, 2] = x[5, 2] = 1.0
        result = gen.cell_statistics(x, 0.025, 0.975, chunk=3)
        self.assertEqual(result.fixed_failure_cells, 2)
        self.assertEqual(sorted(np.flatnonzero(result.fixed_failing)), [2, 7])

    def test_is_fixed_cell(self):
        def row(lo, hi):
            return {"min": lo, "max": hi}
        self.assertTrue(stats_io.is_fixed_cell(row(2.5, 2.5)))
        self.assertTrue(stats_io.is_fixed_cell(row(math.nan, math.nan)))
        self.assertFalse(stats_io.is_fixed_cell(row(0.0, 0.0)))
        self.assertFalse(stats_io.is_fixed_cell(row(1.0, 2.0)))

    def test_is_miss(self):
        row = {"lo": 1.0, "hi": 3.0}
        self.assertEqual([stats_io.is_miss(v, row) for v in (0.5, 1.0, 3.0, 3.5, math.nan)],
                         [True, False, False, True, True])
        nan_row = {"lo": math.nan, "hi": math.nan}
        self.assertEqual([stats_io.is_miss(v, nan_row) for v in (math.nan, 0.0)], [False, True])

    def test_mixed_nan_is_an_error(self):
        x = np.zeros((10, 2))
        x[0, 1] = math.nan
        with self.assertRaisesRegex(gen.ReferenceError, "NaN in some replicates"):
            gen.check_nan(x, [("a",), ("b",)])
        x[:, 1] = math.nan
        gen.check_nan(x, [("a",), ("b",)])


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

    def test_aggregate_inputs_cover_every_aggregate(self):
        cells = self.cells([(1, 1, m, 1.0) for m in (0, 3, 14, 15, 18, 19, 36)] + [(1, 1, 11, 4.0)],
                           [(0, "simulated EIR", 1.0), (0, "input EIR", 1.0)])
        subset = {k: v for k, v in cells.items() if stats_io.is_aggregate_input(k)}
        self.assertEqual(len(subset), 8)
        self.assertEqual(stats_io.aggregates(subset), stats_io.aggregates(cells))


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
        self.assertEqual(clinical["lo"], np.quantile(total, 0.005 / 3))
        self.assertEqual(clinical["hi"], np.quantile(total, 1 - 0.005 / 3))

    def test_round_trip_is_exact(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        copy = self.dir / "copy.csv"
        stats_io.write_reference(copy, ref.meta, list(ref.rows.items()))
        self.assertEqual(copy.read_text(), out.read_text())

    def test_gzip_round_trip_is_exact_and_reproducible(self):
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        ref = stats_io.read_reference(out)
        packed = self.dir / "ref.csv.gz"
        stats_io.write_reference(packed, ref.meta, list(ref.rows.items()))
        self.assertEqual(gzip.decompress(packed.read_bytes()), out.read_bytes())
        first = packed.read_bytes()
        stats_io.write_reference(packed, ref.meta, list(ref.rows.items()))
        self.assertEqual(packed.read_bytes(), first)
        unpacked = stats_io.read_reference(packed)
        self.assertEqual(unpacked.meta, ref.meta)
        self.assertEqual(unpacked.loo_counts, ref.loo_counts)
        self.assertEqual(unpacked.rows.keys(), ref.rows.keys())

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
