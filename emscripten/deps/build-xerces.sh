#!/usr/bin/env bash
# Cross-compile Xerces-C 3.2.5 to a static wasm32 library for the OpenMalaria
# Emscripten build. XMl input is validated via xsd/xsdcxx, which
# generates C++ parser classes from schema/*.xsd at build time (on the host,
# see build-model.sh), but that generated code sits on top of the real
# Xerces-C DOM/SAX runtime to do the actual parsing at run time. Since
# scenario parsing happens inside the wasm module on every run, that runtime
# has to be compiled to wasm too.
set -euo pipefail

DEPS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${DEPS_DIR}/_work"
INSTALL_DIR="${DEPS_DIR}/_install"

XERCES_VERSION=3.2.5
XERCES_TARBALL="xerces-c-${XERCES_VERSION}.tar.gz"
XERCES_URL="https://archive.apache.org/dist/xerces/c/3/sources/${XERCES_TARBALL}"
XERCES_SRC_DIR="${WORK_DIR}/xerces-c-${XERCES_VERSION}"
XERCES_BUILD_DIR="${WORK_DIR}/xerces-c-${XERCES_VERSION}-build"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "error: emcmake not on PATH -- run 'source ~/git/emsdk/emsdk_env.sh' first" >&2
  exit 1
fi

mkdir -p "${WORK_DIR}" "${INSTALL_DIR}"

if [ ! -f "${WORK_DIR}/${XERCES_TARBALL}" ]; then
  echo "> Downloading Xerces-C ${XERCES_VERSION}"
  curl -fsSL -o "${WORK_DIR}/${XERCES_TARBALL}" "${XERCES_URL}"
fi

if [ ! -d "${XERCES_SRC_DIR}" ]; then
  echo "> Extracting Xerces-C ${XERCES_VERSION}"
  tar -xzf "${WORK_DIR}/${XERCES_TARBALL}" -C "${WORK_DIR}"
fi

if [ -f "${INSTALL_DIR}/lib/libxerces-c.a" ]; then
  echo "> Xerces-C already built at ${INSTALL_DIR}, skipping (delete ${INSTALL_DIR} to force a rebuild)"
  exit 0
fi

mkdir -p "${XERCES_BUILD_DIR}"
cd "${XERCES_BUILD_DIR}"

echo "> Configuring Xerces-C for wasm32"
# network=OFF             -- no real socket layer in the wasm sandbox the way Xerces
#                            expects, and OpenMalaria's schema is always local/
#                            embedded (never fetched by URL)
# transcoder=gnuiconv     -- iconv (via emscripten libc) instead of the default
#                            ICU, which is many MB for what's essentially always
#                            UTF-8 scenario XML here.
# message-loader=inmemory -- compile the error/exception message catalog
#                            straight into the binary instead of loading it from an
#                            external locale file at runtime
# threads=OFF             -- this build is deliberately single-threaded (real wasm
#                            threads need SharedArrayBuffer, which needs COOP/COEP
#                            response headers). Exactly one scenario is ever parsed 
#                            per module instance, so thread-safety is not relevent
# -fexceptions            -- Emscripten disables C++ exceptions by default. Xerces
#                            throws real exceptions on malformed XML that
#                            OpenMalaria's util/errors.h machinery catches; without
#                            this flag they'd silently fail to propagate across the
#                            link
emcmake cmake "${XERCES_SRC_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DBUILD_SHARED_LIBS=OFF \
  -Dnetwork=OFF \
  -Dtranscoder=gnuiconv \
  -Dmessage-loader=inmemory \
  -Dthreads=OFF \
  -DCMAKE_CXX_FLAGS="-fexceptions"

echo "> Building Xerces-C"
emmake cmake --build . -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "> Installing Xerces-C to ${INSTALL_DIR}"
emmake cmake --install .

echo "> Xerces-C build complete: ${INSTALL_DIR}/lib/libxerces-c.a"
