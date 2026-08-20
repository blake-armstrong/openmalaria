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

Pin emcc **6.0.3** specifically -- this build hasn't been verified against
other versions, and GSL/Xerces cross-compilation is exactly the kind of
thing that can break silently across toolchain versions.

```sh
cd emscripten
bash deps/build-gsl.sh       # GSL 2.8 -> deps/_install/lib/libgsl.a
bash deps/build-xerces.sh    # Xerces-C 3.2.5 -> deps/_install/lib/libxerces-c.a
bash build-model.sh          # -> ../js/wasm/openMalaria.{mjs,wasm}
```

Each dependency script is idempotent (skips rebuilding if its `_install/`
output already exists; delete it to force a rebuild).

## Design

The repo's `CMakeLists.txt` is left untouched apart from one line (guarding
the `pthread` library lookup with `AND NOT EMSCRIPTEN`, since Emscripten is
single-threaded and reports `UNIX=TRUE` but has no separate pthread
library to find). Everything else needed for the wasm build lives in this
directory, driven entirely by pre-seeding the exact CMake cache variables
the repo's hand-rolled `cmake/Find{GSL,XercesC,Z,XSD}.cmake` modules would
otherwise search for themselves (`-D` flags on the `cmake` invocation in
`build-model.sh`) -- so the native build, and any future upstream commit,
never needs to know a wasm build exists.

Key decisions, in the order you'd hit them building this from scratch:

1. **Static linking only**, for both dependencies and the final module.
   Wasm has no mature dynamic-linking runtime; static linking also lets the
   linker dead-strip unused symbols (GSL's ~13MB archive contributes only a
   few KB to the final link), which matters for download size in a
   browser.
2. **GSL and Xerces-C are cross-compiled to wasm** (`deps/build-*.sh`)
   because OpenMalaria links against their compiled runtime, and scenario
   parsing happens inside the wasm module on every run.
3. **`xsd`/`xsdcxx` schema codegen runs on the host**, not cross-compiled --
   it's a code generator that emits portable C++ once at build time; only
   that generated C++ needs to compile to wasm. Its include dir must be an
   xsd-only keg (no sibling `xercesc/` tree), or the host's own Xerces
   headers get compiled against while the wasm-built archive gets linked --
   an ABI mismatch that surfaces as confusing link errors, not a version
   error.
4. **`callMain()` + MEMFS `FS`, not Embind.** The wasm module's "API" is
   the CLI's existing file-in/file-out contract (`-s scenario.xml -o
   output.txt`), replayed through an in-memory filesystem -- zero new C++
   surface, and verifiably byte-identical output to the native binary (see
   `../js/test/run.mjs`, or diff a native and wasm run of the same scenario
   with the same `iseed` yourself).
5. **`-sMODULARIZE=1` (a fresh module instance per run).** OpenMalaria's
   C++ has global static state (`sim`, `master_RNG`,
   `mon::internal::runtime`, `ModelOptions`, `InterventionManager`,
   `WithinHost::Genotypes`, `CommandLine`'s own static fields) that's set
   up once per run and never reset -- two runs must never share an
   instance. This is exactly what `../js/src/index.mjs` does: a fresh
   `createOpenMalaria()` call per `runScenario()`.
6. **Everything the module needs is embedded** (`--embed-file`): the
   inlined schema (`schema/scenario_49.xsd`, staged as
   `scenario_current.xsd`), `test/densities.csv` (always read), and
   `test/autoRegressionParameters.csv` (read only if a scenario selects the
   Empirical within-host model -- embedded unconditionally since a
   browser-generated scenario can't declare its resource path up front the
   way a native CLI invocation could). Zero runtime network dependency;
   works offline, in Node, or on any static host.
7. **`-fexceptions` on both compile and link.** Emscripten disables C++
   exceptions by default. Xerces throws real exceptions on malformed XML
   that `main()`'s own try/catch (via `model/util/errors.h`) must catch.
   Getting this flag onto the linker but not the compiler produces no
   build error at all -- just every exception silently propagating
   uncaught past `callMain()` as an opaque `CppException` at runtime
   instead of being converted to the numeric exit code the JS wrapper
   expects. (Found the hard way during bring-up here -- see git history.)
8. **`-DCMAKE_BUILD_TYPE=Release`** strips `assert()` calls (one directly
   in `main.cpp`, more reachable in model code) that would otherwise abort
   the whole wasm heap on a browser-supplied edge case instead of
   surfacing a catchable error.
9. **`.js` → `.mjs` rename.** Emscripten's `EXPORT_ES6` output is still
   literally named `openMalaria.js`; Node resolves `.mjs` as ESM
   unambiguously regardless of the nearest `package.json`'s `"type"`
   field, sidestepping a common "why is this loading as CommonJS"
   consumer-side failure.

See the full flag-by-flag rationale as comments in `build-model.sh` itself,
and `../js/API.md` for the resulting JS-facing contract (including why runs
must happen in a Web Worker in a browser, and why no COOP/COEP headers are
needed).

## Known gotcha collected during bring-up

If you hit a **runtime-only** failure where errors that should be caught
and reported cleanly instead surface as an unhandled `CppException`
(rather than a schema/model error message), check that `-fexceptions` is
present in *both* `CMAKE_CXX_FLAGS` and the linker flags -- it will build
and link successfully either way, so this doesn't fail loudly.

If you're running under Node and see the whole process exit with a
scenario's error code even though your own code caught the rejected
Promise: this is `../js/src/index.mjs` restoring `process.exitCode`, not a
build issue -- Emscripten's Node target sets that process-global as a side
effect of every `callMain()` call (success or failure), which is
meaningless for a library and gets undone after every call.
