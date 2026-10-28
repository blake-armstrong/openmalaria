#!/usr/bin/env bash
set -euo pipefail

LLVM_VERSION=22.1.8
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OM_BUILD_DIR:-build}"
SOURCE_REGEX='^(model|unittest)/.*\.(cpp|h)$'
SAFE_FIX_CHECKS='-*,modernize-use-nullptr,modernize-use-override,modernize-use-equals-default,modernize-use-equals-delete,modernize-use-using,modernize-redundant-void-arg,modernize-use-bool-literals,cppcoreguidelines-init-variables,performance-for-range-copy,performance-unnecessary-copy-initialization'
MODERNIZE_CHECKS="$SAFE_FIX_CHECKS,modernize-make-unique,modernize-make-shared,modernize-loop-convert,modernize-use-default-member-init,modernize-use-emplace,modernize-use-std-numbers,modernize-concat-nested-namespaces,modernize-deprecated-headers,modernize-use-ranges,modernize-use-starts-ends-with,modernize-type-traits,modernize-unary-static-assert,modernize-shrink-to-fit,cppcoreguidelines-pro-type-member-init,cppcoreguidelines-prefer-member-initializer,performance-unnecessary-value-param,performance-move-const-arg,performance-faster-string-find"

usage() {
    cat <<EOF
Usage: util/style.sh <command> [args]

Runs clang-format / clang-tidy $LLVM_VERSION (fetched with uvx) using the
project .clang-format and .clang-tidy.

  format [args...]      clang-format with the given args
  tidy [args...]        clang-tidy -p \$OM_BUILD_DIR with the given args
  check [base]          G-style gate on changes since base (default HEAD):
                        clang-format on the changed files, then clang-tidy-diff on
                        the changed lines, skipping lines that only commits in
                        .git-blame-ignore-revs touched (fix does the same)
  changed [base]        list model/ and unittest/ C++ files changed since base
  format-changed [base [paths...]]
                        clang-format -i the files changed since base, optionally
                        only the given paths
  fix [base] [checks]   apply clang-tidy fix-its on lines changed since base, for
                        the given -checks filter (default: the safe list below)
  rename <dir|files...> [opts]
                        rename the identifiers declared in one module directory
                        (or a list of files, for a module that shares its
                        directory) to the naming rules, at every use in every
                        TU, then format-changed (util/rename_module.py; -h)
  modernize <dir|files...> [opts]
                        apply the module-wide fix-its below to one module
                        directory (or list of files) until none is left, then
                        format-changed; --report lists what is left for the
                        hand commit
                        (util/modernize_module.py; -h for options)

Safe fix-it checks (fix):
  $SAFE_FIX_CHECKS

Module-wide fix-it checks (modernize): the safe list, plus
  ${MODERNIZE_CHECKS#"$SAFE_FIX_CHECKS,"}

All [base] arguments default to HEAD and read git diff, so new files must be
staged (or git add -N) to be seen.

Environment:
  OM_BUILD_DIR          build dir holding compile_commands.json (default: build)
EOF
}

require_uvx() {
    command -v uvx >/dev/null || { echo "uvx not found; install uv (https://docs.astral.sh/uv/)" >&2; exit 2; }
}

tidy_args() {
    echo "-extra-arg=-D_CXXTEST_HAVE_EH"
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
    uvx --from "clang-tidy==$LLVM_VERSION" clang-tidy -p "$ROOT/$BUILD_DIR" $(tidy_args) "$@"
}

changed_files() {
    local base="$1"
    shift
    git -C "$ROOT" diff --name-only --diff-filter=ACMR "$base" -- "$@" | grep -E "$SOURCE_REGEX" || true
}

tidy_diff() {
    local base="$1"
    shift
    compile_db
    git -C "$ROOT" diff -U0 "$base" -- $(changed_files "$base") |
        python3 "$ROOT/util/tidy_diff_filter.py" "$base" |
        uvx --from "clang-tidy==$LLVM_VERSION" clang-tidy-diff.py -p1 -quiet \
            -path "$ROOT/$BUILD_DIR" -clang-tidy-binary clang-tidy $(tidy_args) \
            -regex '(model|unittest)/.*\.(cpp|h)' "$@"
}

run_format_changed() {
    local base="${1:-HEAD}"
    shift || true
    local files
    files=$(changed_files "$base" "$@")
    [[ -n "$files" ]] || return 0
    cd "$ROOT"
    run_format -i $files
}

unformatted_files() {
    run_format --dry-run "$@" 2>&1 | sed -nE 's/^([^:]+):[0-9]+:[0-9]+: (warning|error): .*clang-format-violations.*/\1/p' | sort -u
}

run_fix() {
    local base="${1:-HEAD}"
    local checks="${2:-$SAFE_FIX_CHECKS}"
    [[ -n "$(changed_files "$base")" ]] || { echo "no changed C++ files under model/ or unittest/"; return 0; }
    local fixes
    fixes=$(mktemp -d "${TMPDIR:-/tmp}/om-tidy-fixes.XXXXXX")
    cd "$ROOT"
    tidy_diff "$base" -checks="$checks" -export-fixes "$fixes/" || true
    uvx --from "clang-tidy==$LLVM_VERSION" clang-apply-replacements "$fixes"
    rm -rf "$fixes"
    run_format_changed "$base"
}

run_rename() {
    compile_db
    cd "$ROOT"
    uv run --quiet --no-project --with pyyaml python util/rename_module.py \
        --llvm-version "$LLVM_VERSION" --build-dir "$BUILD_DIR" "$@"
    [[ " $* " == *" --dry-run "* || " $* " == *" -h "* || " $* " == *" --help "* ]] || run_format_changed HEAD
}

run_modernize() {
    compile_db
    cd "$ROOT"
    uv run --quiet --no-project --with pyyaml python util/modernize_module.py \
        --llvm-version "$LLVM_VERSION" --build-dir "$BUILD_DIR" --checks="$MODERNIZE_CHECKS" "$@"
    [[ " $* " == *" --dry-run "* || " $* " == *" --report "* || " $* " == *" -h "* || " $* " == *" --help "* ]] ||
        run_format_changed HEAD
}

run_check() {
    local base="${1:-HEAD}"
    cd "$ROOT"
    local files
    files=$(changed_files "$base")
    if [[ -z "$files" ]]; then
        echo "G-style: no changed C++ files under model/ or unittest/"
        return 0
    fi
    local status=0
    echo "== clang-format"
    local file
    for file in $(unformatted_files $files); do
        run_format "$file" | diff -u --label "a/$file" --label "b/$file" "$file" - || true
        status=1
    done
    echo "== clang-tidy-diff"
    tidy_diff "$base" >"${TMPDIR:-/tmp}/om-tidy-diff.$$" 2>&1 || true
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
    changed) changed_files "${1:-HEAD}" ;;
    format-changed) run_format_changed "$@" ;;
    fix) run_fix "$@" ;;
    rename) run_rename "$@" ;;
    modernize) run_modernize "$@" ;;
    -h|--help|help) usage ;;
    *) usage; exit 2 ;;
esac
