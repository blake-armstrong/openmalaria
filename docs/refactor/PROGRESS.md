# Refactor progress

See `PLAN.md` for the session protocol, chunks and gates. Append one log entry per chunk.

## Status

| Chunk | Status | Commits |
|---|---|---|
| A1 | done | cfcb16e3, (this log entry) |
| A2 | done | (this log entry) |
| B1 | done | b7991e9f, 35450066, 8c4dc9b1, dec06531, 2205f6e8, ddc959a8, (this log entry) |
| B2 | done | 0bf623e3, 4882b106, b7052696, (this log entry) |
| B3 | done | 1b9b22f6, 06642bf8, 5efb309c, 1d56c288, db529f37, (this log entry) |
| B4 | todo | |
| B5 | todo | |
| P1 | done | f29d38e3, 81a14090, b0309128, 6bfcf8c0, 4b803c7f, (this log entry) |
| S1 | todo | |
| S2 | todo | |
| S3 | todo | |
| S4 | todo | |
| S5 | todo | |
| S6 | todo | |
| C1 | todo | |
| C2 | todo | |
| D1 | todo | |
| D2 | todo | |
| D2b | todo | |
| D3 | todo | |
| D4 | todo | |
| D5 | todo | |
| D6 | todo | |
| D7 | todo | |
| D8 | todo | |
| D9 | todo | |
| E1 | todo | |
| E2 | todo | |
| E3 | todo | |
| E4 | todo | |
| E5 | todo | |
| E6 | todo | |
| F0 | todo | |
| F1 | todo | |
| F2 | todo | |
| F3 | todo | |
| F4 | todo | |
| F5 | todo | |
| F6 | todo | |
| G1 | todo | |
| G2 | todo | |
| G3 | todo | |
| H1 | todo | |
| H2 | todo | |
| H3 | todo | |
| I1 | todo | |
| I2 | todo | |
| I3 | todo | |
| J1 | todo | |
| J2 | todo | |
| J3 | todo | |
| J4 | todo | |

## Log

