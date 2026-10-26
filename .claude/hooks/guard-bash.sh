#!/usr/bin/env bash
# PreToolUse hook for Bash: enforce the refactor's git and deletion rules
# (docs/refactor/PLAN.md § Session protocol; never rm -rf a variable-built path).
set -euo pipefail

cmd=$(jq -r '.tool_input.command // empty' | awk '
    delim != "" { if ($0 == delim) delim = ""; next }
    { print }
    match($0, /<<-?[[:space:]]*["\x27]?[A-Za-z_]+/) {
        delim = substr($0, RSTART, RLENGTH); sub(/^<<-?[[:space:]]*["\x27]?/, "", delim)
    }')

decide() {
    jq -n --arg d "$1" --arg r "$2" \
        '{hookSpecificOutput: {hookEventName: "PreToolUse", permissionDecision: $d, permissionDecisionReason: $r}}'
    exit 0
}

if grep -qE '(^|[;&|[:space:]])git[[:space:]]+add[[:space:]]+([^;&|]*[[:space:]])?(-A|--all|\.)([[:space:]]|$|[;&|])' <<<"$cmd"; then
    decide deny "PLAN: stage files by name, never git add -A or git add . (.cache/ and python/ are untracked user files)."
fi
if grep -qE '(^|[;&|[:space:]])git[[:space:]]+commit[[:space:]]+([^;&|]*[[:space:]])?-[b-zA-Z]*a[a-zA-Z]*([[:space:]]|$)' <<<"$cmd"; then
    decide deny "PLAN: stage files by name; do not commit with -a."
fi
if grep -qE '(^|[;&|[:space:]])rm[[:space:]]+-[a-zA-Z]*[rR][a-zA-Z]*[[:space:]]+([^;&|]*)\$' <<<"$cmd"; then
    decide deny "Never rm -r a path built from a variable; use a fresh mktemp -d scratch dir and leave it."
fi
if grep -qE '(^|[;&|[:space:]])git[[:space:]]+push\b' <<<"$cmd"; then
    decide ask "PLAN: never push unless the user asks."
fi
if grep -qE '(^|[;&|[:space:]])git[[:space:]]+(stash|clean|reset[[:space:]]+--hard)\b' <<<"$cmd" ||
    grep -qE '(^|[;&|[:space:]])git[[:space:]]+checkout[[:space:]]+(--[[:space:]]+)?(\.|model|unittest|test|util|docs)/?([[:space:]]|$|[;&|])' <<<"$cmd"; then
    decide ask "PLAN: ask the user before discarding or stashing tracked changes."
fi
exit 0
