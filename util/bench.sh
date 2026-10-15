#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OM_BUILD_DIR:-build}"
RUNS=5
SCENARIO="$ROOT/test/bench/scenarioBench.xml"
BASE_BUILD=""
LIMIT_PCT=3

usage() {
    cat <<EOF
Usage: util/bench.sh [-n RUNS] [--compare BASE_BUILD_DIR] [scenario.xml]

Runs openMalaria on the benchmark scenario RUNS times (default $RUNS), each in a
fresh temporary directory, and reports the median wall time.

  --compare DIR         A/B mode (the G-perf gate): also runs the openMalaria in
                        build dir DIR, alternating base and candidate runs
                        (ABBA order), RUNS of each. Reports the median per-pair
                        ratio candidate/base and fails if it exceeds 1 + $LIMIT_PCT%.
  scenario.xml          default: test/bench/scenarioBench.xml
                        input files are looked up in test/ (--resource-path)

Environment:
  OM_BUILD_DIR          candidate build dir, relative to the repo or absolute
                        (default: build)

A base build of commit BASE, configured like the candidate:
  git worktree add ../om-bench-base BASE
  cmake -S ../om-bench-base -B ../om-bench-base/build-rework -DCMAKE_BUILD_TYPE=Release
  cmake --build ../om-bench-base/build-rework -j10 --target openMalaria inlined_xsd

Exit status: 0 ok, 1 a run failed or the A/B check failed, 2 usage error.

On macOS the script re-runs itself under caffeinate -i, so idle sleep cannot
stall a run. Run it on an otherwise idle machine.
EOF
}

if [[ "$(uname)" == Darwin && -z "${OM_BENCH_CAFFEINATED:-}" ]]; then
    export OM_BENCH_CAFFEINATED=1
    exec caffeinate -i "$0" "$@"
fi

args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -n) [[ $# -ge 2 ]] || { usage; exit 2; }; RUNS="$2"; shift 2 ;;
        --compare) [[ $# -ge 2 ]] || { usage; exit 2; }; BASE_BUILD="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage; exit 2 ;;
        *) args+=("$1"); shift ;;
    esac
done
[[ ${#args[@]} -le 1 ]] || { usage; exit 2; }
[[ ${#args[@]} -eq 1 ]] && SCENARIO="$(cd "$(dirname "${args[0]}")" && pwd)/$(basename "${args[0]}")"
[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || { echo "RUNS must be a positive integer" >&2; exit 2; }
[[ -f "$SCENARIO" ]] || { echo "missing $SCENARIO" >&2; exit 2; }
SCHEMA_NAME=$(grep -o 'scenario_[a-z0-9_]*\.xsd' "$SCENARIO" | head -1)

check_build() {
    local dir="$1"
    [[ -x "$dir/openMalaria" ]] || { echo "missing $dir/openMalaria; build first" >&2; exit 2; }
    [[ -f "$dir/schema/$SCHEMA_NAME" ]] || { echo "missing $dir/schema/$SCHEMA_NAME; build target inlined_xsd" >&2; exit 2; }
    "$dir/openMalaria" --help >/dev/null 2>&1 || true
}

[[ "$BUILD_DIR" == /* ]] && CAND_DIR="$BUILD_DIR" || CAND_DIR="$ROOT/$BUILD_DIR"
check_build "$CAND_DIR"
if [[ -n "$BASE_BUILD" ]]; then
    BASE_DIR="$(cd "$BASE_BUILD" 2>/dev/null && pwd)" || { echo "missing $BASE_BUILD" >&2; exit 2; }
    check_build "$BASE_DIR"
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/om-bench.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

run_once() {
    local build="$1" label="$2" i="$3"
    local dir="$WORK/$label$i"
    mkdir "$dir"
    ln -s "$build/schema/$SCHEMA_NAME" "$dir/$SCHEMA_NAME"
    if ! { time (cd "$dir" && "$build/openMalaria" --resource-path "$ROOT/test" --scenario "$SCENARIO" >"$dir/log" 2>&1); } 2>"$dir/time"; then
        echo "$label run $i failed:" >&2
        cat "$dir/log" >&2
        exit 1
    fi
    [[ -f "$dir/output.txt" ]] || { echo "$label run $i wrote no output.txt" >&2; exit 1; }
    local wall user sys
    read -r wall user sys <"$dir/time"
    printf '%-9s run %d: wall %.3f s  cpu %.3f s\n' "$label" "$i" "$wall" "$(echo "$user + $sys" | bc)"
    echo "$wall" >>"$WORK/$label.walls"
    rm -rf "$dir"
}

summarise() {
    sort -n "$1" | awk -v label="$2" '
        { t[NR] = $1 }
        END {
            median = (NR % 2) ? t[(NR + 1) / 2] : (t[NR / 2] + t[NR / 2 + 1]) / 2
            printf "%-9s median wall %.3f s  min %.3f  max %.3f  spread %.2f%% of median\n",
                label, median, t[1], t[NR], 100 * (t[NR] - t[1]) / median
        }'
}

echo "benchmark: $SCENARIO"
echo "candidate: $CAND_DIR/openMalaria"
echo "commit:    $(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet HEAD -- model || echo ' (model/ modified)')"
[[ -n "$BASE_BUILD" ]] && echo "base:      $BASE_DIR/openMalaria"
echo "runs:      $RUNS$([[ -n "$BASE_BUILD" ]] && echo ' per build')"

TIMEFORMAT='%R %U %S'
if [[ -z "$BASE_BUILD" ]]; then
    for ((i = 1; i <= RUNS; i++)); do
        run_once "$CAND_DIR" candidate "$i"
    done
    summarise "$WORK/candidate.walls" candidate
    exit 0
fi

for ((i = 1; i <= RUNS; i++)); do
    if ((i % 2)); then
        run_once "$BASE_DIR" base "$i"
        run_once "$CAND_DIR" candidate "$i"
    else
        run_once "$CAND_DIR" candidate "$i"
        run_once "$BASE_DIR" base "$i"
    fi
done
summarise "$WORK/base.walls" base
summarise "$WORK/candidate.walls" candidate
paste "$WORK/candidate.walls" "$WORK/base.walls" | awk '{ print $1 / $2 }' | sort -n | awk -v limit="$LIMIT_PCT" '
    { r[NR] = $1 }
    END {
        median = (NR % 2) ? r[(NR + 1) / 2] : (r[NR / 2] + r[NR / 2 + 1]) / 2
        printf "candidate/base per-pair ratio: median %.4f (%+.2f%%)  min %.4f  max %.4f\n",
            median, 100 * (median - 1), r[1], r[NR]
        if (median > 1 + limit / 100) {
            printf "G-perf: FAILED (limit +%d%%)\n", limit
            exit 1
        }
        printf "G-perf: ok (limit +%d%%)\n", limit
    }'
