#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${OM_BUILD_DIR:-build}"
RUNS=5
SCENARIO="$ROOT/test/bench/scenarioBench.xml"

usage() {
    cat <<EOF
Usage: util/bench.sh [-n RUNS] [scenario.xml]

Runs openMalaria on the benchmark scenario RUNS times (default $RUNS), each in a
fresh temporary directory, and reports the median wall time. This is the G-perf
measurement; compare the median with the baseline in docs/refactor/PROGRESS.md.

  scenario.xml          default: test/bench/scenarioBench.xml
                        input files are looked up in test/ (--resource-path)

Environment:
  OM_BUILD_DIR          build dir holding openMalaria (default: build)

On macOS the script re-runs itself under caffeinate -i, so idle sleep cannot
stall a run. Run it on an otherwise idle machine.
EOF
}

if [[ "$(uname)" == Darwin && -z "${OM_BENCH_CAFFEINATED:-}" ]]; then
    export OM_BENCH_CAFFEINATED=1
    exec caffeinate -i "$0" "$@"
fi

while getopts "n:h" opt; do
    case "$opt" in
        n) RUNS="$OPTARG" ;;
        h) usage; exit 0 ;;
        *) usage; exit 2 ;;
    esac
done
shift $((OPTIND - 1))
[[ $# -le 1 ]] || { usage; exit 2; }
[[ $# -eq 1 ]] && SCENARIO="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || { echo "RUNS must be a positive integer" >&2; exit 2; }

EXE="$ROOT/$BUILD_DIR/openMalaria"
[[ -x "$EXE" ]] || { echo "missing $EXE; build first or set OM_BUILD_DIR" >&2; exit 2; }
[[ -f "$SCENARIO" ]] || { echo "missing $SCENARIO" >&2; exit 2; }
SCHEMA_NAME=$(grep -o 'scenario_[a-z0-9_]*\.xsd' "$SCENARIO" | head -1)
SCHEMA="$ROOT/$BUILD_DIR/schema/$SCHEMA_NAME"
[[ -f "$SCHEMA" ]] || { echo "missing $SCHEMA" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/om-bench.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

echo "benchmark: $SCENARIO"
echo "binary:    $EXE"
echo "commit:    $(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet HEAD -- model || echo ' (model/ modified)')"
echo "runs:      $RUNS"

TIMEFORMAT='%R %U %S'
for ((i = 1; i <= RUNS; i++)); do
    dir="$WORK/run$i"
    mkdir "$dir"
    ln -s "$SCHEMA" "$dir/$SCHEMA_NAME"
    if ! { time (cd "$dir" && "$EXE" --resource-path "$ROOT/test" --scenario "$SCENARIO" >"$dir/log" 2>&1); } 2>"$dir/time"; then
        echo "run $i failed:" >&2
        cat "$dir/log" >&2
        exit 1
    fi
    [[ -f "$dir/output.txt" ]] || { echo "run $i wrote no output.txt" >&2; exit 1; }
    read -r wall user sys <"$dir/time"
    printf 'run %d: wall %.3f s  cpu %.3f s\n' "$i" "$wall" "$(echo "$user + $sys" | bc)"
    echo "$wall" >>"$WORK/walls"
    rm -rf "$dir"
done

sort -n "$WORK/walls" | awk '
    { t[NR] = $1 }
    END {
        median = (NR % 2) ? t[(NR + 1) / 2] : (t[NR / 2] + t[NR / 2 + 1]) / 2
        printf "median wall %.3f s  min %.3f  max %.3f  spread %.2f%% of median\n",
            median, t[1], t[NR], 100 * (t[NR] - t[1]) / median
    }'
