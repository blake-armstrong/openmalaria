#!/usr/bin/env bash
# Run the refactor's gates (docs/refactor/PLAN.md § Gates) and print one line per gate.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RELEASE="${OM_BUILD_DIR:-build-rework}"
DEBUG="${OM_DEBUG_DIR:-build-debug}"
TSAN="${OM_TSAN_DIR:-build-tsan}"
BASE="${OM_BASE_BUILD:-../om-bench-base/build-rework}"
JOBS="${OM_JOBS:-4}"

usage() {
    cat <<EOF
Usage: util/gates.sh [options]

Builds $RELEASE (Release) and $DEBUG (Debug) and stops at the first build
error, then runs G-exact, G-unit and G-two-sims (ctest -LE statistical) in
both. Prints one line per gate; details of failures follow the line. Every
result is also appended, with HEAD and the date, to $RELEASE/gates.log,
which /close-chunk reads.

  --release-only      build and test $RELEASE only
  --ab [GROUP...]     A/B: util/ab_scenarios.py against the base build's
                      openMalaria (all groups if none given; options end at
                      the next argument starting with --)
  --cli               G-cli: util/ab_cli.sh against the base build
  --stat              G-stat: ctest -L statistical
  --tsan              G-two-sims under ThreadSanitizer: ctest -L two-sims in
                      $TSAN (configured with -DOM_SANITIZE=thread if missing;
                      env OM_TSAN_DIR)
  --style BASE        G-style: util/style.sh check BASE
  --perf              G-perf: util/bench.sh -n 7 --compare BASE_BUILD
  --all BASE          everything: --ab --cli --stat --tsan --style BASE --perf
  --base DIR          base build dir (default $BASE; env OM_BASE_BUILD)
  -j N                parallel jobs (default $JOBS; env OM_JOBS)
  -h                  this help

Exit status: 0 if every gate run passed, 1 otherwise, 2 for a build error.
EOF
}

ab=0 ab_groups=() cli=0 stat=0 tsan=0 style_base="" perf=0 release_only=0
while (($#)); do
    case "$1" in
    --release-only) release_only=1 ;;
    --ab)
        ab=1
        while (($# > 1)) && [[ "$2" != --* ]]; do
            ab_groups+=("$2")
            shift
        done
        ;;
    --cli) cli=1 ;;
    --stat) stat=1 ;;
    --tsan) tsan=1 ;;
    --style) style_base="$2"; shift ;;
    --perf) perf=1 ;;
    --all) ab=1 cli=1 stat=1 tsan=1 perf=1 style_base="$2"; shift ;;
    --base) BASE="$2"; shift ;;
    -j) JOBS="$2"; shift ;;
    -h | --help) usage; exit 0 ;;
    *) echo "unknown option $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

cd "$ROOT" || exit 2
LOG="$RELEASE/gates.log"
SCRATCH="$(mktemp -d "${TMPDIR:-/tmp}/om-gates.XXXXXX")"
failed=0
dirs=("$RELEASE")
((release_only)) || dirs+=("$DEBUG")

{
    echo "== $(date '+%Y-%m-%d %H:%M') HEAD $(git rev-parse --short HEAD)$(git diff --quiet -- model unittest test || echo ' +uncommitted')"
} >>"$LOG"

result() {
    echo "$1"
    echo "$1" >>"$LOG"
}

for dir in "${dirs[@]}"; do
    out="$SCRATCH/build-$(basename "$dir").txt"
    if ! cmake --build "$dir" -j "$JOBS" >"$out" 2>&1; then
        result "build $dir: FAILED"
        grep -E 'error|FAILED' -A3 "$out" | head -40
        exit 2
    fi
    warnings=$(grep -E 'warning:' "$out" | grep -v 'duplicate libraries' | sort -u)
    [[ -z "$warnings" ]] || { echo "build $dir warnings:"; echo "$warnings" | head -20; }
done

exact_line="G-exact/G-unit/G-two-sims:"
for dir in "${dirs[@]}"; do
    out="$SCRATCH/ctest-$(basename "$dir").txt"
    (cd "$dir" && caffeinate -i ctest -j "$JOBS" -LE statistical >"$out" 2>&1)
    summary=$(grep -E 'tests passed' "$out" | tail -1)
    total=$(sed -nE 's/.* out of ([0-9]+).*/\1/p' <<<"$summary")
    failing=$(grep -E '^\s+[0-9]+ - .* \((Failed|Timeout|SEGFAULT|Not Run|Exception)' "$out")
    passed=$((total - $(printf '%s' "$failing" | grep -c .)))
    exact_line+=" $(basename "$dir") $passed/$total"
    if [[ -n "$failing" ]]; then
        failed=1
        details+="$dir failing:"$'\n'"$failing"$'\n'
    fi
done
result "$exact_line$([[ -n "${details:-}" ]] && echo ' FAILED')"
[[ -z "${details:-}" ]] || printf '%s' "$details"

