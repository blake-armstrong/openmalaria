#!/usr/bin/env bash
# Cross-compile the OpenMalaria model to a single WebAssembly module.
#
# the CMakeLists.txt hand-rolled cmake/Find*.cmake
# modules do plain hardcoded-path find_library/find_path calls with no wasm
# awareness. Instead of patching them, this script pre-seeds the exact cache
# variable names each module would otherwise set itself via -D flags on the
# initial `cmake` invocation; CMake skips a find_* call once the variable
# already has a cache value, so the native build (and any future upstream
# commit) never needs to know a wasm build exists.
#
set -euo pipefail # <- kill immediately upon error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEPS_INSTALL="${SCRIPT_DIR}/deps/_install"
BUILD_DIR="${SCRIPT_DIR}/build"
EMBED_DIR="${SCRIPT_DIR}/_embed"
JS_WASM_DIR="${REPO_DIR}/js/wasm"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "error: emcmake not on PATH: run 'source ~/git/emsdk/emsdk_env.sh' first" >&2
  exit 1
fi

if [ ! -f "${DEPS_INSTALL}/lib/libgsl.a" ] || [ ! -f "${DEPS_INSTALL}/lib/libxerces-c.a" ]; then
  echo "error: build dependencies first: bash deps/build-gsl.sh && bash deps/build-xerces.sh" >&2
  exit 1
fi

# ---  Locate the host xsd/xsdcxx compiler  ---
#
# xsd/xsdcxx is a code generator. Given an .xsd it
# emits portable C++ that itself calls the Xerces-C API, once, at
# build time. The C++ then gets compiled to wasm along with
# everything else, so the generator tool itself never runs in the browser.
# The fast native xsd binary is pointed at wasm-built Xerces for the
# code it emits to link against
XSD_EXECUTABLE="$(command -v xsdcxx || command -v xsd || true)"
if [ -z "${XSD_EXECUTABLE}" ]; then
  echo "error: no xsd/xsdcxx compiler found on PATH (brew install xsd)" >&2
  exit 1
fi

# xsd's generated code needs xsd's own runtime headers (xsd/cxx/parser/elements.hxx).
# If XSD_INCLUDE_DIRS also contains a xercesc/ header tree (e.g., a full Xerces
# install that xsd depends on), the compiler picks up those (possibly
# different-version) Xerces headers while actually linking against our
# wasm-built Xerces archive which manifests as an ABI mismatch surfacing as confusing link
# errors, not a clean version error. Homebrew keeps xsd's own headers in a
# keg separate from its xerces-c dependency keg, so prefer that if present.
if command -v brew >/dev/null 2>&1 && brew --prefix xsd >/dev/null 2>&1; then
  XSD_INCLUDE_DIRS="$(brew --prefix xsd)/include"
else
  XSD_INCLUDE_DIRS="$(dirname "$(dirname "${XSD_EXECUTABLE}")")/include"
fi
if [ -d "${XSD_INCLUDE_DIRS}/xercesc" ]; then
  echo "error: ${XSD_INCLUDE_DIRS} also contains a xercesc/ tree -- this will shadow the" >&2
  echo "       wasm-built Xerces headers with the host's. Point at an xsd-only include dir." >&2
  exit 1
fi

# TODO: test on other OS

# ---  Locate zlib  ---
#
# zlib is one of a handful of common C libraries (zlib, libpng, SDL, ...)
# Emscripten ships as a built-in "port": it builds and caches it for you on
# first reference rather than needing a hand-rolled build script here.
EM_CACHE="$(em-config CACHE)"
if [ ! -f "${EM_CACHE}/sysroot/lib/wasm32-emscripten/libz.a" ]; then
  echo "> Building the Emscripten zlib port"
  embuilder build zlib
fi
Z_INCLUDE_DIRS="${EM_CACHE}/sysroot/include"
Z_LIBRARIES="${EM_CACHE}/sysroot/lib/wasm32-emscripten/libz.a"

# ---  Stage embedded runtime data files  ---
#
# --embed-file needs real files on disk at link time. Emscripten's
# file-packager reads them and bakes their bytes into the module's data
# segment, mounted into MEMFS at whatever virtual path follows '@'.
# The resulting module has zero runtime network dependency and works identically in Node, an offline
# browser tab, or any static host. All three files below are embedded even
# though autoRegressionParameters.csv is only read if a scenario selects the
# empirical within-host model. The file is small enough that embedding  seems reasonable.
echo "> Staging embedded files"
mkdir -p "${EMBED_DIR}"
cp "${REPO_DIR}/schema/scenario_50.xsd" "${EMBED_DIR}/scenario_current.xsd"
cp "${REPO_DIR}/test/densities.csv" "${EMBED_DIR}/densities.csv"
cp "${REPO_DIR}/test/autoRegressionParameters.csv" "${EMBED_DIR}/autoRegressionParameters.csv"

