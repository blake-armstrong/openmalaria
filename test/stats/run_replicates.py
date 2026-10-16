#!/usr/bin/env python3
"""Run one OpenMalaria scenario once per seed and collect each run's outputs.

Each replicate runs in its own temporary directory, on a copy of the scenario whose
<computationParameters iseed> is set to the seed. Outputs land in OUT/<seed>/output.txt,
plus OUT/<seed>/ctsout.txt when the scenario writes one; OUT/meta.json records what
produced them. Seeds that already have an output are skipped, so rerunning the same
command resumes an interrupted job.

Paths, the schema link and binary-to-text conversion come from the CMake-configured
<build>/test/run.py, so the build dir must be configured and openMalaria built.
"""

import argparse
import concurrent.futures
import datetime
import gzip
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ISEED = re.compile(rb"""(<computationParameters\b[^>]*?\biseed\s*=\s*)(["'])(-?\d+)\2""")
INT32 = range(-(2**31), 2**31)
DEFAULT_FIRST_SEED = 1001
STDERR_TAIL = 2000


class ReplicateError(Exception):
    pass


@dataclass(frozen=True)
class Job:
    run_py: object
    scenario_bytes: bytes
    schema: Path
    prefix: str
    out: Path
    binary: bool


def load_run_py(build_dir):
    path = build_dir / "test" / "run.py"
    if not path.is_file():
        raise ReplicateError(f"{path} not found; configure {build_dir} with CMake first")
    spec = importlib.util.spec_from_file_location("om_test_run", path)
    module = importlib.util.module_from_spec(spec)
    saved_path = list(sys.path)
    try:
        spec.loader.exec_module(module)
    except (Exception, SystemExit) as e:
        raise ReplicateError(f"loading {path}: {e}") from e
    finally:
        sys.path[:] = saved_path
    return module


def resolve_scenario(run_py, name):
    named = Path(run_py.testSrcDir) / f"scenario{name}.xml"
    if named.is_file():
        return named, name
    path = Path(name)
    if path.is_file():
        return path.resolve(), path.stem
    raise ReplicateError(f"no such scenario file {named} or {name}")


def resolve_schema(run_py, scenario):
    name = run_py.getSchemaName(str(scenario))
    for base in (run_py.testSrcDir, run_py.testBuildDir):
        path = Path(base).parent / "schema" / name
        if path.is_file():
            return path
    raise ReplicateError(f"can't find {name}")


def with_seed(scenario_bytes, seed):
    text, count = ISEED.subn(lambda m: m[1] + m[2] + str(seed).encode() + m[2], scenario_bytes)
    if count != 1:
        raise ReplicateError(f"expected one <computationParameters iseed>, found {count}")
    return text


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout.strip()


def source_revision(src_dir):
    try:
        commit = git("-C", str(src_dir), "rev-parse", "HEAD")
        dirty = bool(git("-C", str(src_dir), "status", "--porcelain", "--untracked-files=no"))
    except (OSError, subprocess.CalledProcessError):
        return None, None
    return commit, dirty


def write_meta(out, meta):
    path = out / "meta.json"
    if path.is_file():
        existing = json.loads(path.read_text())
        for key in ("scenario_sha256", "binary_sha256", "output_format"):
            if existing.get(key) != meta[key]:
                raise ReplicateError(
                    f"{path} has {key} {existing.get(key)}, this run has {meta[key]}; use a new output dir"
                )
        return
    path.write_text(json.dumps(meta, indent=2) + "\n")


def decompress(src, dest):
    with gzip.open(src, "rb") as f_in, open(dest, "wb") as f_out:
        shutil.copyfileobj(f_in, f_out)


def collect(job, sim_dir, dest):
    output = dest / "output.txt"
    if (sim_dir / "output.txt").is_file():
        shutil.copy2(sim_dir / "output.txt", output)
    elif (sim_dir / "output.txt.gz").is_file():
        decompress(sim_dir / "output.txt.gz", output)
    elif job.binary:
        for name in ("output.bin", "output.bin.gz"):
            if (sim_dir / name).is_file():
                job.run_py.binaryToText(str(sim_dir / name), str(output))
                break
    if not output.is_file():
        raise ReplicateError(f"no survey output in {sim_dir}")
    if (sim_dir / "ctsout.txt").is_file():
        shutil.copy2(sim_dir / "ctsout.txt", dest / "ctsout.txt")
    elif (sim_dir / "ctsout.txt.gz").is_file():
        decompress(sim_dir / "ctsout.txt.gz", dest / "ctsout.txt")