### A1: Create the branch and put the plan in the repo (done, 2026-10-15)
- Fetched upstream: `upstream/main` moved from 7b1d8919 (#430) to 0b264527 (#468).
- `throwaway_ai_rework` was created from `upstream/main` with `--no-track`. Before the docs commit, `git rev-parse HEAD upstream/main` gave matching hashes and `git diff --quiet upstream/main` was clean.
- `core-only` had no tracked changes. The untracked `.cache/`, `python/` and `.README.md.un~` (a vim undo file, probably ignored on `core-only`) carried over. All three were left alone.
- Commits: cfcb16e3 (PLAN.md and PROGRESS.md), plus the commit that adds this entry.
- Gates: none for this chunk.
- Next: A2.

### A2: Baseline build and tests (done, 2026-10-15)
- **Canonical build dir: `build-rework/`.** It is ignored by `build*/` in `.gitignore`. The existing `build/` came from `core-only` and was left untouched. Configure with:
  `cmake -S . -B build-rework -DCMAKE_BUILD_TYPE=Release -DOM_BOXTEST_ENABLE=ON -DOM_CXXTEST_ENABLE=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  Build with `cmake --build build-rework -j10`, then test with `ctest -j1 --output-on-failure` in `build-rework/`.
- Toolchain:
  - Apple clang 21.0.0 (clang-2100.1.1.101), CMake 4.4.2, macOS on 10 cores
  - xsd at `/opt/homebrew/bin/xsd`; Xerces-C and GSL from Homebrew
- Full build time: ~17 s wall at -j10. The only warnings are `sprintf` deprecated at `model/main.cpp:68` and duplicate `-lc++` from the linker.
- **G-exact / G-unit baseline:** 52/52 tests pass and there are no known failures. That is 51 scenario tests, each run with `--checkpoint-stop` and compared to `test/expected/`, plus the CxxTest `unittest` binary as one ctest test.
- Total ctest time at -j1: 71.9 s wall.
- Per-test wall times at -j1, slowest first, in seconds. These include the checkpoint stop/resume, so a plain run is somewhat faster:
  Genotypes 6.21, MSAT_HRP2 3.77, Penny 3.68, BSV 2.89, TBV 2.87, PEV 2.87, NonHumanHosts 2.78, SimpleMPDLarviciding 2.07, MSAT 1.97, LifeNet2 1.89, TrapTest 1.86, LifeNet1 1.86, VecFullTest 1.85, EffectiveDrug 1.83, AvailabilityFilter 1.83, Empirical 1.78, IRS30 1.71, ExactNv0 1.68, 2ITNs 1.65, Rach5IC 1.60, Molineaux 1.56, 4 1.34, NoMPDLarviciding 1.31, HetVecDailyEIR 1.31, Cohort 1.26, NoInterv 1.23, SubPopRemoval 1.19, ESTS 1.18, DecisionTree5DayDielmo 1.10, KK_20150612 0.94, 12 0.92, NamawalaArabiensis 0.88, InfectionOrigin 0.84, unittest 0.83, SimpleMPDTest 0.80, VecTest 0.75, 1 0.71, 11 0.54, MolPairwise 0.52, GaussianCopulaLognormal 0.48, TriggeredMSAT 0.44, GaussianCopulaGamma 0.43, VecMonthly 0.42, Vivax 0.32, 5 0.31, 6 0.29, ModelNameModelOptionOverrides 0.28, ModelNameManyOverrides 0.28, ModelNameParamOverrides 0.24, ModelNameNoOverrides 0.22, 9 0.21, 10 0.12
- Notes for later chunks:
  - Only 51 of the 57 `test/scenario*.xml` files are registered in `test/CMakeLists.txt`. The disabled ones are 2, 3, IPT, VT, MultiVecPopInterventions and the VecLC* scenarios. S1–S5 should use the registered list (`OM_BOXTEST_NAMES`).
  - S5 estimate: one replicate of every scenario costs ≤ ~71 s of CPU, so N = 500 is ≤ ~10 h serial, or ~1 h at -j10. Genotypes (6.2 s) is the slowest scenario.
  - Binary survey output (`OM_BOXTEST_BINARY=ON`, `--binary`) is not part of this baseline.
  - The raw log and JUnit XML are `build-rework/ctest-baseline.log` and `build-rework/ctest-baseline.xml`. They are not committed.
- Next: any of B1, P1, S1 or C1.

### B1: Style configs (done, 2026-10-15)
- **Pinned LLVM 22.1.8**, for both clang-format and clang-tidy. LLVM was not installed. The tools come from the PyPI wheels and run through `uvx`, with no global install. The version is set once, in `util/style.sh`.
- Commits:
  - b7991e9f: `.clang-format`, plus `DisableFormat` in `contrib/` and `schema/`
  - 35450066: `.clang-tidy`
  - 8c4dc9b1: `CMAKE_EXPORT_COMPILE_COMMANDS` on by default
  - dec06531: allow short functions on one line
  - 2205f6e8: `util/style.sh`
  - ddc959a8: `docs/refactor/STYLE.md`
- **Naming, confirmed by the user during the chunk:** classes and other types are PascalCase; methods and functions move to snake_case. This matches the plan's § Code style. The user also asked for every change to be tracked in its own git commit, hence the six small commits.
- Gates:
  - G-exact: 52/52 pass.
    - The first run reported 1000 s total. The Mac went into idle sleep for 990 s mid-run (pmset log, 14:55:04), which stalled the 10 tests in flight; all of them passed.
    - Rerun under `caffeinate -i`: 52/52 in 11.2 s at `-j10`.
    - Long ctest runs should be wrapped in `caffeinate -i`.
  - G-style: now available, via `util/style.sh check [base]`. On this chunk it reports no changed C++ files.
    - Tried on throwaway edits to `model/Population.cpp`, then reverted: a badly formatted, badly named function failed on both halves, and a well-formed one passed.
- Done criteria:
  - `util/style.sh format` and `util/style.sh tidy` both run on `model/Population.cpp` with the project configs.
  - Every tidy exclusion and its reason is listed in `docs/refactor/STYLE.md`.
- Deviations and surprises:
  - **G-style checks changed lines only** (`git-clang-format --diff`), not `clang-format --dry-run -Werror` on whole changed files. Until B2, every legacy file fails a whole-file check. After B2 the two are equivalent. B5's CI job should use the same `check` command.
  - **CMake:** `CMAKE_EXPORT_COMPILE_COMMANDS` has to be set before `project()`, because `project()` creates an empty cache entry first.
  - **macOS sysroot:** CMake 4 does not put `-isysroot` in `compile_commands.json`, so the PyPI clang-tidy cannot find libc++ headers. `util/style.sh` adds `-isysroot $(xcrun --show-sdk-path)` on Darwin.
  - **Whole-tree tidy runs** must anchor the file regex at the repo root. Otherwise the generated CxxTest runner `<build>/unittest/tests.cpp` is checked too. The command is in STYLE.md.
  - **clang-format overrides** were chosen by measuring churn: about 13k of 36.6k lines will change in B2 whatever the config, because the current spacing is inconsistent.
- Baseline diagnostics with the final config (`model/` + `unittest/`): 5626 distinct, including 3176 naming hits (437 methods, 144 functions, 426 members, 32 namespaces).
- Follow-ups:
  - B2: format only `model/` and `unittest/`; `util/` has no C++. Then run `util/style.sh format -i`.
  - B3: skills should call `util/style.sh`, not raw tool names.
  - B4: clang-tidy does not strip the `m_` and `_` member prefixes; that needs a scripted step. Header guards (`Hmod_*`) will be renamed to UPPER_CASE.
  - B5: the pre-commit hook should pin clang-format 22.1.8. CI needs `uv`.
- Next: B2 (or P1, S1, C1).

### B2: Whole-tree format (done, 2026-10-15)
- **Upstream merge question:** `git fetch upstream` showed no new commits since A1 (`upstream/main` still 0b264527), so there was nothing to merge before the reformat.
- Commits:
  - 0bf623e3: `util/style.sh format -i` over all 173 tracked `.cpp`/`.h` files under `model/` and `unittest/`. 171 files changed, +13,162 / -14,526 lines. Mechanical only.
  - 4882b106: `.git-blame-ignore-revs`, listing 0bf623e3
  - b7052696: STYLE.md updated (B2 stats, how to enable the blame-ignore file)
- Done criterion: `clang-format --dry-run -Werror` over every tracked C++ file in the repo (including `contrib/`, which has `DisableFormat`) exits 0.
- Gates:
  - G-exact / G-unit: 52/52 pass under `caffeinate -i`, 13.1 s at `-j10`. The only build warnings are the baseline ones (`sprintf` deprecated, now at `model/main.cpp:65`; duplicate `-lc++`).
  - CxxTest discovers tests by regex over the headers, so the count was checked too: 86 `void test*()` methods match `cxxtestgen`'s pattern both before and after the reformat. The runner executes 66 of them, the subset CMake registers.
  - G-style was not run: it is not a B2 gate, and clang-tidy-diff over 13k reformatted lines would only report legacy names. The git-clang-format half is clean by the done criterion.
- Notes:
  - `blame.ignoreRevsFile` was set in this clone's local git config. Other clones need the command in STYLE.md.
  - `util/` has no C++, as noted in B1, so only `model/` and `unittest/` were formatted.
- Follow-ups:
  - B4: each rename commit appends its hash to `.git-blame-ignore-revs`.
  - From now on, `util/style.sh check` on a whole changed file and on changed lines give the same format verdict.
- Next: P1.

### P1: Benchmark (done, 2026-10-15)
- **Benchmark scenario:** `test/bench/scenarioBench.xml`, a copy of `scenarioMSAT.xml` with `popSize` 100 → 1000 and a new `name`. It is a separate file so later edits to the test scenario cannot shift the baseline.
  - Why MSAT: 1-day steps and the most hot paths of any registered scenario: Molineaux within-host, PK/PD, dynamic vector transmission, ITNs, MDA, the clinical event scheduler.
  - Run time is linear in `popSize` (100: 1.9 s, 1000: 15 s). Most of it is the 90-year warmup, so interventions are a small share, as in real runs.
  - It is not registered with CTest and has no expected output.
- **Script:** `util/bench.sh [-n RUNS] [scenario.xml]`, with `OM_BUILD_DIR` as in `util/style.sh`. It runs each repeat in a fresh temp dir (schema linked as `test/run.py` does), checks for a zero exit and an `output.txt`, and prints each run's wall and CPU time, then the median, min, max and spread ((max − min) / median). On macOS it re-runs itself under `caffeinate -i`.
- **Stored baseline (sanity check only; G-perf itself is the A/B comparison below):** median wall **14.60 s** at 63d87323, `build-rework/` Release, Apple clang 21.0.0, on the 10-core Mac from A2, on battery with Low Power Mode off. A plain run far from this (more than ~3%) means the machine or toolchain has changed.
  - 14.60 s is the median of the 10 runs in batches 3 and 4, taken with the machine quieter (load average ~1.3–1.6).
  - Batch 3: 14.264, 14.504, 14.458, 14.612, 14.550 → median 14.504, spread 2.40%.
  - Batch 4: 14.934, 14.590, 14.602, 14.710, 14.634 → median 14.634, spread 2.35%.
  - Batches 1 and 2 ran earlier, while a pytest run in another repo kept the load average at ~2.5–3.5. They gave medians of 14.931 s (spread 1.00%) and 15.024 s (spread 2.48%).
  - In every batch, CPU time tracks wall time within 0.2%.
  - **The load cost ~2.5–3% in median, but did not change the spread.** So the median of the same build moves by about as much as the 3% gate allows, depending on what else the machine is doing. Batch 2's 15.024 s would be within 0.1% of failing a +3% limit on this baseline, for unchanged code.
- Gates: G-exact / G-unit 52/52 pass (12.7 s at `-j10`). No C++ changed.
- Follow-ups:
  - G-perf: `OM_BUILD_DIR=build-rework util/bench.sh --compare ../om-bench-base/build-rework`. Even with A/B, don't run it while S5 is going: a saturated CPU raises noise for both sides.
  - The baseline is tied to this machine, compiler and power state. If any of them changes, re-measure at the last G-perf-passing commit and record the new baseline here.
- **A/B mode added (4b803c7f), now the G-perf gate.** The stored baseline moves by about as much as the 3% limit between sessions, so G-perf now compares the candidate against a build of the chunk's base commit, in the same session.
  - `util/bench.sh --compare <base build dir>` alternates base and candidate runs in ABBA order (5 of each by default, ~2.5 min). It reports both medians and the median per-pair ratio candidate/base, and exits 1 if that ratio is over 1.03. The per-pair ratio cancels slow drift (load, heat, power) that hits both runs of a pair.
  - Base build recipe (in `util/bench.sh -h`): `git worktree add ../om-bench-base <base>`, then configure with `-DCMAKE_BUILD_TYPE=Release` and build the `openMalaria` and `inlined_xsd` targets. The worktree can be kept between chunks: `git -C ../om-bench-base checkout --detach <new base>` and rebuild.
  - `OM_BUILD_DIR` may now also be an absolute path.
  - Each binary gets one untimed `--help` launch first. macOS added ~0.6 s to the first launch of a freshly built binary (0.61 s, then 0.00 s for `--help`). In the first A/A test, the base's first run showed 15.04 s wall against 14.61 s CPU because of it.
  - Validation:
    - A/A: HEAD in `build-rework/` against a separate Release build of the same commit, 5 pairs: median ratio 0.9992 (−0.08%), exit 0. That run predates the warm-up fix; the base's first run carried the launch cost and still passed.
    - Known regression: candidate built with `-DCMAKE_CXX_FLAGS_RELEASE=-O1`, 2 pairs: median ratio 1.0409 (+4.09%), per-pair 1.039–1.043, exit 1.
    - Usage errors (missing `--compare` argument, missing dir, unknown flag) exit 2; plain mode still works.
- Next: B3.

### B3: `cpp-style` and `cpp-lint` skills (done, 2026-10-16)
- Commits:
  - 1b9b22f6: `util/style.sh` gains `changed [base]`, `format-changed [base [paths...]]` and `fix [base] [checks]`
  - 06642bf8: `.claude/skills/cpp-style/SKILL.md`
  - 5efb309c: `.claude/skills/cpp-lint/SKILL.md`
  - 1d56c288: cpp-style: drop the reference to the hook (see below)
  - db529f37: STYLE.md documents the new commands and the skills
- **New `util/style.sh` commands**, so that the skills call the script rather than raw tools (B1 follow-up):
  - `format-changed` runs `git-clang-format --force <base>`, which formats exactly the lines `check` verifies. A first version ran whole-file `clang-format -i`. On a function appended before a namespace's closing `}`, it kept a blank line that `check`'s line-range format then flagged. So the B2 note "whole-file and changed-lines give the same verdict" has at least this exception. `git-clang-format` exits 1 when it changed something, and the script treats that as success.
  - `fix` applies fix-its on changed lines only, for a safe list (`nullptr`, `override`, `= default`/`= delete`, `using`, redundant `void`, bool literals, `init-variables`, `for-range-copy`, `unnecessary-copy-initialization`). `clang-tidy-diff.py` runs one clang-tidy per changed file in parallel (`-j` defaults to all cores), so a `.cpp` fix that also edits its header could race with the header's own run. Fixes are therefore exported (`-export-fixes`) and applied once with `clang-apply-replacements`. Then it runs `format-changed`.
- **Hook not added.** The optional `PostToolUse` format-on-edit hook (`.claude/settings.json` plus a script calling `util/style.sh format-changed HEAD <file>`) was blocked by the Claude Code auto-mode classifier as self-modification of agent settings. It was not worked around; it is left for the user to add if wanted.
- **Trials** (the new skills are not loadable in the session that created them, so their steps were followed by hand; both edits reverted):
  - cpp-style, `model/util/AgeGroupInterpolation.cpp`: a new `final` subclass written to the rules (snake_case method and member, `explicit`, `override` on the legacy `firstGlobalMaximum`), deliberately badly indented. `check` flagged only formatting, so a legacy-named override is not flagged by `readability-identifier-naming`. `format-changed`, then `check`: clean. Built.
  - cpp-lint, same file: a new `CountAbove(std::vector<double>, const double*)` with `NULL`, an uninitialised `int`, a narrowing `size()` → `int` and a by-value vector, plus a touched legacy line (`virtual double eval(double ageYears)`).
    - `fix` applied `use-override`, `init-variables` and `use-nullptr`.
    - `check` then left 4 findings: naming of `ageYears` (legacy parameter) and of `CountAbove`, `unnecessary-value-param`, and narrowing. All four were fixed by hand per the skill's triage table, and the redundant `count = 0` that `init-variables` left was restructured.
    - **Surprise:** the rebuild showed `-Winconsistent-missing-override` on the class's two other overrides, because `use-override` had marked only the changed line. The skill now says to mark the rest of the class.
    - Final `check`: clean. ctest 52/52.
  - Skill change made from the trial: legacy *parameters and locals* of an edited function are renamed by hand, because the rename stays local. Other legacy names on touched lines are not renamed in a logic commit and not `NOLINT`ed: either run `/rename-module` first, or report them to the user.
- Gates (clean HEAD db529f37, rebuilt):
  - G-exact / G-unit: 52/52, 13.0 s at `-j10`.
  - G-style: `util/style.sh check 86f8196c` reports no changed C++ files.
  - G-perf: not run. No C++, CMake or test file differs from 86f8196c, so the binary is unchanged.
- Follow-ups:
  - B4: `rename-module` should reuse `util/style.sh fix <base> '-*,readability-identifier-naming'` only for single-TU symbols. For a whole module it needs run-clang-tidy over every TU that uses the module, with exported fixes merged by `clang-apply-replacements`, the same dedup reason as `fix`. Strip `m_`/`_` separately (B1 note).
  - B5: CI should call `util/style.sh check <merge-base>`. A pre-commit hook could call `util/style.sh format-changed HEAD` on staged files.
  - The user may want to add the format-on-edit hook by hand (above).
- Next: B4.
