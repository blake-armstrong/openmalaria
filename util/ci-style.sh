#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export OM_BUILD_DIR="${OM_BUILD_DIR:-build-style}"

usage() {
    cat <<EOF
Usage: util/ci-style.sh [base-ref]

The CI style job (.github/workflows/style.yml), runnable by hand: G-style
(util/style.sh check) on the changes between HEAD and its merge base with
base-ref (default: origin/main).

If \$OM_BUILD_DIR (default: build-style) has no compile_commands.json, it is
configured first. The schema target is built so that clang-tidy finds the
generated scnXml headers.

Environment:
  OM_BUILD_DIR          build dir, relative to the repo root (default: build-style)
  CC, CXX               compilers for a fresh configure; use clang so that
                        clang-tidy understands the compile flags
EOF
}

case "${1:-}" in
    -h|--help) usage; exit 0 ;;
esac
[[ $# -le 1 ]] || { usage; exit 2; }
base_ref="${1:-origin/main}"

cd "$ROOT"
base=$(git merge-base "$base_ref" HEAD)
echo "G-style on $base_ref...HEAD (merge base $(git rev-parse --short "$base"))"

if [[ ! -f "$OM_BUILD_DIR/compile_commands.json" ]]; then
    cmake -S . -B "$OM_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DOM_CXXTEST_ENABLE=ON
fi
cmake --build "$OM_BUILD_DIR" --target schema --parallel

util/style.sh check "$base"
