#!/usr/bin/env python3
"""G-two-sims: run several scenarios in one process and check that each gives output.txt
and ctsout.txt byte-identical to test/expected/, i.e. to a run of its own.

The driver (openMalaria-two-sims) runs one Simulation per group of command-line arguments:
by default in order, each constructed after the previous one is destroyed; with --interleave
all alive together, taking turns one time step at a time. The run happens in a fresh
temporary directory, which is removed afterwards unless the check fails or --keep is given.
"""
import argparse
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile

TEST = os.path.dirname(os.path.abspath(__file__))
SCHEMA = os.path.join(os.path.dirname(TEST), "schema")


def schema_name(path):
    with open(path) as f:
        return re.search(r'schemaLocation="\S+ (\S+)"', f.read()).group(1)


def link_schema(driver, scenario, work):
    xsd = schema_name(scenario)
    target = os.path.join(work, xsd)
    if not os.path.exists(target):
        built = os.path.join(os.path.dirname(os.path.abspath(driver)), "schema", xsd)
        os.symlink(built if os.path.isfile(built) else os.path.join(SCHEMA, xsd), target)


def check(work, index, name):
    failures = []
    for kind in ("output", "ctsout"):
        expected = os.path.join(TEST, "expected", f"{kind}{name}.txt")
        actual = os.path.join(work, f"{kind}-{index}-{name}.txt")
        if not os.path.isfile(expected):
            if os.path.isfile(actual):
                failures.append(f"{os.path.basename(actual)} written, but no expected file")
        elif not os.path.isfile(actual):
            failures.append(f"{os.path.basename(actual)} missing")
        elif not filecmp.cmp(expected, actual, shallow=False):
            failures.append(f"{os.path.basename(actual)} differs from expected/{kind}{name}.txt")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("driver", help="the openMalaria-two-sims binary")
    parser.add_argument("scenarios", nargs="+", help="scenario names, as in test/scenario<NAME>.xml, run in order")
    parser.add_argument("--interleave", action="store_true", help="interleave the simulations' time steps")
    parser.add_argument("--keep", action="store_true", help="keep the working directory")
    parser.add_argument("--timeout", type=float, default=120, help="seconds before the driver is killed (default 120)")
    opts = parser.parse_args()

    work = tempfile.mkdtemp(prefix="two_sims-")
    cmd = [os.path.abspath(opts.driver)] + (["--interleave"] if opts.interleave else [])
    for index, name in enumerate(opts.scenarios):
        scenario = os.path.join(TEST, f"scenario{name}.xml")
        link_schema(opts.driver, scenario, work)
        if index:
            cmd.append("--next")
        cmd += ["--resource-path", TEST, "--scenario", scenario,
                "--output", f"output-{index}-{name}.txt", "--ctsout", f"ctsout-{index}-{name}.txt"]
    try:
        ret = subprocess.run(cmd, cwd=work, timeout=opts.timeout).returncode
        failures = [] if ret == 0 else [f"driver exit status {ret}"]
    except subprocess.TimeoutExpired:
        failures = [f"driver killed after {opts.timeout:g} s"]
    for index, name in enumerate(opts.scenarios):
        failures += check(work, index, name)
    for failure in failures:
        print(failure)
    if failures or opts.keep:
        print(f"working directory kept: {work}")
    else:
        shutil.rmtree(work)
    mode = "interleaved" if opts.interleave else "one after another"
    print(f"{len(opts.scenarios)} simulations in one process, {mode}: {'FAIL' if failures else 'pass'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
