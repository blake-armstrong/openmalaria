#!/usr/bin/env bash
set -euo pipefail

root="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
file=$(jq -r '.tool_input.file_path // empty')
rel="${file#"$root"/}"
[[ "$rel" =~ ^(model|unittest)/.*\.(cpp|h)$ && -f "$root/$rel" ]] || exit 0

if git -C "$root" ls-files --error-unmatch -- "$rel" >/dev/null 2>&1; then
    "$root/util/style.sh" format-changed HEAD "$rel"
else
    "$root/util/style.sh" format -i "$root/$rel"
fi
