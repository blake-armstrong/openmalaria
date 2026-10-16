---
name: cpp-lint
description: Run clang-tidy on the C++ lines changed since a base commit, apply the safe fix-its, rebuild, and report what is left that needs judgement. This is the G-style gate. Use after editing C++ under model/ or unittest/ and before committing. Optional argument is the base commit (default HEAD).
---

# Lint changed C++ lines

Only lines changed since `<base>` are checked, so legacy code never blocks a change.
Config: `.clang-tidy`; every disabled check and its reason are in `docs/refactor/STYLE.md`.

## 1. Set up

- `<base>` is the skill argument if given. Otherwise use the current chunk's base commit
  when working a refactor chunk (to cover everything the chunk changed), or `HEAD` for
  uncommitted work.
- `export OM_BUILD_DIR=build-rework`. `compile_commands.json` must exist there. If a new
  `.cpp` was added to `model/CMakeLists.txt`, re-run `cmake -S . -B build-rework` first.
- New files must be visible to `git diff`: `git add -N <file>`.
- `util/style.sh changed <base>` lists the files in scope (`model/`, `unittest/`).

## 2. Apply the safe fix-its

```sh
util/style.sh fix <base>
```

This applies fix-its only on changed lines and only for the safe list in
`util/style.sh -h`: `nullptr`, `override`, `= default`/`= delete`, `using`, redundant
`void` args, bool literals, variable initialisation, and the two `performance-*` checks
that add `const&`. Then it runs `format-changed`. Fixes are exported and applied once
with `clang-apply-replacements`, so a header shared by two changed files is not patched
twice.

Note what it changed (`git diff`). `cppcoreguidelines-init-variables` initialises
doubles to `NAN` and may add `#include <cmath>`. If the variable is assigned on every
path, prefer a real initial value or restructure so it is declared at first assignment.

## 3. Rebuild

```sh
cmake --build build-rework -j10
```

A failure here caused by a fix-it means the fix was not safe in context. Undo that
hunk, fix by hand, and mention it in the report.

The only baseline warnings are `sprintf` deprecated in `model/main.cpp` and a duplicate
`-lc++` at link time. Fix any new warning. In particular, when `modernize-use-override`
marks one method of a class `override`, clang warns `-Winconsistent-missing-override`
on that class's other overrides: mark them `override` too (replacing `virtual`).

## 4. List what is left

```sh
util/style.sh check <base>
```

Triage each remaining finding:

| Finding | Action |
|---|---|
| `readability-identifier-naming` on an identifier the change **declares** | Rename it by hand, with all its uses, per `/cpp-style`. |
| `readability-identifier-naming` on a legacy **parameter or local** of a function the change edits | Rename it by hand. The rename stays inside that function, plus the matching parameter name in its declaration. |
| `readability-identifier-naming` on any other **legacy** name (member, function, type, namespace, macro) whose declaration line the change touched | Don't rename it inside a logic commit and don't `NOLINT` it. If the module's rename is due (B4 on), run `/rename-module` first as its own commit. Otherwise list it in the report as a legacy hit and let the user decide. |
| `*-narrowing-conversions` | Intended: explicit `static_cast`. Not intended: fix the type. |
| other `bugprone-*` | Treat as a possible bug. Fix it, or explain in the report why not. |
| `modernize-*` / `performance-*` outside the safe list (e.g. `performance-unnecessary-value-param`, `modernize-pass-by-value`, `modernize-use-emplace`, `modernize-loop-convert`) | Read the fix-it. These can change signatures or semantics. Apply by hand, or one check at a time with `util/style.sh fix <base> '-*,<check>'`, then rebuild. |
| a check STYLE.md lists under "Kept on, with expected legacy hits", in one of the places it names | `// NOLINT(<check>)` on that line. |
| anything else that is a genuine false positive | `// NOLINT(<check>)`, naming one check, and say why in the report. |

Re-run steps 2–4 until `check` prints `G-style: clean` or only reported legacy hits
remain.

## 5. Re-test if code changed

If steps 2 or 4 changed anything beyond formatting, run G-exact and G-unit. Wrap the
run in `caffeinate -i` on macOS, because idle sleep has stalled test runs before:

```sh
cd build-rework && caffeinate -i ctest -j10 --output-on-failure
```

## 6. Report

Keep it short:
- the base, and the files checked
- auto-fixed: count per check
- fixed by hand: `file:line`, check, what was done
- left: `file:line`, check, and why (legacy hit for the user to decide, or a justified
  `NOLINT`)
- the build and ctest result, if they ran
- the final `util/style.sh check <base>` verdict
