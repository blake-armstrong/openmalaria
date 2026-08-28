"""Python bindings for the OpenMalaria malaria-transmission simulator.

See README.md for usage and, importantly, the "Limitations" section: a
process may call run() at most once.
"""

from __future__ import annotations

from typing import Optional

import pandas as pd

from ._openmalaria import OpenMalariaError, _run, _version

__all__ = ["run", "version", "OpenMalariaError"]


def run(
    *,
    xml: Optional[str] = None,
    path: Optional[str] = None,
    resource_path: str = "",
    validate_only: bool = False,
    verbose: bool = False,
    progress: bool = False,
    seed: Optional[int] = None,
) -> dict:
    """Run one OpenMalaria scenario end-to-end in this process.

    Exactly one of `xml` (scenario XML content) or `path` (path to a
    scenario XML file) must be given.

    May be called AT MOST ONCE per process (see README.md "Limitations").
    For many scenarios, use one process per scenario (mpi4py with one MPI
    rank per scenario, or multiprocessing/ProcessPoolExecutor), not a loop
    calling run() repeatedly in one process.

    Returns a dict:
        "survey": pd.DataFrame with columns survey/column/measure/value,
            mirroring output.txt's own row schema exactly (column encodes
            age-group/cohort/species/genotype/drug the same way output.txt
            does).
        "continuous": pd.DataFrame with one row per reported timestep and
            one column per enabled continuous-output metric (titles taken
            from the scenario's monitoring/continuous options), or None if
            the scenario has no <continuous> monitoring configured.

    Raises OpenMalariaError on any OpenMalaria-side failure (invalid
    scenario, command-line/config error, XSD schema error).
    """
    raw = _run(
        xml=xml,
        path=path,
        resource_path=resource_path,
        validate_only=validate_only,
        verbose=verbose,
        progress=progress,
        seed=seed,
    )

    survey_df = pd.DataFrame({
        "survey": raw.survey.survey,
        "column": raw.survey.column,
        "measure": raw.survey.measure,
        "value": raw.survey.value,
    })

    continuous_df = None
    if raw.continuous.column_titles:
        continuous_df = pd.DataFrame({
            title.strip(): col
            for title, col in zip(raw.continuous.column_titles, raw.continuous.columns)
        })

    return {"survey": survey_df, "continuous": continuous_df}


def version() -> dict:
    """Return OpenMalaria's version info, equivalent to the CLI's `--version`.

    Returns a dict:
        "program_version": str, e.g. "schema-50.0"
        "schema_version": int, the scenario XML schema version scenarios
            are validated against (see model/util/DocumentLoader.h's
            SCHEMA_VERSION)
    """
    v = _version()
    return {"program_version": v.program_version, "schema_version": v.schema_version}
