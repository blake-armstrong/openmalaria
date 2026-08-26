# OpenMalaria → WebAssembly build

Compiles the OpenMalaria simulation model (`model/`) into a single
self-contained WebAssembly module, consumed by the `openmalaria-wasm` npm
package in `../js/`.

## Build

```sh
git clone https://github.com/emscripten-core/emsdk
cd emsdk && ./emsdk install 6.0.3 && ./emsdk activate 6.0.3
source ./emsdk_env.sh
```

emcc **6.0.3** is pinned specifically. This build hasn't been verified against
other versions. GSL/Xerces cross-compilation could break silently across
toolchain versions.

```sh
cd emscripten
bash deps/build-gsl.sh       # GSL 2.8 -> deps/_install/lib/libgsl.a
bash deps/build-xerces.sh    # Xerces-C 3.2.5 -> deps/_install/lib/libxerces-c.a
bash build-model.sh          # -> ../js/wasm/openMalaria.{mjs,wasm}
```

Each dependency script is idempotent (skips rebuilding if its `_install/` output
already exists; delete it to force a rebuild).

## Design

`CMakeLists.txt`: the `pthread` library lookup is guarded with
`AND NOT EMSCRIPTEN`, since Emscripten is single-threaded and reports
`UNIX=TRUE` but has no separate pthread library to find. The wasm build lives in
this directory, driven entirely by pre-seeding the exact CMake cache variables
the repo's hand-rolled `cmake/Find{GSL,XercesC,Z,XSD}.cmake` modules would
otherwise search for themselves (`-D` flags on the `cmake` invocation in
`build-model.sh`).

Build & design decisions:

1. **Static linking only**, for both dependencies and the final module. Wasm has
   no mature dynamic-linking runtime; static linking also lets the linker
   dead-strip unused symbols (GSL's ~13MB archive contributes only a few KB to
   the final link), which matters for download size in a browser.
1. **GSL and Xerces-C are cross-compiled to wasm** (`deps/build-*.sh`) because
   OpenMalaria links against their compiled runtime, and scenario parsing
   happens inside the wasm module on every run.
1. **`xsd`/`xsdcxx` schema codegen runs on the host**. The portable C++ emitted
   at build time is compile to wasm. Its include dir must be an xsd-only keg (no
   sibling `xercesc/` tree), or the host's own Xerces headers get compiled
   against while the wasm-built archive gets linked.
1. **`callMain()` + MEMFS `FS`, not Embind.** The wasm module's "API" is the
   CLI's existing file-in/file-out contract (`-s scenario.xml -o output.txt`),
   replayed through an in-memory filesystem. See `../js/test/run.mjs`, or diff a
   native and wasm run of the same scenario with the same `iseed` to ensure
   consistency.
1. **`-sMODULARIZE=1` (a fresh module instance per run).** The global static
   state (`sim`, `master_RNG`, `mon::internal::runtime`, `ModelOptions`,
   `InterventionManager`, `WithinHost::Genotypes`, `CommandLine`'s own static
   fields) is set up once per run and never reset. -- two runs must never share
   an instance. `../js/src/index.mjs` does a fresh `createOpenMalaria()` call
   per `runScenario()` to ensure multiple runs do not share an instance. **NB:
   This is a workaround to prevent significant modifications to the codebase.
   However, this leaves the codebase vulnerable to subtle errors. We should
   explicitly clear and free memory**.
1. **Everything the module needs is embedded** (`--embed-file`): the inlined
   schema (`schema/scenario_XX.xsd`, staged as `scenario_current.xsd`),
   `test/densities.csv` (always read), and `test/autoRegressionParameters.csv`
   (read only if a scenario selects the Empirical within-host model and is
   embedded unconditionally). Zero runtime network dependency; works offline, in
   Node, or on any static host.
1. **`-fexceptions` on both compile and link.** Emscripten disables C++
   exceptions by default. Xerces throws real exceptions on malformed XML that
   `main()`'s own try/catch (via `model/util/errors.h`) must catch. Getting this
   flag onto the linker but not the compiler produces no build error. Instead,
   every exception silently propagating uncaught past `callMain()` is identified
   as `CppException` at runtime instead of being converted to the numeric exit
   code the JS wrapper expects.
1. **`-DCMAKE_BUILD_TYPE=Release`** strips `assert()` calls (one directly in
   `main.cpp`, more reachable in model code) that would otherwise abort the
   whole wasm heap on a browser-supplied edge case instead of surfacing a
   catchable error.
1. **`.js` → `.mjs` rename.** Emscripten's `EXPORT_ES6` output is still
   literally named `openMalaria.js`; Node resolves `.mjs` as ESM unambiguously
   regardless of the nearest `package.json`'s `"type"` field.

See the full flag-by-flag rationale as comments in `build-model.sh` itself, and
`../js/API.md` for the resulting JS-facing contract (including why runs must
happen in a Web Worker in a browser, and why no COOP/COEP headers are needed).

## Issues during testing

If you hit a **runtime-only** failure where errors that should be caught and
reported cleanly instead surface as an unhandled `CppException` (rather than a
schema/model error message), check that `-fexceptions` is present in *both*
`CMAKE_CXX_FLAGS` and the linker flags.

If you're running under Node and see the whole process exit with a scenario's
error code even though your own code caught the rejected Promise, this is
`../js/src/index.mjs` restoring `process.exitCode`. Emscripten's Node target
sets that process-global as a side effect of every `callMain()` call (success or
failure), which is meaningless for a library and gets undone after every call.
