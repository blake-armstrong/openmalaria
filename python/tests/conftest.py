"""Shared pytest fixtures for the openmalaria Python binding tests.

These tests run scenarios from test/ (the repo's existing black-box test
fixtures, also used by test/run.py to drive the CLI) and compare results
against test/expected/*.txt, the same golden files the CLI's own test suite
uses.

Schema lookup resolves relative to the working directory (see
model/util/DocumentLoader.h's loadScenario()/loadScenarioFromString()
comments) rather than to the scenario file's own directory, so tests chdir
into a temp directory containing a copy of scenario_current.xsd alongside
the scenario file.

Important: om.run() may be called AT MOST ONCE per process (see
README.md "Limitations"). scenario1_result below runs it exactly once per
test *session* and is shared by every test that just needs to inspect a
successful result; tests that need their own independent run (validate_only,
xml=, or deliberately calling run() twice) do so in a subprocess instead of
the pytest process itself.
"""

from __future__ import annotations

import os
import shutil
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = REPO_ROOT / "test"
EXPECTED_DIR = TEST_DIR / "expected"


def find_inlined_schema() -> Path:
    """Locate the generated scenario_current.xsd (the `inlined_xsd` CMake
    target's output)."""
    candidate = REPO_ROOT / "build" / "schema" / "scenario_current.xsd"
    if candidate.is_file():
        return candidate
    pytest.skip(
        "scenario_current.xsd not found; build the `inlined_xsd` CMake "
        "target first (e.g. `cmake --build build --target inlined_xsd`)"
    )


@pytest.fixture(scope="session")
def resource_path() -> str:
    return str(TEST_DIR)


@pytest.fixture
def scenario1_path(tmp_path: Path) -> Path:
    """A copy of test/scenario1.xml alongside scenario_current.xsd in a temp
    dir, for tests that drive their own (subprocess) run."""
    shutil.copy(find_inlined_schema(), tmp_path / "scenario_current.xsd")
    dest = tmp_path / "scenario1.xml"
    shutil.copy(TEST_DIR / "scenario1.xml", dest)
    return dest


@pytest.fixture(scope="session")
def scenario1_result(tmp_path_factory, resource_path):
    """Runs scenario1 exactly once for the whole test session and returns
    om.run()'s result dict. Every test that just needs to inspect a
    successful run's output should depend on this fixture rather than
    calling om.run() itself"""
    import openmalaria as om

    sim_dir = tmp_path_factory.mktemp("sim")
    shutil.copy(find_inlined_schema(), sim_dir / "scenario_current.xsd")
    scenario_path = sim_dir / "scenario1.xml"
    shutil.copy(TEST_DIR / "scenario1.xml", scenario_path)

    old_cwd = os.getcwd()
    os.chdir(sim_dir)
    try:
        return om.run(path=str(scenario_path), resource_path=resource_path)
    finally:
        os.chdir(old_cwd)


@pytest.fixture
def expected_output1() -> Path:
    return EXPECTED_DIR / "output1.txt"


@pytest.fixture
def expected_ctsout1() -> Path:
    return EXPECTED_DIR / "ctsout1.txt"
