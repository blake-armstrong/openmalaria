# openmalaria (npm) API

`openmalaria` (npm) runs the OpenMalaria malaria simulation model entirely
client-side, compiled to WebAssembly via Emscripten. It exposes one operation,
`runScenario`, which mirrors the native CLI's file-in/file-out design
(`openMalaria -s scenario.xml -o output.txt`) replayed through an in-memory
filesystem inside the wasm module.

## `runScenario(scenarioXml, options?)`

```js
import { runScenario, OpenMalariaError } from 'openmalaria';

const result = await runScenario(scenarioXmlString);
console.log(result.output); // tab-separated survey data
```

- `scenarioXml` (`string`, required): the full contents of a scenario XML
  document, valid against the embedded schema (currently schema version 50).
- `options.validateOnly` (`boolean`): parse, schema-validate, and build the
  model graph, then return without running the timestep loop. Much cheaper than
  a full run (milliseconds vs. seconds/minutes depending on scenario size).This
  is useful for validating a scenario as a user edits it in a browser form,
  before committing to a full run. `result.output` is omitted (no `output.txt`
  is written in this mode); structural/parameter errors are still surfaced by
  rejecting with `OpenMalariaError`.
- `options.onStdout` / `options.onStderr` (`(line: string) => void`): called
  once per line of stdout/stderr produced during the run.

**Errors.** A synchronous `TypeError` is thrown immediately if `scenarioXml` is
not a non-empty string (a caller error, distinct from a simulation failure). If
the simulation itself fails (malformed XML, a schema violation, an unsatisfiable
parameter combination, etc.), the returned Promise rejects with an
`OpenMalariaError`:

```ts
class OpenMalariaError extends Error {
    code: number;      // process exit code -- see model/util/errors.h
    stderr: string[];  // all stderr lines emitted before the failure
}
```

Common `code` values (full list: `Error::ErrorCodes` in `model/util/errors.h`):
`64` = generic error, `67` = XSD/schema error, `68` = invalid scenario content,
`70` = checkpoint error.

**Result shape:**

```ts
interface RunResult {
    output?: string;    // output.txt contents; omitted if validateOnly
    cts?: string;       // ctsout.txt contents; only present if the
                         // scenario's <monitoring> declares <continuous>
    warnings: string[]; // every stderr line emitted (progress + warnings,
                         // not necessarily errors)
}
```

## Output format

`output` is tab-separated, **no header row**, one row per
`(survey, group, measureId, value)`:

```
1	0	0	979
1	0	1	220
1	1	0	977
```

- `survey`: 1-based survey index (matches the scenario's `<surveys>` list).
- `group`: a packed integer encoding age group, cohort, and
  genotype/species/drug dimensions (see `writeMeasure()` in
  `model/mon/Monitoring.cpp` for the exact packing per measure; it varies:
  per-species measures pack differently than per-age-group ones). Do not assume
  a single fixed decomposition without checking the measure's `dims` in
  `model/mon/OutMeasures.h`.
- `measureId`: identifies which reported quantity this row is (see below).
- `value`: the reported value (integer count or floating-point, depending on the
  measure).

`cts`, when present, is a wide TSV with a header row, one column per
continuous-output series, one row per report interval.

### Measure IDs

The full, authoritative catalog is `model/mon/OutMeasures.h` in the OpenMalaria
source (each entry: name, numeric id, whether it's a floating-point value, and
its dimensions). A few commonly used ones:

| id | name | meaning |
|----|------|---------|
| 0  | nHost | number of hosts |
| 1  | nInfect | number of infected hosts |
| 3  | nPatent | number of patent (detectable) infections |
| 14 | nUncomp | number of uncomplicated episodes |
| 15 | nSevere | number of severe episodes |
| 35 | inputEIR | input entomological inoculation rate |
| 36 | simulatedEIR | simulated entomological inoculation rate |

## Running in a browser

Simulation runs are CPU-bound and synchronous inside the wasm call. Depending on
population size and run length, this can take anywhere from milliseconds
(`validateOnly`) to tens of seconds. **Run `runScenario` in a Web Worker**, not
the main thread, so the page stays responsive.

- Serve `openMalaria.wasm` with `Content-Type: application/wasm`.
- No COOP/COEP response headers are required. The module is deliberately
  single-threaded (no `SharedArrayBuffer`/wasm-threads dependency), so it works
  on any static host without special CORS isolation.
- The module has **zero runtime network dependency**: the schema
  (`scenario_current.xsd`, currently schema version 50) and the reference data
  files it needs (`densities.csv`, `autoRegressionParameters.csv`) are embedded
  into the wasm binary at build time, not fetched.
- Bundlers: `src/index.mjs` imports `../wasm/openMalaria.mjs` by relative path,
  which in turn locates `openMalaria.wasm` next to itself by default. Make sure
  your bundler serves `wasm/openMalaria.wasm` as a static asset rather than
  trying to process it as a JS module.

## Concurrency and isolation

Every `runScenario()` call instantiates a **fresh** wasm module. OpenMalaria's
C++ uses global static/singleton state (`sim`, `master_RNG`,
`mon::internal::runtime`, `ModelOptions`, `InterventionManager`,
`WithinHost::Genotypes`, `CommandLine`'s own static fields) that's initialised
once per run and never reset. Reusing one module instance across two runs would
leak state between them. A fresh instance per call means concurrent
`runScenario()` calls (e.g., one per Worker) are safe and fully isolated from
each other.

## What this package does not do

- **No scenario XML authoring/generation.** This package only runs a scenario
  you already have; building one (from a form, a template, or by hand) is a
  separate concern (e.g., ChatBot project). See the OpenMalaria schema docs and
  the `test/*.xml` files in the main repo for examples.
- **No checkpoint/resume support.** OpenMalaria's native checkpointing is an
  inherently multi-process workflow (write checkpoint, exit, re-invoke to
  resume) that doesn't fit a single-instance-per-run browser model; it isn't
  exposed here.
- **No streaming/partial results.** `runScenario` returns the complete output
  only after the run finishes. At the moment, there's no mid-run progress data
  beyond the stderr lines surfaced via `onStderr`.