def run_seed(job, seed):
    dest = job.out / str(seed)
    if (dest / "output.txt").is_file():
        return None
    start = time.monotonic()
    with tempfile.TemporaryDirectory(prefix=f"{job.prefix}-{seed}-") as tmp:
        sim_dir = Path(tmp)
        scenario = sim_dir / "scenario.xml"
        scenario.write_bytes(with_seed(job.scenario_bytes, seed))
        job.run_py.linkOrCopy(str(job.schema), str(sim_dir / job.schema.name))
        cmd = [
            job.run_py.openMalariaExec,
            "--resource-path", str(Path(job.run_py.testSrcDir).resolve()),
            "--scenario", str(scenario),
        ]
        if job.binary:
            cmd += ["--output-format", "bin"]
        proc = subprocess.run(cmd, check=False, cwd=sim_dir, capture_output=True, text=True)
        if proc.returncode != 0:
            raise ReplicateError(
                f"seed {seed}: openMalaria exited with {proc.returncode}\n{proc.stderr[-STDERR_TAIL:]}"
            )
        partial = job.out / f"{seed}.partial"
        shutil.rmtree(partial, ignore_errors=True)
        partial.mkdir()
        collect(job, sim_dir, partial)
    shutil.rmtree(dest, ignore_errors=True)
    partial.replace(dest)
    return time.monotonic() - start


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("scenario", help="NAME for test/scenarioNAME.xml, or a path to a scenario file")
    parser.add_argument("-o", "--out", required=True, type=Path, help="output directory")
    seeds = parser.add_mutually_exclusive_group()
    seeds.add_argument("-n", "--replicates", type=int, default=1, help="number of seeds (default: 1)")
    seeds.add_argument("--seeds", type=int, nargs="+", help="explicit seeds, instead of -n")
    parser.add_argument(
        "--first-seed", type=int, default=DEFAULT_FIRST_SEED,
        help=f"first of the -n consecutive seeds (default: {DEFAULT_FIRST_SEED})",
    )
    parser.add_argument(
        "-j", "--jobs", type=int, default=os.cpu_count(), help="parallel runs (default: all cores)"
    )
    parser.add_argument(
        "--build-dir", type=Path, default=Path(os.environ.get("OM_BUILD_DIR", "build")),
        help="CMake build dir, relative to the repo root unless absolute (default: $OM_BUILD_DIR or build)",
    )
    parser.add_argument("-b", "--binary", action="store_true", help="have openMalaria write binary survey output; the decoded output.txt lists integer measures "
        "before double ones, so it matches text output by (survey, group, measure), not line by line",
    )
    parser.add_argument("-q", "--quiet", action="store_true", help="print only errors")
    args = parser.parse_args(argv)
    args.seeds = list(dict.fromkeys(args.seeds)) if args.seeds else [
        args.first_seed + i for i in range(args.replicates)
    ]
    if args.replicates < 1 or args.jobs < 1:
        parser.error("-n and -j must be at least 1")
    if any(seed not in INT32 for seed in args.seeds):
        parser.error("seeds must fit in a 32-bit int (the schema's iseed is xs:int)")
    if not args.build_dir.is_absolute():
        args.build_dir = ROOT / args.build_dir
    return args


def prepare(build_dir, scenario_name, out, binary=False):
    run_py = load_run_py(build_dir)
    scenario, prefix = resolve_scenario(run_py, scenario_name)
    schema = resolve_schema(run_py, scenario)
    scenario_bytes = scenario.read_bytes()
    with_seed(scenario_bytes, 0)
    commit, dirty = source_revision(run_py.testSrcDir)
    out.mkdir(parents=True, exist_ok=True)
    write_meta(out, {
        "scenario": str(scenario),
        "scenario_sha256": hashlib.sha256(scenario_bytes).hexdigest(),
        "schema": schema.name,
        "binary": run_py.openMalariaExec,
        "binary_sha256": sha256(run_py.openMalariaExec),
        "commit": commit,
        "dirty": dirty,
        "output_format": "bin" if binary else "txt",
        "created": datetime.datetime.now(datetime.UTC).isoformat(timespec="seconds"),
    })
    return Job(run_py, scenario_bytes, schema, prefix, out.resolve(), binary)


def run_seeds(job, seeds, jobs, quiet=False):
    failed = []
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(run_seed, job, seed): seed for seed in seeds}
        try:
            for future in concurrent.futures.as_completed(futures):
                seed = futures[future]
                done += 1
                try:
                    elapsed = future.result()
                except (ReplicateError, OSError) as e:
                    failed.append(seed)
                    print(f"[{done}/{len(futures)}] error: {e}", file=sys.stderr)
                    continue
                if not quiet:
                    status = "exists, skipped" if elapsed is None else f"done in {elapsed:.1f} s"
                    print(f"[{done}/{len(futures)}] seed {seed} {status}", file=sys.stderr)
        except KeyboardInterrupt:
            pool.shutdown(cancel_futures=True)
            raise
    return sorted(failed)


def main(argv):
    args = parse_args(argv)
    try:
        job = prepare(args.build_dir, args.scenario, args.out, args.binary)
    except (ReplicateError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    failed = run_seeds(job, args.seeds, args.jobs, args.quiet)
    if failed:
        print(f"{len(failed)} of {len(args.seeds)} seeds failed: {failed}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
