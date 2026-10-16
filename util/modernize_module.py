"""Apply the safe modernize-* and Core Guidelines fix-its to one module directory.

Run through `util/style.sh modernize`, which supplies the LLVM version, build dir and
check list.
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys
import tempfile

import yaml

REPORT_EXTRA_CHECKS = "misc-const-correctness"
REQUIRED_HEADERS = {"std::numbers::": "numbers"}
INCLUDE_RE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"][^\n]*\n", re.MULTILINE)
CODE_NOISE_RE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)
PATTERNS = (
    ("owning raw pointer: new", re.compile(r"\bnew\b")),
    ("owning raw pointer: delete", re.compile(r"(?<!= )(?<!=)\bdelete\b")),
    ("printf-family / stream formatting", re.compile(r"\b(?:s?n?printf|fprintf|\w*stringstream)\b")),
    ("macro", re.compile(r"^[ \t]*#[ \t]*define[ \t]+(?!HMOD_|H_)\w+", re.MULTILINE)),
    ("plain enum", re.compile(r"\benum\b(?!\s+(?:class|struct)\b)")),
)


def parse_args():
    p = argparse.ArgumentParser(prog="util/style.sh modernize", description=__doc__)
    p.add_argument(
        "module",
        nargs="+",
        help="module directory, e.g. model/PkPd; or the files of a module that shares its directory with others",
    )
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="print the fixes of the first pass, change nothing")
    mode.add_argument(
        "--report", action="store_true", help="list the findings left for the hand commit, change nothing"
    )
    p.add_argument("--passes", type=int, default=5, help="maximum fix passes (default 5)")
    p.add_argument("-j", type=int, default=os.cpu_count())
    p.add_argument("--checks", required=True, help="clang-tidy checks whose fix-its are applied")
    p.add_argument("--llvm-version", required=True)
    p.add_argument("--build-dir", required=True)
    return p.parse_args()


class Modernizer:
    def __init__(self, args):
        self.args = args
        self.root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
        self.paths = []
        for m in args.module:
            path = os.path.normpath(m)
            if os.path.isdir(os.path.join(self.root, path)):
                path += "/"
            elif not os.path.isfile(os.path.join(self.root, path)):
                sys.exit(f"no such directory or file: {path}")
            self.paths.append(path)
        self.module = " ".join(self.paths)
        build = args.build_dir
        self.build = build if os.path.isabs(build) else os.path.join(self.root, build)
        if not os.path.isfile(os.path.join(self.build, "compile_commands.json")):
            sys.exit(f"missing {self.build}/compile_commands.json; configure with cmake first")

    def rel(self, path):
        return os.path.relpath(path, self.root)

    def in_module(self, path):
        rel = self.rel(os.path.join(self.root, path))
        return any(rel.startswith(p) if p.endswith("/") else rel == p for p in self.paths)

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True)

    def run_tidy(self, out_dir, checks):
        cmd = [
            "uvx",
            "--from",
            f"clang-tidy=={self.args.llvm_version}",
            "run-clang-tidy.py",
            "-p",
            self.build,
            "-j",
            str(self.args.j),
            "-quiet",
            f"-checks={checks}",
            "-header-filter=^("
            + "|".join(re.escape(os.path.join(self.root, p)) + ("" if p.endswith("/") else "$") for p in self.paths)
            + ")",
            "-export-fixes",
            out_dir + "/",
        ]
        if sys.platform == "darwin":
            sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
            cmd.append(f"-extra-arg-before=-isysroot{sdk}")
        build_re = re.escape(os.path.realpath(self.build))
        cmd.append(f"^({re.escape(self.root)}/(model|unittest)/|{build_re}/unittest/)")
        result = subprocess.run(
            cmd, check=False, cwd=self.root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )
        if result.returncode not in (0, 1):
            sys.exit(f"run-clang-tidy failed ({result.returncode}):\n{result.stdout[-4000:]}")
        errors = [l for l in result.stdout.splitlines() if ": error: " in l]
        if errors:
            sys.exit("compile errors during clang-tidy run:\n" + "\n".join(errors[:20]))

    def diagnostics(self, checks):
        with tempfile.TemporaryDirectory(prefix="om-modernize-") as tmp:
            self.run_tidy(tmp, checks)
            seen = {}
            for f in sorted(glob.glob(os.path.join(tmp, "*.yaml"))):
                with open(f) as fh:
                    doc = yaml.safe_load(fh)
                for diag in (doc or {}).get("Diagnostics") or []:
                    msg = diag["DiagnosticMessage"]
                    if not msg.get("FilePath") or not self.in_module(msg["FilePath"]):
                        continue
                    reps = tuple(
                        sorted(
                            (r["FilePath"], r["Offset"], r["Length"], r["ReplacementText"])
                            for r in msg.get("Replacements") or []
                        )
                    )
                    key = (diag["DiagnosticName"], msg["FilePath"], msg["FileOffset"], reps)
                    seen.setdefault(
                        key,
                        {
                            "check": diag["DiagnosticName"],
                            "message": msg["Message"],
                            "path": msg["FilePath"],
                            "offset": msg["FileOffset"],
                            "reps": reps,
                        },
                    )
        return sorted(seen.values(), key=lambda d: (d["path"], d["offset"], d["check"]))

    def location(self, diag):
        with open(diag["path"], "rb") as fh:
            line = fh.read(diag["offset"]).count(b"\n") + 1
        return f"{self.rel(diag['path'])}:{line}"

    @staticmethod
    def conflicts(rep, taken):
        _, offset, length, text = rep
        end = offset + length
        for t_offset, t_length, t_text in taken:
            t_end = t_offset + t_length
            if (offset, length, text) == (t_offset, t_length, t_text):
                continue
            if length == 0 and t_length == 0:
                if offset == t_offset:
                    return True
            elif length == 0:
                if t_offset <= offset < t_end:
                    return True
            elif t_length == 0:
                if offset <= t_offset < end:
                    return True
            elif max(offset, t_offset) < min(end, t_end):
                return True
        return False

    @staticmethod
    def drop_existing_includes(diags):
        includes = {}
        for d in diags:
            kept = []
            for rep in d["reps"]:
                path, _, length, text = rep
                if length == 0 and INCLUDE_RE.fullmatch(text):
                    if path not in includes:
                        with open(path, encoding="utf-8") as fh:
                            includes[path] = {m.group().strip() for m in INCLUDE_RE.finditer(fh.read())}
                    if text.strip() in includes[path]:
                        continue
                kept.append(rep)
            d["reps"] = tuple(kept)

    def select(self, diags):
        self.drop_existing_includes(diags)
        taken = collections.defaultdict(set)
        accepted, deferred, crossing, nofix = [], [], [], []
        for d in diags:
            if not d["reps"]:
                nofix.append(d)
            elif not all(self.in_module(r[0]) for r in d["reps"]):
                crossing.append(d)
            elif any(self.conflicts(r, taken[r[0]]) for r in d["reps"]):
                deferred.append(d)
            else:
                accepted.append(d)
                for path, offset, length, text in d["reps"]:
                    taken[path].add((offset, length, text))
        return accepted, deferred, crossing, nofix, taken

    def apply(self, taken):
        with tempfile.TemporaryDirectory(prefix="om-modernize-apply-") as tmp:
            reps = [
                {"FilePath": path, "Offset": offset, "Length": length, "ReplacementText": text}
                for path, entries in sorted(taken.items())
                for offset, length, text in sorted(entries)
            ]
            doc = {
                "MainSourceFile": "",
                "Diagnostics": [
                    {
                        "DiagnosticName": "modernize",
                        "DiagnosticMessage": {
                            "Message": "modernize",
                            "FilePath": reps[0]["FilePath"],
                            "FileOffset": reps[0]["Offset"],
                            "Replacements": reps,
                        },
                        "Level": "Warning",
                        "BuildDirectory": self.build,
                    }
                ],
            }
            with open(os.path.join(tmp, "fixes.yaml"), "w") as fh:
                yaml.safe_dump(doc, fh)
            subprocess.run(
                ["uvx", "--from", f"clang-tidy=={self.args.llvm_version}", "clang-apply-replacements", tmp],
                check=True,
            )

    def add_missing_headers(self, taken):
        added = collections.Counter()
        for path in taken:
            with open(path, encoding="utf-8") as fh:
                text = fh.read()
            code = CODE_NOISE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)
            for token, header in REQUIRED_HEADERS.items():
                if token not in code or re.search(rf"#\s*include\s*<{header}>", text):
                    continue
                includes = list(INCLUDE_RE.finditer(text))
                at = includes[-1].end() if includes else 0
                text = f"{text[:at]}#include <{header}>\n{text[at:]}"
                added[f"#include <{header}>"] += 1
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(text)
        return added

    def print_diags(self, title, diags):
        if diags:
            print(f"\n{title}:")
            for d in diags:
                print(f"{self.location(d)}: [{d['check']}] {d['message']}")

    def fix(self):
        if not self.args.dry_run and self.git("status", "--porcelain", "--", "model", "unittest").strip():
            sys.exit("model/ or unittest/ has uncommitted changes; commit them so the fix-its are their own commit")
        applied = collections.Counter()
        deferred = crossing = nofix = []
        for n in range(1, self.args.passes + 1):
            print(f"fix pass {n}: clang-tidy over all TUs in {self.rel(self.build)} ...", file=sys.stderr)
            accepted, deferred, crossing, nofix, taken = self.select(self.diagnostics(self.args.checks))
            if self.args.dry_run:
                self.print_diags("Fixes the first pass would apply", accepted)
                break
            if not accepted:
                break
            applied.update(d["check"] for d in accepted)
            self.apply(taken)
            applied.update(self.add_missing_headers(taken))
        else:
            print(f"\nstopped after {self.args.passes} passes with fixes still pending", file=sys.stderr)
        if not self.args.dry_run:
            outside = [
                f
                for f in self.git("diff", "--name-only", "HEAD", "--", "model", "unittest").split()
                if not self.in_module(f)
            ]
            if outside:
                sys.exit("changed files outside the module: " + ", ".join(outside))
            if applied:
                print("\nApplied: " + ", ".join(f"{n} {c}" for c, n in applied.most_common()))
            else:
                print(f"nothing to modernize in {self.module}")
        self.print_diags("Overlapping fixes left for the next run", deferred)
        self.print_diags("Not applied: the fix reaches outside the module", crossing)
        self.print_diags("No fix-it (fix by hand)", nofix)
        return 0

    def module_files(self):
        files = self.git("ls-files", "--", *self.paths).split()
        return [f for f in files if f.endswith((".h", ".cpp"))]

    def report(self):
        checks = "-readability-identifier-naming," + REPORT_EXTRA_CHECKS
        config_checks = self.config_checks()
        diags = self.diagnostics(f"{config_checks},{checks}")
        by_check = collections.defaultdict(list)
        for d in diags:
            by_check[d["check"]].append(d)
        print(f"clang-tidy findings in {self.module}, by check:")
        for check in sorted(by_check, key=lambda c: (-len(by_check[c]), c)):
            print(f"\n[{check}] {len(by_check[check])}")
            for d in by_check[check]:
                print(f"  {self.location(d)}: {d['message']}")
        print("\nText patterns for the hand commit:")
        for title, pattern in PATTERNS:
            hits = []
            for f in self.module_files():
                with open(os.path.join(self.root, f), encoding="utf-8", errors="replace") as fh:
                    code = CODE_NOISE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), fh.read())
                hits += [f"{f}:{code.count(chr(10), 0, m.start()) + 1}" for m in pattern.finditer(code)]
            print(f"\n[{title}] {len(hits)}")
            for h in hits:
                print(f"  {h}")
        return 0

    def config_checks(self):
        with open(os.path.join(self.root, ".clang-tidy")) as fh:
            checks = yaml.safe_load(fh)["Checks"]
        return ",".join(c.strip() for c in checks.split(",") if c.strip())

    def main(self):
        return self.report() if self.args.report else self.fix()


if __name__ == "__main__":
    sys.exit(Modernizer(parse_args()).main())
