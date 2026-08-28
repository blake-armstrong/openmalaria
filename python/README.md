# openmalaria (Python bindings)

Python bindings for [OpenMalaria](https://github.com/SwissTPH/openmalaria),
built with [nanobind](https://github.com/wjakob/nanobind). Runs a scenario
in-process and returns pandas DataFrames directly. No `output.txt`/ `ctsout.txt`
files involved.

## Install

```sh
pip install ./python
```

(editable, for development: `pip install -e ./python`)

## Usage

```python
import openmalaria as om

result = om.run(path="scenario.xml")
result["survey"]       # pd.DataFrame: survey, column, measure, value
result["continuous"]   # pd.DataFrame (one row per timestep) or None
```

Or pass scenario XML content directly instead of a file path:

```python
result = om.run(xml=scenario_xml_string, resource_path="/path/to/resources")
```

NB: schema lookup resolves relative to the current working directory for both
`path=` and `xml=` (not relative to the scenario file's own directory, if using
`path=`). Run from a directory containing `scenario_current.xsd`, or otherwise
ensure the schema is discoverable from the working directory.

`om.run()` also accepts `validate_only=True` (parse/validate the scenario and
stop before any timestep evolution. This acts as a cheap sanity check,
equivalent to the CLI's `--validate-only`), `seed=<int>` (override the
scenario's `@iseed`), and `verbose=True`/`progress=True` (equivalent to the CLI
flags of the same name).

### `survey` DataFrame schema

Mirrors `output.txt`'s own row schema exactly: `survey` (1-based survey number),
`column` (encodes age-group/cohort/species/genotype/drug the same way
`output.txt` does), `measure` (the OutMeasure id), `value`.

### `continuous` DataFrame schema

One row per reported timestep, one column per enabled `monitoring/continuous`
metric (column names taken from the scenario's own metric titles). `None` if the
scenario has no `<continuous>` monitoring configured.

## Version info

```python
>>> om.version()
{'program_version': 'schema-50.0', 'schema_version': 50}
```

Equivalent to the CLI's `openMalaria --version`.

## Parallelism (mpi4py)

```python
from mpi4py import MPI
import openmalaria as om

comm = MPI.COMM_WORLD
scenario_paths = [...]  # one per rank, or distribute a longer list up front

result = om.run(path=scenario_paths[comm.rank])
```

Each MPI rank runs exactly one scenario per process (see Limitations below). Pin
ranks to individual cores via your launcher, e.g.
`mpirun --bind-to core -np N python script.py`.

## Limitations

**`om.run()` may be called at most once per process.** OpenMalaria's C++ core
keeps several pieces of state as process-global statics that are populated once
and never reset:

- `interventions::InterventionManager` -- append-only; throws on a second run
  reusing any `<component id="...">` name, and silently duplicates/accumulates
  timed and continuous deployments otherwise.
- `WithinHost::Genotypes` -- allele/frequency maps are insert-only, never
  cleared.
- `mon::internal::runtime.conditions` -- push_back-only, never cleared.
- `util::CommandLine` -- several statics throw if set a second time via
  `parse()`; others are simply never reset.

This is the same conclusion already reached and documented by this repo's
Emscripten/WASM/JS binding (`js/src/index.mjs`, `js/API.md` "Concurrency and
isolation"), "fresh module instance per call" there, "fresh OS process per call"
here. For N scenarios, use one process per scenario (`mpi4py`, one MPI rank per
scenario; or Python `multiprocessing`/`concurrent.futures.ProcessPoolExecutor`),
not a loop calling `run()` repeatedly in one process.

**No checkpoint/resume support.** Checkpointing (`-c`/`--checkpoint-file` on the
CLI) remains a CLI-only feature; `om.run()` exposes no checkpoint parameters.

**CPU-core pinning is the caller's responsibility.** OpenMalaria's simulation
engine has no internal threading (no OpenMP, no `std::thread` anywhere in the
C++ core), so single-core execution is achieved externally:
`mpirun --bind-to core -np N python script.py`, or
`os.sched_setaffinity(0, {core_id})` (Linux) at the start of a worker process.
