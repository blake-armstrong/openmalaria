---
name: modernize-module
description: Modernise one C++ module directory (e.g. model/PkPd) to C++20 practice in two commits - the safe modernize-* and Core Guidelines fix-its applied by util/style.sh modernize (one tool-made commit, listed in .git-blame-ignore-revs), then a hand commit for the judgement items. Use right after a module's /rename-module in stages D on, or when asked to modernise a module. Argument is the module directory.
---

# Modernise one module

A module that shares its directory with others (e.g. the `model/` root files) is given as a
list of files instead of `<dir>`; everything below then applies to those files.

Two commits, in this order, after the module's rename (`/rename-module`):

1. **Tool commit.** `util/style.sh modernize <dir>` applies the fix-its of a fixed list of
   checks whose rewrites keep behaviour (`util/style.sh -h` prints `MODERNIZE_CHECKS`).
   No hand edits go into this commit.
2. **Hand commit.** The judgement items of `docs/refactor/PLAN.md` § Modernisation
   policy, from the list `--report` prints.

Both must keep G-exact: a modernisation never changes outputs. Style rules: `/cpp-style`.

## What the tool does

- Runs clang-tidy with `MODERNIZE_CHECKS` over every TU in `$OM_BUILD_DIR`, with the header
  filter set to the module, so headers are fixed even when only other modules include them.
- Keeps diagnostics located in the module whose fixes stay inside it. A fix that reaches
  another directory (e.g. a by-value parameter whose definition is elsewhere) is listed,
  not applied.
- Drops duplicates from different TUs and include insertions for headers the file already
  has. A fix that overlaps one already taken waits for the next pass.
- Applies each pass once with `clang-apply-replacements` and repeats until nothing is left
  (at most 5 passes). Adds `#include <numbers>` where `modernize-use-std-numbers` replaced
  a macro (it inserts no include then). Then `format-changed HEAD`.
- Refuses to start unless `model/` and `unittest/` are clean, and fails if anything
  outside the module changed.

## 1. Preconditions

- The module's rename commit is in (`/rename-module`), so the hand commit's new code is
  checked against final names.
- `export OM_BUILD_DIR=build-rework`, configured with `-DOM_CXXTEST_ENABLE=ON` and built.
  Re-run `cmake -S . -B build-rework` if `.cpp` files were added.
- Note the chunk's base commit; G-style and G-perf compare against it.

## 2. Dry run

```sh
util/style.sh modernize <dir> --dry-run
```

About 10 s per pass. It prints the first pass's fixes, then three lists:
overlapping fixes (left for the next pass), fixes that reach outside the module, and
findings with no fix-it. Read the fixes for anything that would change behaviour in this
module, e.g. a `modernize-loop-convert` over a container the loop body modifies. If one
would, stop and tell the user; the check list may need narrowing.

## 3. Apply, build, test

```sh
util/style.sh modernize <dir>
cmake --build build-rework -j10
cd build-rework && caffeinate -i ctest -j10 --output-on-failure
```

The output ends with `Applied:` (counts per check, for the commit message) and the lists
from § 2. A compile error or a G-exact failure means a fix was not safe here: revert
(`git checkout -- <dir>`), report the check and file to the user, and do not patch the
tool's output by hand in this commit.

Re-run `util/style.sh modernize <dir> --dry-run`. It must apply nothing more.

## 4. G-style and commit the tool change

`util/style.sh check <base>` reports legacy findings on every line the tool touched, as
for a rename. For the tool commit, G-style is:

- the `== clang-format` half of `util/style.sh check HEAD` prints nothing;
- the dry run in § 3 applies nothing;
- after the `.git-blame-ignore-revs` commit, `util/style.sh check <tool commit>^` is clean.

Stage the changed files by name:

```sh
git add $(git diff --name-only -- model unittest)
git commit
```

Message: `Modernize <dir> with the safe fix-its`, a body with the `Applied:` counts, the
line `Mechanical: made by util/style.sh modernize; no hand edits.`, and the attribution
lines. Then a second commit appends the hash to `.git-blame-ignore-revs`:

```
# <chunk>: <dir> safe modernize fix-its (util/style.sh modernize)
<full hash>
```

## 5. The hand commit

```sh
util/style.sh modernize <dir> --report
```

It lists every clang-tidy finding left in the module (the full `.clang-tidy` set minus
naming, plus `misc-const-correctness`), and text matches for `new`/`delete`,
`printf`/`stringstream`, `#define` and plain `enum`. Work through them per the policy:

| Item | Do |
|---|---|
| owning raw pointer, `new`/`delete` | `std::unique_ptr` or a value; `make_unique<Derived>` for `unique_ptr<Base>(new Derived)` |
| C array; pointer + length | `std::array`; `std::span` |
| `printf`, `ostringstream` formatting | `std::format`. Keep text identical: `ostream << double` is `{:g}`, not `{}` |
| macro | `constexpr` or an `inline` function |
| plain `enum` | `enum class`, unless the value is checkpointed or mapped from XML (leave those for H3/E) |
| output parameter | return a value or struct; a true in/out parameter becomes a reference, not a pointer |
| `misc-const-correctness` | Apply its fix-its to the module's files only (`util/style.sh tidy -checks='-*,misc-const-correctness' -fix <file>`, one file at a time). It also edits headers of other modules: `git checkout` those. It writes east const (`double const x`); rewrite to west const (`const double x`). |
| deleted special member that is private | make it public |
| rule-of-five noise (`special-member-functions`) | prefer the rule of zero when the members already give the right semantics |
| other `bugprone-*` | treat as a possible bug; fix, or say why not in the report |
| `performance-avoid-endl` | leave it: `endl` → `'\n'` changes flush timing. On a line you touch, add `// NOLINT(performance-avoid-endl)` |
| globals and file statics | leave them for the module's D chunk |
| checkpoint `operator&`, `scnXml` uses | leave them for H3 and E |

Not every item has to go; record what was left and why in the commit message.

If any console text could change (anything written to `cout`/`cerr`, or an exception
message), run the affected command line on the base build and the candidate and compare
stdout, stderr and the exit code (G-cli formalises this from M3).

Then build, ctest, `util/style.sh check HEAD` (must be clean; triage per `/cpp-lint`), and
commit with a message that lists the items done and the items left.

## 6. Gates and report

- G-exact and G-unit after each commit.
- G-style: `util/style.sh check <base>` is clean once the ignore-revs commit is in.
- G-perf: `util/bench.sh --compare <base build>` (recipe in `util/bench.sh -h`).

Report: the module, the `Applied:` counts, the fixes not applied (outside the module, no
fix-it) and why, the hand items done and left, the gate results, and the three commit
hashes.
