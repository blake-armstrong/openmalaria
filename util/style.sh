#!/usr/bin/env bash
set -euo pipefail

LLVM_VERSION=22.1.8
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OM_BUILD_DIR:-build}"
SOURCE_REGEX='^(model|unittest)/.*\.(cpp|h)$'

usage() {
    cat <<EOF
Usage: util/style.sh <command> [args]

Runs clang-format / clang-tidy $LLVM_VERSION (fetched with uvx) using the
project .clang-format and .clang-tidy.

  format [args...]      clang-format with the given args
  tidy [args...]        clang-tidy -p \$OM_BUILD_DIR with the given args
  check [base]          G-style gate on lines changed since base (default HEAD):
                        git-clang-format --diff, then clang-tidy-diff

Environment:
  OM_BUILD_DIR          build dir holding compile_commands.json (default: build)
EOF
}

require_uvx() {
    command -v uvx >/dev/null || { echo "uvx not found; install uv (https://docs.astral.sh/uv/)" >&2; exit 2; }
}

sysroot_args() {
    if [[ "$(uname)" == Darwin ]]; then
        echo "-extra-arg-before=-isysroot$(xcrun --show-sdk-path)"
    fi
}

compile_db() {
    local db="$ROOT/$BUILD_DIR/compile_commands.json"
    [[ -f "$db" ]] || { echo "missing $db; configure with cmake first or set OM_BUILD_DIR" >&2; exit 2; }
}

run_format() {
    uvx --from "clang-format==$LLVM_VERSION" clang-format "$@"
}

run_tidy() {
    compile_db
    uvx --from "clang-tidy==$LLVM_VERSION" clang-tidy -p "$ROOT/$BUILD_DIR" $(sysroot_args) "$@"
}

run_check() {
    local base="${1:-HEAD}"
    cd "$ROOT"
    local files
    files=$(git diff --name-only --diff-filter=ACMR "$base" | grep -E "$SOURCE_REGEX" || true)
    if [[ -z "$files" ]]; then
        echo "G-style: no changed C++ files under model/ or unittest/"
        return 0
    fi
    local status=0
    echo "== git-clang-format"
    local format_diff
    format_diff=$(uvx --from "clang-format==$LLVM_VERSION" git-clang-format --binary clang-format --diff "$base" -- $files || true)
    if grep -q '^@@' <<<"$format_diff"; then
        echo "$format_diff"
        status=1
    fi
    echo "== clang-tidy-diff"
    compile_db
    git diff -U0 "$base" -- $files |
        uvx --from "clang-tidy==$LLVM_VERSION" clang-tidy-diff.py -p1 -quiet \
            -path "$ROOT/$BUILD_DIR" -clang-tidy-binary clang-tidy $(sysroot_args) \
            -regex '(model|unittest)/.*\.(cpp|h)' >"${TMPDIR:-/tmp}/om-tidy-diff.$$" 2>&1 || true
    if grep -qE ': (warning|error): ' "${TMPDIR:-/tmp}/om-tidy-diff.$$"; then
        grep -E ': (warning|error): ' "${TMPDIR:-/tmp}/om-tidy-diff.$$"
        status=1
    fi
    rm -f "${TMPDIR:-/tmp}/om-tidy-diff.$$"
    if [[ $status -eq 0 ]]; then echo "G-style: clean"; else echo "G-style: FAILED"; fi
    return $status
}

[[ $# -ge 1 ]] || { usage; exit 2; }
require_uvx
cmd="$1"
shift
case "$cmd" in
    format) run_format "$@" ;;
    tidy) run_tidy "$@" ;;
    check) run_check "$@" ;;
    -h|--help|help) usage ;;
    *) usage; exit 2 ;;
esac
