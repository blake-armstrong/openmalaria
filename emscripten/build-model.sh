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

# Stage only the xsd/ header subtree so the cross-compiler never sees the rest of the host's /usr/include (glibc, host xercesc/).
if command -v brew >/dev/null 2>&1 && brew --prefix xsd >/dev/null 2>&1; then
  XSD_HOST_INCLUDE_DIRS="$(brew --prefix xsd)/include"
else
  XSD_HOST_INCLUDE_DIRS="$(dirname "$(dirname "${XSD_EXECUTABLE}")")/include"
fi
if [ ! -d "${XSD_HOST_INCLUDE_DIRS}/xsd" ]; then
  echo "error: no xsd/ header tree found under ${XSD_HOST_INCLUDE_DIRS}" >&2
  exit 1
fi

XSD_STAGE_DIR="${SCRIPT_DIR}/_xsd_include"
rm -rf "${XSD_STAGE_DIR}"
mkdir -p "${XSD_STAGE_DIR}"
cp -R "${XSD_HOST_INCLUDE_DIRS}/xsd" "${XSD_STAGE_DIR}/xsd"
XSD_INCLUDE_DIRS="${XSD_STAGE_DIR}"

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

# The JS-facing linker flags (MODULARIZE, EXPORTED_RUNTIME_METHODS,
# --embed-file, etc.) live in the root CMakeLists.txt now, behind the
# OM_BUILD_WASM option (see the "WASM/Emscripten linker configuration"
# section there for the full flag-by-flag rationale) -- this script only
# needs to turn that option on and tell it where the staged embed files are.

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
  -DOM_BUILD_WASM=ON \
  -DOM_WASM_EMBED_DIR="${EMBED_DIR}" \
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
  -DXSD_INCLUDE_DIRS="${XSD_INCLUDE_DIRS}"

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
