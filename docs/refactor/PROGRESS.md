# Refactor progress

See `PLAN.md` for the session protocol, chunks and gates. Append one log entry per chunk.

## Status

| Chunk | Status | Commits |
|---|---|---|
| A1 | done | cfcb16e3, (this log entry) |
| A2 | done | (this log entry) |
| B1 | done | b7991e9f, 35450066, 8c4dc9b1, dec06531, 2205f6e8, ddc959a8, (this log entry) |
| B2 | done | 0bf623e3, 4882b106, b7052696, (this log entry) |
| B3 | done | 1b9b22f6, 06642bf8, 5efb309c, 1d56c288, db529f37, 5dc5baab, b3685dfe, (this log entry) |
| B4 | done | 57afa057, e7074755, cae5d3b5, f82cb9a5, ee7e0471, (this log entry) |
| B5 | done | 16851d20, 65710e16, dcae0a40, 6b870e04, 658cf62b, ab06df46, 8ca36c75, (this log entry) |
| P1 | done | f29d38e3, 81a14090, b0309128, 6bfcf8c0, 4b803c7f, (this log entry) |
| S1 | done | b372c2f6, (this log entry) |
| S2 | done | 841b2a9f, 720c0f09, 3c40c24b, (this log entry) |
| S3 | done | 46cef63b, e61e95d4, 2b6281d6, 5384584d, 012e38d8, ed38bba4, 7472b8b5, fcbfc337, (this log entry) |
| S4 | todo | |
| S5 | todo | |
| S6 | todo | |
| C1 | done | b02303db, (this log entry) |
| C2 | done | 20e8bf39, (this log entry) |
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
| M0 | done | e5205c8a, 331463a7, 45d57ba3, (this log entry) |
| M1 | done | de71bd96, c7293e4e, 80034802, 941c2e77, (this log entry) |
| M2 | done | afd3fa8c, 9183ffe4, ca51c409, bff08fc0, b979260e, a0f1112c, d4869a62, d9d45adb, 7b330170, a592041f, 565239f1, (this log entry) |
| M3 | todo | |
| M4 | todo | |
| M5 | todo | |
| M6 | todo | |
| M7 | todo | |
| M8 | todo | |
| M9 | todo | |

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
- **Hook not added.** The optional `PostToolUse` format-on-edit hook (`.claude/settings.json` plus a script calling `util/style.sh format-changed HEAD <file>`) was blocked by the Claude Code auto-mode classifier as self-modification of agent settings. It was not worked around; it is left for the user to add if wanted. At the user's request the script alone was then added (b3685dfe, `.claude/hooks/format-cpp.sh`). It was tested with hook-style JSON: a tracked file got its changed lines formatted, an untracked new header was formatted whole, and paths outside `model/` and `unittest/` were skipped (all exit 0). The `settings.json` entry is still up to the user.
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

### B4: `rename-module` skill plus a pilot (done, 2026-10-16)
- Commits:
  - 57afa057: `util/style.sh rename <dir>` and `util/rename_module.py`
  - e7074755: `.claude/skills/rename-module/SKILL.md`
  - cae5d3b5: **pilot**, `model/PkPd` renamed (24 files, +505 / -503)
  - f82cb9a5: cae5d3b5 added to `.git-blame-ignore-revs`
  - ee7e0471: STYLE.md documents the tool and the rename-commit G-style rule
- **How the tool works** (details in STYLE.md § Module renames). It runs `readability-identifier-naming` over all 75 TUs, including the CxxTest runner, so uses in other modules and in `unittest/` are found. It keeps symbols declared in the module, plus namespaces declared only there. It applies the merged fixes once with `clang-apply-replacements`, then runs `format-changed HEAD`. Two clang-tidy passes take ~45 s at -j10.
- **Gaps in clang-tidy found by the pilot, now covered by the tool:**
  - The out-of-line definitions of overrides were not renamed (`LSTMDrugOneComp::getIndex` etc.), though their in-class declarations were. Left alone, this is a compile error. The tool now renames `Class::old` for classes declared in the module.
  - Header guards: only the `#define` was renamed, not `#ifndef`, which silently breaks the guard. The tool renames the name on every preprocessor line.
  - Namespace diagnostics land on whichever declaration a TU sees first. So `WithinHost` and `Host` were reported "in" PkPd files, and filtering by location would have partly renamed them. Only namespaces the module owns are renamed.
  - Member prefixes: `m_drugs` is valid lower case, so it isn't flagged. A second pass, with member kinds set to `UPPER_CASE`, finds every member, and the tool strips `m_`/`_`.
- **Safety checks** (all passed on the pilot):
  - The pre-apply collision report flagged two real collisions:
    - member `PD` → `pd` would have bound to a local `pd` in the `LSTMDrugType` constructor, so it was mapped to `pd_params`;
    - namespace `pkpd` (my first choice) would share its name with variables named `pkpd` in `unittest/`, so the tool's `pk_pd` was kept.
  - The post-apply leftover list was all different symbols: `util::Error::PkPd`, `CommonInfection::Kn`, `scnXml::PD`, `SpeciesIndexChecker::getIndex`, and locals in other modules.
  - Token check: across all 24 files, the only non-whitespace differences are the 124 distinct planned old → new pairs.
  - A second run from a clean tree gave a byte-identical diff.
- Pilot: 171 renames (41 members incl. `m_drugs` → `drugs`, 30 methods, 28 local constants, 27 variables, 23 parameters, 9 functions, 8 header guards, 2 global constants, 2 structs, namespace `PkPd` → `pk_pd`). Maps: `IC50` → `ic50` (the tool gives `i_c50`, and likewise for `IC50_pow_slope` and `IC50_correlated_sample`), and `PD` → `pd_params`.
- Gates (at cae5d3b5):
  - G-exact / G-unit: 52/52, 11.9 s at `-j10`. Only the baseline build warnings.
  - G-style: **deviation.** Plain `util/style.sh check HEAD` cannot pass on a rename commit. Every call site touched counts as changed, and still holds other legacy names and hits: about 70 findings, none of them a naming hit for a PkPd symbol. Among them is a pre-existing `ETS_ASSERT` parse error from checking `unittest/UnittestUtil.h` on its own. For rename commits, G-style is redefined in STYLE.md and the skill:
    - the git-clang-format half of `check` is clean (it was);
    - `rename model/PkPd --dry-run` prints `nothing to rename` (it did);
    - the token check passed (it did), which proves the other findings were already there.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base 52b91fa7): median per-pair ratio 1.0051 (+0.51%), ok. The load average was ~9 at the start, from the base build.
  - The base worktree `../om-bench-base` was created at 52b91fa7 and kept for later chunks.
