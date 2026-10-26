#!/usr/bin/env python3
"""Refactor bookkeeping for docs/refactor (PLAN.md, PROGRESS.md, FINDINGS.md, README.md).

  next [ID]          what a chunk session needs at its start: the chunk (the first
                     not done in PLAN's recommended order, or ID), its PLAN text and
                     stage rules, PLAN Part 1's protocol, gates and practical notes,
                     the previous log entry's follow-ups, plan amendments and open
                     findings that name it, and the state of the base worktree
  section NAME       print one PLAN section by (part of) its heading, e.g.
                     "Modernisation policy"
  commits BASE       the commits since BASE, as PROGRESS's status table lists them
  gates [BASE]       the gate lines util/gates.sh logged for commits since BASE
  check-docs         drift checks: README's bridge list against the code, the
                     module status table against .git-blame-ignore-revs, and the
                     status table against the log entries; exit 1 on any finding
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs", "refactor")
CHUNK_RE = re.compile(r"\b([A-Z]\d+[a-z]?\d?)\b")
PRACTICAL_NOTES = "Practical notes for every chunk:"


def read(name):
    with open(os.path.join(DOCS, name), encoding="utf-8") as fh:
        return fh.read()


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True).stdout.strip()


def section(text, heading_re):
    """The text from the first heading matching heading_re to the next heading of the same or higher level."""
    m = re.search(heading_re, text, re.M)
    if not m:
        return ""
    level = len(m.group(0)) - len(m.group(0).lstrip("#"))
    end = re.compile(rf"^#{{1,{level}}} ", re.M).search(text, m.end())
    return text[m.start(): end.start() if end else len(text)].rstrip() + "\n"


def status_table(progress):
    rows = []
    for m in re.finditer(r"^\| ([A-Z]\d+[a-z]?\d?) \| (\w+) \|(.*)\|$", progress, re.M):
        rows.append((m.group(1), m.group(2), m.group(3).strip()))
    return rows


def recommended_order(plan, rows):
    ids = [r[0] for r in rows]
    order = []
    for line in section(plan, r"^## Recommended order").splitlines():
        cells = line.split("|")
        if len(cells) < 4 or not cells[1].strip().rstrip("abc").isdigit():
            continue
        tokens = re.findall(r"[A-Z]\d+[a-z]?\d?|→", cells[2])
        i = 0
        while i < len(tokens):
            if i + 2 < len(tokens) and tokens[i + 1] == "→":
                start, end = tokens[i], tokens[i + 2]
                last = max(j for j, c in enumerate(ids) if c == end or c.startswith(end))
                order += ids[ids.index(start): last + 1]
                i += 3
            else:
                if tokens[i] != "→":
                    order.append(tokens[i])
                i += 1
    seen = set()
    return [c for c in order if not (c in seen or seen.add(c))]


def chunk_text(plan, chunk):
    """The chunk's own PLAN text: its ### section, or its stage's rules plus its table row."""
    own = section(plan, rf"^### {re.escape(chunk)}:")
    stage_letter = chunk[0]
    stage = section(plan, rf"^## Stage {stage_letter}\b")
    kept = []
    in_sub = False
    for line in stage.splitlines():
        if line.startswith("### "):
            in_sub = True
            continue
        if in_sub:
            if line.startswith("## "):
                in_sub = False
            else:
                continue
        row = re.match(r"\| \*\*([A-Z]\d+[a-z]?\d?)\*\*", line)
        if row and not chunk.startswith(row.group(1)):
            continue
        kept.append(line)
    stage_rules = "\n".join(kept).rstrip() + "\n"
    if own:
        return own + "\n" + stage_rules
    return stage_rules


def log_entries(progress):
    return [(m.group(1), m.start()) for m in re.finditer(r"^### (.*)$", progress, re.M)]


def entry_text(progress, start):
    end = re.compile(r"^### ", re.M).search(progress, start + 4)
    return progress[start: end.start() if end else len(progress)]


def bullets(entry, label):
    m = re.search(rf"^- {label}:?(.*?)(?=^- [A-Z][\w /-]*:|\Z)", entry, re.M | re.S)
    return ("- " + label + ":" + m.group(1)).rstrip() + "\n" if m else ""


def open_findings(findings, words):
    open_part = findings.split("\n## Closed", 1)[0]
    hits = []
    for entry in re.findall(r"^- \*\*F\d+\*\*.*$", open_part, re.M):
        if any(re.search(rf"\b{re.escape(w)}\b", entry) for w in words):
            hits.append(entry)
    return hits


def cmd_next(args):
    plan, progress, findings = read("PLAN.md"), read("PROGRESS.md"), read("FINDINGS.md")
    rows = status_table(progress)
    status = {c: s for c, s, _ in rows}
    if args:
        chunk = args[0]
    else:
        pending = [c for c in recommended_order(plan, rows) if status.get(c) not in ("done", "split")]
        if not pending:
            print("every chunk in the recommended order is done")
            return 0
        chunk = pending[0]
    print(f"# Next chunk: {chunk} (status {status.get(chunk, 'not in the status table')})\n")
    print("## PLAN text for the chunk and its stage\n")
    print(chunk_text(plan, chunk))
    for line in section(plan, r"^## Recommended order").splitlines():
        if re.search(rf"\b{re.escape(chunk)}\b", line) and line.startswith("|"):
            print("Recommended-order row: " + line + "\n")
    refs = sorted(set(re.findall(r"§ ([A-Z][\w-]+(?: [\w-]+)*)", chunk_text(plan, chunk))))
    if refs:
        print("Part 3 sections it names (print with `util/chunk.py section NAME`): " + "; ".join(refs) + "\n")
    print("## PLAN Part 1 (protocol, gates, practical notes)\n")
    print(section(plan, r"^## Session protocol"))
    print(section(plan, r"^## Gates"))
    order = section(plan, r"^## Recommended order")
    print(order[order.index(PRACTICAL_NOTES):] if PRACTICAL_NOTES in order else "")
    entries = log_entries(progress)
    chunk_entries = [i for i, (t, _) in enumerate(entries) if re.match(r"[A-Z]\d+[a-z]?\d?: .*\(done", t)]
    for title, start in entries[chunk_entries[-1] if chunk_entries else -1:]:
        entry = entry_text(progress, start)
        print(f"## Log entry: {title}\n")
        print(bullets(entry, "Follow-ups") + bullets(entry, "Next"))
    for title, start in entries:
        entry = entry_text(progress, start)
        if title.startswith("Plan amendment") and re.search(rf"\b{re.escape(chunk)}\b", entry):
            print(f"## {title}\n\n" + entry.split("\n", 1)[1].strip() + "\n")
    own = section(plan, rf"^### {re.escape(chunk)}:") or "\n".join(
        line for line in plan.splitlines() if line.startswith(f"| **{chunk}**"))
    words = [chunk] + [w for w in re.findall(r"`([\w/.:]{5,})`", own) if "/" in w or w[0].isupper()]
    hits = open_findings(findings, words)
    print(f"## Open findings naming the chunk or its modules ({len(hits)})\n")
    for h in hits:
        print(h + "\n")
    head = git("rev-parse", "--short", "HEAD")
    base = subprocess.run(["git", "-C", os.path.join(ROOT, "..", "om-bench-base"), "rev-parse", "--short", "HEAD"],
                          text=True, capture_output=True).stdout.strip()
    print(f"## State\n\nHEAD {head}; tree {'clean' if not git('status', '--porcelain', '--', 'model', 'unittest', 'test', 'util', 'docs') else 'has changes'}; "
          f"base worktree ../om-bench-base at {base or '?'}" + ("" if base == head else f" (move it to {head} and rebuild)"))
    return 0


def cmd_section(args):
    if not args:
        sys.exit("give a heading, e.g. util/chunk.py section 'Modernisation policy'")
    text = section(read("PLAN.md"), rf"^#+ .*{re.escape(args[0])}")
    if not text:
        sys.exit(f"no PLAN heading contains {args[0]!r}")
    print(text)
    return 0


def cmd_commits(args):
    if not args:
        sys.exit("give the chunk's base commit")
    hashes = git("log", "--reverse", "--format=%h", f"{args[0]}..HEAD").split()
    print(", ".join(hashes + ["(this log entry)"]))
    for line in git("log", "--reverse", "--format=%h %s", f"{args[0]}..HEAD").splitlines():
        print("  " + line)
    return 0


def cmd_gates(args):
    log = os.path.join(ROOT, os.environ.get("OM_BUILD_DIR", "build-rework"), "gates.log")
    if not os.path.exists(log):
        sys.exit(f"no {log}; run util/gates.sh")
    commits = None
    if args:
        commits = set(git("log", "--format=%h", f"{args[0]}..HEAD").split())
    with open(log, encoding="utf-8") as fh:
        blocks = re.split(r"^(?=== )", fh.read(), flags=re.M)
    for block in blocks:
        m = re.match(r"== (\S+ \S+) HEAD (\w+)", block)
        if m and (commits is None or m.group(2) in commits):
            print(block.rstrip())
    return 0


def cmd_check_docs(_):
    problems = []
    readme, progress = read("README.md"), read("PROGRESS.md")

    bridge_files = set()
    for f in git("ls-files", "model").split():
        if f.endswith(".h"):
            with open(os.path.join(ROOT, f), encoding="utf-8", errors="replace") as fh:
                if re.search(r"^namespace bridge \{", fh.read(), re.M):
                    bridge_files.add(os.path.basename(f))
    m = re.search(r"pointer in `namespace bridge` \((.*?)\) to the object", readme, re.S)
    listed = {os.path.basename(p) for p in re.findall(r"`([\w/]+\.h)`", m.group(1))} if m else set()
    if bridge_files or m:
        for f in sorted(bridge_files - listed):
            problems.append(f"README bridge list lacks {f}, which declares namespace bridge")
        for f in sorted(listed - bridge_files):
            problems.append(f"README bridge list names {f}, which no longer declares namespace bridge")

    table = progress[progress.find("- Module status"):]
    table = table[: table.find("\n\n", table.find("|"))]
    with open(os.path.join(ROOT, ".git-blame-ignore-revs"), encoding="utf-8") as fh:
        comment = ""
        for line in fh:
            line = line.strip()
            if line.startswith("#"):
                comment = line
            elif re.fullmatch(r"[0-9a-f]{40}", line) and "(util/style.sh" in comment:
                if not re.search(rf"\b{line[:8]}", table):
                    problems.append(f"module status table lacks {line[:8]} ({comment.lstrip('# ')})")

    rows = {c: s for c, s, _ in status_table(progress)}
    for title, _ in log_entries(progress):
        m = re.match(r"([A-Z]\d+[a-z]?\d?): .*\(done, ", title)
        if m and rows.get(m.group(1)) != "done":
            problems.append(f"{m.group(1)} has a 'done' log entry but status {rows.get(m.group(1))!r} in the table")

    for p in problems:
        print(p)
    print("check-docs: " + (f"{len(problems)} finding(s)" if problems else "clean"))
    return 1 if problems else 0


COMMANDS = {"next": cmd_next, "section": cmd_section, "commits": cmd_commits, "gates": cmd_gates,
            "check-docs": cmd_check_docs}


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        return 2
    return COMMANDS[sys.argv[1]](sys.argv[2:])


if __name__ == "__main__":
    sys.exit(main())
