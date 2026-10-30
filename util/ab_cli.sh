#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if (($# < 1)) || [[ "$1" == -h || "$1" == --help ]]; then
    echo "Usage: util/ab_cli.sh BASE_BUILD [CANDIDATE_BUILD]  (candidate default: \$OM_BUILD_DIR or build-rework)"
    exit 2
fi
base="$1"
candidate="${2:-${OM_BUILD_DIR:-build-rework}}"
cd "$ROOT"
exec python3 util/ab_scenarios.py "$base/openMalaria" "$candidate/openMalaria" cli