if ((ab)); then
    out="$SCRATCH/ab.txt"
    if [[ ! -x "$BASE/openMalaria" ]]; then
        result "A/B: FAILED (no $BASE/openMalaria)"
        failed=1
    else
        base_head=$(git -C "$BASE/.." rev-parse --short HEAD 2>/dev/null || echo '?')
        caffeinate -i python3 util/ab_scenarios.py "$BASE/openMalaria" "$RELEASE/openMalaria" ${ab_groups[@]+"${ab_groups[@]}"} >"$out" 2>&1
        status=$?
        summary=$(grep -E 'variants differ' "$out" | tail -1)
        [[ -n "$summary" ]] || summary="did not finish (exit $status)"
        result "A/B (base $base_head${ab_groups:+, ${ab_groups[*]}}): $summary"
        if ((status)); then
            failed=1
            grep -E '^DIFF' "$out" | cut -c1-160
            [[ "$summary" == *differ* ]] || tail -20 "$out"
        fi
    fi
fi

if ((cli)); then
    out="$SCRATCH/cli.txt"
    if [[ ! -x "$BASE/openMalaria" ]]; then
        result "G-cli: FAILED (no $BASE/openMalaria)"
        failed=1
    else
        base_head=$(git -C "$BASE/.." rev-parse --short HEAD 2>/dev/null || echo '?')
        caffeinate -i util/ab_cli.sh "$BASE" "$RELEASE" >"$out" 2>&1
        status=$?
        summary=$(grep -E 'variants differ' "$out" | tail -1)
        [[ -n "$summary" ]] || summary="did not finish (exit $status)"
        result "G-cli (base $base_head): $summary"
        if ((status)); then
            failed=1
            grep -E '^DIFF' "$out" | cut -c1-160
            [[ "$summary" == *differ* ]] || tail -20 "$out"
        fi
    fi
fi

if ((stat)); then
    out="$SCRATCH/stat.txt"
    (cd "$RELEASE" && caffeinate -i ctest -L statistical -j "$JOBS" >"$out" 2>&1)
    summary=$(grep -E 'tests passed' "$out" | tail -1)
    disabled=$(grep -cE '^\s+[0-9]+ - .*\(Disabled\)' "$out")
    result "G-stat: ${summary:-no summary}, $disabled disabled"
    if grep -qE '\((Failed|Timeout)\)' "$out"; then
        failed=1
        grep -E '^\s+[0-9]+ - .*\((Failed|Timeout)\)' "$out"
    fi
fi

if ((tsan)); then
    out="$SCRATCH/tsan.txt"
    if [[ ! -f "$TSAN/CMakeCache.txt" ]]; then
        cmake -S . -B "$TSAN" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOM_SANITIZE=thread \
            -DOM_BOXTEST_STRICT=ON >"$out" 2>&1
    fi
    if ! cmake --build "$TSAN" -j "$JOBS" --target openMalaria-two-sims inlined_xsd >>"$out" 2>&1; then
        result "G-two-sims ThreadSanitizer: FAILED (build $TSAN)"
        failed=1
        grep -E 'error|FAILED' -A3 "$out" | head -20
    else
        (cd "$TSAN" && caffeinate -i ctest -j "$JOBS" -L two-sims --output-on-failure >>"$out" 2>&1)
        summary=$(grep -E 'tests passed' "$out" | tail -1)
        races=$(grep -c 'WARNING: ThreadSanitizer' "$out")
        result "G-two-sims ThreadSanitizer: ${summary:-no summary}, $races race reports"
        if grep -qE '\((Failed|Timeout)\)' "$out" || [[ -z "$summary" ]]; then
            failed=1
            grep -E '^\s+[0-9]+ - .*\((Failed|Timeout)\)' "$out"
            grep -A12 -m3 'WARNING: ThreadSanitizer' "$out"
        fi
    fi
fi

if [[ -n "$style_base" ]]; then
    out="$SCRATCH/style.txt"
    OM_BUILD_DIR="$RELEASE" util/style.sh check "$style_base" >"$out" 2>&1
    status=$?
    result "G-style (since $style_base): $(tail -1 "$out")"
    if ((status)); then
        failed=1
        grep -E 'warning:|error:|^[-+]' "$out" | head -30
    fi
fi

if ((perf)); then
    out="$SCRATCH/perf.txt"
    load=$(sysctl -n vm.loadavg 2>/dev/null | awk '{print $2}')
    OM_BUILD_DIR="$RELEASE" caffeinate -i util/bench.sh -n 7 --compare "$BASE" >"$out" 2>&1
    status=$?
    ratio=$(grep -E 'per-pair ratio' "$out" | sed -E 's/.*median ([0-9.]+) \(([^)]*)\).*/\1 (\2)/')
    spreads=$(grep -E 'median wall' "$out" | sed -E 's/.*spread ([0-9.]+%).*/\1/' | paste -sd/ -)
    result "G-perf: $(tail -1 "$out"), median ratio $ratio, spreads $spreads (base/candidate), load ${load:-?} at start"
    ((status == 0)) || failed=1
fi

echo "(logs: $SCRATCH)"
exit "$failed"
