#!/usr/bin/env python3
"""Unit tests for check_scenario.py on a synthetic reference and synthetic runs."""

import io
import itertools
import math
import statistics
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_scenario as chk
import stats_io

N = 499
LOO_COUNTS = list(range(N))


def row(mean, sd, lo, hi, minimum=None, maximum=None):
    return {"mean": mean, "sd": sd, "min": lo if minimum is None else minimum,
            "max": hi if maximum is None else maximum, "lo": lo, "hi": hi}


def survey_key(survey, group, measure):
    return (stats_io.SURVEY, str(survey), str(group), str(measure))


def make_reference(count_cells=600):
    rows = {survey_key(1, 1, 0): row(100, 0, 100, 100), survey_key(1, 1, 3): row(40, 5, 30, 50)}
    for i in range(count_cells):
        rows[survey_key(2, i + 1, 14)] = row(5, 2, 1, 9, 0, 12)
    rows[survey_key(2, 1, 12)] = row(0, 0, 0, 0)
    rows[(stats_io.CTSOUT, "0", "", "input EIR")] = row(3.5, 0, 3.5, 3.5)
    rows[(stats_io.CTSOUT, "0", "", "simulated EIR")] = row(math.nan, math.nan, math.nan, math.nan)
    rows[(stats_io.AGGREGATE, "", "", "mean_prevalence")] = row(0.4, 0.05, 0.3, 0.5)
    meta = {"format_version": stats_io.FORMAT_VERSION, "replicates": str(N), "seeds": f"1001-{1000 + N}",
            "commit": "abc", stats_io.LOO_COUNTS: " ".join(map(str, LOO_COUNTS))}
    ref = stats_io.Reference(meta, rows, list(LOO_COUNTS))
    return ref


def passing_run(ref):
    cells = {k: (r["lo"] + r["hi"]) / 2 for k, r in ref.rows.items() if k[0] != stats_io.AGGREGATE}
    cells[(stats_io.CTSOUT, "0", "", "simulated EIR")] = math.nan
    return cells


def judge(ref, cells, seed=1):
    return chk.judge_run(ref, list(cells), list(cells.values()), seed)


TOTAL_14 = (stats_io.TOTAL, "", stats_io.WHOLE, "14")
PREVALENCE = (stats_io.AGGREGATE, "", "", "mean_prevalence")


