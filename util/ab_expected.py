#!/usr/bin/env python3
"""Check a deliberate console change: run util/ab_scenarios.py variants with a
base and a candidate openMalaria and require, for every run, that the
candidate's exit code, stdout and stderr equal the named transform applied to
the base's, and that the files they write are equal."""

import argparse
import os
import re
import shutil
import sys
import tempfile

import ab_scenarios as ab

PROGRESS_SEGMENT = re.compile(r"\r\d+%\t")
STALE_PERROR = re.compile(r"OpenMalaria: No such file or directory\n\Z")


def collapse_progress(code, stdout, stderr):
    out, last, pos = [], None, 0
    for m in PROGRESS_SEGMENT.finditer(stderr):
        out.append(stderr[pos:m.start()])
        if m.group() != last:
            out.append(m.group())
        last, pos = m.group(), m.end()
    out.append(stderr[pos:])
    return code, stdout, "".join(out)


def drop_stale_perror(code, stdout, stderr):
    line_start = stderr.rfind("\n", 0, len(stderr) - 1) + 1
    if stderr.startswith("Command-line error: ", line_start):
        return code, stdout, STALE_PERROR.sub("\n", stderr)
    return code, stdout, STALE_PERROR.sub("", stderr)


TRANSFORMS = {
    "progress": (collapse_progress, "F1: one progress segment per percent, not per step"),
    "f2": (drop_stale_perror, "F2: no trailing perror; Command-line error ends its line"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base")
    parser.add_argument("candidate")
    parser.add_argument("transforms", help=f"comma-separated, applied in order: "
                                           f"{'; '.join(f'{k} ({d})' for k, (_, d) in sorted(TRANSFORMS.items()))}")
    parser.add_argument("groups", nargs="*", default=ab.DEFAULT_GROUPS + ["cli"])
    opts = parser.parse_args()
    names = opts.transforms.split(",")
    for name in names:
        if name not in TRANSFORMS:
            parser.error(f"unknown transform {name}")

    def transform(*run):
        for name in names:
            run = TRANSFORMS[name][0](*run)
        return run

    root = tempfile.mkdtemp(prefix="ab_expected-")
    changed = failed = total = 0
    try:
        for g in opts.groups:
            for v in ab.GROUPS[g]:
                text = v.transform(ab.read(v.scenario))
                base = ab.run(os.path.abspath(opts.base), v, text, os.path.join(root, g, v.name, "base"))
                cand = ab.run(os.path.abspath(opts.candidate), v, text, os.path.join(root, g, v.name, "candidate"))
                expected = [transform(*r) for r in base[0]]
                ok = cand[0] == expected and cand[1] == base[1]
                total += 1
                failed += not ok
                changed += ok and cand[0] != base[0]
                if not ok:
                    print(f"FAIL  {g}/{v.name}  runs as expected: {cand[0] == expected}, files equal: "
                          f"{cand[1] == base[1]}")
    finally:
        shutil.rmtree(root)
    print(f"{opts.transforms}: {failed} of {total} variants differ from the expected change "
          f"({changed} changed as expected)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