- Notes:
  - The untracked `python/` was only grepped, not changed. It uses none of the renamed PkPd names.
  - Comments that mention old names (e.g. `updateConcentration` in a TODO) are not renamed. Only code and preprocessor lines are.
  - The CxxTest suites that are not registered (AnophelesModel, DescriptiveInfection, ESCaseManagement, MosqLifeCycle) are not compiled, so a rename can't reach them. None of them uses PkPd.
- Follow-ups:
  - Stage D onward: run `/rename-module <dir>` right after each module's refactor. Shared namespaces (`OM`, `WithinHost`, `Host`, `Transmission`, …) need a separate tree-wide rename at some point. `OM` → `om` touches every file, so it is best done when no other branch is in flight.
  - B5: CI's G-style job will fail on rename commits for the reason above. Either skip commits listed in `.git-blame-ignore-revs`, or run the rename-commit variant for them.
- Next: B5.

### B5: Pre-commit and CI (done, 2026-10-16)
- Base: 169a49cb.
- Commits:
  - 16851d20: whole-file clang-format of `model/PkPd/Drug/LSTMDrugConversion.cpp` and `LSTMDrugThreeComp.cpp`. Whitespace only: trailing comments the PkPd rename left misaligned.
  - 65710e16: 16851d20 added to `.git-blame-ignore-revs`
  - dcae0a40: `util/style.sh`: `check`'s format half and `format-changed` work on whole changed files
  - 6b870e04: `util/tidy_diff_filter.py`, wired into `tidy_diff` (used by `check` and `fix`)
  - 658cf62b: `.pre-commit-config.yaml`
  - ab06df46: `.github/workflows/style.yml` and `util/ci-style.sh`
  - 8ca36c75: STYLE.md (new § Pre-commit and CI), cpp-style and rename-module skills
- **Pre-commit:** `pre-commit/mirrors-clang-format` at `v22.1.8` on `model/` and `unittest/` C++. Tested with `uvx pre-commit run`: passes on all files, and reformats (and fails on) a misformatted throwaway edit. Not installed in this clone; that is up to the user (`uv tool install pre-commit && pre-commit install`).
- **CI:** job `g-style` on ubuntu-24.04 for every push and PR, `permissions: contents: read`. It installs clang, cmake, GSL, Xerces-C and xsdcxx, sets up uv, then runs `util/ci-style.sh origin/<PR base or default branch>` with `CC=clang CXX=clang++`. The script takes the merge base, configures `build-style/` if it has no compile database, builds the `schema` target (for the generated `scnXml` headers), then runs `util/style.sh check <merge base>`. Actions are pinned to exact tags at least two weeks old: `actions/checkout@v7.0.1`, `astral-sh/setup-uv@v10.2.0`. `actionlint` (1.7.12, via `uvx --from actionlint-py`) reports nothing.
- **Done criterion (job passes locally, run by hand):** `act` and docker are not installed, so the script was run by hand on macOS with Apple clang:
  - fresh `build-style/`, `util/ci-style.sh upstream/main` (merge base 0b264527): clean, 5.7 s wall including configure and the schema build.
  - With a throwaway global `int legacyName = NULL;` appended to `model/PkPd/LSTMTreatments.cpp` (which reaches the generated `schema/pharmacology.h`): fails with the naming and non-const-global findings, so clang-tidy really runs against `build-style/`. Reverted.
  - `util/ci-style.sh origin/main` (merge base 7b1d8919): clean.
  - Not run on Linux. The first GitHub run is the real test of the apt package names and of clang-tidy 22 against Ubuntu's libstdc++ headers.
- **Deviations and surprises:**
  - **Check against a merge base could not pass before this chunk**, for two reasons, both fixed here:
    1. *Format half.* Line-range formatting (`git-clang-format`) depends on which lines are in range. Against 0b264527 it flagged 33 hunks: blank lines before a namespace's closing `}` that B2's whole-file format kept (the B3 surprise again), and trailing comments in two PkPd files that the rename had aligned for `HEAD^..HEAD`. A whole-file `clang-format --dry-run` over `model/` and `unittest/` failed only on those two PkPd files, which 16851d20 fixed. So `check` and `format-changed` now work on whole changed files (dcae0a40), which is a fixed point whatever the base. This changes the B3 tool semantics: `format-changed` now reformats whole changed files, not just changed lines. Since B2 every file is clean whole-file, so in practice it only formats what the edit touched.
    2. *Tidy half.* The diff since 0b264527 includes every line B2 and the rename touched: 3575 distinct legacy findings. `util/tidy_diff_filter.py` keeps only added lines that `git blame --ignore-revs-file .git-blame-ignore-revs` attributes to a non-ignored commit in `base..HEAD` or to uncommitted changes (it blames the working-tree contents with `--contents`). Lines blame can't move off an ignored commit (e.g. blank lines B2 added) stay on it and are dropped. With no ignored commit in the range, the diff passes through unchanged, so a chunk-local `check HEAD` behaves as before. Blaming 171 files took ~0.7 s.
  - **That also settles the B4 follow-up** about CI and rename commits: once a rename is listed in `.git-blame-ignore-revs`, plain `check` across it is clean (verified: `check cae5d3b5^` is clean). The rename-module skill's G-style rule gains that as a fourth point. The other three (format half clean, dry-run, token check) still apply before the ignore-revs commit exists.
  - The fork's `origin/main` is still at upstream #430 (7b1d8919), but this branch is based on `upstream/main` #468, so push runs here diff against the older base. That is clean today. Moving `origin/main` up to `upstream/main` would make push runs check only this branch's own changes. That is a push, so it is left to the user.
  - zsh doesn't word-split `$files`, so ad hoc pipelines copied from `style.sh` must run under `bash -c`. The first unfiltered count came out as "No relevant changes found" for this reason.
