#!/usr/bin/env python3
"""Drop added lines that only commits in .git-blame-ignore-revs touched from a `git diff -U0`.

Usage: git diff -U0 <base> -- <files> | util/tidy_diff_filter.py <base>
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IGNORE_REVS = ROOT / ".git-blame-ignore-revs"
UNCOMMITTED = "0" * 40
HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")
BLAME_LINE = re.compile(r"^([0-9a-f]{40}) \d+ (\d+)")


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True, text=True).stdout


def ignored_revs():
    if not IGNORE_REVS.exists():
        return set()
    revs = set()
    for line in IGNORE_REVS.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            revs.add(git("rev-parse", "--verify", line + "^{commit}").strip())
    return revs


def added_lines(diff):
    files = {}
    current = None
    line_no = 0
    for line in diff.splitlines():
        if line.startswith("+++ "):
            path = line[4:]
            current = None if path == "/dev/null" else files.setdefault(path[2:] if path.startswith("b/") else path, {})
            continue
        match = HUNK.match(line)
        if match:
            line_no = int(match.group(1))
            continue
        if current is not None and line.startswith("+"):
            current[line_no] = line[1:]
            line_no += 1
    return files


def attributed_lines(base, path, wanted):
    blame = git(
        "blame", "--porcelain", "--ignore-revs-file", str(IGNORE_REVS),
        "--contents", str(ROOT / path), f"{base}..HEAD", "--", path,
    )
    return {
        int(match.group(2))
        for match in map(BLAME_LINE.match, blame.splitlines())
        if match and match.group(1) in wanted
    }


def runs(numbers):
    start = previous = None
    for number in sorted(numbers):
        if previous is not None and number == previous + 1:
            previous = number
            continue
        if start is not None:
            yield start, previous
        start = previous = number
    if start is not None:
        yield start, previous


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    base = sys.argv[1]
    diff = sys.stdin.read()
    in_range = set(git("rev-list", f"{base}..HEAD").split())
    ignored = in_range & ignored_revs()
    if not ignored:
        sys.stdout.write(diff)
        return
    wanted = (in_range - ignored) | {UNCOMMITTED}
    for path, lines in added_lines(diff).items():
        kept = set(lines) & attributed_lines(base, path, wanted)
        if not kept:
            continue
        print(f"--- a/{path}\n+++ b/{path}")
        for first, last in runs(kept):
            print(f"@@ -0,0 +{first},{last - first + 1} @@")
            for number in range(first, last + 1):
                print("+" + lines[number])


if __name__ == "__main__":
    main()
