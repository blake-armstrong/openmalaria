#!/usr/bin/env python3
"""Plant a bug, show which gates catch it, and restore the code.

Each --replace FILE OLD NEW replaces the one occurrence of OLD in FILE (it
must occur exactly once); --patch FILE applies a unified diff instead. The
touched files are saved first and copied back at the end, whatever happens,
then the Release build is rebuilt so later gates test the restored code.

The planted build must compile: a build error stops the run with exit 2, so
no gate is ever run against a stale binary. Then ctest (-LE statistical) runs
in the Release build, and util/ab_scenarios.py for the given groups against
the base build. Exit 0 if something caught the bug, 1 if nothing did.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def parse_args():
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--replace", nargs=3, action="append", default=[], metavar=("FILE", "OLD", "NEW"))
    p.add_argument("--patch", help="unified diff to apply with git apply")
    p.add_argument("--ab", nargs="*", metavar="GROUP", help="A/B groups to run (no group given: all)")
    p.add_argument("--no-ctest", action="store_true", help="skip ctest")
    p.add_argument("--name", default="planted bug", help="label for the report")
    p.add_argument("--build", default=os.environ.get("OM_BUILD_DIR", "build-rework"))
    p.add_argument("--base", default=os.environ.get("OM_BASE_BUILD", "../om-bench-base/build-rework"))
    p.add_argument("-j", dest="jobs", default=os.environ.get("OM_JOBS", "4"))
    args = p.parse_args()
    if not args.replace and not args.patch:
        p.error("give --replace or --patch")
    if args.no_ctest and args.ab is None:
        p.error("nothing to run: --no-ctest without --ab")
    return args


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=kw.pop("cwd", ROOT), text=True, capture_output=True, **kw)


def touched_files(args):
    files = [os.path.normpath(f) for f, _, _ in args.replace]
    if args.patch:
        out = run(["git", "apply", "--numstat", args.patch]).stdout
        files += [line.split("\t")[2] for line in out.splitlines() if line.count("\t") >= 2]
    return sorted(set(files))


def plant(args):
    for path, old, new in args.replace:
        full = os.path.join(ROOT, path)
        with open(full, encoding="utf-8") as fh:
            text = fh.read()
        n = text.count(old)
        if n != 1:
            raise SystemExit(f"{path}: OLD occurs {n} times, must be once: {old!r}")
        with open(full, "w", encoding="utf-8") as fh:
            fh.write(text.replace(old, new))
    if args.patch:
        r = run(["git", "apply", args.patch])
        if r.returncode:
            raise SystemExit(f"git apply failed:\n{r.stderr}")


def build(args):
    return run(["cmake", "--build", args.build, "-j", args.jobs])


def main():
    args = parse_args()
    files = touched_files(args)
    saved = tempfile.mkdtemp(prefix="om-plant-")
    for f in files:
        os.makedirs(os.path.dirname(os.path.join(saved, f)), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, f), os.path.join(saved, f))
    caught = []
    status = 0
    try:
        plant(args)
        b = build(args)
        if b.returncode:
            errors = [line for line in b.stdout.splitlines() if "error" in line][:10]
            print(f"{args.name}: the planted build does not compile; no gate run\n" + "\n".join(errors))
            status = 2
            return status
        if not args.no_ctest:
            r = run(["caffeinate", "-i", "ctest", "-j", args.jobs, "-LE", "statistical"], cwd=os.path.join(ROOT, args.build))
            failing = re.findall(r"^\s+\d+ - (\S+) \(", r.stdout, re.M)
            total = re.search(r"out of (\d+)", r.stdout)
            print(f"ctest: {len(failing)} of {total.group(1) if total else '?'} fail" +
                  (f": {', '.join(failing)}" if failing else ""))
            if failing:
                caught.append(f"{len(failing)} ctest tests fail")
        if args.ab is not None:
            cmd = ["caffeinate", "-i", sys.executable, "util/ab_scenarios.py",
                   os.path.join(args.base, "openMalaria"), os.path.join(args.build, "openMalaria"), *args.ab]
            r = run(cmd)
            diffs = [line.split()[1] for line in r.stdout.splitlines() if line.startswith("DIFF")]
            summary = (re.findall(r"^\d+ of \d+ variants differ$", r.stdout, re.M) or [f"A/B exit {r.returncode}"])[-1]
            print(f"A/B ({', '.join(args.ab) or 'all groups'}): {summary}" + (f": {', '.join(diffs)}" if diffs else ""))
            if diffs:
                caught.append(f"{len(diffs)} A/B variants differ")
        status = 0 if caught else 1
        print(f"{args.name}: " + ("caught (" + "; ".join(caught) + ")" if caught else "NOT CAUGHT"))
        return status
    finally:
        for f in files:
            shutil.copy2(os.path.join(saved, f), os.path.join(ROOT, f))
        if any(subprocess.run(["cmp", "-s", os.path.join(saved, f), os.path.join(ROOT, f)]).returncode for f in files):
            print(f"RESTORE FAILED: compare {saved} with the tree", file=sys.stderr)
            status = 3
        elif build(args).returncode:
            print("restored, but the rebuild failed", file=sys.stderr)
            status = 3
        else:
            print(f"restored {', '.join(files)} and rebuilt {args.build}")
        sys.exit(status)


if __name__ == "__main__":
    main()