- Gates (at 8ca36c75):
  - G-exact / G-unit: 52/52, 12.5 s at `-j10` under `caffeinate -i`. Only the baseline build warnings.
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check 169a49cb` is clean.
  - G-perf: not run. The rebuilt `openMalaria` is byte-identical to the one before this chunk (sha1 `ad1f4b1c…`), since the only C++ change is whitespace with no line shifts.
- Notes:
  - `build-style/` (from the local CI script runs) is ignored by `build*/` and can be deleted.
  - `.claude/settings.json` was untracked before this chunk and is still untouched.
- Follow-ups:
  - User: push the branch (or open a PR) to see the first real `g-style` run; install the pre-commit hook if wanted; consider moving `origin/main` to `upstream/main`.
  - Every bulk mechanical commit (renames, reformats) must have its `.git-blame-ignore-revs` entry in the same push, or CI's tidy half will report the legacy hits on its lines.
  - The format-on-edit hook script (`.claude/hooks/format-cpp.sh`) now formats the whole edited file through `format-changed`. Its behaviour for tracked files changed with dcae0a40, and its `settings.json` entry is still the user's call (B3).
- Next: C1.

### Untracked-file cleanup (2026-10-16, after B5)
- At the user's request, every untracked file was either committed, ignored or deleted. `git status` is now clean.
  - f8b4be86: `.gitignore` gains `*.un~` (as on `core-only`) and `.cache/` (clangd index). `.README.md.un~` and `.cache/` stay on disk, ignored.
  - db00bd3b: `.claude/settings.json` committed. It registers the B3 format-on-edit hook (`.claude/hooks/format-cpp.sh`), so that hook is now active for Claude Code sessions in this repo.
  - `python/` was **deleted** (user's choice). It held `examples/` (`plot_outputs.py`, `scenario1.xml`, `densities.csv`, `scenario_50.xsd`, which were in no commit on any branch) plus `__pycache__`/`.pytest_cache` left over from the bindings work on `main`/`wasm-python-packages`. Stage J's question about the untracked `python/` directory no longer applies. The tracked bindings on `main` are untouched.

### C1: Move the initialisation into `Simulation` (done, 2026-10-16)
- Base: 6441f0fc.
- Commit b02303db: new `model/Simulation.{h,cpp}` (added to `Model_CPP`), and `main.cpp` now constructs a `Simulation`.
  - `Simulation(std::unique_ptr<scnXml::Scenario>)` takes ownership of the parsed scenario and runs the init chain from `Genotypes::init` through the warmup-length calculation, in the original order. It owns the scenario, the `Population` and the `TransmissionModel`, declared in that order, so they are destroyed in the same order as main's old locals (transmission, population, scenario).
  - Accessors for C2 to absorb: `scenario()`, `population()`, `transmission()`, `human_warmup_length()`, `survey_only_new_ep()`.
  - Left in `main()`: GSL handler, `CommandLine::parse`, `loadScenario`, `XMLChecker` (input handling, done before construction), plus everything from the checkpoint-name lookup on (C2's scope).
  - `ModelNameProvider` and `Parameters` are locals of the constructor, as they were locals of `main()`. Nothing keeps a reference to either after init (grepped), so their earlier destruction is unobservable.
- Deviations from a pure cut-paste, all to pass G-style on the moved lines, none observable:
  - `only_new_episode` (the `getOnlyNewEpisode()` read) moved into the member initializer list, i.e. before `Genotypes::init` instead of after `mon::initCohorts`. It is a side-effect-free generated getter (tidy `prefer-member-initializer`).
  - The warmup length is computed in a local, then assigned once. Assigning the member step by step tripped `prefer-member-initializer`, which can't be followed: the value needs `sim::init` and the transmission model.
  - `static_cast<int>(pop_size)` makes the existing implicit `size_t` → `int` narrowing into `createTransmissionModel` explicit.
  - `std::make_unique<Population>` instead of `unique_ptr(new …)`.
  - New locals in `main()` (`population`, `transmission`, `human_warmup_length`, `survey_only_new_ep`) are snake_case references into the `Simulation`.
- Gates (at b02303db):
  - G-exact / G-unit: 52/52, 13.0 s at `-j10` under `caffeinate -i`. Only the baseline build warnings.
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check HEAD` (base 6441f0fc) clean. The commit's pre-commit clang-format hook also passed, so the hook is now installed in this clone.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to 6441f0fc and rebuilt): median per-pair ratio 1.0028 (+0.28%), ok. Load average was ~7 at the start.
- Follow-ups:
  - C2: main still includes headers only the init chain needed (`ModelNameProvider.h`, `ModelOptions.h`, …). Trim them when the phases move out.
  - C2: the default member initializer `only_new_episode = false` is now redundant with the constructor's initializer list; tidy it up when the class grows.
- Next: C2.

### C2: Move the phases and the step loop (done, 2026-10-16)
- Base: 60678ae9.
- Commit 20e8bf39: the warmup, EIR calibration, pre-main-phase init, checkpoint write/resume, intervention period and final flush moved from `main()` into `Simulation`.
  - Public: `run(const CheckpointOptions&)` (the whole program after construction), `run_phase(name, end)` (the verbose Starting/Finishing lines around `run_until`), `run_until(end)`, `step()` (the old loop body).
  - Private: `start_new()` (fresh start up to the checkpoint write and the `--checkpoint-stop` throw), `resume(file)`, `finish()`.
  - `CheckpointOptions { file_name = "checkpoint", enabled, stop_after_write }` is built in `main.cpp` from `CommandLine`. Simulation still reads `CommandLine` for `VERBOSE`/`PROGRESS` (globals stay until D).
  - `end_time`, `est_end_time` and the old `static int lastPercent` are now members. `checkpoint.{h,cpp}` is unchanged; `Simulation` passes its members to `readCheckpoint`/`writeCheckpoint` by reference as main did with its locals.
  - `main.cpp`: 303 → 123 lines (101 without the licence header). What's left is includes, `checkpoint_options()`, parse/load/`XMLChecker`/construct/`run`, and the unchanged catch chain (~50 lines). The done criterion says "under ~100"; the remaining bulk is the exception → exit-code mapping, which belongs to the CLI and is left for stage I.
  - C1 follow-ups done: main's init-only includes are trimmed; the redundant `only_new_episode = false` default is gone.
- Deviations from a pure cut-paste, none observable:
  - `print_errno` builds its `perror` prefix with `std::string` + `std::to_string` instead of `sprintf` into `char[256]` (tidy: C arrays, varargs, array decay, C-style cast). It saves `errno` before the allocation and restores it before `perror`, so the text is the same. This also removes the baseline `sprintf` deprecation warning from A2.
  - The three moved verbose `cout ... << endl` lines carry `// NOLINT(performance-avoid-endl)`. Changing them to `'\n'` would change stdout flush timing relative to stderr. `performance-avoid-endl` is in STYLE.md's "kept on, with expected legacy hits" list, so the NOLINT is the sanctioned route. The `cerr << "In: " ...` line in main (touched only by the local rename) uses `'\n'`, since `cerr` is unbuffered.
  - `~Simulation()` is defined out of line (`= default` in the .cpp) so `main.cpp` doesn't need `Clinical/ClinicalModel.h` for `unique_ptr<ClinicalModel>` inside `Human`. Copy and move are `= delete` (rule of five; nothing needs to move a `Simulation` yet).
  - main's locals `exitStatus`/`scenarioFile` became `exit_status`/`scenario_file` (declarations on changed lines).