# ---  Configure  ---
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Linker flags to shape the JS-facing API:
#
# -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createOpenMalaria
#   Without MODULARIZE, Emscripten splats a single global Module object into
#   whatever scope loads it. MODULARIZE instead wraps the module in an async
#   factory function returning a fresh, independent instance each call. 
#   OpenMalaria has global static state (sim, master_RNG,
#   mon::internal::runtime, ModelOptions, InterventionManager, Genotypes,
#   CommandLine's own static fields) that's set up once per run and never
#   reset, so two scenario runs in one browser tab must not share an
#   instance.
#
# -sEXPORTED_RUNTIME_METHODS=FS,callMain
#   The chosen alternative to Embind: FS (the MEMFS filesystem object) and
#   callMain (invoke the existing, unmodified main(argc, argv), get its exit
#   code back, without auto-running). The wasm build's "API" is literally
#   the CLI's existing file-in/file-out contract, replayed through an
#   in-memory filesystem. Trade-off: a whole-XML-in/whole-TSV-out API, no
#   fine-grained/streaming calls (the CLI is already designed like this anyway).
#
# -sINVOKE_RUN=0
#   Emscripten's default auto-invokes main() the instant the module finishes
#   loading. The JS wrapper needs to write the scenario XML into MEMFS
#   before main() runs (main reads it as a -s argument), so auto-invocation
#   must be suppressed and main called manually via callMain() once the
#   file is staged.
#
# -sEXIT_RUNTIME=0
#   A real CLI process calls exit() after main() returns, tearing down
#   stdio. Here callMain() is just a function call, not a process exit.
#   The JS side still needs to read files back out of FS afterward.
#
# -sFORCE_FILESYSTEM=1
#   The C++ side never itself calls anything that would make the build
#   decide it needs the fuller MEMFS filesystem code (dead-code elimination
#   would otherwise strip it), but the JS wrapper uses Module.FS.writeFile/
#   readFile directly.
#
# -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
#   Wasm linear memory is fixed-size at compile time unless grown. Scenario
#   population sizes and run lengths vary enormously; a fixed cap would
#   either waste memory on small scenarios or hard-crash on large ones.
#
# -sSTACK_SIZE=1MB
#   Bumped from the default: a C++ simulator has non-trivial call depth
#   (recursive XML DOM traversal during parsing, nested simulation logic).
#   A stack overflow here would otherwise manifest as an opaque crash.
#
# -sASSERTIONS=1
#   Emscripten's runtime sanity checks and clearer failure messages.
#
# -fexceptions
#   Must match the flag Xerces was built with (see deps/build-xerces.sh).
#   Emscripten disables C++ exceptions by default, and both Xerces and
#   OpenMalaria's own util/errors.h machinery rely on C++ exceptions
#   propagating across this link.

LINKER_FLAGS="-fexceptions"
LINKER_FLAGS="${LINKER_FLAGS} -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createOpenMalaria"
LINKER_FLAGS="${LINKER_FLAGS} -sEXPORTED_RUNTIME_METHODS=FS,callMain"
LINKER_FLAGS="${LINKER_FLAGS} -sINVOKE_RUN=0 -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1"
LINKER_FLAGS="${LINKER_FLAGS} -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB"
LINKER_FLAGS="${LINKER_FLAGS} -sASSERTIONS=1"
LINKER_FLAGS="${LINKER_FLAGS} --embed-file ${EMBED_DIR}/scenario_current.xsd@/work/scenario_current.xsd"
LINKER_FLAGS="${LINKER_FLAGS} --embed-file ${EMBED_DIR}/densities.csv@/work/densities.csv"
LINKER_FLAGS="${LINKER_FLAGS} --embed-file ${EMBED_DIR}/autoRegressionParameters.csv@/work/autoRegressionParameters.csv"

echo "> Configuring OpenMalaria for wasm32"
# -fexceptions must be passed to the *compiler* here, not just the linker:
# Emscripten's exception support affects codegen (landing pads/catch
# dispatch), so main.cpp's own try/catch around XSD/xerces/OpenMalaria
# exceptions only works if main.cpp itself was compiled with -fexceptions. 
# Matching just the linker flags produces no link error, only a silent
# runtime failure where every exception propagates uncaught past callMain()
# as a raw CppException instead of being handled and converted to an exit
# code the JS wrapper can inspect.
emcmake cmake "${REPO_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOM_CXXTEST_ENABLE=OFF \
  -DOM_BOXTEST_ENABLE=OFF \
  -DCMAKE_CXX_FLAGS="-fexceptions" \
  -DGSL_INCLUDE_DIR="${DEPS_INSTALL}/include" \
  -DGSL_INCLUDE_DIR2="${DEPS_INSTALL}/include/gsl" \
  -DGSL_LIB_OPT="${DEPS_INSTALL}/lib/libgsl.a" \
  -DGSL_CBLAS_LIB_OPT="${DEPS_INSTALL}/lib/libgslcblas.a" \
  -DXERCESC_INCLUDE_DIRS="${DEPS_INSTALL}/include" \
  -DXERCESC_LIB_OPT="${DEPS_INSTALL}/lib/libxerces-c.a" \
  -DZ_INCLUDE_DIRS="${Z_INCLUDE_DIRS}" \
  -DZ_LIBRARIES="${Z_LIBRARIES}" \
  -DXSD_EXECUTABLE="${XSD_EXECUTABLE}" \
  -DXSD_INCLUDE_DIRS="${XSD_INCLUDE_DIRS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LINKER_FLAGS}"

echo "> Building openMalaria (wasm)"
emmake cmake --build . --target openMalaria -- -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

# ---  .js -> .mjs rename  ---
#
# Emscripten's EXPORT_ES6 output is still literally named openMalaria.js
# regardless of the flag. Node's module resolution is extension-based: .mjs
# is unambiguously ESM no matter what; .js depends on the nearest
# package.json's "type" field, a common source of "why is this loading as
# CommonJS" confusion in consuming projects. The wasm's sibling reference
# inside the glue file is a relative filename, so the rename doesn't break
# the loader.
echo "> Renaming openMalaria.js -> openMalaria.mjs and copying to js/wasm/"
mkdir -p "${JS_WASM_DIR}"
cp "${BUILD_DIR}/openMalaria.js" "${JS_WASM_DIR}/openMalaria.mjs"
cp "${BUILD_DIR}/openMalaria.wasm" "${JS_WASM_DIR}/openMalaria.wasm"

ls -la "${JS_WASM_DIR}"
echo "> Done: ${JS_WASM_DIR}/openMalaria.mjs, ${JS_WASM_DIR}/openMalaria.wasm"
