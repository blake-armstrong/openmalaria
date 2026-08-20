#!/usr/bin/env bash
# Cross-compile GSL 2.8 to a static wasm32 library for the OpenMalaria
# Emscripten build. OpenMalaria uses GSL for RNG, statistical CDFs, Brent's
# method root-finding (vector/EIR calibration), and BLAS (dgemv) in the
# vector transmission matrix math -- see `grep -rl gsl_ model/`.
set -euo pipefail

DEPS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${DEPS_DIR}/_work"
INSTALL_DIR="${DEPS_DIR}/_install"

GSL_VERSION=2.8
GSL_TARBALL="gsl-${GSL_VERSION}.tar.gz"
GSL_URL="https://ftp.gnu.org/gnu/gsl/${GSL_TARBALL}"
GSL_SRC_DIR="${WORK_DIR}/gsl-${GSL_VERSION}"

if ! command -v emconfigure >/dev/null 2>&1; then
  echo "error: emconfigure not on PATH -- run 'source ~/git/emsdk/emsdk_env.sh' first" >&2
  exit 1
fi

mkdir -p "${WORK_DIR}" "${INSTALL_DIR}"

if [ ! -f "${WORK_DIR}/${GSL_TARBALL}" ]; then
  echo "==> Downloading GSL ${GSL_VERSION}"
  curl -fsSL -o "${WORK_DIR}/${GSL_TARBALL}" "${GSL_URL}"
fi

if [ ! -d "${GSL_SRC_DIR}" ]; then
  echo "==> Extracting GSL ${GSL_VERSION}"
  tar -xzf "${WORK_DIR}/${GSL_TARBALL}" -C "${WORK_DIR}"
fi

cd "${GSL_SRC_DIR}"

# GSL 2.8's bundled config.sub/config.guess predate the wasm32-unknown-emscripten
# triple and will reject it as an invalid target before configure even starts.
# This is a generic autotools-cross-compilation gotcha (test with
# `./config.sub wasm32-unknown-emscripten`), not GSL-specific -- refresh both
# from upstream GNU config so the triple is recognized.
echo "==> Refreshing config.sub / config.guess"
curl -fsSL -o config.sub "https://git.savannah.gnu.org/cgit/config.git/plain/config.sub"
curl -fsSL -o config.guess "https://git.savannah.gnu.org/cgit/config.git/plain/config.guess"

if [ -f "${INSTALL_DIR}/lib/libgsl.a" ] && [ -f "${INSTALL_DIR}/lib/libgslcblas.a" ]; then
  echo "==> GSL already built at ${INSTALL_DIR}, skipping (delete ${INSTALL_DIR} to force a rebuild)"
  exit 0
fi

echo "==> Configuring GSL for wasm32-unknown-emscripten"
emconfigure ./configure \
  --host=wasm32-unknown-emscripten \
  --prefix="${INSTALL_DIR}" \
  --disable-shared \
  --enable-static \
  GSL_DISABLE_TESTS=1

echo "==> Building GSL"
# Skip GSL's own test suite: they'd build as wasm executables with no host
# to run them on, so building them at all is pure waste.
emmake make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "==> Installing GSL to ${INSTALL_DIR}"
emmake make install

echo "==> GSL build complete: ${INSTALL_DIR}/lib/libgsl.a, ${INSTALL_DIR}/lib/libgslcblas.a"