def add_group(ref, k=4, threshold=3.0):
    ref.group_values = {PREVALENCE: [0.4] + [0.375, 0.425] * (N // 2), TOTAL_14: [3000.0] + [2990.0, 3010.0] * (N // 2)}
    ref.meta.update({"group_k": str(k), "group_threshold": str(threshold), "group_alpha": "0.0001"})
    return ref


class FakeResult:
    def __init__(self, failed):
        self.failed = failed


class JudgeRunTests(unittest.TestCase):
    def setUp(self):
        self.ref = make_reference()
        self.cells = passing_run(self.ref)

    def test_passing_run(self):
        result = judge(self.ref, self.cells)
        self.assertFalse(result.failed)
        self.assertEqual(result.misses, [])
        self.assertEqual(result.p, 1.0)
        self.assertEqual(result.fixed_cells, 3)
        self.assertEqual(result.outside_aggregates, [])

    def test_miss_count_p_value(self):
        count_keys = [k for k in self.cells if k[3] == "14"]
        for n_misses, failed in ((0, False), (10, False), (494, False), (495, True), (600, True)):
            cells = dict(self.cells)
            for k in count_keys[:n_misses]:
                cells[k] = 12
            result = judge(self.ref, cells)
            self.assertEqual(len(result.misses), n_misses)
            self.assertEqual(result.p, (1 + max(N - n_misses, 0)) / (N + 1))
            self.assertEqual(result.failed, failed, n_misses)

    def test_nan_measures_are_left_out(self):
        self.ref.meta[stats_io.NAN_MEASURES] = "survey:36"
        self.ref.rows[(stats_io.AGGREGATE, "", "", "mean_eir")] = row(6, 1, 4, 8)
        self.cells[survey_key(1, 0, 36)] = math.nan
        self.cells[survey_key(2, 0, 36)] = 6.0
        result = judge(self.ref, self.cells)
        self.assertFalse(result.failed)
        self.assertEqual(result.misses, [])
        self.assertEqual(result.aggregates["mean_eir"].value, 6.0)
        self.assertNotIn((stats_io.TOTAL, "", stats_io.WHOLE, "36"), stats_io.group_quantities(
            self.cells, self.ref.nan_measures))

    def test_unlisted_measure_is_still_an_extra_cell(self):
        self.cells[survey_key(1, 0, 36)] = 6.0
        with self.assertRaisesRegex(chk.CheckError, "different cells"):
            judge(self.ref, self.cells)

    def test_constant_cell_of_a_varying_column_is_only_a_miss(self):
        births = (stats_io.CTSOUT, "1", "", "input EIR")
        self.ref.rows[births] = row(3.5, 0.2, 3.5, 3.5, 3.5, 4.5)
        self.cells[births] = 3.5
        self.cells[(stats_io.CTSOUT, "0", "", "input EIR")] = 4.0
        result = judge(self.ref, self.cells)
        self.assertEqual(result.fixed_cells, 2)
        self.assertEqual(result.fixed_mismatches, [])
        self.assertEqual(len(result.misses), 1)
        self.assertFalse(result.failed)

    def test_rare_event_cell_is_only_a_miss(self):
        self.cells[survey_key(2, 1, 12)] = 1
        result = judge(self.ref, self.cells)
        self.assertEqual(len(result.misses), 1)
        self.assertEqual(result.fixed_mismatches, [])
        self.assertFalse(result.failed)

    def test_fixed_cell_mismatch_fails(self):
        for key, value in ((("ctsout", "0", "", "input EIR"), 3.6),
                           (("ctsout", "0", "", "simulated EIR"), 1.0),
                           (survey_key(1, 1, 0), math.nan)):
            cells = dict(self.cells)
            cells[key] = value
            result = judge(self.ref, cells)
            self.assertTrue(result.failed, key)
            self.assertEqual([c.key for c in result.fixed_mismatches], [key])
            self.assertEqual([c.key for c in result.misses], [key])

    def test_aggregate_outside_fails(self):
        self.cells[survey_key(1, 1, 3)] = 49
        self.assertEqual(judge(self.ref, self.cells).outside_aggregates, [])
        self.cells[survey_key(1, 1, 3)] = 55
        result = judge(self.ref, self.cells)
        self.assertEqual(result.outside_aggregates, ["mean_prevalence"])
        self.assertEqual(len(result.misses), 1)
        self.assertTrue(result.failed)

    def test_missing_and_extra_cells_are_errors(self):
        del self.cells[survey_key(1, 1, 3)]
        with self.assertRaisesRegex(chk.CheckError, "1 missing"):
            judge(self.ref, self.cells)
        self.cells[survey_key(1, 1, 3)] = 40
        self.cells[survey_key(9, 9, 9)] = 0
        with self.assertRaisesRegex(chk.CheckError, "1 extra"):
            judge(self.ref, self.cells)

    def test_group_quantities(self):
        add_group(self.ref)
        result = judge(self.ref, self.cells)
        self.assertEqual(result.group, {PREVALENCE: 0.4, TOTAL_14: 600 * 5.0})
        self.ref.group_values[(stats_io.TOTAL, "", stats_io.SPLIT, "14")] = [1.0] * N
        with self.assertRaisesRegex(chk.CheckError, "no group quantity total of measure 14"):
            judge(self.ref, self.cells)

    def test_z(self):
        self.assertEqual(chk.Cell(None, 9, row(5, 2, 1, 9)).z, 2.0)
        self.assertEqual(chk.Cell(None, 1, row(5, 2, 1, 9)).z, 2.0)
        self.assertEqual(chk.Cell(None, 4, row(3.5, 1e-14, 3.5, 3.5)).z, math.inf)
        self.assertEqual(chk.Cell(None, math.nan, row(5, 2, 1, 9)).z, math.inf)


class VerdictTests(unittest.TestCase):
    def test_rerun_verdict(self):
        passed, failed = FakeResult(False), FakeResult(True)
        self.assertFalse(chk.rerun_verdict(passed, [])[0])
        self.assertFalse(chk.rerun_verdict(failed, [passed] * 5)[0])
        self.assertIn("pass after rerun", chk.rerun_verdict(failed, [passed] * 5)[1])
        self.assertTrue(chk.rerun_verdict(failed, [passed] * 4 + [failed])[0])

    def test_binomial_tail_matches_enumeration(self):
        for n, p in ((5, 0.02), (7, 0.3)):
            for k in range(n + 1):
                brute = math.fsum(p ** sum(o) * (1 - p) ** (n - sum(o))
                                  for o in itertools.product((0, 1), repeat=n) if sum(o) >= k)
                self.assertAlmostEqual(chk.binomial_tail(k, n, p), brute, places=12)
        self.assertAlmostEqual(chk.binomial_tail(40, 2000, 0.02), 0.52166573861216, places=9)


class GroupTests(unittest.TestCase):
    def setUp(self):
        self.ref = add_group(make_reference())
        self.cells = passing_run(self.ref)

    def runs(self, shift=0.0, k=4):
        results = []
        for i in range(k):
            cells = dict(self.cells)
            cells[survey_key(2, i + 1, 14)] += shift
            results.append(judge(self.ref, cells, 2001 + i))
        return results

    def test_scores(self):
        mean, sd = statistics.fmean(self.ref.group_values[TOTAL_14]), statistics.stdev(self.ref.group_values[TOTAL_14])
        results = self.runs(shift=8.0)
        scores = {s.key: s for s in chk.group_scores(self.ref, results)}
        self.assertEqual(mean, 3000.0)
        self.assertEqual(scores[TOTAL_14].mean, 3008.0)
        self.assertAlmostEqual(scores[TOTAL_14].z, 8.0 / (sd * math.sqrt(1 / 4 + 1 / N)), places=12)
        self.assertAlmostEqual(scores[PREVALENCE].z, 0.0, places=9)
        self.assertEqual(chk.group_scores(self.ref, results)[0].key, TOTAL_14)

    def test_verdict(self):
        sd = statistics.stdev(self.ref.group_values[TOTAL_14])
        one_z = sd * math.sqrt(1 / 4 + 1 / N)
        failed, summary, _ = chk.group_verdict(self.ref, self.runs(shift=2.99 * one_z))
        self.assertFalse(failed, summary)
        self.assertIn("pass: largest |z| 2.99 <= 3", summary)
        failed, summary, _ = chk.group_verdict(self.ref, self.runs(shift=-3.02 * one_z))
        self.assertTrue(failed)
        self.assertIn("largest |z| 3.02 (total of measure 14 (whole population)) > 3", summary)

    def test_fixed_cell_fails_the_group(self):
        results = self.runs()
        self.assertFalse(chk.group_verdict(self.ref, results)[0])
        cells = dict(self.cells)
        cells[("ctsout", "0", "", "input EIR")] = 3.6
        results[2] = judge(self.ref, cells, 2003)
        failed, summary, _ = chk.group_verdict(self.ref, results)
        self.assertTrue(failed)
        self.assertIn("1 of 4 runs differ in fixed cells", summary)

    def test_group_size(self):
        self.assertEqual(chk.group_size(self.ref, 0), 4)
        self.assertEqual(chk.group_size(self.ref, 4), 4)
        with self.assertRaisesRegex(chk.CheckError, "calibrated for K = 4, not 5"):
            chk.group_size(self.ref, 5)
        self.ref.meta["group_threshold"] = "none"
        with self.assertRaisesRegex(chk.CheckError, "no group test threshold"):
            chk.group_size(self.ref, 0)

    def test_describe(self):
        results = self.runs(shift=8.0)
        text = chk.describe_group(self.ref, results, chk.group_scores(self.ref, results), False)
        self.assertIn("group test, K = 4: threshold 3", text)
        self.assertIn("total of measure 14 (whole population): mean of 4 runs 3008; reference mean 3000", text)
        self.assertIn("per-run rules, for information: 0 of 4 runs failed", text)


class HelperTests(unittest.TestCase):
    def test_parse_seeds(self):
        self.assertEqual(chk.parse_seeds("1001-1003 7"), {1001, 1002, 1003, 7})
        self.assertEqual(chk.parse_seeds(""), set())

    def test_own_seed(self):
        self.assertEqual(chk.own_seed(b'<computationParameters iseed ="42"/>'), 42)
        with self.assertRaises(chk.CheckError):
            chk.own_seed(b"<scenario/>")

    def test_default_reference(self):
        self.assertEqual(chk.default_reference("1").name, "scenario1.csv.gz")
        self.assertEqual(chk.default_reference("/x/scenarioFoo.xml").name, "scenarioFoo.csv.gz")


class OutputsModeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.ref_path = self.dir / "ref.csv"
        ref = make_reference(count_cells=3)
        stats_io.write_reference(self.ref_path, ref.meta, list(ref.rows.items()))
        self.ref = stats_io.read_reference(self.ref_path)
        self.runs = self.dir / "runs"

    def tearDown(self):
        self.tmp.cleanup()

    def write_run(self, seed, cells):
        d = self.runs / str(seed)
        d.mkdir(parents=True)
        survey = [k for k in cells if k[0] == stats_io.SURVEY]
        (d / "output.txt").write_text("".join(f"{k[1]}\t{k[2]}\t{k[3]}\t{cells[k]}\n" for k in survey))
        (d / "ctsout.txt").write_text(f"##\t##\ntimestep\tinput EIR\tsimulated EIR\n"
                                      f"0\t{cells[('ctsout', '0', '', 'input EIR')]}\tnan\n")

    def run_main(self, *extra):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = chk.main(["1", "--reference", str(self.ref_path), "--outputs", str(self.runs), *extra])
        return code, out.getvalue(), err.getvalue()

    def test_reference_round_trip(self):
        self.assertEqual(self.ref.loo_counts, LOO_COUNTS)

    def test_group_values_round_trip(self):
        ref = add_group(make_reference(count_cells=3))
        stats_io.write_reference(self.ref_path, ref.meta, list(ref.rows.items()), ref.group_values)
        self.assertEqual(stats_io.read_reference(self.ref_path).group_values, ref.group_values)
        self.ref_path.write_text(self.ref_path.read_text().replace(" 0.425\n", "\n", 1))
        with self.assertRaisesRegex(stats_io.FormatError, "498 values of"):
            stats_io.read_reference(self.ref_path)

    def test_passing_and_failing_runs(self):
        cells = passing_run(self.ref)
        self.write_run(1, cells)
        code, out, err = self.run_main()
        self.assertEqual(code, 0, err)
        self.assertIn("0 of 1 runs failed", out)
        cells[("ctsout", "0", "", "input EIR")] = 4.0
        self.write_run(2, cells)
        code, out, _ = self.run_main()
        self.assertEqual(code, 1)
        self.assertIn("1 of 2 runs failed", out)
        self.assertIn("fixed cells: 1 of 3 differ <- fails", out)

    def test_unreadable_run_is_an_error(self):
        (self.runs / "1").mkdir(parents=True)
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("can't read run 1", err)

    def test_wrong_format_version_is_an_error(self):
        self.ref_path.write_text(self.ref_path.read_text().replace(f"format_version: {stats_io.FORMAT_VERSION}", "format_version: 1"))
        self.write_run(1, passing_run(self.ref))
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("format_version", err)


if __name__ == "__main__":
    unittest.main()
