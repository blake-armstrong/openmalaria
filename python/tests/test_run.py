from __future__ import annotations

import subprocess
import sys
import textwrap
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

import openmalaria as om

from conftest import REPO_ROOT


def _read_expected_output(path: Path) -> pd.DataFrame:
    return pd.read_csv(path, sep="\t", header=None, names=["survey", "column", "measure", "value"])


def _read_expected_ctsout(path: Path) -> pd.DataFrame:
    # First line is a LiveGraph delimiter marker ("##\t##"), not a header;
    # the real header is the second line. The in-memory capture path skips
    # that marker line entirely (see model/mon/Continuous.cpp's initCapture).
    return pd.read_csv(path, sep="\t", skiprows=1)


def test_survey_schema_and_values(scenario1_result, expected_output1):
    survey = scenario1_result["survey"]

    assert list(survey.columns) == ["survey", "column", "measure", "value"]
    assert survey["survey"].dtype.kind == "i"
    assert survey["column"].dtype.kind == "i"
    assert survey["measure"].dtype.kind == "i"
    assert survey["value"].dtype.kind == "f"
    assert len(survey) > 0

    expected = _read_expected_output(expected_output1)
    assert survey.shape == expected.shape
    assert (survey[["survey", "column", "measure"]].to_numpy() == expected[["survey", "column", "measure"]].to_numpy()).all()
    # rtol/atol looser than the CLI's own file-vs-file 1e-6 (util/compareOutput.py):
    # expected_output1.txt was serialized with ~6 significant digits (default
    # ostream precision), while `survey["value"]` here retains full double
    # precision.
    assert np.allclose(survey["value"].to_numpy(), expected["value"].to_numpy(), rtol=1e-5, atol=1e-5)


def test_continuous_schema_and_values(scenario1_result, expected_ctsout1):
    continuous = scenario1_result["continuous"]
    assert continuous is not None

    expected = _read_expected_ctsout(expected_ctsout1)
    assert list(continuous.columns) == list(expected.columns)
    assert continuous.shape == expected.shape
    assert np.allclose(
        continuous.to_numpy(dtype=float), expected.to_numpy(dtype=float), rtol=1e-6, atol=1e-6, equal_nan=True
    )


def test_missing_scenario_raises(tmp_path):
    # Deliberately resource_path="" (default): a non-empty resource_path
    # would consume the one-per-process budget on util::CommandLine's
    # resourcePath static (see conftest.py's module docstring), and this
    # test doesn't need CSV resources to demonstrate the "file not found"
    # error path.
    with pytest.raises(om.OpenMalariaError):
        om.run(path=str(tmp_path / "does_not_exist.xml"))


def test_xml_and_path_both_given_raises():
    with pytest.raises((TypeError, ValueError)):
        om.run(xml="<x/>", path="dummy.xml")


def test_neither_xml_nor_path_raises():
    with pytest.raises((TypeError, ValueError)):
        om.run()


def _run_in_subprocess(script: str, cwd: Path) -> subprocess.CompletedProcess:
    """Runs `script` in a fresh Python process with this package importable.
    Used for anything that needs its own independent om.run() call -- see
    conftest.py's module docstring on the one-run-per-process constraint.
    """
    python_dir = str(REPO_ROOT / "python")
    preamble = f"import sys; sys.path.insert(0, {python_dir!r})\n"
    return subprocess.run(
        [sys.executable, "-c", preamble + script], cwd=cwd, capture_output=True, text=True
    )


def test_validate_only_is_fast_and_empty(scenario1_path, resource_path):
    script = textwrap.dedent(f"""
        import openmalaria as om
        r = om.run(path={str(scenario1_path)!r}, resource_path={resource_path!r}, validate_only=True)
        assert r["survey"].shape == (0, 4), r["survey"].shape
        assert r["continuous"] is None, r["continuous"]
        print("OK")
    """)
    proc = _run_in_subprocess(script, cwd=scenario1_path.parent)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "OK" in proc.stdout


def test_xml_matches_path(scenario1_path, resource_path):
    script = textwrap.dedent(f"""
        import openmalaria as om
        xml_content = open({str(scenario1_path)!r}).read()
        r = om.run(xml=xml_content, resource_path={resource_path!r})
        assert r["survey"].shape[0] > 0
        assert list(r["survey"].columns) == ["survey", "column", "measure", "value"]
        print("SHAPE", r["survey"].shape[0])
    """)
    proc = _run_in_subprocess(script, cwd=scenario1_path.parent)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "SHAPE" in proc.stdout


def test_second_run_in_same_process_is_unsupported(scenario1_path, resource_path):
    """om.run() may be called at most once per process (see README.md
    "Limitations"). This test pins down which guard fires first so a regression
    here is caught: as of writing, util::CommandLine's `resourcePath` static 
    throws on a second `--resource-path` (see model/util/CommandLine.cpp), since both
    calls below pass resource_path=.
    """
    script = textwrap.dedent(f"""
        import openmalaria as om
        om.run(path={str(scenario1_path)!r}, resource_path={resource_path!r})
        try:
            om.run(path={str(scenario1_path)!r}, resource_path={resource_path!r})
        except om.OpenMalariaError as e:
            print("SECOND_RUN_FAILED:", e)
        else:
            print("SECOND_RUN_SUCCEEDED_UNEXPECTEDLY")
    """)
    proc = _run_in_subprocess(script, cwd=scenario1_path.parent)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "SECOND_RUN_FAILED" in proc.stdout, proc.stdout
    assert "may only be given once" in proc.stdout