- Gates (at 20e8bf39):
  - G-exact / G-unit: 52/52, 12.0 s at `-j10` under `caffeinate -i`. Build warnings: only the duplicate `-lc++`.
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check HEAD` (before commit, base 60678ae9) clean; the pre-commit clang-format hook passed.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to 60678ae9 and rebuilt): median per-pair ratio 1.0031 (+0.31%), ok. Load average ~2.6 at the start.
  - Extra A/B check beyond G-exact (stdout and stderr aren't covered by ctest): `scenario5.xml` and `scenarioVecTest.xml`, each with `--verbose --progress`, `--checkpoint-stop --verbose --progress` and `--checkpoint --verbose --progress`, run twice in the same dir (so the second run resumes from the checkpoint where one was written). Base (60678ae9) and candidate gave byte-identical stdout, stderr, `output.txt`, `checkpoint`, `checkpoint0.gz` and exit codes for all 12 runs.
- Surprises / follow-ups:
  - **Pre-existing bug, kept as is:** `print_progress(int last_percent, ...)` takes `last_percent` by value, so the caller's value never changes (it was a function-static `-1` in main, now a member). With `--progress`, every step prints a `\r N%` line (~40 KB of stderr on scenario5) instead of one per percent. The fix is to pass it by reference; it only changes `--progress` stderr. Left for the user to approve as a separate behaviour change.
  - The C1 accessors `population()`, `transmission()`, `scenario()`, `human_warmup_length()`, `survey_only_new_ep()` now have no callers. Kept as the start of the public surface; D/H decide which stay.
  - `step()` still reaches the old globals (`sim::`, `InterventionManager`, `mon::`, `CommandLine`). That is stage D's job.
- Next: S1 (recommended order item 7).

### Plan amendment: Stage M, modernisation (2026-10-16, after C2)
- At the user's request, the plan now covers migrating legacy C++ practices and adopting modern libraries. The new **Stage M** (M0–M9) adds: a dependency mechanism and `DEPENDENCIES.md` (M0), Catch2 (M1, M9), a `modernize-module` skill run on each module after its rename in D (M2), spdlog logging (M3), portable random distributions in place of `gsl_ran_*` (M4), Eigen (M5), and Boost.Math in place of the rest of GSL and R_nmath (M6). It also adds oneTBB for replicate-level (M7) and per-human (M8) parallelism.
- New gates: G-diff (differential numeric tests, from M1), G-cli (byte-identical console I/O, formalising the C2 A/B check, from M3), G-threads (from M8). The draw-order exception now also covers deliberate floating-point changes.
- New reference section: § Modernisation policy (C++20 usage, library/licence table, rules for adding a library, what the per-module pass migrates).
- New open decisions 5 (how dependencies are obtained, before M0), 6 (oneTBB's Apache-2.0 licence vs GPLv2, before M7) and 7 (how far to replace GSL, before M5).
- Recommended order changes: M0 → M2 come next, before S1 (so D1 can use them). M4 follows D2b, so `test/expected/` is regenerated once. M3/M7 come after D9, M5/M6 after F6, and M8 after G3. D chunks now also need M2.
- Next: M0 (ask Decision 5 first).

### M0: Dependency mechanism (done, 2026-10-16)
- **Decision 5 (asked at the start of this chunk): FetchContent with `FIND_PACKAGE_ARGS`**, the recommended option: an installed package first, otherwise a pinned, hash-checked fetch.
- Base: 3b19a607.
- Commits:
  - e5205c8a: `cmake_minimum_required` 3.15 → 3.24
  - 331463a7: `cmake/Dependencies.cmake` with `om_dependency(<name> VERSION URL SHA256 [FIND_PACKAGE_ARGS ...])`, included from the root `CMakeLists.txt`. No dependency is declared yet.
  - 45d57ba3: `docs/refactor/DEPENDENCIES.md`
- **`om_dependency`** calls `FetchContent_Declare(URL, URL_HASH SHA256=, FIND_PACKAGE_ARGS <VERSION> GLOBAL ...)`, then `FetchContent_MakeAvailable`, and prints whether the dependency was found installed or fetched. `GLOBAL` makes an installed package's imported targets visible tree-wide, whichever directory calls it. The overrides (`FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`, `FETCHCONTENT_FULLY_DISCONNECTED`, `FETCHCONTENT_SOURCE_DIR_<NAME>`) are documented in DEPENDENCIES.md.
- Mechanism test: a scratch project with `fmt` 12.2.0, which Homebrew has installed.
  - default: used the installed copy
  - `TRY_FIND_PACKAGE_MODE=NEVER`: fetched, built and linked statically
  - a minimum version above the installed one: fell back to the fetch
  - a wrong SHA256: configure failed with "Hash mismatch"
- **Policy check for 3.15 → 3.24.** Fresh configures of the base and the candidate (both Release with tests, paths normalised) give identical `compile_commands.json`. The rebuilt `openMalaria` (sha1 `7a934957…`) and `unittest` (`ae86e04e…`) are byte-identical to the base build, both after the bump and after the include.
- **CI style job's install step: no change needed in M0.** Ubuntu 24.04's apt CMake is 3.28, which meets 3.24, and GitHub runners also preinstall a newer CMake. No library is fetched yet. `util/ci-style.sh upstream/main` from a fresh `build-style/` is clean.
- Gates (at 45d57ba3):
  - G-exact / G-unit: 52/52, 11.1 s at `-j10` under `caffeinate -i`
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check 3b19a607` reports no changed C++ files
  - G-perf: not a gate here. The binaries are byte-identical anyway.
