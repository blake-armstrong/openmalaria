#!/usr/bin/env python3
"""F1 check: with --progress --verbose, the candidate's stderr must equal the
base's with consecutive repeats of each "\\r N%\\t" segment collapsed, and
stdout and exit codes must be equal."""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST = os.path.join(REPO, "test")
SEGMENT = re.compile(rb"\r\d+%\t")


def collapse(stderr):
    out, last, pos = [], None, 0
    for m in SEGMENT.finditer(stderr):
        out.append(stderr[pos:m.start()])
        if m.group() != last:
            out.append(m.group())
        last, pos = m.group(), m.end()
    out.append(stderr[pos:])
    return b"".join(out)


def run(binary, scenario, work):
    shutil.copy(os.path.join(os.path.dirname(binary), "schema", "scenario_current.xsd"), work)
    return subprocess.run([binary, "--resource-path", TEST, "--scenario", os.path.join(TEST, scenario),
                           "--progress", "--verbose"], cwd=work, capture_output=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base")
    parser.add_argument("candidate")
    parser.add_argument("scenarios", nargs="*", default=["scenario5.xml", "scenarioVecTest.xml"])
    opts = parser.parse_args()
    failed = 0
    for scenario in opts.scenarios:
        with tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            base = run(os.path.abspath(opts.base), scenario, b)
            cand = run(os.path.abspath(opts.candidate), scenario, c)
        ok = (base.returncode == cand.returncode and base.stdout == cand.stdout
              and cand.stderr == collapse(base.stderr))
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'}  {scenario}  exit {base.returncode}/{cand.returncode}  progress segments "
              f"{len(SEGMENT.findall(base.stderr))} -> {len(SEGMENT.findall(cand.stderr))}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
