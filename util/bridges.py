#!/usr/bin/env python3
"""List what is left of the D-stage bridges (docs/refactor/README.md).

For each accessor declared in a `namespace bridge { ... }` block: its direct
uses, the facade functions whose body calls it (e.g. `Sim::now`,
`Genotypes::count`), the functions in the bridge's own files that reach it
through a facade (e.g. `mon::record_event` through `record_value`), and the
call sites of each, qualified as callers outside the facade's class or
namespace write them. Uses inside the facade's own class or namespace, and
calls through function pointers (e.g. `mon::write_survey_data` choosing
`write_text`), are not counted, so the numbers are a lower bound;
`--files` lists the files.

Exit status is 0. With no bridge left it prints "no bridges left".
"""

import argparse
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIRS = ("model", "unittest", "test")
CONTROL = {"if", "for", "while", "switch", "return", "catch", "sizeof", "assert", "throw", "static_assert"}
NOISE_RE = re.compile(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)
BRIDGE_BLOCK_RE = re.compile(r"^namespace bridge \{\n(.*?)^\}", re.S | re.M)
ACCESSOR_RE = re.compile(r"^inline [^\n(]*?\b(\w+)\(\)\s*\{", re.M)
USE_RE = re.compile(r"\bbridge::(\w+)\(\)")
HEAD_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?(?:(?:static|inline|virtual|constexpr|explicit|friend)\s+)*"
    r"(?P<type>(?:const\s+)?[\w:]+(?:<[^()]*>)?(?:\s*[*&]+\s*|\s+))+(?P<name>(?:\w+::)*~?\w+)\s*\("
    r"|^(?P<ctor>(?:\w+::)+~?\w+)\s*\(")
NOT_TYPES = {"return", "throw", "else", "case", "delete", "new", "goto", "co_return", "using", "typedef"}
SCOPE_RE = re.compile(r"\bnamespace\s+([\w:]+)\s*\{|\b(?:class|struct)\s+(\w+)(?:\s+final)?\s*(?::[^;{}]*)?\{|\{|\}")


def tracked_files():
    out = subprocess.check_output(["git", "ls-files", *DIRS], cwd=ROOT, text=True).split()
    return [f for f in out if f.endswith((".h", ".cpp")) and os.path.isfile(os.path.join(ROOT, f))]


def strip_noise(text):
    return NOISE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)


def scopes_at(text, offset):
    """The named namespaces and classes open at offset, outermost first, as (kind, name)."""
    stack = []
    for m in SCOPE_RE.finditer(text, 0, offset):
        if m.group() == "}":
            if stack:
                stack.pop()
        elif m.group(1):
            stack.append(("namespace", m.group(1).split("::")[-1]))
        elif m.group(2):
            stack.append(("class", m.group(2)))
        else:
            stack.append(None)
    return [s for s in stack if s]


def head_name(line):
    """The function name if line starts a function declaration or definition, else None."""
    m = HEAD_RE.match(line)
    if not m:
        return None
    if m.group("ctor"):
        return m.group("ctor")
    first = line.split()[0] if line.split() else ""
    if first in NOT_TYPES or m.group("name").split("::")[-1] in CONTROL:
        return None
    return m.group("name")


def enclosing(text, lines, index):
    """Return (qualifier, name) of the function whose body holds lines[index]."""
    for i in range(index, -1, -1):
        name = head_name(lines[i])
        if not name or lines[i].rstrip().endswith(";"):
            continue
        if "::" in name:
            qualifier, _, short = name.rpartition("::")
            return qualifier, short
        scopes = scopes_at(text, sum(len(line) + 1 for line in lines[:i]))
        classes = [n for kind, n in scopes if kind == "class"]
        if classes:
            return classes[-1], name
        namespaces = [n for kind, n in scopes if kind == "namespace" and n != "OM"]
        return "::".join(namespaces), name
    return "", "?"


def module_files(texts, files):
    """The files with direct uses plus their header/source partners."""
    stems = {os.path.splitext(f)[0] for f in files}
    return [f for f in texts if os.path.splitext(f)[0] in stems]


def add_indirect_facades(texts, files, facades):
    """Add the functions in the bridge's own files that call a facade (record_event → record_value)."""
    pending = set(facades)
    while pending:
        _, callee = pending.pop()
        call_re = re.compile(rf"\b{re.escape(callee)}\s*\(")
        for f in module_files(texts, files):
            text = texts[f]
            lines = text.split("\n")
            for i, line in enumerate(lines):
                if not call_re.search(line):
                    continue
                head = head_name(line)
                if head and head.split("::")[-1] == callee:
                    continue
                qualifier, func = enclosing(text, lines, i)
                if qualifier and func != callee and (qualifier, func) not in facades:
                    facades.add((qualifier, func))
                    pending.add((qualifier, func))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--files", action="store_true", help="list the files of each facade's call sites")
    args = parser.parse_args()

    texts = {}
    for f in tracked_files():
        with open(os.path.join(ROOT, f), encoding="utf-8", errors="replace") as fh:
            texts[f] = strip_noise(fh.read())

    accessors = {}
    for f, text in texts.items():
        for block in BRIDGE_BLOCK_RE.finditer(text):
            for m in ACCESSOR_RE.finditer(block.group(1)):
                accessors[m.group(1)] = f
    if not accessors:
        print("no bridges left")
        return 0

    direct = collections.defaultdict(list)
    facades = collections.defaultdict(set)
    for f, text in texts.items():
        lines = text.split("\n")
        for i, line in enumerate(lines):
            for m in USE_RE.finditer(line):
                name = m.group(1)
                if name not in accessors:
                    continue
                qualifier, func = enclosing(text, lines, i)
                if "Binding" in qualifier or func == name:
                    continue
                direct[name].append((f, i + 1, qualifier, func))
                if qualifier and qualifier != "bridge":
                    facades[name].add((qualifier, func))

    for name in accessors:
        add_indirect_facades(texts, {f for f, _, _, _ in direct[name]}, facades[name])

    for name in sorted(accessors, key=lambda n: (-len(direct[n]), n)):
        uses = direct[name]
        files = sorted({f for f, _, _, _ in uses})
        print(f"bridge::{name}()  (declared in {accessors[name]})")
        print(f"  direct uses: {len(uses)} in {len(files)} file(s): {', '.join(files)}")
        total_sites, total_files = 0, set()
        rows = []
        for qualifier, func in sorted(facades[name]):
            call_re = re.compile(rf"\b{re.escape(qualifier)}::{re.escape(func)}\s*\(")
            hits = collections.Counter()
            for f, text in texts.items():
                n = len(call_re.findall(text))
                if n:
                    hits[f] += n
            definition = rf"^\s*[\w:<>,*&\s]*\b{re.escape(qualifier)}::{re.escape(func)}\s*\([^;]*\{{"
            for f in list(hits):
                hits[f] -= len(re.findall(definition, texts[f], re.M))
                if hits[f] <= 0:
                    del hits[f]
            sites = sum(hits.values())
            total_sites += sites
            total_files |= set(hits)
            rows.append((sites, f"{qualifier}::{func}", hits))
        if rows:
            print(f"  facades: {len(rows)}, {total_sites} qualified call sites in {len(total_files)} file(s)")
            for sites, label, hits in sorted(rows, key=lambda r: (-r[0], r[1])):
                print(f"    {label}: {sites}")
                if args.files:
                    for f, n in sorted(hits.items()):
                        print(f"      {f}: {n}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
