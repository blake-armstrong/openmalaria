#!/usr/bin/env bash
# PostToolUse hook for Bash: after a command that edits files in place, clang-format the
# C++ files under model/ and unittest/ that differ from HEAD, as format-cpp.sh does for
# the Edit and Write tools.
set -euo pipefail

root="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
cmd=$(jq -r '.tool_input.command // empty')

grep -qE '(python3?|uv run)[^|;&]*[[:space:]]-([[:space:]]|$)|<<|perl[[:space:]]+-[a-z]*i|sed[[:space:]]+-i' <<<"$cmd" || exit 0
grep -qE 'util/(style\.sh|plant\.py)' <<<"$cmd" && exit 0

changed=$(git -C "$root" diff --name-only HEAD -- 'model/*.cpp' 'model/*.h' 'unittest/*.cpp' 'unittest/*.h')
[[ -n "$changed" ]] || exit 0
# shellcheck disable=SC2086
"$root/util/style.sh" format-changed HEAD $changed >/dev/null 2>&1 || true
exit 0
