"""Rename the identifiers declared in one module directory to the project naming rules.

Run through `util/style.sh rename`, which supplies the LLVM version and build dir.
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

KIND_RE = re.compile(r"invalid case style for (.+?) '(.+)'$")
PREFIX_RE = re.compile(r"^(m_|_)(?=[A-Za-z])")
CODE_NOISE_RE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)
CODE_NOISE_BYTES_RE = re.compile(CODE_NOISE_RE.pattern.encode(), re.DOTALL)
DIRECTIVE_RE = re.compile(rb"^[ \t]*#[^\n]*", re.MULTILINE)
NAMESPACE_DECL_RE = re.compile(r"\bnamespace\s+([A-Za-z_]\w*(?:\s*::\s*(?:inline\s+)?[A-Za-z_]\w*)*)\s*\{")
TOKEN_RE = re.compile(r"[A-Za-z_]\w*|\d[\w.]*|\S")
LOCAL_KINDS = {"parameter", "local constant", "local variable", "constant parameter"}
MEMBER_KINDS = (
    "MemberCase",
    "ConstantMemberCase",
    "ClassMemberCase",
    "PrivateMemberCase",
    "ProtectedMemberCase",
    "PublicMemberCase",
)


def parse_args():
    p = argparse.ArgumentParser(prog="util/style.sh rename", description=__doc__)
    p.add_argument("module", help="module directory, e.g. model/PkPd")
    p.add_argument(
        "--map",
        action="append",
        default=[],
        metavar="OLD=NEW",
        help="use NEW instead of the tool's name for every symbol named OLD (repeatable)",
    )
    p.add_argument(
        "--skip", action="append", default=[], metavar="OLD", help="leave every symbol named OLD unchanged (repeatable)"
    )
    p.add_argument(
        "--namespace",
        action="append",
        default=None,
        metavar="NAME",
        help="namespace the module owns and renames (repeatable); default: namespaces declared only inside the module",
    )
    p.add_argument("--dry-run", action="store_true", help="print the plan, change nothing")
    p.add_argument("-j", type=int, default=os.cpu_count())
    p.add_argument("--llvm-version", required=True)
    p.add_argument("--build-dir", required=True)
    return p.parse_args()


class Renamer:
    def __init__(self, args):
        self.args = args
        self.root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
        self.module = os.path.normpath(args.module).rstrip("/") + "/"
        if not os.path.isdir(os.path.join(self.root, self.module)):
            sys.exit(f"no such directory: {self.module}")
        build = args.build_dir
        self.build = build if os.path.isabs(build) else os.path.join(self.root, build)
        if not os.path.isfile(os.path.join(self.build, "compile_commands.json")):
            sys.exit(f"missing {self.build}/compile_commands.json; configure with cmake first")
        self.overrides = dict(m.split("=", 1) for m in args.map)
        self.skips = set(args.skip)
        self.namespaces = set(args.namespace) if args.namespace is not None else self.owned_namespaces()

    def rel(self, path):
        return os.path.relpath(path, self.root)

    def in_module(self, path):
        return self.rel(path).startswith(self.module)

    def is_source(self, path):
        rel = self.rel(path)
        return not rel.startswith("..") and not os.path.realpath(path).startswith(os.path.realpath(self.build) + os.sep)

    def owned_namespaces(self):
        decls = collections.defaultdict(set)
        for path in self.tree_files():
            for names in NAMESPACE_DECL_RE.findall(self.code(path)):
                for name in re.findall(r"[A-Za-z_]\w*", names):
                    if name != "inline":
                        decls[name].add(self.in_module(path))
        return {name for name, where in decls.items() if where == {True}}

    def run_tidy(self, out_dir, config_file=None):
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
            "-checks=-*,readability-identifier-naming",
            "-export-fixes",
            out_dir + "/",
        ]
        if config_file:
            cmd.append(f"-config-file={config_file}")
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

    def prefix_config(self, path):
        with open(os.path.join(self.root, ".clang-tidy")) as fh:
            config = yaml.safe_load(fh)
        config["Checks"] = "-*,readability-identifier-naming"
        options = config.setdefault("CheckOptions", {})
        for kind in MEMBER_KINDS:
            options[f"readability-identifier-naming.{kind}"] = "UPPER_CASE"
        with open(path, "w") as fh:
            yaml.safe_dump(config, fh)

    @staticmethod
    def load(out_dir):
        docs = []
        for f in sorted(glob.glob(os.path.join(out_dir, "*.yaml"))):
            with open(f) as fh:
                doc = yaml.safe_load(fh)
            if doc and doc.get("Diagnostics"):
                docs.append(doc)
        return docs

    def wanted(self, kind, old, path):
        if kind == "namespace":
            return old in self.namespaces
        return self.in_module(path)

    def plan(self, base_docs, prefix_docs):
        renames = {}
        for docs, prefix_pass in ((base_docs, False), (prefix_docs, True)):
            for doc in docs:
                for diag in doc["Diagnostics"]:
                    msg = diag["DiagnosticMessage"]
                    match = KIND_RE.search(msg["Message"])
                    if not match:
                        continue
                    kind, old = match.groups()
                    if prefix_pass and not PREFIX_RE.match(old):
                        continue
                    if not self.wanted(kind, old, msg["FilePath"]) or old in self.skips:
                        continue
                    key = (kind, old) if kind == "namespace" else (msg["FilePath"], msg["FileOffset"])
                    entry = renames.setdefault(
                        key,
                        {
                            "kind": kind,
                            "old": old,
                            "decl": msg["FilePath"],
                            "offset": msg["FileOffset"],
                            "tool": None,
                            "prefix": False,
                            "reps": {},
                            "nofix": False,
                        },
                    )
                    reps = msg.get("Replacements") or []
                    if not reps:
                        entry["nofix"] = True
                    if prefix_pass:
                        entry["prefix"] = True
                    elif reps:
                        entry["tool"] = reps[0]["ReplacementText"]
                    for r in reps:
                        entry["reps"][(r["FilePath"], r["Offset"], r["Length"])] = doc["MainSourceFile"]
        for entry in renames.values():
            name = entry["tool"] or entry["old"]
            if entry["prefix"]:
                name = PREFIX_RE.sub("", name)
            entry["new"] = self.overrides.get(entry["old"], name)
        return [e for e in renames.values() if e["new"] != e["old"]]

    def collisions(self, entries):
        texts = {}
        found = set()
        for e in entries:
            for path in {path for path, _, _ in e["reps"]}:
                if path not in texts:
                    texts[path] = self.code(path)
                for m in re.finditer(rf"\b{re.escape(e['new'])}\b", texts[path]):
                    line = texts[path].count("\n", 0, m.start()) + 1
                    found.add(
                        f"{self.rel(path)}:{line}: '{e['new']}' (new name for {e['kind']} '{e['old']}') already appears"
                    )
        olds_by_new = collections.defaultdict(set)
        for e in entries:
            for path in {path for path, _, _ in e["reps"]}:
                olds_by_new[(e["new"], path)].add(e["old"])
        for (new, path), olds in olds_by_new.items():
            if len(olds) > 1:
                found.add(f"{self.rel(path)}: '{new}' is the new name of {', '.join(sorted(olds))}")
        return sorted(found)

    def tree_files(self):
        files = subprocess.check_output(["git", "ls-files", "model", "unittest"], cwd=self.root, text=True).split()
        return [os.path.join(self.root, f) for f in files if f.endswith((".h", ".cpp"))]

    @staticmethod
    def code_bytes(path):
        with open(path, "rb") as fh:
            raw = fh.read()
        return raw, CODE_NOISE_BYTES_RE.sub(lambda m: re.sub(rb"[^\n]", b" ", m.group()), raw)

    def add_textual_uses(self, entries):
        sources = {path: self.code_bytes(path) for path in self.tree_files()}
        classes = set()
        for path, (_, code) in sources.items():
            if self.in_module(path):
                classes.update(m.decode() for m in re.findall(rb"\b(?:class|struct)\s+([A-Za-z_]\w*)", code))
        qualifier = b"|".join(re.escape(c.encode()) for c in sorted(classes))
        for e in entries:
            old = re.escape(e["old"].encode())
            if e["kind"] == "method" and classes:
                pattern = re.compile(rb"\b(?:" + qualifier + rb")\s*::\s*(" + old + rb")\b")
                for path, (_, code) in sources.items():
                    for m in pattern.finditer(code):
                        e["reps"].setdefault((path, m.start(1), len(e["old"])), path)
            elif e["kind"] == "macro definition":
                pattern = re.compile(rb"\b" + old + rb"\b")
                for path, (raw, _) in sources.items():
                    for line in DIRECTIVE_RE.finditer(raw):
                        for m in pattern.finditer(raw, line.start(), line.end()):
                            e["reps"].setdefault((path, m.start(), len(e["old"])), path)

    def code(self, path):
        with open(path, encoding="utf-8", errors="replace") as fh:
            return CODE_NOISE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), fh.read())

    def leftovers(self, entries):
        olds = sorted({e["old"] for e in entries if e["kind"] not in LOCAL_KINDS})
        if not olds:
            return []
        pattern = re.compile(r"\b(" + "|".join(map(re.escape, olds)) + r")\b")
        found = []
        for path in self.tree_files():
            text = self.code(path)
            for m in pattern.finditer(text):
                found.append(f"{self.rel(path)}:{text.count(chr(10), 0, m.start()) + 1}: {m.group()}")
        return found

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, text=True)

    def verify_tokens(self, entries):
        pairs = {(e["old"], e["new"]) for e in entries}
        problems = []
        for f in self.git("diff", "--name-only", "HEAD", "--", "model", "unittest").split():
            before = TOKEN_RE.findall(self.git("show", f"HEAD:{f}"))
            with open(os.path.join(self.root, f), encoding="utf-8") as fh:
                after = TOKEN_RE.findall(fh.read())
            if len(before) != len(after):
                problems.append(f"{f}: token count changed ({len(before)} -> {len(after)})")
                continue
            problems.extend(
                f"{f}: unplanned change {a} -> {b}" for a, b in zip(before, after) if a != b and (a, b) not in pairs
            )
        return problems

    def write_fixes(self, entries, out_dir):
        by_tu = collections.defaultdict(list)
        dropped = set()
        for e in entries:
            for (path, offset, length), tu in e["reps"].items():
                if not self.is_source(path):
                    dropped.add(self.rel(path))
                    continue
                by_tu[tu].append({"FilePath": path, "Offset": offset, "Length": length, "ReplacementText": e["new"]})
        for i, (tu, reps) in enumerate(sorted(by_tu.items())):
            doc = {
                "MainSourceFile": tu,
                "Diagnostics": [
                    {
                        "DiagnosticName": "readability-identifier-naming",
                        "DiagnosticMessage": {
                            "Message": "rename",
                            "FilePath": reps[0]["FilePath"],
                            "FileOffset": reps[0]["Offset"],
                            "Replacements": reps,
                        },
                        "Level": "Warning",
                        "BuildDirectory": self.build,
                    }
                ],
            }
            with open(os.path.join(out_dir, f"rename{i}.yaml"), "w") as fh:
                yaml.safe_dump(doc, fh)
        return sorted(dropped)

    def report(self, entries, collisions, dropped):
        counts = collections.Counter(e["kind"] for e in entries)
        for e in sorted(entries, key=lambda e: (self.rel(e["decl"]), e["offset"])):
            files = sorted({self.rel(p) for p, _, _ in e["reps"]})
            outside = [f for f in files if not f.startswith(self.module)]
            how = "map" if e["old"] in self.overrides else ("strip" if e["prefix"] else "tool")
            note = f"  [+{len(outside)} outside: {', '.join(outside)}]" if outside else ""
            nofix = "  NO FIX-IT (used in a macro or conflicts)" if e["nofix"] else ""
            print(f"{self.rel(e['decl'])}: {e['kind']} {e['old']} -> {e['new']} ({how}){note}{nofix}")
        print(f"\n{len(entries)} renames: " + ", ".join(f"{n} {k}" for k, n in counts.most_common()))
        print("namespaces renamed: " + (", ".join(sorted(self.namespaces)) or "none"))
        if dropped:
            print("replacements dropped in generated files: " + ", ".join(dropped))
        if collisions:
            print("\nPossible collisions (check each for shadowing before trusting the build):")
            print("\n".join(collisions))

    def main(self):
        if not self.args.dry_run and self.git("status", "--porcelain", "--", "model", "unittest").strip():
            sys.exit(
                "model/ or unittest/ has uncommitted changes; commit or stash them so the rename is its own commit"
            )
        with tempfile.TemporaryDirectory(prefix="om-rename-") as tmp:
            base_dir, prefix_dir, out_dir = (os.path.join(tmp, d) for d in ("base", "prefix", "out"))
            for d in (base_dir, prefix_dir, out_dir):
                os.mkdir(d)
            print(f"clang-tidy naming pass over all TUs in {self.rel(self.build)} ...", file=sys.stderr)
            self.run_tidy(base_dir)
            config = os.path.join(tmp, "prefix.clang-tidy")
            self.prefix_config(config)
            print("member-prefix pass ...", file=sys.stderr)
            self.run_tidy(prefix_dir, config)
            entries = self.plan(self.load(base_dir), self.load(prefix_dir))
            if not entries:
                print(f"nothing to rename in {self.module}")
                return 0
            self.add_textual_uses(entries)
            nofix = [e for e in entries if e["nofix"]]
            dropped = self.write_fixes(entries, out_dir)
            self.report(entries, self.collisions(entries), dropped)
            if self.args.dry_run:
                return 0
            if nofix:
                sys.exit("\nrefusing to apply: some symbols have no fix-it; --skip them or fix by hand first")
            subprocess.run(
                ["uvx", "--from", f"clang-tidy=={self.args.llvm_version}", "clang-apply-replacements", out_dir],
                check=True,
            )
            leftovers = self.leftovers(entries)
            if leftovers:
                print("\nOld names still used in code (each must be a different symbol that shares the name):")
                print("\n".join(leftovers))
            problems = self.verify_tokens(entries)
            if problems:
                print("\nToken check FAILED: the diff contains more than the planned renames:")
                print("\n".join(problems))
                return 1
            print("\nToken check: the diff against HEAD is the planned renames plus whitespace only")
        return 0


if __name__ == "__main__":
    sys.exit(Renamer(parse_args()).main())