- Surprises, recorded in DEPENDENCIES.md:
  - **Licences: GSL is GPL-3.0-or-later and Xerces-C is Apache-2.0**, so a distributed `openMalaria` binary is already GPLv3-only in effect. Decision 6's concern (oneTBB's Apache-2.0 vs GPLv2) therefore does not narrow today's binaries. It matters only for a future build without GSL and Xerces-C. Mention this when asking Decision 6.
  - `contrib/README.txt` is stale. It says CxxTest 3.10.1 under LGPL-2.1, but the tree has CxxTest 4.4 under LGPL-3.0 (826bd5a8, refreshed in #398). Left as is; DEPENDENCIES.md is now the source of truth.
  - `docker/Dockerfile` uses Ubuntu 22.04's apt CMake (3.22), which is below the new minimum. It builds upstream's `schema-50` branch, not this tree, so it is unaffected for now.
- Follow-ups:
  - M1: declare Catch2 through `om_dependency` and pin a release tarball plus SHA256. Add the Catch2 apt package to the CI install step only if the style job's configure would otherwise fetch it on every run. Ubuntu 24.04 ships Catch2 3.4.0, so the pinned `VERSION` decides whether that copy is accepted.
  - If headers from fetched libraries start producing compiler warnings, `FetchContent_Declare(SYSTEM)` needs CMake 3.25.
  - H2 (install/export): fetched projects bring their own `install()` rules into ours. Turn them off per dependency (e.g. `CATCH_INSTALL_DOCS`/`..._INSTALL` options) or use `EXCLUDE_FROM_ALL`.
- Next: M1.

### M1: Catch2 as a second unit-test target (done, 2026-10-16)
- Base: 053b4d56.
- Commits:
  - de71bd96: `.clang-tidy` sets `cppcoreguidelines-avoid-do-while.IgnoreMacros: true` (recorded in STYLE.md § Options)
  - c7293e4e: Catch2 3.16.0 through `om_dependency` in `unittest/CMakeLists.txt`; new `unittest-catch2` executable (`Catch2::Catch2WithMain` plus the same libraries as `unittest`), registered as ctest test `unittest-catch2` with `--warn NoAssertions`; `UtilVectorsSuite.h` ported to `unittest/UtilVectorsTests.cpp` and removed from the CxxTest list (git records it as a rename)
  - 80034802: cpp-style skill gains § Unit tests (new tests are Catch2, anonymous namespace, tags, `WithinRel || WithinAbs` with named tolerances)
  - 941c2e77: DEPENDENCIES.md records Catch2 (pin, hash, licence, notes)
- **Pin:** v3.16.0 (released 2026-08-25). v3.16.1 (2026-10-08) was skipped as under two weeks old. Tarball SHA256 `0957cae5…af34`.
- **Pilot:** `UtilVectorsSuite` (one test, `logDFT` → `expIDFT` round trip). Chosen because it links `model`, needs approximate comparison (as every G-diff test will), and covers `util/vectors.h`, which M5 replaces. Same 6 inputs. CxxTest's `TS_ASSERT_APPROX` passes when `|x/y − 1| < 1e-7` or `|x − y| ≤ 1e-7`. The port uses `WithinRel(1e-7) || WithinAbs(1e-7)`; Catch2's relative test is `|x − y| ≤ ε·max(|x|, |y|)`, which is equivalent to first order. Mutation check: shifting the expected values by a relative 2e-7 makes the test fail; reverted.
- **Installed vs fetched, both tested:** the default here fetches (no Homebrew Catch2). A Catch2 3.16.0 installed into a scratch prefix and passed with `-DCMAKE_PREFIX_PATH` was picked up (`using installed 3.16.0`), built and passed.
- **CI:** no change to the install step. Ubuntu 24.04's Catch2 3.4.0 is below the pin, so the job fetches 1.2 MB at configure time. `util/ci-style.sh upstream/main` from a fresh `build-style/`: Catch2 fetched, G-style clean, 10 s wall. `catch_user_config.hpp` is generated at configure time, so clang-tidy parses the test without building Catch2.
- Gates (at 941c2e77):
  - G-exact / G-unit: 53/53 (51 scenarios, `unittest`, `unittest-catch2`), 11.8 s at `-j10` under `caffeinate -i`. CxxTest now runs 65 tests (one fewer); Catch2 runs 7 assertions in 1 test case.
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check 053b4d56` clean.
  - G-perf: not a gate here; no `model/` code changed.
- Surprises / deviations:
  - **Catch2 v3 dropped `--warn NoTests`** (an unknown warning name makes the binary exit 1). v3 already fails a run that matches no tests, so only `--warn NoAssertions` is passed.
  - **Every Catch2 assertion hit `cppcoreguidelines-avoid-do-while`**, because the macros wrap their bodies in `do { } while (false)`. Fixed tree-wide with `IgnoreMacros: true` rather than per-line NOLINTs. It also hides any legacy macro hits in `model/`, which are the same idiom.
  - Nothing else in Catch2's macros (`TEST_CASE` registrars, `CAPTURE`) tripped the enabled checks, once the test cases are in an anonymous namespace.
  - Catch2 as a subproject has no `install()` rules (they need it to be top level), so M0's H2 follow-up does not apply to Catch2.
  - The configure option is still called `OM_CXXTEST_ENABLE` and now covers both frameworks. Rename it (e.g. `OM_UNITTEST_ENABLE`) in M9 when CxxTest goes; `build.sh` and `util/ci-style.sh` pass it.
- Follow-ups:
  - M9: port the remaining 14 compiled CxxTest suites (+ 4 disabled ones, plus `ExtraAsserts.h`/`UnittestUtil.h` helpers), then fold `unittest-catch2` into `unittest` and rename the option.
  - If the per-case granularity of `catch_discover_tests` is wanted (one ctest test per `TEST_CASE`), it needs `include(Catch)` from `${catch2_SOURCE_DIR}/extras` or the installed config dir. Left out to keep one registration that works both ways.
- Next: M2 (`modernize-module` skill, pilot on `model/PkPd/`).

### M2: `modernize-module` skill plus a pilot (done, 2026-10-16)
- Base: 3f290fc4.
- Commits:
  - afd3fa8c: `rename_module.py` reads nested namespace declarations (`namespace OM::pk_pd {`), see below
  - 9183ffe4: `util/style.sh modernize <dir>` and `util/modernize_module.py`
  - ca51c409: `.clang-tidy`: `cppcoreguidelines-init-variables.MathHeader: '<cmath>'` (the default inserts `<math.h>`)
  - bff08fc0, b979260e: tool fixes found by the pilot (missing `<numbers>`, duplicate includes)
  - a0f1112c: **pilot tool commit**, `model/PkPd` safe fix-its (15 files, +91 / -105)
  - d4869a62: a0f1112c added to `.git-blame-ignore-revs`
  - d9d45adb: `--report` also lists safe-list findings that have no fix-it
  - 7b330170: **pilot hand commit**, `model/PkPd` judgement items
  - a592041f: `.claude/skills/modernize-module/SKILL.md`
  - 565239f1: STYLE.md § Module modernisation
- **How the tool works** (details and the checks left out, with reasons, in STYLE.md § Module modernisation): clang-tidy with `MODERNIZE_CHECKS` (the `fix` safe list plus 18 more) over every TU with the header filter set to the module; keep each diagnostic located in the module whose replacements all stay inside it; merge duplicates across TUs; defer overlapping fixes to the next pass; apply each pass once with `clang-apply-replacements`; repeat to a fixed point (max 5); then `format-changed`. `--dry-run` shows the first pass; `--report` lists what is left for the hand commit. About 10 s per pass over all 75 TUs.
- **Pilot tool commit** (3 passes): 18 `use-override`, 10 `use-default-member-init`, 9 `concat-nested-namespaces`, 8 `init-variables`, 7 `use-using`, 5 `pro-type-member-init`, 2 each of `use-equals-default`, `use-std-numbers`, `prefer-member-initializer`, `use-emplace`, `loop-convert`, 1 each of `deprecated-headers`, `use-ranges`, `unnecessary-value-param`, `use-equals-delete`, plus 2 `#include <numbers>` added by the tool. A second `--dry-run` applies nothing.
- **Pilot hand commit:** `make_unique` for the 3 `unique_ptr<LSTMDrug>(new …)`; the 2 stream formattings → `std::format` (`{:g}` matches `ostream << double`); `calc_factor`'s in/out `double*` → `double&`; `set_conversion_parameters(p, bm)` → `p = conversion_parameters(bm)`; deleted members public; `LSTMDrugPD` rule of zero and `explicit`; the assignment in an `if`; a no-op `std::move`; 24 `misc-const-correctness` consts (west const). Left, with reasons in the commit message: `endl` (flush timing; one `NOLINT` on a touched line), globals and GSL workspaces (D5), the `const_cast` for GSL's `void*` params, reference data members, the file-local helpers that take `ParamsConvFactor` by value.
- Gates:
  - G-exact / G-unit: 53/53 after the tool commit (16.0 s) and after the hand commit (13.7 s), `-j10` under `caffeinate -i`. Only the duplicate `-lc++` warning.
  - G-style: `OM_BUILD_DIR=build-rework util/style.sh check 3f290fc4` clean. Before the ignore-revs entry, `check HEAD` on the tool commit had a clean format half and 10 tidy findings: 9 legacy `non-private-member-variables-in-classes` on touched lines, and the private `= delete` that `use-equals-delete` wrote (fixed in the hand commit).
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to 3f290fc4 and rebuilt), candidate at 565239f1: median per-pair ratio 0.9899 (−1.01%), per-pair 0.9515–1.0171, ok. Load average ~6.4 at the start; base spread 6.2%, candidate 2.2%.
  - Extra: `--print-genotypes` on scenarioGenotypes, scenarioMolPairwise and scenarioEffectiveDrug, base (3f290fc4) vs candidate: byte-identical stdout, stderr and exit codes.
- Surprises:
  - **`modernize-use-std-numbers` inserts no include when the constant comes from a macro** (`PI` in `Global.h`), so the next pass failed to compile. The tool now adds `#include <numbers>`.
  - **`modernize-use-ranges` inserts an include the file already has** (`<algorithm>` in `LSTMDrug.cpp`). The tool drops such insertions.
  - **`cppcoreguidelines-init-variables` defaulted to `<math.h>`.** Changed to `<cmath>` in `.clang-tidy`, which also affects `util/style.sh fix`.
  - **`modernize-concat-nested-namespaces` would have broken the rename tool's namespace ownership test**, which matched only `namespace X {`. After it, a module writing `namespace OM::pk_pd {` would no longer count as declaring `OM`. Fixed first (afd3fa8c).
  - **`misc-const-correctness` writes east const and edits headers in other modules** (`util/vectors.h`, `random.h`, … reached through the PkPd TUs). The skill applies it per file, reverts other modules and rewrites to west const.
  - `modernize-loop-convert` wrote `for (unsigned int& phen : genotype_mapping)`; the hand commit made it `const uint32_t`.
  - `cppcoreguidelines-prefer-member-initializer` left an empty first line in a constructor body; the hand commit tidied it (and moved `ic50` into the initializer list too).
- Module status (rename = `/rename-module`, modernise = `/modernize-module`); every other module is still to do:

  | Module | Rename | Modernise |
  |---|---|---|
  | `model/PkPd` | cae5d3b5 (B4) | a0f1112c + 7b330170 (M2) |

- Follow-ups:
  - D chunks: run `/rename-module` then `/modernize-module` on each module the chunk owns, and add a row to the table above.
  - The tool keeps fixes inside the module. A fix that spans two modules (e.g. `performance-unnecessary-value-param` with the declaration in one and the definition in the other) is listed under "reaches outside the module" and is never applied by the tool from either side. Do those by hand in the hand commit. PkPd had none.
  - `ParamsConvFactor` (≈20 doubles) is passed by value to four file-local helpers in `LSTMDrugConversion.cpp`. `const&` would be cheaper; left for D5, which owns PkPd's hot path, with G-perf.
- Next: S1 (recommended order item 7b).

### S1: Seeded replicate runner (done, 2026-10-16)
- Base: 9b3372f5.
- Commit b372c2f6: `test/stats/run_replicates.py`.
  - Usage: `OM_BUILD_DIR=build-rework test/stats/run_replicates.py <NAME|path.xml> -o OUT [-n N | --seeds S...] [--first-seed 1001] [-j JOBS] [-b] [-q]`. The default seeds are 1001…1000+N, as in § Statistical tests.
  - Each seed runs in its own temporary directory on a byte copy of the scenario with only the `iseed` value changed. The regex allows `iseed ="1"`, which the two GaussianCopula scenarios use. The scenario must contain exactly one `iseed`, or the runner exits 2. The run is plain, with no checkpoint flags.
  - Output layout: `OUT/<seed>/output.txt`, plus `OUT/<seed>/ctsout.txt` when the scenario writes one. `.gz` outputs are decompressed. `OUT/meta.json` records the scenario path and SHA256, the schema name, the binary path and SHA256, the source commit and dirty flag of the build's source tree, the output format, and the creation time. This is S2's metadata source.
  - **Resumable:** seeds that already have `output.txt` are skipped. Each seed is written to `<seed>.partial` and renamed when complete. On a rerun into an existing `OUT`, a changed scenario hash, binary hash or output format is refused (exit 2), so one directory never mixes builds.
  - Reuse of `test/run.py`: the runner loads the CMake-configured `<build>/test/run.py` as a module (`getSchemaName`, `linkOrCopy`, `binaryToText`, `openMalariaExec`, `testSrcDir`). It restores `sys.path` afterwards, because `run.py` overwrites `sys.path[0]`. `run.py` itself is unchanged.
  - Exit codes: 0 if every seed succeeded; 1 if any seed failed (the other seeds still run, and the failure prints the tail of openMalaria's stderr); 2 for usage or setup errors.
- Done criterion, on `scenario1`:
  - Seeds 1001–1020 were run into two directories, at `-j10` and at `-j3`. `diff -r` shows only `meta.json`'s `created` line; the 40 `output.txt`/`ctsout.txt` files are byte-identical.
  - The 20 seeds give 20 distinct `output.txt` files. 1001 vs 1002: `compareOutput.py` reports 507 significant differences.
  - Extra check: `--seeds 1` (the scenario's own `iseed`) reproduces `test/expected/output1.txt` and `ctsout1.txt` byte for byte. So the copy, rewrite and run path matches what ctest runs.
- Other checks: resume (`-n 5` over 4 existing seeds runs only 1005); meta mismatch refused; missing scenario, missing build dir, no `iseed`, out-of-range seed (all exit 2); a scenario that makes openMalaria fail (exit 1, stderr shown); the temp dirs are removed. `scenarioBench.xml` by path: 2 seeds in 16 s at `-j2`. `uvx ruff check` is clean.
- Gates: G-exact / G-unit 53/53, 13.1 s at `-j10` under `caffeinate -i`. No C++ changed, so G-style and G-perf don't apply.
- Surprise: **binary mode output is not line-identical to text output.** `run.py`'s `binaryToText` writes all integer measures before the double ones, so `-b` on seed 1 gives the same 792 rows as `expected/output1.txt` in a different order (`compareOutput.py`: no differences). S2 and S3 must key cells by (survey, group, measure), not by line. The `-b` help says so.
- Follow-ups:
  - S2: read `OUT/*/output.txt` and `OUT/meta.json`. `meta.json` has the commit, but not the schema *version* that § Statistical tests (1) asks for: take it from the `schemaVersion` attribute of the scenario, or from `meta["schema"]`.
  - S5: run one `OUT` per scenario. `--first-seed` and `--seeds` let a slow scenario be split across sessions, and the skip-existing behaviour makes reruns safe.
  - I1: switch from rewriting `iseed` to the new `--seed` flag (already in the plan).
- Next: S2.

### Plan amendment: findings register (2026-10-16, after S1)
- At the user's request, bugs, surprises and strange design decisions now go in a new register, `docs/refactor/FINDINGS.md`, with stable IDs (`F<n>`). Before this, they were only in the per-chunk log entries. PLAN.md § Session protocol gains step 8 (record findings as you go; don't fix output-changing ones inside a refactor chunk without asking), and step 2 now reads the open entries for the modules a chunk touches.
- Seeded with F1–F14, gathered from earlier log entries plus four new ones from S1: F2 (stale `errno` printed after errors), F3 (bad input reported as a C++ bug), F4 and F5 (dead or wrong code in `test/run.py`).

### S2: Reference builder (done, 2026-10-16)
- Base: 17935594.
- Commits:
  - 841b2a9f: `test/stats/generate_reference.py`, `test/stats/stats_io.py`, `test/stats/test_generate_reference.py`
  - 720c0f09: `test/reference/scenario1.csv`, a provisional reference at N = 50 (seeds 1001–1050, built at 17935594) for S3 and S4 to work against. S5 replaces it.
  - 3c40c24b: findings F15–F17
- **Usage:** `test/stats/generate_reference.py <run_replicates OUT dir> [-o file.csv] [--allow-dirty]`. The default output is `test/reference/<scenario file stem>.csv`, e.g. `scenario1.csv`. It refuses replicates from a dirty tree unless `--allow-dirty` is passed, and needs at least 10 replicates. Exit 2 on errors.
- **numpy through uv.** The system Python has no numpy, and pure Python would be too slow at 190k cells × 500 replicates. Both new scripts have a `uv run --script` shebang with PEP 723 metadata (`numpy>=2,<3`; uv picked 2.5.2), so running them directly is enough. `stats_io.py` is stdlib only, so S3's checker can run under ctest without numpy.
- **What the reference holds** (format in `stats_io.py`):
  - Cells keyed by `(file, survey, group, measure)`: `survey` rows from `output.txt` (by key, not line, per F7); `ctsout` rows keyed by the row's first column and the column header (repeated headers get `#2`, …); `aggregate` rows.
  - Columns `mean, sd, min, max, lo, hi`. Cells: `lo`/`hi` are the 2.5%/97.5% quantiles. Aggregates: `lo`/`hi` are Bonferroni-widened over the aggregates present (e.g. 0.5%/99.5% for 5). Floats are written with `repr`, so they read back exactly. A cell is zero-width when `min == max`, or when it is NaN in every replicate.
  - Quantiles are Hyndman–Fan type 7 (numpy's default), computed from sorted arrays. The unit tests check them against `np.quantile`.
  - **Leave-one-out threshold:** each replicate's miss count against the interval of the other N − 1, using the same rule S3 will use (`v < lo or v > hi`). The threshold is `ceil` of the 99.9th percentile. The LOO quantiles come from the full sort plus each replicate's rank, with no recomputation. The unit tests check them against brute force, with ties.
  - **Aggregates** (`stats_io.AGGREGATES`), using whole-population rows only (group < 1000, i.e. no cohort or genotype split): `clinical_episodes` (measure 14), `severe_episodes` (15), `deaths` (18 + 19), `mean_prevalence` (Σ nPatent / Σ nHost), `mean_eir` (mean of measure 36, or else the mean of ctsout's `simulated EIR` without NaNs). An aggregate is included only if every replicate has it. scenario1 has only `mean_prevalence` and `mean_eir`.
  - Metadata header: format version, scenario and SHA256, schema, schema version (from the scenario's `schemaVersion`, only if its hash still matches `meta.json`; S1 follow-up), commit, dirty, binary SHA256, output format, N, seeds, dates, quantile method, cell counts, zero-width count, LOO miss summary and threshold, `loo_zero_width_failures`, aggregates and their interval.
  - A cell that is NaN in some replicates but not all is an error. No such cell has turned up yet; S5 will show whether one does.
- Done criterion:
  - scenario1 at N = 50: 1036 cells (792 survey, 244 ctsout), 198 of them zero-width. LOO misses: min 5, median 46, mean 50.8, max 99. Threshold 99.
  - Unit tests: 22 pass (`test/stats/test_generate_reference.py`), on synthetic data: quantiles, LOO against brute force, zero-width failures, NaN handling, parsers, aggregates, and end to end (metadata, known statistics, reordered binary-style output, exact round trip, mismatched cells, dirty tree, too few replicates, changed scenario).
  - Preview for S3: the scenario's own seed 1 (`test/expected/output1.txt`, `ctsout1.txt`) has 26 interval misses (threshold 99), no zero-width mismatches, and both aggregates inside their intervals.
  - `uvx ruff check test/stats/` is clean.
- Scale: ModelNameNoOverrides, 189,800 cells × 38 replicates: 3.9 s, 337 MB peak. The work is done in column chunks of 16k cells, so N = 500 needs ~0.8 GB for the data matrix plus small per-chunk buffers.
- Gates: G-exact / G-unit 53/53, 13.6 s at `-j10` under `caffeinate -i`. No C++ changed, so G-style and G-perf don't apply.
- Surprises (in FINDINGS.md):
  - **F15:** the plan's exact-match rule for zero-width cells would give many false failures. Rare events, such as an `nTreatments` cell that is 0 in 49 of 50 runs, look zero-width. In leave-one-out on scenario1, 19 of 50 replicates would fail that rule. Each reference records this as `loo_zero_width_failures`.
  - **F16:** all four `ModelName*` scenarios abort with `Transmission warmup exceeded 30 iterations!` (exit 76) for 40–80% of seeds 1001+. The scenarios' own seed works.
  - **F17:** for strongly correlated outputs the LOO miss counts are heavy-tailed, so the 99.9% threshold is near the maximum and gives little power.
- Follow-ups:
  - S3: settle the zero-width rule (F15) first. The data suggests counting a zero-width mismatch as an ordinary miss, since the LOO threshold already includes such misses, with exact match only for cells known to be deterministic. That changes the plan's § Statistical tests (2), so **ask the user**. Use `stats_io.read_reference`, `read_run` and `aggregates`, so the cell keys and aggregate definitions can't drift from the generator.
  - S4: register `test/stats/test_generate_reference.py` with ctest under the `statistical` label. It needs uv (and numpy, fetched on first use).
  - S5: the `ModelName*` scenarios need a decision (F16): skip them, use only seeds that converge (which biases the reference), or fix the model. Failed seeds leave no directory, so the generator just sees fewer replicates. Choose `-n` so that enough seeds succeed.
  - S6: F17 bears on the power study.
- Next: S3.

### S3: Checker with reruns (done, 2026-10-16)
- Base: 0f1faa5d.
- **Method changed with the user before any code.** The plan's exact-match rule for zero-width cells would fail about 1 in 3 correct runs (F15). The user proposed per-run probabilities plus automatic reruns. The method now in PLAN.md § Statistical tests (2):
  - One run fails if (a) its miss count has p ≤ 1% against the stored leave-one-out counts, (b) an aggregate is outside its 99%-family interval, or (c) a fixed cell differs. A fixed cell is constant at a non-zero value (or NaN) in every replicate, in practice the echoed input EIR. Cells that were 0 in every replicate are ordinary misses.
  - Run 1 uses the scenario's own `iseed`. If it fails, seeds 2001–2005 run, and the check fails if any of them also fails.
  - `--replicates K` runs K seeds up front and fails on a binomial tail ≤ 0.1% at a 2% per-run rate.
  - Distributions are empirical, not fitted (the user's first idea), because many cells are small counts that are mostly 0.
- Commits:
  - 46cef63b: PLAN.md, the method above (S3 and S6 chunk text, § Statistical tests (1), (2) and (4))
  - e61e95d4: `run_replicates.py` `main` split into `prepare` and `run_seeds` for reuse, no behaviour change. Seed 1 still reproduces `test/expected/output1.txt` and `ctsout1.txt`, and resume still works.
  - 2b6281d6: reference **format 2**. It adds `loo_miss_counts` (all N counts), `run_alpha`, `max_passing_misses` (replaces `miss_threshold`) and `loo_run_failures` (leave-one-out estimate of the per-run failure rate under (a)–(c)). The aggregate family moves from 95% to 99%. `stats_io` gains `RUN_ALPHA`, `run_p_value`, `is_fixed_cell`, `is_miss` and `same_value`.
  - 5384584d, then ed38bba4: `test/reference/scenario1.csv` at **N = 500** (seeds 1001–1500), regenerated after the SD fix
  - 012e38d8: SD 0 for constant cells (F21)
  - 7472b8b5: `test/stats/check_scenario.py`, `test/stats/test_check_scenario.py`
  - fcbfc337: findings
- **Usage:** `OM_BUILD_DIR=build-rework test/stats/check_scenario.py <NAME|path.xml> [--reference CSV] [--retry-seeds S...] [--replicates K | --outputs DIR] [--work-dir DIR] [-j N] [-v]`.
  - The default reference is `test/reference/<scenario file stem>.csv`.
  - `--outputs` judges existing `DIR/<seed>` run directories without running anything. `--work-dir` keeps run outputs and reuses those already there.
  - The checker refuses seeds that are in the reference's seed range. It warns if the scenario's hash differs from the reference's.
  - Exit 0 pass, 1 statistical fail, 2 error. A crashed run, missing or extra cells, and a wrong format version are errors.
  - Stdlib only, so ctest needs no numpy.
- Done criteria, scenario1:
  - Unchanged build: passes (run seed 1: 13 misses, p = 0.74; both aggregates inside; 62 fixed cells equal).
  - EIR × 1.5 (all 853 `EIRDaily` values scaled): fails. Run 1 fails (a), (b) and (c) (150 misses, p = 0.004), and so do all 5 reruns.
- Extra evidence:
  - Leave-one-out at N = 500: 10 of 500 replicates fail per run (5 on misses, 7 on aggregates, 0 on fixed cells), 2.0%.
  - 1600 fresh seeds (2001–3600, via `--replicates 1600`): 31 runs fail (1.9%): 8 on misses, 23 on aggregates (F18), 0 on fixed cells. The implied false-failure rate of the whole procedure is 0.019 × (1 − 0.981⁵) ≈ 0.18% (target ≤ 0.5%).
  - EIR × 1.1: fails in both modes (rerun mode 6/6 runs; `--replicates 20` 20/20). This is mainly through the fixed `input EIR` cells; on miss count alone, 3 of 6 runs fail (F19).
- Tests: `test_generate_reference.py` 27 pass, `test_check_scenario.py` 17 pass. `uvx ruff check test/stats/` is clean.
- Gates: G-exact / G-unit 53/53, 15.2 s at `-j10` under `caffeinate -i`. No C++ changed, so G-style and G-perf don't apply.
- Deviations from the approved S3 plan:
  - The planned extra finding (a single wrong fixed cell would not trigger reruns) went into F15's closing note instead.
  - `max_passing_misses` replaces `miss_threshold` instead of keeping it: under the p-value rule, a fixed threshold would mislead.
  - The planned "50 fresh seeds" check became 1600 fresh seeds.
- Bug found on the way: `binomial_tail` overflowed for K ≈ 1600 (`math.comb` × float). It now works in log space.
- Follow-ups:
  - S4: register `stat_<scenario>` as `check_scenario.py <NAME>` with label `statistical`. Register the two unit-test scripts too: `test_check_scenario.py` needs only python3, `test_generate_reference.py` needs uv. Every run writes to a temp dir, so `-j` is safe.
  - S5: references must use N ≥ 100 (F20); 500 is planned. Format 2 holds N counts in one metadata line, about 2.5 kB at N = 500. ModelName* still needs a decision (F16); a crashed rerun seed is an error for now.
  - S6: measure per-run rates on more scenarios (F18). Use power perturbations that aren't echoed into the output (F19). Tune α, K and the rerun rule if needed.
- Next: S4.
