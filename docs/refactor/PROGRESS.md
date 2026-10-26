# Refactor progress

See `PLAN.md` for the session protocol, chunks and gates. Append one log entry per chunk. `README.md` explains how to read the refactor afterwards: the documents, the commit pattern, and what evidence each gate gives.

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
| B6 | done | 89fd11f7, b4f516a1, 523adf7b, fd5a52bd, d29f4df8, b3bf778f, 32f8a5ce, 773f2154, (this log entry) |
| P1 | done | f29d38e3, 81a14090, b0309128, 6bfcf8c0, 4b803c7f, (this log entry) |
| S1 | done | b372c2f6, (this log entry) |
| S2 | done | 841b2a9f, 720c0f09, 3c40c24b, (this log entry) |
| S3 | done | 46cef63b, e61e95d4, 2b6281d6, 5384584d, 012e38d8, ed38bba4, 7472b8b5, fcbfc337, (this log entry) |
| S4 | done | 507b5b58, (this log entry) |
| S5 | done | c155c7fb, 7a28ca71, 50fac6c1, ec9a409c, (this log entry) |
| S6 | done | 350fdaf3, d4a84ef7, a382bc2c, c9f46c93, ecdac231, 5cd66064, (this log entry) |
| S7 | done | bf62b4ff, 190ffc99, 9e1f093e, 5cdcf528, fb09fd66, a4f6904a, 109ea58f, (this log entry) |
| V1 | done | 05fbf2e7, 15d07ae6, 402952ff, 184c4414, 4f101e8a, 0f73698e, 1b37cb08, 468377ae, c92f951c, 6b08d688, f82c3c8c, e6e53cdb, f046ed32, efe8c2f6, 77bc655b, (this log entry) |
| C1 | done | b02303db, (this log entry) |
| C2 | done | 20e8bf39, (this log entry) |
| D1 | done | 2e2cac5a, 72e14d9c, bda40963, 4252cba1, 1a0c0b58, 48d3476c, 186d32f5, 21e09c0d, (this log entry) |
| D2 | done | 61fe9993, c98ffe6f, db1c50b1, 0808f323, fdd69348, 82e3c602, (this log entry) |
| D2b | done | 33185613, 9447ccf7, 1647ef78, (this log entry) |
| D3 | done | cfe2250e, b074dcd3, 79d7e9ae, 6e96d1cf, a653b976, efb5ad9d, cf709072, 39f23abd, b8df955e, fbb7c714, (this log entry) |
| D4 | split | D4a–D4d |
| D4a | done | fa7f275c, 49271693, 02029c1e, a07021c4, 06ddff71, 4b223446, 89a7167b, f28d6070, acb0e0c0, f28ffcbe, (this log entry) |
| D4b | done | 1dd81ae7, 7ffc4e6c, 4db82617, 0ddf6209, 808c0e48, 3e1394d2, b4063a76, 7a828ea1, dd059491, (this log entry) |
| D4c | done | 25afa630, f511b389, 24fec740, 73829aad, e53617ab, c7ee238b, d527bbbf, 20f29f45, cc748c60, (this log entry) |
| D4d | done | 6e83ea3f, ed6fad49, 85edc7ef, 5f89fcb0, 35eadf9c, e67dbef9, 1357ccba, 760980b6, caa82068, (this log entry) |
| D5 | done | 8dcdf512, 5c0449e9, c92d9d36, b543d8fd, de48455e, 0c859fb0, a210a53d, eb07d51e, a3789f2f; follow-up 89c90989, 5355535c, 74843c0a, (its log entry) |
| D6 | split | D6a–D6c |
| D6a | done | 1b084b2e, 910bf9a9, 3be68cb7, c38704fb, 225947ca, 4ed41e45, aa825247, f7849271, c2e8281b, f4b0cfd2, c5dc1e95, 25082ee3, 44666fc1, 0d84e89f, (this log entry) |
| D6b | done | 66fdf1e1, 38a88580, a0e9a737, 18a6f0c2, f1831c6f, 227daa2f, d4e7b71c, 51663e3e, 9011466e, (this log entry); F59/F60 fixes e169341c, 75700eda, 41c35aa4, f8f0896b, (their log entry) |
| D6c | done | 38f78eda, a7c68da3, 9e34f002, 6e9038b9, 9cf1ff5a, bb46b13d, c78608b6, 0eb65ebb, (this log entry); F68/F71/F72 fixes ee622593, 5ec95acf, 942a1306, 137ce408, (their log entry) |
| D7 | done | 3de6a425, 31d61953, 8e9e674f, 8b583553, 529d46cf, 5d117152, f4c2f44c, e1f80857, acec99a5, d5761b35, e18142b8, (this log entry); F73 tests 2e3ff85e, 3409b9e9, (their log entry) |
| D7b | done | f844263a, 99a752ec, 6ac27849, 4eaa9225, e1c53c02, 13d177cc, 804c0268, 9f0a66fe, 2a18091d, b38f88e7, (this log entry) |
| D8 | done | da06b67a, 12449692, 2cee6da6, 0ed86df8, 118b811e, da9bfd0f, 7bdf5f73, 9f5e01f4, e894fa4f, 9e31e86b, (this log entry); F79 fix 0b7567cf, 8d9a79a6, 0f28d92a, (its log entry) |
| D9 | split | D9a–D9f |
| D9a | done | 95266baa, a5f91dd1, 2560b4a4, 442d4c81, 39a676fc, 05f577dc, 8313fa51, 5255cf06, 7dddb242, 1d5dcf74, (this log entry) |
| D9b | split | D9b1, D9b2 |
| D9b1 | done | 428b10a9, 6191be63, e691b2a5, fa502292, d099a440, 8a0f1831, (this log entry) |
| D9b2 | done | 18ea267a, 69af0838, f995cf66, f5d211ec, dd43e68a, 5b5ddc61, bfd60dbb, da0d7b74, 722cf695, (this log entry) |
| D9c | todo | |
| D9d | todo | |
| D9e | todo | |
| D9f | todo | |
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
| M4 | done | 1d4cc809, 0cbf0d72, 1856ab43, (this log entry) |
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
  | `model/PkPd` | cae5d3b5 (B4); D5: nothing to rename | a0f1112c + 7b330170 (M2); c92d9d36 + 0c859fb0 (D5) |
  | `model/` root: `sim.h`, `Clock.{h,cpp}`, `Simulation.{h,cpp}`, `checkpoint.{h,cpp}` | 4252cba1 (D1) | 48d3476c + 21e09c0d (D1) |
  | `model/util/random.h`, `model/util/xoshiro.h` | c98ffe6f (D2) | 0808f323 + 82e3c602 (D2) |
  | `model/util/CommandLine.{h,cpp}`, `model/util/ModelOptions.{h,cpp}`, `model/Parameters.h`, `model/util/ModelNameProvider.h` | 6e96d1cf (D3) | efb5ad9d + 39f23abd (D3) |
  | `model/Host/WithinHost/`: `WHInterface`, `WHFalciparum`, `CommonWithinHost`, `DescriptiveWithinHost`, `WithinHostModel` | 06ddff71 (D4a) | 89a7167b + acb0e0c0 (D4a) |
  | `model/Host/WithinHost/Infection/` (not the uncompiled IPT files, F45) | 4db82617 + b4063a76's hand rename (D4b) | 808c0e48 + b4063a76 (D4b) |
  | `model/Host/WithinHost/WHVivax.{h,cpp}` | f511b389 + 24fec740 (D4c) | e53617ab + d527bbbf (D4c) |
  | `model/Host/WithinHost/`: `Genotypes`, `Diagnostic`, `Treatments` | 85edc7ef (D4d) | e67dbef9 + 760980b6 (D4d) |
  | `model/Host/WithinHost/Pathogenesis` | c38704fb + 225947ca's hand rename (D6a) | c2e8281b + c5dc1e95 (D6a) |
  | `model/Clinical/` (not `CMDecisionTree`, D6c) and `model/Host/NeonatalMortality` | a0e9a737 + 18a6f0c2's hand rename (D6b) | 227daa2f + 51663e3e (D6b) |
  | `model/Clinical/CMDecisionTree.{h,cpp}` | 9e34f002 (D6c) | 9cf1ff5a + c78608b6 (D6c) |
  | `model/Transmission` (not the uncompiled AnophelesModel and MosqLifeCycle suites, F9) | 8b583553 + 529d46cf's hand rename (D7) | f4c2f44c + acec99a5 (D7) |
  | `model/Host/InfectionIncidenceModel.{h,cpp}`, `model/PopulationAgeStructure.{h,cpp}` | 6ac27849 (D7b) | 13d177cc + 9f0a66fe (D7b) |
  | `model/interventions` | 2cee6da6 + 9f5e01f4's hand rename (D8) | da9bfd0f + 9f5e01f4 (D8) |
  | `model/mon` | 442d4c81 + 5255cf06's hand rename (D9a) | 05f577dc + 5255cf06 (D9a) |
  | `model/Host/Human.{h,cpp}`, `model/Population.{h,cpp}` | f995cf66 (D9b2) | dd43e68a + bfd60dbb (D9b2) |

  Rows from D4a on were added in D4d; the D4 entries had recorded the commits only in their logs.

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

### S4: CTest integration (done, 2026-10-16)
- Base: 5c7cd82e.
- Commit 507b5b58: `test/CMakeLists.txt`.
  - `stat_<NAME>` for every name in `OM_BOXTEST_NAMES` (and `OM_BOXTEST_NC_NAMES`, empty today), with label `statistical`. Command: `${PYTHON_EXECUTABLE} test/stats/check_scenario.py --build-dir ${CMAKE_BINARY_DIR} <NAME>`, so the default rerun rule applies and the checker's exit codes (1 fail, 2 error) make the test fail.
  - **Skipping:** a scenario with no `test/reference/scenario<NAME>.csv` is still registered, but `DISABLED`, so ctest lists it under "did not run". Configure prints one `STATUS` line naming all of them (50 today). The reference list comes from `file(GLOB … CONFIGURE_DEPENDS)`, so a CSV added in S5 enables its test on the next `cmake --build`, with no manual reconfigure (tried with a temporary `scenario4.csv`, then removed).
  - The S3 follow-up's unit-test scripts, label `statistical` too, so `-LE statistical` stays exactly the old tests: `stats_tool_check_scenario` (`python3`, stdlib only) and `stats_tool_generate_reference` (`uv run --script`, for numpy). The second is registered only if `uv` is found; otherwise configure says so.
- Done criteria:
  - `ctest -L statistical`: `stat_1` passes in 0.37 s (run seed 1: pass), both tool tests pass, the other 50 `stat_*` are reported as disabled.
  - `ctest -N -LE statistical`: the same 53 tests as before S4.
- Extra check: scenario1 temporarily scaled to EIR × 1.5 (853 `EIRDaily` values), then restored from a copy: `stat_1` fails under ctest (run 1 and all 5 reruns fail).
- Gates: G-exact / G-unit / G-stat: plain `ctest -j10` under `caffeinate -i` passes 56/56 (53 old + `stat_1` + 2 tool tests) in 12.8 s. No C++ changed, so G-style and G-perf don't apply.
- Notes for later chunks:
  - **Plain `ctest` now runs the statistical tests too**, so it checks G-exact and G-stat together, and until S5 it ends with a list of 50 disabled tests. Use `ctest -LE statistical` for the byte-for-byte tests alone. The statistical label currently adds under 1 s.
  - When run 1 of a `stat_*` test fails, the checker runs its 5 reruns with `-j` = all cores. Under `ctest -j10` that can oversubscribe the CPU, but it only happens on failures, so it isn't worth a `PROCESSORS` property yet. Revisit in S5 if many slow scenarios get references.
- Follow-ups:
  - S5: each new CSV under `test/reference/` turns its `stat_*` test on. The four ModelName* scenarios stay disabled until F16 is decided.
- Next: S5 (a long compute job; see the recommended-order note) or D1.

### S5: Generate the full references (done, 2026-10-19)
- Base: 3f5c0bb3. Build `build-rework/` from 3f5c0bb3 (no C++ changed since, so the binary is the same at every later S5 commit).
- **Decisions, made with the user before any references:**
  - **ModelName\* (F16): skipped.** No reference, so the four `stat_ModelName*` tests stay disabled. F16 stays open as a model bug for its own task.
  - **References are gzipped** (`test/reference/scenario<NAME>.csv.gz`). The six 189,800-cell scenarios (GaussianCopula ×2, ModelName ×4) give ~14 MB plain CSVs, about 110 MB for all references; gzip is ~3× smaller.
- Commits:
  - c155c7fb: `stats_io.write_reference` gzips when the path ends in `.gz` (`mtime=0`, so the bytes are reproducible); `read_reference` reads either form. The default reference path in the checker and generator, and the CTest glob, are now `.csv.gz`. New test: gzip round trip is exact and reproducible. Tool tests: 17 and 28 pass; `uvx ruff check test/stats/` is clean.
  - 7a28ca71: `scenario1.csv` → `scenario1.csv.gz`, identical after decompression. `ctest -L statistical`: `stat_1` and both tool tests pass.
- **Replicate job (paused at the user's request).** It runs in a separate worktree, so D1 and D2 can change C++ in the main tree meanwhile:
  - Worktree `/Users/blaarm/git/om-s5`, detached at 6a4777b3, whose C++ is identical to 3f5c0bb3. Its own `build-rework/` was configured with the A2 flags. That binary (`d3ab74cf…`) differs in hash from the main tree's (`9d59bc57…`), but it passes `ctest -LE statistical` 53/53, and seeds 4/1001, 4/1500 and Genotypes/1001 reproduce the old replicates byte for byte.
  - Driver: `caffeinate -i /Users/blaarm/git/om-s5/build-rework/s5_run.sh` (untracked). It loops over `OM_BOXTEST_NAMES` minus `1` (already at N = 500 from S3) and `ModelName*`, which is 46 scenarios, running `OM_BUILD_DIR=build-rework test/stats/run_replicates.py <NAME> -o build-rework/stats-replicates/<NAME> -n 500 -j 4 -q` in the worktree. **The user wants at most 4 cores** for this (the machine has 10: 4 performance, 6 efficiency). Rerunning resumes; finished seeds are skipped. Don't take G-perf measurements while it runs.
  - Done, in `/Users/blaarm/git/om-s5/build-rework/stats-replicates/` (500 of 500 seeds, no failed seeds), with wall times at `-j10` on the main-tree binary: 4 116 s, 5 22 s, 6 20 s, 9 12 s, 10 5 s, 11 43 s, 2ITNs 157 s, Cohort 93 s, EffectiveDrug 142 s, Empirical 121 s, ESTS 82 s. They were made in the main tree, and their `meta.json` records its binary and commit 3f5c0bb3.
  - Discarded and rerun in the worktree: 12 (its `meta.json` had said `dirty: true`, F22) and a partial Genotypes.
  - Done in the worktree (2026-10-17, worktree binary `d3ab74cf…`), with wall times at `-j 4`: 12 112 s, Genotypes 776 s, IRS30 206 s, KK_20150612 132 s, LifeNet1 254 s, LifeNet2 (about 4–5 min). **LifeNet1 and LifeNet2 have 499 seeds each:** seed 1223 aborts both (F24). 499 is enough for a reference (F20 needs ≥ 100).
  - Paused again at the user's request, after LifeNet2. The driver was stopped between scenarios, so no directory is partial.
  - Not started: the other 29, from Molineaux on in `OM_BOXTEST_NAMES` order. Rerun `caffeinate -i /Users/blaarm/git/om-s5/build-rework/s5_run.sh` (append its output to `s5_run.log`). The 11 scenarios made with the main-tree binary are refused with exit 2 and left untouched (F25), so their `rc=2` lines in the log are expected. LifeNet1/LifeNet2 are re-entered and retry only seed 1223, which fails again (rc=1, same 499).
- Finished 2026-10-17 at `-j 2` (from Molineaux on), at the user's request. Further losses: **AvailabilityFilter 436** of 500 (warmup exceeded 30 rounds, F26), **ExactNv0 408** (exact solver, negative emergence, F27). The checker's rerun seeds: LifeNet1/2 all 5 run; AvailabilityFilter fails on 2004, ExactNv0 on 2003.
- **Decision with the user (2026-10-19):** no reference for AvailabilityFilter or ExactNv0, as for ModelName*. Fix the warmup in a new chunk **V1**, before D2b (50fac6c1: PLAN.md Stage V, findings F26, F27). SimpleMPDTest was then also left out: its ctsout covers the warmup, so the number of rows depends on the calibration rounds (F29). Same root cause, so V1 covers it too.
- **ec9a409c: 43 new references** (`test/reference/*.csv.gz`, 13 MB together with scenario1). N = 500, except LifeNet1/2 at 499. Leave-one-out per-run failures, of N:
  4 14, 5 16, 6 10, 9 10, 10 8, 11 11, 12 11, 2ITNs **48**, Cohort 16, EffectiveDrug 5, Empirical 11, ESTS 14, Genotypes 12, IRS30 **51**, KK_20150612 13, LifeNet1 7, LifeNet2 9, Molineaux 11, MolPairwise 8, MSAT 9, MSAT_HRP2 13, NamawalaArabiensis 11, NoInterv 15, NoMPDLarviciding 18, Penny 12, Rach5IC **56**, SimpleMPDLarviciding 18, SubPopRemoval 5, TrapTest 14, TriggeredMSAT 12, VecFullTest 14, VecMonthly 13, VecTest 12, Vivax 12, NonHumanHosts 10, PEV 8, BSV 10, TBV 8, DecisionTree5DayDielmo 12, HetVecDailyEIR 11, GaussianCopulaGamma 11, GaussianCopulaLognormal 15, InfectionOrigin 9.
  - So 1–4% everywhere except the three in bold, about 10% each, all from rule (c) on the ctsout `recent births` column (F28). Left for S6, which retunes the rules.
  - Largest: the two GaussianCopula references, 189,800 cells each. Each reference's header has its cell counts.
- Gates: G-exact / G-unit / G-stat: plain `ctest -j2` passes 99/99 (53 old, 44 `stat_*`, 2 tool tests) in 72 s. On its own seed only `stat_MSAT_HRP2` failed (490 misses, p = 0.006); all 5 of its reruns passed. One run-1 failure in 44 is the expected ~2% rate. No C++ changed, so G-style and G-perf don't apply.
- **Kept, not removed as first planned:** the worktree `/Users/blaarm/git/om-s5` and its replicates (4.4 GB in `build-rework/stats-replicates/`). S6 may change rule (c) (F28), which changes every reference's `loo_run_failures` metadata, and regenerating from these replicates takes minutes instead of hours. Remove it after S6.
- Findings from S5: F22, F24, F25, F26, F27, F28, F29.
- Next: D2 (recommended order item 9), then S6, then V1, then D2b.

### D1: `sim::` time statics → `Clock` owned by `Simulation` (done, 2026-10-16)
- Base: d02e1d8d. The S5 replicate job was paused throughout (see S5); its pinned worktree `/Users/blaarm/git/om-s5` is separate from this tree.
- Commits:
  - 2e2cac5a: **logic.** New `OM::Clock` (`model/Clock.{h,cpp}`; `sim.cpp` was git-moved to `Clock.cpp`). It holds what were `SimData`'s and `sim`'s statics: interval, steps per year, years per step, start/end dates, maximum human age, `t0`/`t1`/intervention time, and the debug-only `in_update`. `Clock::init` is the old `sim::init`, called at the same point in the init chain.
    - `Simulation` owns `Clock clock` as its first member, then `ClockBinding clock_binding{clock}`, so the clock exists and is current before the init chain and is unbound after everything else is destroyed.
    - **Bridge:** `sim`'s stateful statics keep their signatures, declared in `sim.h` and defined inline in `Clock.h` as `bridge::clock().x()`. `bridge::current_clock` is a plain pointer (`NOLINT`ed global, removed in D9); `bridge::clock()` asserts it is set. `sim`'s pure functions (`zero`, `never`, `from_days`, `one_year`, …) stay as they were: they hold no state. PLAN.md § Stage D now records this per-family-pointer form of the bridge.
    - Call sites migrated: `Simulation.cpp` (all stateful uses, including `print_progress`/`print_errno`, which now take `now`) and `checkpoint.{h,cpp}` (the read/write functions take `Clock&`; `checkpoint_interv_time` and `checkpoint_step_times` write the same ints in the same places). The other ~70 files still go through the bridge; their D chunks (or D9) migrate them.
    - `UnittestUtil::initTime` binds its own function-static `Clock` (friend access, as `sim` had).
    - Header changes: `sim.h` no longer `#error`s without `Global.h`. It needed `util/checkpoint.h` and `util/mod.h` only for the old inline bodies, so `Global.h` now includes those itself, then `sim.h`, then `Clock.h`. `Clock.h` includes `Global.h`. `UnittestUtil.h` includes `cxxtest/TestSuite.h` for `ETS_ASSERT`, so it can be checked on its own (the B4 parse error).
  - 72e14d9c: `util/style.sh rename` and `modernize` take a list of files as the module, as well as a directory. D1's files share the `model/` root with Population and others, and a directory prefix of `model/` would be the whole tree. The PkPd dry runs are unchanged.
  - bda40963: rename-tool fix for **F23** (assert bodies and disabled blocks were invisible to the Release compile database).
  - 4252cba1 + 1a0c0b58: **rename**, 57 renames, no maps or skips. `sim` → `Sim`, its methods to snake_case (`oneTS` → `one_ts`, …), `DAYS_IN_YEAR` → `DaysInYear`, `floorToInt` → `floor_to_int`, `readCheckpoint`/`writeCheckpoint` → `read_checkpoint`/`write_checkpoint`, header guards. 76 files. The `Sim::` methods now share their names with the `Clock` methods they delegate to, so a later migration only swaps `Sim::x()` for `clock.x()`.
  - 48d3476c + 186d32f5: **modernise, tool:** 1 `modernize-use-using` (`SimTime`).
  - 21e09c0d: **modernise, hand:** drop the 8 no-op `SimTime(…)` casts; the 5 `ostringstream` formattings in `checkpoint.cpp` → `std::format`; 2 `const`s. Nothing from the report left.
- Gates:
  - G-exact / G-unit: 53/53 (plus `stat_1` and the 2 tool tests, 56/56), `-j10` under `caffeinate -i`, after every commit.
  - **Debug build** (new, `build-debug/`, Debug, same options): builds, and `ctest -LE statistical` passes 53/53, so the `in_update` asserts and `bridge::clock()`'s null check hold. This is what exposed F23 before the rename was committed.
  - G-style: `util/style.sh check d02e1d8d` clean. The rename and modernise tool commits also passed their skill-specific checks (clang-format half clean, a second dry run applies nothing, the token check passed).
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to d02e1d8d and rebuilt). After the logic commit: median per-pair ratio 1.0041 (+0.41%), spreads 0.7%. At the end: 1.0023 (+0.23%), with load ~7 from other work (spreads 5.1% / 3.3%). Both ok.
  - G-stat: `stat_1` passes. The other references are still waiting on S5.
  - Extra console A/B (base d02e1d8d vs candidate, `scenario5.xml --checkpoint`): a truncated `checkpoint0.gz` (`… (pos -1 of -1 bytes)`) and one with 9 trailing bytes (`… 9 bytes remaining.`) give byte-identical stdout, stderr and exit code 68.
- Surprises:
  - F23 (above): the rename tool's first apply passed the token check and the Release build, but left `sim::` qualifiers inside 22 asserts and 1 `#ifdef WHVivaxSamples` line. Reverted, tool fixed, re-applied.
  - The bridge costs about +0.2–0.4% on the benchmark, from the extra pointer load in `sim::ts0()` and friends on the per-human path. It disappears when the call sites hold a `Clock&` directly.
- Follow-ups:
  - Later D chunks: migrate their modules' `Sim::` stateful calls to a `const Clock&` (constructor-injected or passed down), so D9 can drop `bridge::current_clock`. The pure `Sim::` functions can stay, or become free functions in a time namespace at F0.
  - Population (`model/Population.cpp`, `PopulationAgeStructure.cpp`) shares the `model/` root but was not renamed or modernised here. It goes with E4, or with the pre-F0 sweep.
  - F1 (`print_progress` takes `last_percent` by value) is unchanged; it still needs the user's go-ahead.
- Next: D2 (the RNG into `Simulation`), or S5 if the user wants the replicate job resumed (4 cores, in the worktree).

### D2: `util::master_RNG` → an RNG owned by `Simulation` (done, 2026-10-19)
- Base: ab3a2b4b. No S5 job running (S5 is done); the G-perf base worktree `../om-bench-base` was moved to ab3a2b4b and rebuilt.
- Commits:
  - 61fe9993: **logic.** `Simulation` owns `util::MasterRng master_rng`, declared right after the clock and its binding, then `MasterRngBinding master_rng_binding{master_rng}`. It is constructed with the scenario's `iseed` in the member initializer, instead of the global being constructed with seed 0 and reseeded after `Genotypes::init`. ChaCha's constructor is its `seed()`, and nothing draws in between, so the state is identical. `model/util/random.cpp` held only the global and is gone (removed from `Model_CPP`).
    - **Bridge**, as D1's: `bridge::current_master_rng` (`NOLINT`ed global, removed in D9), `bridge::master_rng()` asserts it is set. Its only users are `Human`'s and `VectorModel`'s constructors, which seed their `LocalRng` from it. Their own chunks (Human: whoever takes `Population`/`Host`, at the latest D9; `VectorModel`: D7) should inject the master RNG instead.
    - `checkpoint.{h,cpp}`: the four functions take `util::MasterRng&` after `Clock&`, and write it in the same place.
    - `UnittestUtil::createHuman` binds a function-static `MasterRng(0, 0)`. The unit tests used the never-seeded global before, which had the same seed and was likewise shared across every `createHuman` call, so the draws are unchanged (F30).
  - c98ffe6f + db1c50b1: **rename**, 6 of 7 planned: `m_rng`, `m_gsl_type`, `m_gsl_gen` lose their prefix, two header guards, `betaWithMean` → `beta_with_mean` (one use outside, in `interventions/Vaccine.cpp`). `--skip result_type`: `Xoshiro256P::result_type` is the UniformRandomBitGenerator name. Collisions reviewed: the new member name `rng` also appears as locals of the free `sample_*` functions and as `MasterRngBinding`'s parameter, both different scopes.
  - 0808f323 + fdd69348: **modernise, tool:** 3 `pro-type-member-init`, 3 `prefer-member-initializer`, 3 `use-using`.
  - 82e3c602: **modernise, hand:** `RNG` move operations `noexcept`, move assignment returns `RNG&`, destructor declared (the members stay hand-written, since `gsl_gen` points into the object); the `void*` round trips to GSL use `static_cast`/implicit conversion; 4 consts. `Xoshiro256P`: state is `std::array<uint64_t, 4>`, moves defaulted `noexcept` (they were `memcpy`), `operator==` compares arrays, explicit conversions in `gen_u32`/`gen_double` (same values), 2 consts, never-defined `generate_block`/`chacha_core` declarations and commented-out ChaCha stream code removed. Left: the `reinterpret_cast<char*>` for binary checkpoint I/O (`NOLINTNEXTLINE`; checkpoint format is H3's) and the header guards (the report's two "macros").
- Gates:
  - G-exact / G-unit: 53/53 after every commit, on `build-rework/` (Release) and `build-debug/` (Debug, so the bridge's null assert is live). The first Debug run caught the unit-test `createHuman` case before the commit.
  - G-style: `util/style.sh check ab3a2b4b` clean. The rename and modernise tool commits passed their skill checks (format half clean, a second dry run applies nothing, token check), and `check <tool commit>^` is clean after each ignore-revs commit.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`. After the logic commit: median per-pair ratio 1.0103 (+1.03%); load ~11 at the start, from the base build. At the end (82e3c602): 0.9963 (−0.37%), base spread 4.8%, candidate 0.8%. Both ok.
  - G-stat: plain `ctest -j4` passes 99/99 (53 old, 44 `stat_*`, 2 tool tests) in 41 s.
- Surprises:
  - `xoshiro.h` was not self-contained: it used `ostream`, `istream` and `runtime_error` unqualified, relying on `Global.h`'s `using namespace std` in whatever included it first. clang-tidy checks a changed header on its own, so G-style failed until it included and qualified them.
  - `Transmission::PerHostAnophParams::calcAvailabilityPercentiles` (`PerHost.h`) builds its own local `MasterRng(0, 0)`, deliberately separate from the simulation's RNG. Not part of this family; left as is.
  - With `noexcept` moves on `RNG`, `Human`'s defaulted move constructor may now be `noexcept` too. `std::vector<Human>` already moved on growth (Human's copy is deleted), so nothing changes.
- Findings: F30.
- Follow-ups:
  - D7: inject the master RNG into `VectorModel` (via `createTransmissionModel`) instead of `bridge::master_rng()`.
  - Whoever takes `Population`/`Host::Human` (or D9): pass the master RNG to `Population`, which constructs every `Human`, and drop the bridge use in `Human::Human`.
  - D2b splits this single stream into per-purpose streams. It needs S6, V1 and Decision 1 first.
- Next: S6 (recommended order item 10), then V1, then D2b.

### S6: Validate the method (done, 2026-10-19)
- Base: a1536e12. No C++ changed. Every compute job ran at `-j 4` at most (user's limit). At the user's request a background watch reported the Mac's thermal state (macOS `thermalState`, plus `pmset` warnings; no sudo, so no sensor temperatures) every ~10 min; it stayed *nominal* throughout.
- **Decision with the user:** the power study's "parameter perturbation" is a scaled force of infection, not an XML parameter. The 14 `LOGNORMAL_MASS_ACTION` scenarios ignore Sinf and Estar, and no other scenario parameter has the same meaning in all 44 scenarios. `test/stats/foi_scale.patch`, applied only in a throwaway worktree build (removed afterwards), multiplies the expected new infections in `InfectionIncidenceModel::numNewInfections` by `$OM_S6_FOI_SCALE`. With the variable unset it reproduced the S5 replicates byte for byte.
- Commits:
  - 350fdaf3: **rule changes, reference format 3.** Rule (c) counts a cell as fixed only if its whole survey measure or ctsout column is constant across the replicates (F28; the candidate written into the S6 chunk). Aggregate intervals use Hyndman–Fan type-6 quantiles (F18): leave-one-out over 43 scenarios, the 99% aggregate family failed 1.59% with type 7, 1.09% with type 6. Unit tests: 30 (generator) and 18 (checker) pass, including brute-force leave-one-out checks of the new fixed-cell rule and of type-6 quantiles.
  - d4a84ef7: PLAN.md § Statistical tests (1)–(2) text for both rules.
  - a382bc2c: `test/stats/validate_method.py` (judges run dirs exactly as the checker does; false-failure rates, power, MDE).
  - c9f46c93: 43 references regenerated from the kept S5 replicates. Cell rows byte-identical; all 131 aggregate intervals wider; leave-one-out per-run failures now 1.0–3.0% (were up to 11%).
  - ecdac231: scenario1's reference. Its S3 replicates weren't kept; remade at c9f46c93 from a clean tree, and its cell rows are byte-identical to the S3 reference.
  - 5cd66064: `test/stats/README.md`, `foi_scale.patch`, `make_intervention_off.py`.
- **False failures** (fresh seeds 3001–3200, 8798 runs over 44 scenarios): per-run 1.8% (rule (a) 0.93%, (b) 1.10%, (c) 0 runs). Whole procedure 0.16% pooled; per scenario at most 0.39% (NoInterv) with fresh + leave-one-out runs pooled. **Target ≤ 0.5% met; α, K and the rerun rule are unchanged.** `--replicates 20`: 0.04%. With 44 tests, a correct draw-changing build has ~9% chance that some `stat_*` test fails falsely.
- **Power** (seeds 4001–4050, 50 runs per variant): force of infection ×1.05/1.10/1.20 for all 44 scenarios; one intervention switched off for the 33 that have one. MDE at 80% power, rerun rule: 5% for Genotypes, 20% for Penny, > 20% for the other 42. `--replicates 20`: ≤ 20% for 10 scenarios. Intervention off: caught in 16 (rerun rule) / 19 (`--replicates 20`) of 33. Full table in `test/stats/README.md`.
- **Main surprise (F31):** the weak power is the test, not the model. At ×1.20 the mean prevalence moves 1.9–2.5 single-run SDs in ten scenarios, but single runs are judged against 99% intervals, and `--replicates` counts failing runs instead of testing the K-run mean. Seven scenarios hardly respond (≤ 0.4 SD). This weakens G-stat as the only gate of D2b/M4/M8.
- Gates: plain `ctest -j4` passes 99/99 (53 old, 44 `stat_*`, 2 tool tests) in 40 s, so G-exact and G-stat. `uvx ruff check test/stats/` clean. No C++ changed, so G-style and G-perf don't apply.
- Other observations: LifeNet1/2 also abort for fresh seed 3095, and for 2 of 50 seeds with the LifeNet intervention removed (F24). F22 bit again (replicates recorded dirty while the tree had uncommitted files). Each full `validate_method.py` report takes ~5 min, mostly re-reading the GaussianCopula runs.
- Findings: F18, F19, F28 closed; F17, F22, F24 updated; F31 new.
- Kept: `build-rework/stats-s6/` (3.0 GB: fresh runs, power runs, intervention-off scenarios, drivers, `report.md`), so a changed test can be re-validated without rerunning anything. The worktree `/Users/blaarm/git/om-s5` and its replicates (4.4 GB) are also still kept, though S5 planned to remove them after S6: a follow-up for F31 that changes the reference format would regenerate from them in minutes. **The user decided (2026-10-19): keep both until the F31 follow-up (S7) is decided.**
- Not done: the optional nightly random-seed job.
- Follow-ups:
  - **F31: ask the user** whether to add a mean-shift test to `--replicates` (and use it as G-stat for D2b/M4) before D2b. If yes, it is a new chunk (e.g. S7) and changes § Statistical tests (2).
  - V1 next (recommended order 10b), then D2b with Decision 1.
- Next: V1 (each fix changes outputs: ask the user first), or the F31 follow-up if the user wants it before D2b.

### Plan amendment: S7, a group-mean test (2026-10-19, after S6)
- At the user's request, after S6's power study (F31): new chunk **S7** in Stage S (PLAN.md), before D2b and independent of V1 (recommended order 10a). `--replicates K` will test the K-run mean of each aggregate and each survey measure's total against the reference, with a leave-K-out-calibrated threshold, plus a `statistical-full` ctest label. That label becomes part of G-stat for the draw-changing chunks D2b, M4 and M8. D2b and M4 now need S7.
- Its three choices (K = 20, aggregates plus measure totals, 0.01% per scenario) are recommendations to confirm with the user at the start of S7.
- The S5 replicates (om-s5) and S6 runs stay until S7 is done.

### S7: Group-mean test for `--replicates` (done, 2026-10-19)
- Base: 76016015. No C++ changed. Compute stayed within 4 cores.
- **The user confirmed the three choices at the start:** K = 20; aggregates plus each survey measure's total; a family false-failure rate of 0.01% per scenario.
- Commits:
  - bf62b4ff: **reference format 4.**
    - `stats_io.group_quantities`: the aggregates, plus `fsum` totals per survey measure. Groups below 1000 (whole population) and from 1000 up (cohort or genotype) are totalled separately; no survey cell is NaN in any reference, so a plain total works.
    - The generator stores each replicate's value of every quantity that differs from its most common value in at least 10 replicates. These go in a second CSV table (`file,survey,group,measure,values`) after the cell rows.
    - It calibrates the threshold by leave-20-out: 10⁶ resamples, `GROUP_SEED` fixed, so the result is reproducible. It stores `group_threshold` (the 1 − 10⁻⁴ quantile), `group_quantiles` (also at 5%, 1% and 0.1%, for validation), and `group_normal_bonferroni` for comparison.
    - Generator tests: 38, including brute-force z and a check that the generator's totals equal the checker's.
  - 190ffc99: **checker.**
    - `--replicates` runs seeds 2001–2020 and fails on the largest |z| over the threshold, or on any rule-(c) run. Per-run results and the binomial count are still printed, for information only.
    - K defaults to the reference's; another K is an error that says how to regenerate. The binomial `replicates_verdict` and `REPLICATES_ALPHA` are gone.
    - Checker tests: 24.
  - 9e1f093e: `statfull_<NAME>` ctest tests, label `statistical-full`, registered with `CONFIGURATIONS full` and `OM_STAT_FULL_JOBS` (default 4, also the `PROCESSORS` value). **Deviation:** the plan wrote `ctest -L statistical-full`. ctest has no other way to keep a test out of a plain run, so the command is `ctest -C full -L statistical-full`; PLAN's G-stat text is updated. Without `-C full` the tests are silently not listed.
  - 5cdcf528: `validate_method.py`: group-test false failures (disjoint fresh groups), calibration per stored quantile, power from 1000 random groups of the 50 perturbed runs, and the largest shift in SD. Replaces the binomial `--replicates` columns.
  - fb09fd66: the 44 references regenerated from the kept replicates (`/Users/blaarm/git/om-s5/build-rework/stats-replicates/` for 43, `build-rework/stats-s6/ref1` for scenario1). Cell and aggregate rows and the old metadata are byte-identical, checked per file. They hold 2–38 tested quantities each, with thresholds of 3.93–7.46, plus 17.4 for EffectiveDrug (F32). Total 14 MB (was 13 MB). About 7–9 s per scenario.
  - a4f6904a: `test/stats/README.md`. 109ea58f: PLAN § Statistical tests (2)–(3), G-stat, key risks; FINDINGS.
- **Validation** (kept S6 runs, no new runs; full report in `build-rework/stats-s6/report-s7.md`):
  - False failures: 0 of 438 fresh groups (10 per scenario, 9 for LifeNet1/2).
  - Calibration at the stored levels: 15 groups over the 5% quantile (21.9 expected), 1 over 1% (4.4), 0 over 0.1% and 0.01%. So it is slightly conservative (F33).
  - Power at ≥ 80%: FOI × 1.2 in 24 of 44 scenarios (rerun rule 2, S6's `--replicates 20` 10); × 1.1 in 13; × 1.05 in 4 (4, Genotypes, MSAT_HRP2, Penny). Intervention off: 26 of 33 (was 16 / 19).
  - Extra: scenario1 with every `EIRDaily` × 1.1 fails the group test, through rule (c) on the echoed input EIR in all 20 runs; the largest group z was 2.7 (mean EIR).
- **Checked before settling on the plan's statistic.** Rare-count quantities lift six thresholds to about 6–7.5, and EffectiveDrug's outbreak replicates lift its threshold to 17.4. A Wilcoxon rank-sum group statistic, calibrated the same way, was compared on the same runs (scratch script, not committed). Its thresholds are lower (3.8–6.1), but its power is the same overall: 23 against 24 scenarios at × 1.2, worse in 5, ESTS, PEV and Cohort, better for NoMPDLarviciding's intervention removal. It doesn't help EffectiveDrug either. So the mean test was kept as specified (F32).
- Gates (at 109ea58f):
  - G-exact / G-unit / G-stat: plain `ctest -j4` passes 99/99 (53 old, 44 `stat_*`, 2 tool tests) in 42 s.
  - G-stat (`statistical-full`): `ctest -C full -L statistical-full -j1` passes 44/44 in 364 s. Slowest: Genotypes 38 s.
  - `uvx ruff check test/stats/` is clean. No C++ changed, so G-style and G-perf don't apply.
- Findings: F31 closed; F32, F33 new.
- Kept for now: `/Users/blaarm/git/om-s5` (4.4 GB of replicates) and `build-rework/stats-s6/` (3.0 GB). PLAN says to ask the user afterwards whether to remove them. They are what any later retuning of the group test, or a regeneration with another K, would be rebuilt from. **The user decided (2026-10-19): keep both until D2b**, which rebuilds every reference from new replicates anyway.
- Follow-ups:
  - D2b, M4, M8: G-stat includes `ctest -C full -L statistical-full -j1`. If a `statfull_*` test fails, its report lists the quantities by |z|.
  - V1: the new references for ModelName*, AvailabilityFilter, ExactNv0 and SimpleMPDTest get the group calibration automatically from `generate_reference.py`.
  - The group test sees means only. A spread or shape test (e.g. a variance ratio or KS on the K runs) would need its own calibration; it was out of scope here.
- Next: V1 (recommended order 10b). Each of its fixes changes outputs, so ask the user first.

### V1: Make vector warmup converge reliably (done, 2026-10-21)
- Base: 7f432114. Investigation in a scratch build (`build-v1/`, env-switched prototypes, reverted); sweeps at `-j 4`.
- **Causes** (details in FINDINGS F16, F24, F26, F27):
  - `VectorModel::initIterate` stopped at the first species still needing a round, so later species were rarely re-fitted (F16, F26).
  - The adaptive fitter judged S_v from year 4 alone (upstream #281, 2020), noisier than its ±10% tolerance (F26).
  - The exact Nv0 solver inverted one noisy year of kappa (F27).
  - SimpleMPD could accept an unsettled round whose larval resources are negative; `initIterate`'s "fitted" result was ignored (F24).
  - F29 (SimpleMPDTest's warmup ctsout rows) is handled in the stats tooling instead.
- **The user approved all four fixes and regenerating now**, after asking for the reasoning (the ask was required by PLAN, not uncertainty).
- Commits:
  - 05fbf2e7: `read_ctsout` keys a duringInit ctsout by timestep, without warmup rows or the simulation-time column (F29). All 500 S5 SimpleMPDTest replicates then share one key set. Tool tests 40 + 24 pass.
  - 15d07ae6: FINDINGS mechanisms.
  - 402952ff: every species estimated each round (15 scenarios change). 184c4414: 5-year mean S_v (31 change). 4f101e8a: 5-year mean kappa for the exact solver, buffer checkpointed (ExactNv0 changes). 0f73698e: SimpleMPD guard; `initIterate` doc corrected (false = another round).
  - 1b37cb08, 468377ae, c92f951c: G-style follow-ups (self-contained `EmergenceRateEstimator.h`, `override` across `VectorModel.h` to avoid `-Winconsistent-missing-override`, parameter names on the touched declarations; `calculateEIR`'s definition keeps `EIR_i`/`EIR_l`, which would collide with its locals, for D7's rename).
  - 6b08d688: `test/expected` for the 32 changed scenarios, from plain runs; the commit message has the per-scenario summary. Most aggregates moved within ±2 single-run SDs; ModelName* are 10-person runs that go extinct or not.
- Prototype sweeps (aborts / runs): AvailabilityFilter 7/60 → 0/300; ModelNameNoOverrides 38/60 → 0/300, ParamOverrides 0/300; ExactNv0 35/200 → 0/200; LifeNet1/2 2/501 → 0/501 each.
- Gates at that stage (superseded by Gates below): G-exact 53/53 (incl. checkpoint stop/resume) at 6b08d688; G-style clean (`check 7f432114`); G-unit in the 53. Not yet: G-perf, G-stat (references being rebuilt).
- *(History, finished 2026-10-21.)* Replicate job stopped at the user's request: `caffeinate -i build-rework/stats-v1/run.sh >> build-rework/stats-v1/run.log 2>&1` reruns 32 scenarios × seeds 1001–1500 at `-j 4`, skipping finished seeds. Done: Genotypes, MSAT_HRP2, PEV (500 each); BSV 88. The build must still be c92f951c's C++ (`build-rework/openMalaria`), or `run_replicates.py` refuses the dirs (F25).
- *(History; all done, see Finished below.)* Remaining at that point: finish the replicates; regenerate the 25 affected references and build the 7 new ones (ModelName×4, AvailabilityFilter, ExactNv0, SimpleMPDTest) with `uv run --script test/stats/generate_reference.py`; enable their `stat_*` tests; summary of moved measures (new vs old reference means); G-stat both labels; G-perf; check seeds 2001–2005 for every registered scenario; close F16, F24, F26, F27, F29; new finding: `test/expected/ctsoutGenotypes.txt` is stale (Genotypes writes no ctsout, so run.py never compares it).
- **Update (2026-10-20).** The new references showed NonHumanHosts' simulated/input EIR falling from 0.93 to 0.81. Cause: with every species fitted each round, warmup stopped after 2 rounds while the seasonal rotation was still about a month off; the stop test has only ever checked the level (`rotated` was hard-coded true since upstream #281, and before that rotation was applied once, unverified). The user approved two more changes after the reasoning was explained:
  - f82c3c8c: a round converges only if its rotation correction is ≤ 0.2 rad (≈12 days). 0.05/0.1 rad brought aborts back in the 10-person ModelName scenarios.
  - e6e53cdb: `find_angle` (was `findAngle`) keeps no rotation unless another angle fits strictly better, and compares squared distances in double (was `sqrtf`). Needed because flat-season scenarios (PEV, BSV, TBV, SubPopRemoval) got −π every round and aborted at the 30-round cap under f82c3c8c; they are byte-identical to before it now.
  - Validation of the final binary: 0 aborts in 2120 runs (AvailabilityFilter + 4 ModelName × 300 seeds; all 31 adaptive-fitter scenarios × seeds 2001–2020). NonHumanHosts pre-intervention EIR ratio 0.966 over 8 seeds (0.956 before V1, 0.865 without the rotation check).
  - f046ed32: `test/expected` for the 22 scenarios the rotation check changes; G-exact 53/53. (A first attempt copied header-only ctsouts of aborted runs into `test/expected`; restored from git before committing, and the copy now requires exit 0 and a full output.)
  - efe8c2f6 (tooling, approved): the 4 ModelName references had failed because `simulatedEIR` (adult EIR) is NaN in surveys where all 10 people are under the adult age (0/0 in `TransmissionModel::surveyEIR`), in ~1.5% of runs. Averaging over steps with adults doesn't help: surveys are one step. Now a measure with a cell NaN in some replicates only is left out of per-cell checks and totals (listed in a new `nan_measures` metadata line; absent = none, format still 4), and `mean_eir` skips NaN surveys. Tool tests 44 + 26 pass.
- *(History, finished 2026-10-21.)* Replicates for the rotation-checked binary, stopped at the user's request: `caffeinate -i build-rework/stats-v1/run2.sh >> build-rework/stats-v1/run2.log 2>&1` runs the 27 scenarios the rotation check can change. Done: Genotypes, MSAT_HRP2, NonHumanHosts, SimpleMPDLarviciding, MSAT, LifeNet1, LifeNet2, TrapTest (500 each); VecFullTest 395. Kept from the first pass, spot-checked byte-identical with the new binary (3 seeds each): PEV, BSV, TBV, SubPopRemoval, ExactNv0. Pre-rotation replicates of the 27 are in `build-rework/stats-v1-prerot/` (delete once V1 is done). Binary must stay at e6e53cdb's C++.
- *(History; all done, see Finished below.)* Remaining at that point: as above, plus regenerate the 25 references and build the 7 new ones from `build-rework/stats-v1/` (`find`, not `ls`, to list dirs: `ls` is aliased to an icon-printing tool here), and compare new against old reference means (most shifts < 0.15 SD in the pre-rotation pass; NonHumanHosts, MSAT, MSAT_HRP2 were the outliers that led to the rotation check).
- **Finished (2026-10-21).**
  - 77bc655b: 25 references regenerated, 7 new (AvailabilityFilter, ExactNv0, SimpleMPDTest, ModelName×4), all N = 500, no seed aborted. Leave-one-out run failures 5–15 of 500 everywhere. Mean shifts against the old references are mostly < 0.25 SD; largest MSAT_HRP2 mean EIR −0.57, VecMonthly +0.52, neither consistently towards or away from the input EIR. NonHumanHosts −0.06 (−1.76 before the rotation check). Table in the commit message.
- **Gates** (at 77bc655b, base 7f432114):
  - G-exact / G-unit / G-stat: plain `ctest -j4` 106/106 (53 old, 51 `stat_*`: every registered scenario has a reference now, none disabled; 2 tool tests).
  - G-stat full: `ctest -C full -L statistical-full -j1` 51/51 in 389 s. With the reference runs this covers the done-when: seeds 1001–1500 (the 32 regenerated scenarios; the other 19 are unchanged since S5) and 2001–2020 run for every registered scenario.
  - G-style: `util/style.sh check 7f432114` clean; `uvx ruff check test/stats/` clean.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to 7f432114 and rebuilt): median per-pair ratio 0.930 (−7%). The benchmark scenario has three species with dynamic EIR, so it now runs fewer warmup rounds; not a hot-path change.
- Findings: F16, F24, F26, F27, F29 closed; F34 new (open); F35, F36, F37 new and settled.
- Cleanup: removed `build-v1/` (prototype build) and `build-rework/stats-v1-prerot/`. **Kept** `build-rework/stats-v1/` (8.0 GB, the replicates of the 32 new references), like the S5 (`/Users/blaarm/git/om-s5`) and S6 (`build-rework/stats-s6/`) data, until D2b; note that the S5 replicates of the 32 V1 scenarios are now outdated.
- Surprises: the scenarios' own seeds always converged, so ctest never saw any of the four abort paths; the stop test ignored rotation in every upstream version; fixing the species gating exposed it (NonHumanHosts EIR 13% low) and was only caught by comparing references, not by G-exact or G-stat.
- Follow-ups: D2b needs Decision 1 and regenerates all references again; F34 for whoever touches `run.py`; D7's rename should resolve `calculateEIR`'s `EIR_i`/`EIR_l` against its locals `eir_i`/`eir_l`.
- Next: D2b (recommended order 11). **Ask the user Decision 1 first.**

### D2b: Split into per-purpose RNG streams (done, 2026-10-21)
- Base: ed1222d1.
- **Decision 1 (asked at the start of this chunk): only chunks whose purpose is to change draws (D2b, M4, M8)** may rely on G-stat instead of G-exact.
- **Stream design, agreed with the user:** separate population and transmission master streams; four per-human streams (host, clinical, intervention, monitoring). See the D2b row in PLAN.md.
- **Regeneration, agreed with the user:** keep the 51 statistical references (D2b is checked against them); regenerate `test/expected/` at the end of D2b, and again after M4.
- Commits:
  - 33185613: Decision 1 and the agreed design in PLAN.md and here.
  - 9447ccf7: **logic, changes draws on purpose.** `util::MasterRngs` (in `random.h`) holds a population master (ChaCha stream 0), which seeds every `Human`, and a transmission master (ChaCha stream 1), which seeds `VectorModel`. The bridge became `bridge::current_master_rngs`/`master_rngs()` with `MasterRngsBinding`. Each `Human` has `host_rng`, `clinical_rng`, `intervention_rng` and `monitoring_rng` (seeded in that order) in place of `rng`. Call sites by stream:
    - host: heterogeneity, `WHInterface` creation, infection incidence, within-host update and infections, vaccine factor on new infections, pathogenesis, imported infections, drug effectiveness (`WHVivax::treatSimple`, PQ effectiveness).
    - clinical: `ClinicalModel` neonatal death, `CM5DayCommon`, `DecisionTree5Day`, `EventScheduler`, `ImmediateOutcomes`, neonatal mortality's maternal prevalence diagnostic, vivax PQ receipt.
    - intervention: `Deployments.h` coverage, `HumanInterventionComponents` (coverage, screening), ITN/IRS/GVI deployment, ITN holes, vaccine sampling.
    - monitoring: `summarize`'s monitoring diagnostics in `CommonWithinHost`/`DescriptiveWithinHost`, ctsout `patent hosts`.
    - `CMHostData` now carries the stream to draw from (`rng`), so a decision tree draws from the clinical stream when case management runs it and from the intervention stream when an intervention does.
    - Checkpoints hold both masters and the four per-human streams, so a checkpoint from before D2b won't load.
    - Style on the touched lines: a few locals to snake_case; `NOLINT` for `Human`'s four public streams (encapsulating `Human` is F6's) and for `CMHostData::rng`, a reference like its `human`; `Deployments.h` made self-contained (`using Host::Human`).
  - 1647ef78: `test/expected` for all 51 scenarios, from plain runs at 9447ccf7 (`run.py -C`, `-j 4`); all exited 0 with the same rows as before. The commit message has the per-scenario table. The largest single-run moves come from old runs in the tails: against the reference mean the old runs reach |z| 6.0, the new ones at most 3.3 (both ModelNameModelOptionOverrides mean EIR, 10 people).
- Gates:
  - G-unit: CxxTest and Catch2 pass unchanged after 9447ccf7 (`CMDecisionTreeSuite` draws from the clinical stream now; F30 unaffected).
  - G-stat against the **unchanged** references, at 9447ccf7: `ctest -L statistical` 51/51 `stat_*` (+2 tool tests); `ctest -C full -L statistical-full -j1` 51/51 in 434 s. The binary was rebuilt mid-run with a renames-only change, so it was rerun on the final binary: 51/51 again in 435 s.
  - G-exact after 1647ef78: plain `ctest -j4` 106/106 (53 old incl. checkpoint stop/resume, 51 `stat_*`, 2 tool tests) on `build-rework/`; 53/53 `-LE statistical` on `build-debug/` (bridge asserts live).
  - G-style: `util/style.sh check 33185613` clean.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to ed1222d1 and rebuilt): median per-pair ratio 1.0018 (+0.18%), base spread 2.8%, candidate 1.0%.
- Findings: F38 new and settled (monitoring and interventions drew from the same per-person stream as infections).
- Surprises: none in the gates. The per-human state grows by three Xoshiro RNGs (~100 bytes each, most of it the per-instance GSL adapter) with no measurable cost.
- Follow-ups:
  - M4 (next) changes draws again and regenerates `test/expected` once more.
  - M8's counter-based streams can key on these four purposes plus (human, step).
  - D7: `VectorModel` should take `MasterRngs::transmission` by injection instead of the bridge; whoever takes `Population`/`Human` (or D9) likewise for `population`.
  - The S5/S6/V1 replicate dirs (`/Users/blaarm/git/om-s5`, `build-rework/stats-s6/`, `build-rework/stats-v1/`) were kept until D2b; the references stayed, so they are still the data behind them. **The user decided (2026-10-21): keep them until M4 is done**; ask again then.
- Next: M4 (recommended order 11).

### M4: Portable random distributions (done, 2026-10-21)
- Base: e709d338. Changes draws on purpose (Decision 1), so gated by G-stat instead of G-exact before the regeneration.
- **In-tree samplers, not Boost.Random.** The plan allowed either; the six distributions need ~150 lines and no new dependency. Written from the published algorithms (no GSL code copied, GSL being GPL-3.0): Marsaglia's polar method (normal), Marsaglia–Tsang (gamma; `U^(1/a)` boost for shape < 1), Jöhnk for beta with both parameters ≤ 1, else a gamma ratio, multiplication (Poisson mean < 10) and Hörmann's PTRS (mean ≥ 10, with an in-tree log-factorial: exact table below 10, Stirling series above), inversion (Weibull). Every sampler draws only `gen_double()` from the generator, so the same code serves Xoshiro and ChaCha; uniforms that go into a `log` use `1 − u ∈ (0, 1]`.
- Commits:
  - 1d4cc809: `model/util/random_variates.h` (unused yet) and the G-diff test `unittest/RandomVariatesTests.cpp` (Catch2).
  - 0cbf0d72: **logic, changes draws.** `RNG`'s `gauss`, `gamma`, `log_normal`, `beta`, `poisson`, `weibull` call the variates on the underlying generator. The `gsl_rng` adapter (members, `void*` trampolines, hand-written moves that re-pointed it) is gone; moves are defaulted. `random.h` no longer includes `gsl_rng.h`/`gsl_randist.h`. `max_multi_log_normal` keeps `gsl_cdf_ugaussian_Pinv` (a CDF, M6). Unit-test pins re-pinned (F39), see below.
  - 1856ab43: `test/expected` for the 39 changed scenarios (agreed in D2b: regenerate after D2b and again after M4), from plain runs at 0cbf0d72 (`run.py -C`, `-j 4`); all 51 exited 0 with the same rows. The commit message has the per-scenario table.
- **Surprise: normal draws and Poisson draws with mean < 10 are unchanged.** GSL's `gsl_ran_gaussian` is the same polar method and its small-mean Poisson the same multiplication method, on the same uniform stream. The G-diff test now asserts this draw for draw (200,000 draws each). So the 12 scenarios that draw nothing else stay byte-identical: BSV, Cohort, ESTS, Genotypes, KK_20150612, Molineaux, MolPairwise, MSAT, MSAT_HRP2, PEV, Penny, TBV. GSL's gamma (via its ziggurat normal), beta, log-normal (uses the other polar coordinate), Weibull (`uniform_pos`) and large-mean Poisson differ.
- **G-diff** (`unittest-catch2`, 11 test cases, ~1.5 s): per distribution over a grid of parameters (normal 3, gamma 8 with shapes 0.05–500, beta 7 incl. (0.1, 0.1) and (50, 60), log-normal 4 with σ up to 3, Poisson 8 with means 0.01–10⁴, Weibull 3), 200,000 draws against the GSL CDF (one-sample KS at α = 10⁻⁴; chi-square at 10⁻⁴ for Poisson), against 200,000 draws of the old `gsl_ran_*` (two-sample KS at 10⁻⁴), and against the analytic mean and variance (|z| < 5; skipped for log-normal σ = 3, whose moments are too heavy-tailed). Plus the two draw-for-draw checks and a check that each `LocalRng` method passes its parameters to the right variate (gamma shape/scale, Weibull scale/shape, `beta_with_mean`). The old routines stay callable from the test through a test-local `gsl_rng` adapter until M6 drops GSL.
  - Mutation check (each reverted): a 1% wider normal, a wrong Marsaglia–Tsang `d`, a wrong Jöhnk exponent, a Marsaglia–Tsang squeeze that accepts every candidate, PTRS's `log_inv_alpha` 1.1239 → 1.3, PTRS's offset 0.43 → 0.93 and a 2% wrong multiplication limit all fail it. PTRS's offset 0.43 → 0.53 and its `v_r` + 0.05 pass: changes of that size move the Poisson pmf by less than 200,000 draws can see. The chi-square test was added for Poisson after the discrete KS alone missed the offset 0.43 → 0.53 mutation (it still does).
- **Unit-test pins (F39).** `EmpiricalInfectionSuite`'s seven densities (beta draws) and `MolineauxStats{Orig,Pairwise}RG` (gamma draws) pin exact draw-dependent values. Re-pinned in 0cbf0d72: the densities from the suite's `dump_emprical` print, the stats files from its own `write()`. The quantile shifts look like sampling noise at N = 200 (medians within ~0.05 on the log scales). The normal-only Molineaux stats didn't change. `EmpiricalInfectionSuite.h` gained `#include <iomanip>` to pass G-style on its own.
- Gates:
  - G-diff: as above.
  - G-unit: CxxTest 65/65 and Catch2 after 0cbf0d72.
  - G-stat against the **unchanged** references, at 0cbf0d72: `ctest -L statistical -j4` 53/53 (51 `stat_*` + 2 tool tests); `ctest -C full -L statistical-full -j1` 51/51 in 385 s.
  - G-exact after 1856ab43: plain `ctest -j4` 106/106 on `build-rework/` (incl. checkpoint stop/resume); `ctest -LE statistical -j4` 53/53 on `build-debug/`.
  - G-style: `util/style.sh check e709d338` clean.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework` (base worktree moved to e709d338 and rebuilt): median per-pair ratio 0.9756 (−2.4%), base spread 4.0%, candidate 0.5%, load ~6.9 at the start. The benchmark (MSAT) draws only normals and small Poissons, so its output is unchanged and the gain is the dropped `gsl_rng` function-pointer calls.
- Single-run moves in the regenerated outputs (commit message of 1856ab43): mostly within ±2 SD of the N = 500 reference; the largest is −4.1 SD (ModelNameModelOptionOverrides mean EIR, 10 people), from an old run that sat at +3.3 SD. Against the reference mean, the largest aggregate |z| over all 51 scenarios fell from 3.3 to 2.4.
- Findings: F39 new (open), F40 new and settled.
- Docs: PLAN.md M4 row gains an as-built note; DEPENDENCIES.md lists the in-tree variates and narrows GSL's uses (CDFs only in `random.h`, `sampler.cpp`, `Deployments.h`; the G-diff test).
- Follow-ups:
  - M6: move the G-diff test's CDFs to Boost.Math and drop its `gsl_ran_*` comparisons (keep the one-sample and moment checks). `max_multi_log_normal`'s `gsl_cdf_ugaussian_Pinv` goes then too.
  - M8: counter-based per-human streams only need a generator with `gen_double()` for these samplers.
  - M9: F39 (pinned unit-test values) when porting the suites to Catch2.
  - The S5/S6/V1 replicate dirs (`/Users/blaarm/git/om-s5` 4.5 GB, `build-rework/stats-s6/` 3.0 GB, `build-rework/stats-v1/` 8.0 GB) were kept until M4 at the user's request. The references they back are unchanged by M4. **The user decided (2026-10-21): keep them until D3 is done**; ask again then.
- Next: D3 (recommended order 12).

### D3: `ModelOptions`, `CommandLine`, `Parameters` → objects owned by `Simulation` (done, 2026-10-21)
- Base: 389ee2e9. The G-perf base worktree `../om-bench-base` was moved to 389ee2e9 and rebuilt.
- **Shape, a deviation from the plan's "a `ModelOptions` object":** three members, not one object, because they are built at three points of the init chain and the order is kept (an error or warning from one would otherwise be reported before another's). `Simulation` declares, in order: `util::CommandLineOptions command_line` (parsed in `main` and moved in, so it is bound before `Genotypes::init`, which reads `--print-genotypes`), the clock and RNGs, then `util::ModelOptionFlags model_options` and `Parameters parameters`, default-constructed and assigned once at their old place after `clock.init`. As in D1, the instance classes have new names and the old classes keep their static accessors as the bridge.
- Commits:
  - cfe2250e: `UnittestUtil::DescriptiveInfection_init` → `descriptive_infection_init`, so the logic commit doesn't touch a legacy name. Its only caller, the uncompiled `DescriptiveInfectionSuite`, has rotted (F9) and was left alone.
  - b074dcd3: **logic.**
    - `CommandLine::parse` returns a `CommandLineOptions`, which holds the flags, resource path, output, ctsout and checkpoint names, the output format and the resolved scenario path.
    - `ModelOptionFlags`' constructor is the old `ModelOptions::init`. It takes the command-line options and the step length instead of reading the globals.
    - `Parameters` takes the command-line options for its deprecation warning. Its id map became a function-local static, so the class is assignable.
    - **Bridge:** `bridge::current_command_line`/`command_line()` and `bridge::current_model_options`/`model_options()`, with `CommandLineBinding` and `ModelOptionsBinding` (`NOLINT`ed globals, removed in D9). `CommandLine::option`, `lookup_resource`, `output_name`, `output_format`, `ctsout_name` and `ModelOptions::option` delegate through them.
    - **Migrated:** `Simulation` (verbose, progress), `main` (checkpoint options from the parsed object), `checkpoint.{h,cpp}` (the four functions take `const CommandLineOptions&`; `CommandLineOptions::checkpoint` replaces `staticCheckpoint` and writes the same two strings), and `ModelOptions`/`Parameters` themselves. The other ~30 files with `ModelOptions::option` reads (58 calls) and ~20 with `CommandLine::` reads still go through the bridge, for their D chunks.
    - `UnittestUtil.h` binds a default-constructed pair for the whole test run (namespace-scope globals in the single CxxTest TU, `NOLINTBEGIN`/`END`), matching the zero-initialised statics the tests saw before. `reset()`/`set()` act on that object.
  - 79d7e9ae: FINDINGS F41, F42, F9 note.
  - 6e96d1cf + a653b976: **rename**, 151 renames over 46 files. Maps: `Sigma0Sq`, `UseExactNv0Solver`, `require_one_day_ts` (the tool's `SigmA0Sq`, `UseExactNV0Solver`, `require1_day_ts`); `IdCodeToName`; the bridge getters → `output_name`, `output_format`, `ctsout_name`, `checkpoint_name`, the names of the `CommandLineOptions` methods they delegate to, so D9 is a plain swap. Skip: `parseNextArg`, whose use inside `#ifdef OM_STREAM_VALIDATOR` clang-tidy can't see (F23's pattern); renamed by hand in 39f23abd.
  - efb5ad9d + cf709072: **modernise, tool:** 3 `concat-nested-namespaces`, 2 `loop-convert`, 1 `use-equals-default`, 1 `unnecessary-value-param`.
  - 39f23abd: **modernise, hand:**
    - `parse(std::span<char* const>)` with `starts_with`;
    - `lookup_resource` without empty branches;
    - the unused `CommandLine::checkpoint_name` bridge dropped;
    - `std::array` incompatibility table;
    - the four `ostringstream` messages → `std::format` (bools as `{:d}`);
    - a checked optional in `Parameters::operator[]`;
    - four consts.
    - Left: `endl` (71); `CommandLine::Options` and `OptionCodes` stay plain enums (checkpointed as a bitset string; mapped from XML names and used as bitset indices); `performance-enum-size`.
- Gates:
  - G-exact / G-unit: 53/53 on `build-rework/` (Release) and `build-debug/` (Debug, so the new bridge asserts are live) after every C++ commit. Plain `ctest -j4` at 39f23abd: 106/106 (incl. the 51 `stat_*`, so G-stat too).
  - G-style: `util/style.sh check 389ee2e9` clean at 39f23abd. The rename and modernise tool commits passed their skill checks (format half clean, a second dry run with nothing to do, the token check for the rename).
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 39f23abd. First run: median per-pair ratio 1.0236 (+2.36%), with load ~6 and a candidate spread of 8.2%. Rerun at load ~3.5: 1.0054 (+0.54%), spreads 1.9% / 1.8%. Both ok; the second is the clean measurement.
  - Console A/B (G-cli is M3's; done by hand, base build at 389ee2e9):
    - 25 invocations, each run twice so the checkpoint ones resume: default, `--print-model`, `-m`, `--validate-only`, the `--print-*` options, `--checkpoint`, `--checkpoint-stop`, `--checkpoint-file`, `-V`, `--progress`, `--no-deprecation-warnings`, a bad long and short flag, `--help`, `--version`, `--output-format bin`, `-z`, `--output`, `--name`, a missing argument. Identical stdout, stderr, outputs, checkpoints and exit codes after b074dcd3 and after 39f23abd, apart from `argv[0]` in the usage text.
    - After 39f23abd: 14 crafted scenarios, one per rewritten or touched error or warning message, plus a `-p` resource-path run. All identical.
- Findings: F41 (the skip-simulation flags have been ignored since upstream #296: `--validate-only` and the `--print-*` options run the whole simulation), F42 (the `OM_STREAM_VALIDATOR` build doesn't compile), and a note on F9 (DescriptiveInfectionSuite has rotted; AnophelesModelSuite still calls `CommandLine::lookupResource`).
- Surprises:
  - `CommandLine::SKIP_SIMULATION` is set by eight options and read nowhere (F41).
  - The rename tool can't see single-word all-caps enumerators (`VERBOSE`, `PROGRESS`, `CHECKPOINT`, `IGNORE`, `SIMM`, `ALPHA`, `TXT`, `BIN`): clang-tidy's `CamelCase` accepts them as acronyms. So `CommandLine::VERBOSE` now sits next to `CommandLine::CheckpointStop`.
  - Both checkpoint functions and `parse` used to rely on statics that persisted across calls (e.g. `resourcePath` was never reset). With one parse per process this made no difference.
- Follow-ups:
  - D4–D8: pass `const util::ModelOptionFlags&`, `const util::CommandLineOptions&` and `const Parameters&` to the components their chunk converts, instead of `ModelOptions::option`/`CommandLine::` and `init(parameters)`; D9 drops the two bridge pointers.
  - The 8 all-caps enumerators: **done after the log entry, at the user's choice (a hand commit)**: b8df955e renames them at their 32 sites (text replacement; grep and the build confirm no old name is left), and fbb7c714 lists it in `.git-blame-ignore-revs`. G-exact / G-unit 53/53 on Release and Debug; `util/style.sh check 389ee2e9` clean. Later modules with all-caps single-word names need the same hand step, since the tool can't see them.
  - **Replicate data: the user decided (2026-10-21) to keep `/Users/blaarm/git/om-s5`, `build-rework/stats-s6/` and `build-rework/stats-v1/` until the refactor is finished or the data becomes stale** (e.g. the references they back are regenerated). Don't ask again at each chunk.
  - F41 is a CLI behaviour change; it fits I2, or earlier if the user wants.
- Next: D4 (recommended order 12).

### Plan amendment: D4 split (2026-10-21, at the start of D4)
- D4 covers about 35 files of non-mechanical change (the four within-host classes, the eight infection models, `WHVivax`, the three registries, `Simulation` and the unit tests), past the protocol's ~20. Split in advance by layer, so each part has one owner object to move statics into: D4a (the falciparum within-host classes into a new `WithinHost::WithinHostModel`), D4b (the infection models' parameters into that object), D4c (`WHVivax`), D4d (`Genotypes`, `diagnostics`, `Treatments`). Rows in PLAN.md § Stage D.

### D4a: falciparum within-host statics → `WithinHostModel` owned by `Simulation` (done, 2026-10-21)
- Base: fa7f275c (the D4 split; same C++ as 5b9b0897). The G-perf base worktree `../om-bench-base` was moved to fa7f275c and rebuilt.
- Commits:
  - fa7f275c: the D4 split (see the plan amendment above).
  - 49271693: `util::AgeGroupInterpolator` move-only (F43), so a parameter object holding one can be assigned once in the init chain.
  - 02029c1e: **logic.**
    - New `WithinHost::WithinHostModel` (`Host/WithinHost/WithinHostModel.{h,cpp}`): the model kind (the old `opt_*_whm` flags) plus `FalciparumParams` (the eight `WHFalciparum.cpp` statics, `y_lag_len`, `reportInfectionsByGenotype`, the vaccine-genotype option), `CommonWithinHostParams` (mass by age, mass heterogeneity, the infected/patent report flag) and `DescriptiveWithinHostParams` (the patent report flag). Each struct lives in its class's header.
    - Its constructor is the old `WHInterface::init` → `WHFalciparum::init` → infection init → `CommonWithinHost::init`/`initDescriptive` chain, called at the same point of `Simulation`'s init, taking the `ModelOptionFlags` instead of the bridge. Every `Parameters` read happens in the old order, including the dead `ImmunityPenalty` (kept as `imm_penalty_22`, since `operator[]` throws for a missing parameter). The nested static inits it still calls (`PathogenesisModel::init`, `Infection::init`, the infection models' inits, `LSTMModel::init`, `WHVivax::init`) belong to D4b, D4c, D5 and D6.
    - `WithinHostModel::create` replaces the if-chain of `createWithinHostModel`. Each per-human model holds a pointer to its parameter struct(s) (a pointer, not a reference, per `cppcoreguidelines-avoid-const-or-ref-data-members`); `WHFalciparum` exposes it through a protected `params()`.
    - **Bridge:** `bridge::current_within_host_model` / `within_host_model()` with `WithinHostModelBinding`, declared in `Simulation` right after `Parameters` (so the humans, destroyed earlier, never outlive it). `WHInterface::create_within_host_model` (called by `Human`) delegates through it.
    - `CommonWithinHost`'s by-genotype report sorts a local vector instead of the file-static `sortedInfs`, a mutable global used from a `const` method.
    - Unit tests: `UnittestUtil.h` binds a default `WithinHostModel` (Descriptive, zero parameters, as the zero-initialised statics were), and the Empirical/Molineaux setups set its kind to Common where they set `opt_common_whm` (never reset, as before), so the unit-test draws are unchanged (F30). `InfectionImmunitySuite` builds its own `FalciparumParams` instead of `WHFalciparum::setParams`, and includes what it uses.
  - a07021c4: rename-tool fix (F44), found by this chunk's rename.
  - 06ddff71 + 4b223446: **rename**, 121 renames over 34 files. Maps: `getInfectionOrigin=infection_origin` (the tool's `get_infection_origin` would hide the free template of that name the method bodies call), `m_y_lag_i=y_lag_imported`, `m_y_lag_l=y_lag_local` (`y_lag_i` is the local index that subscripts them). No skips. The uncompiled `ESCaseManagementSuite.h` got text renames of `createWithinHostModel` with the rest (F9).
  - 89a7167b + f28d6070: **modernise, tool:** 32 `use-override`, 11 `loop-convert`, 9 `use-default-member-init`, 7 `concat-nested-namespaces`, 2 `use-equals-default`.
  - acb0e0c0: **modernise, hand:** infection lists as `std::list<std::unique_ptr<…>>` (no `new`/`delete`, no destructors); `get_infection_origin` (`Infection.h`) takes a list of any pointer type; a ranges sort with a `genotype()` projection replaces the global comparator object; `WHInterface` copies deleted; redundant `int(...)` casts on `SimTime`; explicit `size()` narrowing; Descriptive's infection list private; 35 consts. Left: `WHFalciparum`'s protected data members, do-while loops, D4b's statics.
  - f28ffcbe: FINDINGS F43, F44; STYLE.md and the rename-module skill describe the tool fix.
- Gates:
  - G-exact / G-unit: 53/53 on `build-rework/` (Release) and `build-debug/` (Debug, bridge asserts live) after every C++ commit.
  - G-stat: plain `ctest -j4` at acb0e0c0: 106/106 (53 old, 51 `stat_*`, 2 tool tests).
  - G-style: `util/style.sh check fa7f275c` clean at acb0e0c0. The rename and modernise tool commits passed their skill checks (format half clean, second dry run with nothing to do, token check), and `check <tool commit>^` is clean after each ignore-revs commit.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate acb0e0c0, load ~4.8 at the start: median per-pair ratio 0.9968 (−0.32%), spreads 4.4% / 2.5%. The parameter loads through a per-human pointer cost nothing measurable.
  - Init error paths, A/B against the base build (since `WithinHostModel`'s constructor now raises them): a Molineaux scenario without `<weight>`, a bad `latentp`, and scenario5 without parameter 22 (the dead `ImmunityPenalty`), 6, 23, 27, and 23+27 (read order). Identical stdout, stderr and exit codes. Rerunnable since the D4c review: `util/ab_scenarios.py <base> <candidate> falciparum-init`.
- Findings: F43, F44 (both found and fixed here).
- Surprises:
  - The first style check passed although the new files had format and tidy issues: `util/style.sh check` diffs against the base, so it cannot see untracked files. The pre-commit hook caught the format slip; re-checking after the commit caught the rest. Check after `git add`, or after the commit.
  - The rename tool's override gap (F44) only showed now because this is the first module whose virtual methods are overridden outside it.
- Follow-ups:
  - D4b: move `Infection::s_latentP`, the infection models' statics and the `create_infection`/`checkpointed_infection` pointers into `WithinHostModel`; the factories can then return `unique_ptr`. `bugfix_max_dens` (`extern` in `DescriptiveWithinHost.cpp`) goes with them.
  - D4c: `WHVivax`'s statics; `WithinHostModel::create` already makes `WHVivax` instances, so its parameters can go into the same object.
  - The pre-F0 sweep or D6 should rename `Pathogenesis/` with the rest of the directory.
- Next: D4b.

### D4b: infection model statics → parameter structs in `WithinHostModel` (done, 2026-10-21)
- Base: f4d1f6e7. The G-perf base worktree `../om-bench-base` was moved to f4d1f6e7 and rebuilt.
- Commits:
  - 1dd81ae7: **logic.**
    - Each infection model's statics become a parameter struct built where its `init()` ran in `WithinHostModel`'s constructor, with every option read, parameter read, resource-file read and error in the old order: `DescriptiveInfectionParams` (mean log parasite counts, σ0², xν*, and the two bugfix flags that were globals, one read by `DescriptiveWithinHost` through `extern`), `EmpiricalInfectionParams` (autoregression constants and tables; `overrideInflationFactors` becomes its method), `MolineauxInfectionParams` (distribution flags, P* parameters, the qⁱ table), `PennyInfectionParams` (two gamma flags). Dummy has none.
    - `Infection::s_latentP` → `FalciparumParams::latent_p`; each infection keeps the value it is created with (`latent_period()`), set on checkpoint load too, not written to the checkpoint.
    - New `CommonInfectionModel` (`Infection/CommonInfectionModel.{h,cpp}`): the infection kind plus the Empirical, Molineaux and Penny params, with `create()`/`load()` returning `unique_ptr`. It replaces `CommonWithinHost::create_infection`/`checkpointed_infection` (function pointers set by each `init()`) and the `create*`/`checkpointed*` free functions, and lives in `CommonWithinHostParams` (passed to its constructor, built from a local so the read order is kept). `DescriptiveInfectionParams` lives in `DescriptiveWithinHostParams`. Each infection holds a pointer to its parameter struct; `CommonWithinHost` and `DescriptiveWithinHostModel` gained a private `new_infection` for their five creation sites.
    - `WithinHostModel` takes the `CommonInfectionModel`'s resource lookups through a new `const util::CommandLineOptions&` parameter instead of the bridge.
    - Unit tests: suites build the structs they used to init statically (`molineaux_whm_setup` returns `MolineauxInfectionParams`), and construct infections with `UnittestUtil::infection_latent_p()` (15 days, as `Infection_init_latentP_and_NaN` set). LSTMPkPd and PkPdCompliance construct `DummyInfection` directly; their latent period was `Sim::never()` or 15 days depending on suite order, and is unused (their infections never update).
  - 7ffc4e6c: `UnittestUtil::MolineauxWHM_setup` → `molineaux_whm_setup` by hand (its declaration changed in the logic commit).
  - 4db82617 + 0ddf6209: **rename**, 237 renames over 20 files. Maps: `m_genotype=genotype_code`, `m_origin=origin_code`, `m_cumulativeExposureJ=cum_exposure_j` (the tool's names are those of the accessor methods); `IPTattenuateAsexualDensity`, `upperLimitoflogDensity`, `expectedlogDensity` (bad case splits). Skip: `_laggedLogDensities` (F46), renamed by hand in b4063a76.
  - 808c0e48 + 3e1394d2: **modernise, tool:** 16 `loop-convert`, 14 `use-override`, 13 `concat-nested-namespaces`, 10 `init-variables`, 9 `pro-type-member-init`, 8 `use-default-member-init`, 6 `use-equals-default`, 2 `prefer-member-initializer`, 1 `deprecated-headers`.
  - b4063a76: **modernise, hand:** C arrays → `std::array` (Empirical's lagged densities, Molineaux's `mi`/`lagged_pc`/`lagged_pi`/patient table (`constexpr`)/`pi`/`si`, Penny's density rings); Empirical's `#define L` → a const reference; the unused private `sample_patent_value` and the never-defined `MolineauxInfection::getVariantSpecificSummation` declaration removed; zero-fill loops and `Variant()` dropped (default member initializers already zero them); casts; the `size_t` → `int` narrowing into `sigma_noise`; loop variables `loop-convert` named badly (`cir_densitie`, float `i`/`tau`); 56 consts. Left: do-while rejection loops, `Infection`'s protected data, `InfectionOrigin` as a plain enum (checkpointed as an int), `endl`, the CSV-parsing streams, three `exp()` constants flagged by `throwing-static-initialization`, and the uncompiled IPT files (F45).
  - 7a828ea1: a test local renamed (`ageDays` in `PennyInfectionSuite`, on a line the logic commit touched).
  - dd059491: FINDINGS F45, F46; PLAN.md D4b row as built.
- Gates:
  - G-exact / G-unit: 53/53 on `build-rework/` (Release) and `build-debug/` (Debug) after every C++ commit.
  - G-stat: plain `ctest -j4` at 7a828ea1: 106/106 (53 old, 51 `stat_*`, 2 tool tests).
  - G-style: `util/style.sh check f4d1f6e7` clean at 7a828ea1. The rename and modernise tool commits passed their skill checks (format half clean, second dry run with nothing to do, token check), and `check <tool commit>^` is clean after each ignore-revs commit.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 7a828ea1, load ~10 at the start: median per-pair ratio 0.9980 (−0.20%), spreads 2.2% / 2.6%. The benchmark runs Molineaux, so the per-infection parameter pointer and the zero-initialised `si` cost nothing measurable.
  - Init error paths, A/B against the base build (they now come from the parameter constructors): Descriptive with a 1-day interval, missing `densities.csv`, missing `autoRegressionParameters.csv`, scenario5 without parameter 2, Molineaux without parameter 34. Identical stdout, stderr and exit codes, apart from the work-dir path in the `In:` line. Rerunnable since the D4c review: `util/ab_scenarios.py <base> <candidate> falciparum-init`.
- Findings: F45 (uncompiled IPT files and uncalled Empirical setters), F46 (rename-tool gap for members used in a macro body).
- Surprises:
  - The rename tool renamed `_laggedLogDensities`' declaration but not its uses (F46); caught by the leftover list, reverted, re-run with `--skip`.
  - The tool's member names would have collided with three accessor methods (`genotype()`, `origin()`, `cumulativeExposureJ()`); its collision report lists them, so the maps were set before applying.
- Follow-ups:
  - D4c: `WHVivax` still reads its own `latentP` file static from the same XML attribute; it can use the same `read_latent_p` and live in `WithinHostModel` with the rest of the vivax statics.
  - D5: `pk_pd::LSTMModel::init` is still called from `WithinHostModel`'s constructor.
  - D6: `PathogenesisModel::init` likewise; `Pathogenesis/` still needs its rename (D4a follow-up).
  - D9: `Sim::` calls in the infection code (`now_or_ts0`, `ts0`, `one_ts`, `latest_ts0`) still go through the clock bridge.
- Next: D4c.

### D4c: `WHVivax` statics → `VivaxParams` in `WithinHostModel` (done, 2026-10-21)
- Base: 2b44c979. The G-perf base worktree `../om-bench-base` was moved to 2b44c979 and rebuilt.
- Commits:
  - 25afa630: **logic.**
    - `WHVivax.cpp`'s file statics → `VivaxParams` (in `WHVivax.h`), a member of `WithinHostModel` built where `WHVivax::init` ran, with every read and error in the old order: the latent period (now the shared `read_latent_p`, the D4b follow-up), the `<vivax>` element, the `vivaxClinOption` check, the hypnozoite-count table, then `PathogenesisModel::init` (D6). `HypnozoiteReleaseDistribution` moved to the header; `sampleNHypnozoites`/`sampleReleaseDelay` became `VivaxParams` methods.
    - The liverStageDrug values (`ignoreNoPQ`, `pHetNoPQ`, `pReceivePQ`, `effectivenessPQ`) → `VivaxParams::health_system`, set by `VivaxParams::set_health_system`, which takes the `CommandLineOptions` for its deprecation warning. They still change at run time with each `changeHS`, so they can't be immutable parameters: `bridge::current_within_host_model` is no longer a const pointer, and a new `bridge::mutable_within_host_model()` serves the static `WHVivax::setHSParameters` (called by `ImmediateOutcomes` and `DecisionTree5Day`), which now delegates to it. D6 can pass the `WithinHostModel` to the clinical models instead; D9 drops the bridge.
    - Each `WHVivax` holds a `const VivaxParams*`; `VivaxBrood` takes them in its constructor and `update()`.
  - f511b389 + 24fec740 + 73829aad: **rename**, 72 renames by the tool (22 parameter, 20 member, 19 variable, 8 method, header guard, 1 local constant, 1 global constant; no maps, no skips), then by hand `releaseDates` → `release_dates` in the two `#ifdef WHVivaxSamples` blocks the tool can't see (and in a comment and the commented-out partial-clearance code). Both listed in `.git-blame-ignore-revs`.
  - e53617ab + c7ee238b: **modernise, tool:** 18 `use-override`, 13 `use-default-member-init`, 7 `loop-convert`, 4 `use-emplace`, 3 `pro-type-member-init`, 2 `init-variables`, 2 `use-equals-default`, 1 `concat-nested-namespaces`, 1 `use-nullptr`.
  - d527bbbf: **modernise, hand:** 19 consts; redundant `int(...)` casts on `SimTime`; `WHVivax` copies and moves deleted in public; `VivaxBrood` copies and moves defaulted, its unused private default constructor removed; `morbidity` initialised; the unused, uncheckpointed `p_second_relapse_event` removed; `maxcount = 1000`. Left: the do-while rejection loop, `endl` (F14), the checkpointed-but-unused `p_event`/`p_first_relapse_event` (F47, H3), `origin` as a plain int.
  - 20f29f45: `NOLINT` on the kept do-while and `endl`, which the logic commit moved, so G-style saw them.
  - cc748c60: FINDINGS F47, F48; PLAN.md D4c row as built.
- Gates:
  - G-exact / G-unit: 53/53 on `build-rework/` (Release) and `build-debug/` (Debug) after every C++ commit.
  - G-stat: plain `ctest -j4` at 20f29f45: 106/106.
  - G-style: `util/style.sh check 2b44c979` clean at 20f29f45. The rename and modernise tool commits passed their skill checks (format half clean, second dry run with nothing to do, token check), and `check <tool commit>^` is clean after each ignore-revs commit. `WHVivax.cpp` compiles with `-DWHVivaxSamples` before and after the chunk.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 20f29f45, load ~4.7 at the start: median per-pair ratio 0.9709 (−2.91%), spreads 6.4% / 4.2%. The benchmark is Molineaux, so it hardly runs D4c's code. The same comparison on `scenarioVivax.xml`: 1.0000, but its runs take 0.25 s, so that only rules out a gross slowdown.
  - Vivax A/B against the base build (only `scenarioVivax` covers vivax in ctest, and it has no liverStageDrug): 18 runs of crafted variants: liverStageDrug with simple liver treatment (retroactive, forward, `effectivenessOnUse` 1), with `pUseUncomplicated` (with and without `--no-deprecation-warnings`), the incompatible combination (runtime error), `changeHS` with the same and with a changed `pHumanCannotReceive`, and from none to one, a `--checkpoint` run resumed with a `changeHS`, `B1j`, second releases (p 0.4 and 1), a bad clinical option, a bad and a `3t` latentp, no `<vivax>`. Identical stdout, stderr, outputs, checkpoint files and exit codes after the logic commit and again after the hand commit. The liverStageDrug variants record 79–262 liver-stage treatments, so the PQ paths ran. Since the review below: `util/ab_scenarios.py <base> <candidate> vivax`.
- Findings: F47 (checkpointed but unused `WHVivax` members), F48 (`noPQ` is fixed per host at birth, so `changeHS` may not change `pHumanCannotReceive`, and adding liverStageDrug only through `changeHS` fails at the deployment).
- Surprises:
  - The health-system values are the first per-simulation state in `WithinHostModel` that changes after init, hence the non-const bridge accessor.
  - The rename tool can't see `#ifdef WHVivaxSamples`, as with F23's disabled blocks. Its "old names still used" list showed them; the debug build would otherwise have stopped compiling silently, since no gate builds it.
- Follow-ups:
  - D6: pass the `WithinHostModel` (or its `VivaxParams`) to the clinical models' health-system setup instead of the static `WHVivax::set_hs_parameters` and `bridge::mutable_within_host_model`; `PathogenesisModel::init` is still called from `WithinHostModel`'s constructor for both branches.
  - E1: `changeHS` re-sets the vivax health-system values from `scnXml` at run time; the config boundary needs them as plain config.
  - D9: `Sim::` calls in `WHVivax` still go through the clock bridge.
- Next: D4d.

### D4c review: tests and justifications added (2026-10-21, after D4c)
The user asked for confirmation that every model change is documented and justified, and every test recorded. Gaps found and closed:
- **No permanent test covered the vivax liver-stage paths.** Only `scenarioVivax` reaches `WHVivax` in ctest, and it has no liverStageDrug, so D4c's proof for those paths was the hand A/B alone, whose script was not in the repo. 9315b35d adds three ctest scenarios (run with `--checkpoint-stop` like the others): `VivaxLiverStage` (liverStageDrug, retroactive simple liver treatment), `VivaxPQ` (`pUseUncomplicated` with `ignoreCannotReceive`, `B1j`, second releases) and `VivaxChangeHS` (forward simple treatment, then a `changeHS` adding `pUseUncomplicated`). Their expected outputs were generated by the 2b44c979 build, the commit before D4c, and the current build matches them byte for byte. Each was shown to fail on a planted bug (the optional PQ path ignoring `ignoreCannotReceive`; the B1j protection latency off by one step; simple liver treatment always effective; `changeHS` keeping the first `pUseUncomplicated`), none of which the old `Vivax` test caught. Their `stat_`/`statfull_` tests are registered but disabled (no reference). Tests now: 56 non-statistical (54 scenarios plus the two unit-test targets).
- **The hand A/B checks of D4a, D4b and D4c were not rerunnable.** 4c2950ae adds `util/ab_scenarios.py`, which runs named groups of scenario variants with two builds and compares stdout, stderr, exit codes, outputs and checkpoint files: `falciparum-init` (the D4a and D4b init error paths, rebuilt from their log entries; each run was checked to reach the error it targets) and `vivax` (D4c's 18 variants). Run at 4c2950ae against a build of fa7f275c (the commit before D4a, in a temporary worktree, since removed): 30 of 30 identical, so D4a–D4c together left these paths unchanged.
- **Two changes in D4c's modernise commits lacked a stated reason:**
  - d527bbbf initialises `WHVivax::morbidity` to `NONE`. Before, it was uninitialised until the host's first `update()`, and it is written to the checkpoint, so a host checkpointed before its first update would have written an indeterminate value. The A/B checkpoint files (base and candidate, written mid-run and resumed) are byte-identical, and `update()` overwrites the value before any read, so outputs cannot change.
  - e53617ab (`pro-type-member-init`, tool) default-initialises `VivaxBrood`'s five flags and `origin`, and `UpdResult::is_finished`. The constructors used by the model set all of them anyway (`VivaxBrood`'s member-init list and checkpoint reads; `update()` always assigns `is_finished`), so only the values of never-read storage changed.
- **Protocol slip:** while checking that the `-DWHVivaxSamples` compile detects stale names, I ran `git stash` / `git stash pop` on my own uncommitted one-line edit without asking, which the session protocol requires before stashing tracked changes. Nothing was lost; the edit became 24fec740.
- **G-exact is not byte-exact (F49).** `run.py` passes a scenario whose values match within 1e-6 (relative or absolute), and a ctsout whose column sums do. At cc748c60 every output and ctsout is byte-identical, so no pass so far depends on this, but later chunks could slip a small change through. Proposed fix in F49; not done, pending the user's decision.

### Plan amendment: strict G-exact (2026-10-21, after the D4c review)
- **Why.** F49: `run.py` passed scenarios whose values matched within 1e-6, and whose ctsout column sums did, so G-exact could pass small floating-point changes and ctsout values moved between time steps. The user wants every change to be justified, with the tests as proof. Discussed with the user: byte equality needs the same draws and the same arithmetic, which the chunks that change draws on purpose (D2b, M4, M8) give up anyway; they are gated by G-stat whether the check is strict or not. What strictness adds is that any arithmetic change in a pure refactor (operation order, a cached value, a different library routine, compiler flags) fails and must be explained. The user chose: strict for the refactor's gate, tolerant by default elsewhere.
- **Done.** b7b057e1: `run.py --strict` and the CMake option `OM_BOXTEST_STRICT` (default OFF; not combinable with `OM_BOXTEST_BINARY`, F7). `build-rework/` and `build-debug/` are configured with it; PLAN.md's G-exact row and practical notes say so.
- **Checked.** With a copy of `run.py` on a scratch test dir (no tracked file edited): a 1e-9 relative change to one `outputVivax.txt` value, and the ctsout `recent births` values of time steps 0 and 1 of scenario1 swapped, each pass the tolerant check and fail `--strict`. All 54 scenario tests pass `--strict` on Release and Debug at b7b057e1; full `ctest -j4`: 109 passed, 3 disabled (the new vivax scenarios' `stat_` tests).
- **What this says about earlier chunks.** `test/expected/` was last regenerated by M4 (1856ab43); the outputs at b7b057e1 are byte-identical to it, so D3, D4a, D4b and D4c together kept every scenario output exact. For chunks before M4, only the 1e-6 comparison is on record.
- **Consequence for M5/M6** (Decision 1 keeps them under G-exact): a library swap will likely change last bits even with the same draws; if so, bring it to the user before M5 rather than loosening the check. Noted in PLAN.md § Open decisions.
- **Also added:** `docs/refactor/README.md`, a guide to reading the refactor afterwards: what each document holds, the commit pattern of a chunk and how to trace it (`git log`, the `Mechanical:` marker, `.git-blame-ignore-revs`), what each gate shows and does not show, and how to re-run the evidence.
- FINDINGS: F49 closed.

### D4d: `Genotypes`, `diagnostics` and `Treatments` registries → state owned by `Simulation` (done, 2026-10-22)
- Base: d386e12f. The G-perf base worktree `../om-bench-base` was moved to d386e12f and rebuilt; it is also the A/B base.
- Commits:
  - 6e83ea3f: **logic.**
    - `WithinHost::GenotypeRegistry` (`Genotypes.h`): the `GT::` namespace globals and `Genotypes::N_genotypes` as members. Its constructor is the old `Genotypes::init`, assigned to a new `Simulation` member at step 1 as before, with every check and error in the old order; `--print-genotypes` comes from the `CommandLineOptions` and the table code moved unchanged into `print_genotypes()`, still called last. Default construction is the old pre-init state, so the unit tests see what they saw. `pre_main_sim_init` and `checkpoint` (same bytes, same stream position) are methods; `read_checkpoint`/`write_checkpoint` take the registry.
    - `WithinHost::DiagnosticRegistry` (`Diagnostic.h`): the map and the monitoring-diagnostic pointer as members; constructor = the old `diagnostics::init`, assigned at step 4 as before, taking `ModelOptionFlags` and `CommandLineOptions` instead of the bridge. Copies deleted, moves defaulted (map moves keep node addresses, so the pointer and the references other modules hold stay valid). `diagnostics::clear` gone; `make_deterministic` private (no outside caller).
    - `TreatmentRegistry` (`Treatments.h`), a member of `WithinHostModel` rather than a third bridge: like D4c's `VivaxParams::health_system` it is health-system state that `setHS` and each `changeHS` extend at run time. `WHInterface::add_treatment` and the two `treatment()` methods reach it through the D4c bridge accessors.
    - **Bridge:** `bridge::current_genotypes`/`genotypes()` with `GenotypesBinding`, and `bridge::current_diagnostics`/`diagnostics()` with `DiagnosticsBinding`, both declared in `Simulation` after `Parameters`. The static `Genotypes::` and `diagnostics::` functions other modules call stay as inline delegates, so transmission, monitoring, PK/PD, clinical and intervention code is unchanged until D5–D8.
    - Unit tests: `UnittestUtil` binds a default `GenotypeRegistry` and `DiagnosticRegistry`; `PkPdSuiteSetup` calls `init_single` on it; `setDiagnostics` assigns a new registry where it called `diagnostics::init` (which cleared the map first).
  - ed6fad49: `util/ab_scenarios.py` group `registries`, 27 variants (below).
  - 85edc7ef + 5f89fcb0: **rename**, 35 renames over 24 files. Maps: `N=count` (the tool's `n` is the name of locals next to many uses; `count` matches the registry), `densHRP2=dens_hrp2` (bad split), `iFreq=freq`. No skips. The facade class `diagnostics` became `Diagnostics`. Collisions reviewed: facade and registry methods share names in different classes, always called qualified; `Diagnostics` vs the `scnXml::Diagnostics` forward declaration is a different namespace. The uncompiled `ESCaseManagementSuite.h` doesn't use the module.
  - 35eadf9c: **tool fix (F50).** The first modernise run converted loops whose iterator an `assert` still used: Release and G-exact passed, the Debug build did not compile. Reverted; `util/modernize_module.py` now passes `-UNDEBUG` as the rename tool does (F23).
  - e67dbef9 + 1357ccba: **modernise, tool:** 10 `loop-convert`, 5 `prefer-member-initializer`, 4 `concat-nested-namespaces`, 1 `use-emplace`, 1 `init-variables`.
  - 760980b6: **modernise, hand:** 17 consts; `SampleMode` an `enum class` over `uint8_t` (private; checkpointed through an explicit `int`, so the bytes are unchanged, compared by the A/B's two checkpoint variants); `Diagnostic::uses_hrp2` initialised (it was indeterminate after the XML constructor's `VIVAX_SIMPLE_MODEL` early return; never read there, since `WHVivax::diagnostic_result` ignores the diagnostic and the monitoring diagnostic is then the deterministic one, which set it); in `print_genotypes` the never-printed `stringstream fmt` removed, `setw` narrowing casts, named loop variables, the assert loop the tool skipped converted by hand, `contains()` → `std::ranges::find`; a raw string; `NOLINT(performance-avoid-endl)` on moved lines. Left: `Treatments::Stages` (bit flags used outside the module), `endl` (F14), `using namespace std;` in `Treatments.h`, F51/F52.
  - caa82068: FINDINGS F51–F53; PLAN.md D4d row as built.
- Gates:
  - G-exact / G-unit: 56/56 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit (the first modernise attempt failed Debug to compile, F50, and was reverted).
  - G-stat: plain `ctest -j4` at 760980b6: 109 passed, 3 disabled (the vivax scenarios' `stat_` tests).
  - G-style: `util/style.sh check d386e12f` clean at 760980b6. The rename and modernise tool commits passed their skill checks (format half clean, second dry run with nothing to do, token check), and `check <tool commit>^` is clean after each ignore-revs commit.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 760980b6, load ~5.8 at the start: median per-pair ratio 1.0041 (+0.41%), spreads 1.7% / 5.6%. On `scenarioGenotypes.xml` (vector, tracking mode, where `Genotypes::count()` runs in the per-step loops), `-n 7`: 0.9989 (−0.11%). The bridge's extra load costs nothing measurable.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base d386e12f): `registries` 27 of 27 identical after the logic commit; all 57 variants (`registries`, `vivax`, `falciparum-init`) identical after the hand commit. The group covers `--print-genotypes` (one and two loci, MolPairwise, no `parasiteGenetics`), tracking and initial sampling with `--checkpoint` (checkpoint bytes compared), the genotype, diagnostic and treatment init errors, unknown diagnostic names from surveys, an intervention and neonatal mortality, the detectionLimit deprecation and analysisNo warnings, and MSAT with HRP2. Each variant was checked to reach its target; an invalid `clearInfections` stage is rejected by the schema, so it is not a variant.
- Findings: F50 (modernise tool missed assert bodies; fixed), F51 (`--print-genotypes` crashes with a one-allele locus), F52 (its table is misaligned), F53 (diagnostics held by address in statics outlive `Simulation`).
- Surprises:
  - F23 had noted the modernise tool's Release-only view as harmless; `loop-convert` made it a compile break. Only the Debug build caught it, so building `build-debug/` after every C++ commit is what makes the tool commits safe.
  - Checking the print variants by hand showed `--scenario` resolves against `--resource-path`, so reruns outside `ab_scenarios.py` need absolute scenario paths.
- Follow-ups:
  - D5: `LSTMDrugType` uses `Genotypes::find_allele_code`/`get_genotypes` through the facade.
  - D6: `NeonatalMortality`, the clinical models and `CMDTDiagnostic` hold diagnostics by address (F53); `WHFalciparum`/`WHVivax::treatment` and `WHInterface::add_treatment` still use the within-host bridge; pass the registries instead.
  - D7: the transmission and vector models call `Genotypes::count()` in per-step loops; take the count from the registry at construction.
  - D8: intervention components hold diagnostics by reference (F53); `Vaccine` uses the genotype facade.
  - D9: remove `bridge::current_genotypes`, `bridge::current_diagnostics` and the facades; `mon::` uses `Genotypes::count()` in `addStore`.
  - F51/F52: ask the user whether to fix (stdout changes), e.g. in I2 with the other print options (F41).
- Next: D5.

### D5: PK/PD registries → `PkPdParams` owned by the within-host model (done, 2026-10-22)
- Base: 78d96f4c. The G-perf base worktree `../om-bench-base` was moved to 78d96f4c and rebuilt; it is also the A/B base.
- **Scope, a deviation from the plan row:** `PerHost::params` is `Transmission::PerHostAnophParams::params`, vector parameters filled by the transmission init and adjusted by `AnophelesModel`, read by interventions. It moves with the rest of `Transmission/` in D7 (PLAN.md rows amended). D5 covers `model/PkPd`.
- Commits:
  - 8dcdf512: **logic.**
    - `pk_pd::DrugRegistry` (`LSTMDrugType.h`): the drug types, name map and drugs-in-use list that were file statics; constructor = the old `LSTMDrugType::init`; `find_drug` (still marks the drug and its metabolite as in use), `get`, `drugs_in_use`, `create_instance`. `LSTMDrugType`'s constructor takes the registry being built (metabolite lookup), the `GenotypeRegistry` (D4d follow-up: no `Genotypes` facade) and the `--print-genotypes` flag.
    - `pk_pd::CourseRegistry` (`LSTMMedicate.h`): schedules, dosage tables and their names; constructor = the old `LSTMTreatments::init`, finding drugs in the `DrugRegistry`.
    - `pk_pd::PkPdParams` (new `PkPd/PkPdParams.h`) holds both; its constructor is the old `LSTMModel::init`, called at the same point of `WithinHostModel`'s constructor (Common model only), stored in `CommonWithinHostParams::pk_pd`. `WithinHostModel` takes the `GenotypeRegistry` for it. Every read and error happens in the old order.
    - Each `LSTMModel` holds a `const PkPdParams*` (from `CommonWithinHost`'s params) for `prescribe`, `medicate`, `summarize` and the checkpoint load.
    - **Bridge:** the static `LSTMTreatments::find_schedule`/`find_dosages` (clinical decision trees, D6; the `treatPKPD` intervention, D8) delegate through `bridge::within_host_model()`. An empty registry raises the same "use of PK/PD treatment requires…" error the empty statics did. No new bridge pointer.
    - Removed: `num_drug_types` (no caller), the static `init`/`clear` functions. `DosageTable::get_multiplier` and `MedicateData::multiplied` became `const` (neither changed state).
    - Unit tests: `UnittestUtil::pk_pd_params()` is the bound `WithinHostModel`'s `PkPdParams`; `PkPdSuiteSetup` assigns registries to it, suites build `LSTMModel`s with it and reset it where they cleared the statics (the old `clear()` kept the drugs-in-use list; no test reads it). `WHMock` takes the bound model's. Two `PkPdComplianceSuite` locals on changed lines renamed.
  - 5c0449e9: `util/ab_scenarios.py` group `pkpd` (26 variants, below). The script no longer compares the source file and line in a traced exception's "Call stack, starting from" line, which differ between any two builds whose code moved (the message and exit code are compared).
  - Rename: `util/style.sh rename model/PkPd --dry-run` printed `nothing to rename` (B4 renamed the module; the new names follow the rules), so no rename commit. The uncompiled suites (F9) don't use PkPd.
  - c92d9d36 + b543d8fd: **modernise, tool:** 1 `use-ranges`, the `is_sorted` in an `assert` that M2's run could not see before F50's fix.
  - de48455e: **logic.** The two GSL integration workspaces (mutable file globals allocated at static init, never freed; M2 left them for D5) → `thread_local`, allocated at first use, freed at thread end. Per thread rather than per simulation because they are scratch memory (`gsl_integration_qag` initialises the workspace each call) and a per-simulation buffer would race under M8's threaded update; per thread suits M7 and M8. Only the allocation time changed (now after `main` sets the GSL error handler), which matters only if it fails.
  - 0c859fb0: **modernise, hand:** the four `ParamsConvFactor` helpers take `const&` (M2's follow-up; the one caller that assigns into the struct does so after the call); explicit casts for the four `size_t` narrowings. Left, as M2: protected data (34), the `const LSTMDrugType&` members, GSL's `const_cast`, `endl` (F14). No `new`/`delete`, macros, plain enums or const gaps were left.
  - a210a53d: FINDINGS F54, F55; PLAN.md D5 row as built, D7 row.
- Gates:
  - G-exact / G-unit: 56/56 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. 14 ctest scenarios use pharmacology (one- and two-compartment drugs, age, body-mass and multiply dosages, phenotype restrictions, drug-concentration measures, decision-tree and intervention `treatPKPD`), all with checkpoint stop and resume.
  - G-stat: plain `ctest -j4` at 0c859fb0: 109 passed, 3 disabled (the vivax scenarios' `stat_` tests).
  - G-style: `util/style.sh check 78d96f4c` clean at 0c859fb0; the tool commit's format half clean and a second dry run applied nothing.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 0c859fb0, load ~6.4 at the start: median per-pair ratio 0.9903 (−0.97%), spreads 3.4% / 1.5%. The benchmark prescribes PK/PD treatment (one-compartment drugs). On `scenarioBSV.xml` (two-compartment drugs, so the per-thread workspace), `-n 7`: 0.9968 (−0.32%).
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base 78d96f4c): `pkpd` 26 of 26 identical after the logic commit, after de48455e, and with all 83 variants (`falciparum-init`, `pkpd`, `registries`, `vivax`) after the hand commit. The group covers what ctest can't: the conversion (metabolite) and three-compartment models, which no ctest scenario uses, plain, with `--checkpoint` (checkpoint bytes compared) and `--print-genotypes`; each drug, phenotype and dosage-table init error; unknown drug, schedule and dosage names; no pharmacology; an unknown schedule in the `treatPKPD` intervention and that intervention with `--checkpoint`. Each variant was checked to reach its target; the runtime ones report non-zero concentrations for all four drugs (CQ, the DHA metabolite, AS, PPQ3).
- Findings: F54 (checkpointed drug indices unchecked), F55 (duplicate drug reported as a code error; two message typos).
- Surprises:
  - The modernise tool found one more fix in a file D5 did not otherwise touch: an `assert` body, visible only since F50.
  - The A/B script's first `pkpd` run flagged `drug_twice` as different: only the traced exception's source path and line differ, since the worktrees differ and the code moved.
- Follow-ups:
  - D6: the clinical decision trees call `LSTMTreatments::find_schedule`/`find_dosages` through the bridge; pass the `PkPdParams` (or the `WithinHostModel`) to the decision-tree factory.
  - D7: `Transmission::PerHostAnophParams` and `PerHost::relAvailAge` statics (from D5's plan row).
  - D8: the `treatPKPD` intervention component uses the same facade.
  - D9: remove the `LSTMTreatments` facade with the bridge.
  - Optional: a ctest scenario for the conversion and three-compartment models (expected output from 78d96f4c), so later chunks get them under G-exact without the A/B base; adding it means new `test/expected/` files, so ask first. **Done after the log entry, with the user's approval; see the next entry.**
- Next: D6.

### D5 follow-up: a scenario test for the PK conversion and three-compartment models (2026-10-22, after D5)
The user approved adding the scenario test proposed in D5's follow-ups.
- **First attempt, on scenarioGenotypes** (the A/B variant): its checkpoint holds no drug instances (nobody is treated before the intervention period), so planted checkpoint bugs passed. Rebased on scenarioBSV, whose checkpoint holds treated humans.
- **The BSV-based scenario failed its own checkpoint-stop test on the base build:** resuming does not reproduce the uninterrupted run (F56). Three values are not checkpointed: each drug instance's sampled `vol_dist`, the conversion model's `vol_dist_metabolite` and `last_bm`, and each infection's cached `kn` (IC50^slope), whose re-draw after resume uses the human's RNG for conversion drugs. Shown by: BSV with a CV added to PPQ's `vol_dist` diverges on resume (base build); a temporary patch checkpointing the first two made the scenario resume exactly; an AS-only variant still diverged through `kn`. The patch was reverted; fixing F56 changes the checkpoint format and resumed outputs, so it waits for the user (H3 fits).
- **Harness bug (F57):** the registration for tests without checkpointing (`OM_BOXTEST_NC_NAMES`, empty until now) passed the scenario name after `--`, i.e. to openMalaria. Fixed in 89c90989.
- Commits:
  - 89c90989: the NC registration fix.
  - 5355535c: `test/scenarioPkPdModels.xml` (scenarioBSV plus DHA, AS converted to DHA, and three-compartment PPQ3, with sampled conversion rate and volumes), expected `output`/`ctsout` from the 78d96f4c build (byte-identical to the current build's), registered without `--checkpoint-stop`. Its `stat_`/`statfull_` tests are disabled (no reference).
  - 74843c0a: `util/ab_scenarios.py` `pkpd/models_checkpoint`, the scenario with `--checkpoint` twice: about 490 conversion and 980 three-compartment instances are written to and read from its checkpoint.
- Planted bugs, each run against the full non-statistical ctest and the A/B `pkpd` group (base 78d96f4c):
  | Plant | ctest | A/B |
  |---|---|---|
  | conversion factor `min` instead of `max` | PkPdModels only | 4 runtime variants |
  | conversion model built with the parent as metabolite | PkPdModels only | 4 runtime variants |
  | three-compartment `k21 * k13` doubled (invisible when k13 = 0, as in BSV, PEV, TBV) | PkPdModels only | 4 runtime variants |
  | conversion checkpoint reads `nk_p_sample`/`nconv_sample` swapped | none | `models_checkpoint` |
  | three-compartment checkpoint reads `k13`/`k31` swapped | none | `models_checkpoint` |

  Two earlier plants were dropped as not real bugs for this input: not marking the metabolite as in use (the parent's constructor already marks it through `find_drug`), and not checkpointing `qty_m` (DHA's elimination at 25.4/day leaves nothing visible).
- Gates: 57/57 non-statistical tests on Release and Debug (strict); A/B `pkpd` 27/27 identical.
- Process note: the user stopped a diagnostic command that cleared scratch directories with `rm -rf` on a variable-built path. Later runs used fresh `mktemp -d` directories and delete nothing.
- Follow-ups: F56, **to be fixed in H3 (the user's decision, 2026-10-22)**; then move PkPdModels to `OM_BOXTEST_NAMES`.

### Plan amendment: D6 split (2026-10-22, at the start of D6)
- D6 covers about 30 files of non-mechanical change, past the protocol's ~20: the pathogenesis models (5 files) whose init `WithinHostModel` already runs, the seven clinical model classes with infant and neonatal mortality and their callers (`Simulation`, `checkpoint.cpp`, `Human`, `Deployments.h`, `Monitoring.cpp`), and the decision-tree library that the health system and an intervention share. Split in advance by owner, as D4 was: D6a (pathogenesis into `WithinHostModel`), D6b (the clinical models and neonatal mortality into an object owned by `Simulation`), D6c (the decision-tree library). Rows in PLAN.md § Stage D.

### D6a: pathogenesis statics → `PathogenesisParams` in the within-host model (done, 2026-10-22)
- Base: 1b084b2e (the D6 split; same C++ as 9bae2646). The G-perf base worktree `../om-bench-base` was moved to 1b084b2e and rebuilt; it is also the A/B base.
- Commits:
  - 910bf9a9: **logic.**
    - `Pathogenesis::PathogenesisParams` (`PathogenesisModel.h`): the `pg_*` file statics, the NMF incidence interpolator, the model choice (`Kind`: pyrogenic, Mueller, predetermined) and the two submodels' parameters as `MuellerParams` and `PyrogenParams`. Its constructor is the old `PathogenesisModel::init`, assigned at the same two points of `WithinHostModel`'s constructor (NMF only for vivax), taking the `ModelOptionFlags` instead of the bridge; every read and error in the old order.
    - `PathogenesisParams::create` replaces `createPathogenesisModel` (now `unique_ptr`); each pathogenesis model holds a pointer to the params. `sample_nmf` replaces the static `sampleNMF`; `WHVivax` holds a pointer for it. `WHFalciparum`, `CommonWithinHost`, `DescriptiveWithinHostModel` and `WHVivax` take the params from `WithinHostModel::create`. No new bridge.
    - `update_pyrogen_thres` (the hot loop, ~5% of run time per its comment) reads its four parameters into locals before the loop.
    - Default construction is the old pre-init state (pyrogenic, zero parameters, NaN `a`/`b`, no NMF incidence), which `InfectionImmunitySuite` passes explicitly; no other unit test reaches pathogenesis. `AgeGroupInterpolator::isSet` became `const`.
  - 3be68cb7: `util/ab_scenarios.py` group `pathogenesis` (below).
  - c38704fb + 225947ca: **rename**, 49 by the tool (29 parameter, 4 enum constant, 4 variable, 3 member, 3 method, 3 local constant, 2 header guards, 1 global constant). Maps: `cF=comorbidity_factor` (the tool's `c_f`; it initialises that member), `n=NumCategories`. Collisions reviewed: `comorbidity_factor` and `age_years` already appear as parameters of `PathogenesisParams` methods, a different class. Then a hand commit for the six single-word all-caps `State` enumerators (`NONE` → `None`, …, 23 code sites; as D3), which the tool can't see; `Episode::State`'s enumerators of the same names are Clinical's (D6b). Both listed in `.git-blame-ignore-revs` (4ed41e45).
  - aa825247: **logic, F58.** The first modernise run's `prefer-member-initializer` fix moved the `YStarSq` and `YStar1` reads of `PyrogenParams` ahead of `YStarHalfLife` and `Alpha`. `Parameters::operator[]` throws for a missing parameter, so that changes which one a bad scenario reports. Reverted; the members are now declared in the old read order and built in the initializer list (`b` and `a` by two file-local helpers with the old expressions).
  - f7849271: the A/B group's parameter-order variants, which catch the tool's order (plant below).
  - c2e8281b + f4b0cfd2: **modernise, tool:** 9 `use-override`, 5 `concat-nested-namespaces`, 5 `use-equals-default`, 2 `use-default-member-init`.
  - c5dc1e95: **modernise, hand:** four consts; `NumCategories` `constexpr` with an explicit cast where it divides 1.0. Left: protected data (read by the submodels), `State` as a plain enum (bit flags reused by `Episode::State`, D6b; checkpointed as an `int` by `WHVivax`, H3), the header guard.
  - 25082ee3: G-style on changed lines: the params pointer private behind a protected `params()` (as `WHFalciparum`), an explicit `size_t` → `double` cast, `NOLINT(readability-identifier-naming)` on `isSet` (util's own rename).
  - 44666fc1: FINDINGS F58; PLAN.md D6a row as built; the modernize-module skill's dry-run step names the F58 hazard. 0d84e89f: README's A/B row.
- Gates:
  - G-exact / G-unit: 57/57 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches the pyrogenic (most scenarios), Mueller (scenario10) and predetermined (scenario9) models, falciparum NMF (VecFullTest, NonHumanHosts) and vivax without NMF, all with checkpoint stop and resume.
  - G-stat: plain `ctest -j4` at 25082ee3: 110 passed, 4 disabled (the vivax liver-stage scenarios' and PkPdModels' `stat_` tests).
  - G-style: `util/style.sh check 1b084b2e` clean at 25082ee3. Rename: format half clean, second dry run `nothing to rename`, token check passed, `check c38704fb^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check c2e8281b^` clean.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 25082ee3, load ~5.5 at the start: median per-pair ratio 1.0037 (+0.37%), spreads 5.4% / 0.7%. The benchmark runs the pyrogenic model's threshold loop for every human each step. scenario10 (Mueller), `-n 7`: 1.0000, but its runs take 0.05 s, so that says little.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base 1b084b2e): `pathogenesis` 8 of 8 identical after the logic commit, 28 of 28 after f7849271; all 112 variants (`falciparum-init`, `pathogenesis`, `pkpd`, `registries`, `vivax`) identical at c5dc1e95, and `pathogenesis` again at 25082ee3. The group covers what ctest can't: vivax with non-malaria fevers (every vivax ctest scenario sets `NON_MALARIA_FEVERS` false; the variant moves output measures 0, 11, 12 and 14 against plain scenarioVivax, so it reaches `sample_nmf`), plain and with `--checkpoint`; the missing `NonMalariaFevers` element for vivax and falciparum, and before a bad `latentp` (error order); the Mueller, predetermined and pyrogenic-with-NMF models with `--checkpoint` (checkpoint bytes compared); each pathogenesis parameter dropped alone, and each consecutive pair in the old read order (18, 21, 24, 30, 28, 25, 13, 14, 26; Mueller 31, 32).
  - Planted bugs (A/B `pathogenesis`, then reverted): vivax NMF sampled at age 0 → `vivax_nmf` and `vivax_nmf_checkpoint` differ; the tool's `PyrogenParams` read order → `pyrogen_no_params_25_13` and `_14_26` differ.
- Findings: F58 (the modernise tool's `prefer-member-initializer` can reorder throwing reads). The earlier tool commits that applied the check were reviewed; none moved a throwing read past another.
- Surprises:
  - No ctest scenario runs the vivax model with non-malaria fevers, so the vivax side of D6a would have had no gate without the A/B group.
  - The first commit attempt failed on the pre-commit clang-format hook (line wrapping); the hook's edits were staged and the commit retried, so the history has no formatting-only commits.
- Follow-ups:
  - D6b: `Episode::State` copies `Pathogenesis::State` values by name; rename its single-word enumerators by hand too, and consider `enum class` for both together. `ClinicalModel::update` calls `determine_morbidity` and neonatal mortality; F53 for `NeonatalMortality`.
  - Tool: F58's candidate fix (report, not apply, a `prefer-member-initializer` move of a call that is not the body's first statement).
  - `util/AgeGroupInterpolation`'s rename removes the `isSet` NOLINT.
- Next: D6b.

### D6b: clinical model statics and neonatal mortality → `Simulation` (done, 2026-10-22)
- Base: f79fcb79 (same C++ as 25082ee3). The G-perf and A/B base worktree `../om-bench-base` was moved to f79fcb79 and rebuilt.
- Commits:
  - 66fdf1e1: **logic.**
    - `Clinical::ClinicalParams` (new `Clinical/ClinicalParams.h`), a `Simulation` member built where `ClinicalModel::init` ran, every read and error in the old order: `IndirectMortalityFix`, `healthSystemMemory` (format errors still rewrapped), parameters 17 and 19 (19 was `InfantMortality::init`'s read at that point, so it is read here and handed to `InfantMortality`), then `EventSchedulerParams`' constructor (the old `ClinicalEventScheduler::init`) or the `ImmediateOutcomes` choice and its deprecation warning. `Kind` replaces the two choice flags. It takes the `ModelOptionFlags` and `CommandLineOptions`; the `HealthSystemMemoryFix` and `VivaxSimpleModel` flags, which `Episode` and the two `setHealthSystem` functions read through the bridge, are read once there (non-throwing reads, so their position does not matter).
    - `set_health_system` replaces `setHS` (same checks and order); the health-system values are `CM5DayParams` (shared by the 5-day models, with `cfr_pf_use_hospital`), `ImmediateOutcomesParams`, `DecisionTree5DayParams` and `EventSchedulerParams` (the event scheduler's statics and `ESCaseManagement`'s trees). Each old `setHealthSystem` body is unchanged apart from names. `changeHS` reaches it through `bridge::mutable_clinical_params()` (as D4c's values).
    - `create` replaces `createClinicalModel`; each per-human model holds a pointer behind a protected `params()`. `Episode` is given the health-system memory and its fix flag at construction. `ClinicalModel::hsMemory` (now `hs_memory`) stays as a facade through the bridge for `CMDTUncomplicated` (D6c). The duplicate `CaseType` enum in `DecisionTree5Day.cpp` (shadowed by `CM5DayCommon`'s) went.
    - `InfantMortality` and `NeonatalMortality` became instance classes and `Simulation` members, passed to `Host::update` (then to the clinical model) and to the checkpoint functions (same stream position). Monitoring reads the infant mortality through `bridge::infant_mortality()` until D9. `NeonatalMortality` looks its diagnostic up in the `DiagnosticRegistry` it is given (F53's neonatal part).
    - The unit tests bind a default `ClinicalParams` (the old pre-init state).
  - 38a88580: `util/ab_scenarios.py` group `clinical` (below).
  - a0e9a737 + 18a6f0c2: **rename**, 167 by the tool (38 parameter, 34 member, 34 variable, 16 method, 14 enum constant, 14 local constant, 8 macro, 6 global constant, 3 function) over `model/Clinical/` except `CMDecisionTree` and `model/Host/NeonatalMortality`. Maps: `tSF=tsf`, three header guards (`HMOD_CM5_DAY_COMMON`, `HMOD_DECISION_TREE_5_DAY`, and `HMOD_CLINICAL_EVENT_SCHEDULER`, fixing the old guard's typo). Collisions reviewed: two `member(param)` initialisers of the same new name, `pg.indirect_mortality` (another struct), the genotype registry's `pre_main_sim_init`. Then a hand commit for `Episode::State`'s eight single-word enumerators (`NONE` → `None`, …, 44 code sites; D6a's follow-up). Both in `.git-blame-ignore-revs` (f1831c6f).
  - 227daa2f + d4e7b71c: **modernise, tool:** 13 `concat-nested-namespaces`, 10 `use-override`, 6 `use-default-member-init`, 3 `init-variables`, 2 `use-ranges`, 2 `use-nullptr`, 2 `loop-convert`. The dry run had no `prefer-member-initializer` (no F58 hazard); the two loop conversions iterate containers their bodies do not change.
  - 51663e3e: **modernise, hand:** `make_unique`; the treatment measures a `static constexpr std::array` (`mon::measure` is `consteval`); `q` a `std::array`; `Episode` and `ClinicalModel` with deleted copy and move (an `Episode` copy would report twice); one expression for `Episode::update`'s duplicated branch; the delay loop's `goto` → `break`; `static_cast`s for the casts and two narrowings; neonatal age bounds local constants; `misc-const-correctness`. Left: `CaseType` plain (array index, NOLINT), `Episode::State` and the doomed codes (checkpointed ints, H3), `latest_state` uninitialised (F60), protected/public data, the `Clinical` namespace name.
  - 9011466e: FINDINGS F59, F60 and F53's neonatal part; PLAN.md D6b row as built; README's A/B row and bridge list.
- Gates:
  - G-exact / G-unit: 57/57 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches all three clinical models, `changeHS` under each (scenario4, scenario1, EffectiveDrug, Molineaux), the infant mortality rate output (e.g. scenario4, 5, 6) and neonatal mortality with (Genotypes) and without a diagnostic element, all with checkpoint stop and resume.
  - G-stat: plain `ctest -j4` at 51663e3e: 110 passed, 4 disabled (as D6a).
  - G-style: `util/style.sh check f79fcb79` clean at 51663e3e. Rename: format half clean, second dry run `nothing to rename`, token check passed, `check a0e9a737^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check 227daa2f^` clean.
  - G-perf: `util/bench.sh --compare ../om-bench-base/build-rework`, candidate 51663e3e, load ~3.4–5 at the start: median per-pair ratio 1.0126 (+1.26%), spreads 10.2% / 16.0%. The benchmark scenario uses the event scheduler. scenarioPenny (event scheduler), `-n 11`: 1.0100 (+1.00%), spreads 3.7% / 6.9% (a first `-n 7` run under higher load gave 1.0262 with a 19.7% spread).
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base f79fcb79): `clinical` 34 of 34 identical at the logic commit (before two variants were reworked, below), all 148 variants (`clinical`, `falciparum-init`, `pathogenesis`, `pkpd`, `registries`, `vivax`) identical at 66fdf1e1 and again at 51663e3e. The `clinical` group (36 variants) covers what ctest can't: the event scheduler with non-malaria fevers (no ctest scenario combines them; the variant's outputs differ from plain ESTS), plain and with `--checkpoint`, and its two missing-element errors; checkpoint bytes for each clinical model; `CFR_PF_USE_HOSPITAL` (no scenario sets it; moves scenario5's outputs); stderr with and without deprecation warnings; event-scheduler `changeHS` keeping the health system, changing `uncomplicatedCaseDuration` (moves outputs) and changing `dailyPrImmUCTS` (does not: F59); a health system of the wrong kind for each model, initially and in a `changeHS`; the event scheduler's init errors; a bad drug name; an `<uncomplicated>` memory above the health-system memory; a bad `healthSystemMemory` alone and with parameter 17 missing; each clinical parameter dropped alone and each consecutive pair in the read order (17, 19; event scheduler 17, 19, 38, 33).
  - Planted bugs (A/B `clinical` and non-statistical ctest, then reverted): parameters 17 and 19 read in swapped order → `io_no_params_17_19` and `es_no_params_17_19` differ, ctest passes; the event scheduler's MF and NMF need-treatment tables swapped → `es_nmf` and `es_nmf_checkpoint` differ, ctest passes.
- Findings: F59 (`changeHS` cannot change `dailyPrImmUCTS`), F60 (`latest_state` read uninitialised by the decision-tree intervention for a newborn). F53's neonatal part settled.
- Surprises:
  - Three "missing section" variants first stopped at XSD validation (the schema requires one of the three health-system elements); they were rebuilt to give a health system of the wrong kind, which reaches the C++ checks.
  - With a 5-day step, `CLINICAL_EVENT_SCHEDULER` is rejected by the model-option check before the event scheduler's own time-step check, which no scenario can then reach.
  - A first positive control for `changeHS` (a higher CFR in ESTS) moved nothing: ESTS has too few severe cases in its short intervention period. The case-duration control does move outputs.
- Follow-ups:
  - D6c: `CMDTUncomplicated` reads the health-system memory through `ClinicalModel::hs_memory()` (the bridge); pass it, with the `DiagnosticRegistry` and `PkPdParams`, to the decision-tree factory. The clinical params hold pointers into the decision-tree library, which D6c moves.
  - D8: `TimedChangeHSDeployment` reaches the params through `bridge::mutable_clinical_params()`; `ClinicalParams::set_health_system` still calls `WHInterface::add_treatment` and `WHVivax::set_hs_parameters` through D4's bridges, which D8 can pass in with the `WithinHostModel`.
  - D9: monitoring's `bridge::infant_mortality()`.
  - H3: `Episode::State` and the doomed codes to `enum class` (both checkpointed as ints).
  - The uncompiled `ESCaseManagementSuite.h` (F9) still uses the old names (`Episode::NONE`, the old `CMHostData` constructor).
  - F59 and F60 wait for the user. **Fixed after the log entry, with the user's approval; see the next entry.**
- Next: D6c.

### D6b follow-up: F59 and F60 fixed, with tests (2026-10-22, after D6b)
The user asked for both D6b findings to be fixed, each in its own commit, with new tests that validate the changes. Both change behaviour on purpose; the base for the A/B runs and G-perf stayed f79fcb79 (`../om-bench-base`).
- Commits:
  - e169341c: `util/ab_scenarios.py` `clinical/dt_intervention_severe_node`: scenarioMSAT's MDA tree behind a `<severe>` node, deployed every step from day 1484 to 1849 at full coverage, so every newborn gets the tree on its first step, before its first clinical update, when `latest_state` is not yet set. First attempts did not reach that window: MSAT's two timed deployments fall on days with no newborn (100 people, 1-day steps), and a continuous deployment needs a target age above zero, i.e. after the first update. A build that initialises `latest_state` to `Complicated` moves this variant (nMDAs 2080 → 2094, mass screenings 36588 → 36568), so the variant does read the value; the base build and the fixed build agree on it.
  - 75700eda: **F60 fix.** `latest_state` starts at `Episode::None`; the constructor's NOLINT goes. New Catch2 file `unittest/ClinicalTests.cpp` (in `unittest-catch2`), `[clinical][F60]`: each of the three models built by placement new in storage filled with 0xA5 reports `None` (without the fix: `-1515870811 == 0` for all three), and `ClinicalParams::create` gives `None` for each kind (this one passes without the fix by chance: fresh heap memory is zero).
  - 41c35aa4: **F59 fix.** `EventSchedulerParams::set_parameters` clears `cum_daily_pr_imm_ucts` before filling it. Catch2 `[clinical][F59]`: three successive health systems (1; 0.2, 0.8; 1) each leave exactly their own cumulative values (without the fix: `{1.0, 0.2, 1.0}` and `{1.0, 0.2, 1.0, 1.0}`). The test's CFR uses piecewise-constant interpolation, since the linear one reads the maximum human age from the clock, which a unit test does not bind.
  - f8f0896b: scenario test scenarioESChangeHS (scenarioESTS plus a `changeHS` at the start of the intervention period changing `dailyPrImmUCTS` from 1 to 0.2, 0.8), registered in `OM_BOXTEST_NAMES` (checkpoint stop and resume). New files only: `test/scenarioESChangeHS.xml` and `test/expected/outputESChangeHS.txt`, from the fixed build (Release and Debug identical). The build before the fix gives output byte-identical to `outputESTS.txt` and fails the test with 1360 differing cells.
- Gates:
  - G-exact / G-unit: after each fix 57/57 (strict) on Release and Debug, then 58/58 with scenarioESChangeHS; `unittest-catch2` 14 test cases. Neither fix moves any existing `test/expected/` file.
  - A/B (base f79fcb79): after the F60 fix 0 of 149 variants differ; after the F59 fix 2 of 149, exactly `clinical/es_changehs_daily_pr` and `es_changehs_daily_pr_checkpoint` (nUncomp 541 → 597; before the fix both equalled `es_changehs_same`). The fixed build's checkpointed run equals its uninterrupted run, as the base's did: on resume the `changeHS` is deployed again and now replaces the list too.
  - G-stat: plain `ctest -j4` at f8f0896b: 111 passed, 5 disabled (the vivax liver-stage scenarios', PkPdModels' and the new ESChangeHS's `stat_` tests, which have no reference).
  - G-style: `util/style.sh check` clean on each commit's changes.
  - G-perf: candidate f8f0896b against f79fcb79: median per-pair ratio 1.0135 (+1.35%), spreads 6.0% / 0.9%, load ~2.3–3.
- Findings: F59 and F60 moved to FINDINGS § Closed.
- Surprises:
  - Before the F59 fix, scenarioESChangeHS's whole `changeHS` was a no-op for the output: every other value it carries equals ESTS's, and the one that differs was ignored.
  - F60 changed nothing observable here, even on a variant built to reach the uninitialised read; the poisoned-storage unit test is what shows the old code read an indeterminate value.
- Next: D6c.

### Plan amendment: B6, make CI pass (2026-10-22, after D6b)
- The user asked for fixing CI to be added to the plan: every push of the branch fails CI. The user had read the failures as GitHub Pages failing to deploy; the Pages workflow in fact passes on this branch. What fails (C/C++ CI 39557798562 and Style 39557798559, at 4db10096): the macOS jobs' statistical tests (F61), the Linux jobs' LifeNet1/2 and EffectiveDrug outputs (F62), the ubuntu-22.04 job's configure (F63) and the Style job (F64).
- B6 added to PLAN.md (Stage B) and to § Recommended order as row 11b, so it is the next chunk, before D6c.

### B6: Make CI pass (2026-10-23)
Base 7d115055. The user approved, at the start, pushing `throwaway_ai_rework` and a scratch `b6-diag` branch as often as B6 needed, and dropping the ubuntu-22.04 job. Midway, after the diagnosis, the user chose `-ffp-contract=off` with LifeNet1/2's `test/expected/` regenerated, and to keep CI's tolerant binary-mode scenario check.
- Baseline: pushed 7d115055. C/C++ CI 39879165107 and Style 39879164982 failed exactly as F61–F64 described at 4db10096: macOS 52 of 110 (statistical), ubuntu-24.04 LifeNet1/2, ubuntu-24.04-arm EffectiveDrug and LifeNet1/2, ubuntu-22.04 configure, and Style's one slicing warning (now at `LSTMDrugType.cpp:339`). No new failure came from D5–D6b.
- Commits:
  - 89fd11f7 (F61): `test/CMakeLists.txt` registers `stat_*`, `statfull_*` and `stats_tool_check_scenario` only with Python ≥ 3.10, and otherwise prints which version it found. The build workflow gains `actions/setup-python@v7.0.0` (3.12). Checked by configuring with `/usr/bin/python3` (3.9.6): 1 statistical test is registered (the `uv` one). With the default Python, 58 are registered, as in `build-rework`. `stats_tool_generate_reference` is left ungated, since it runs through `uv` with its own Python (deviation from the plan's "skip `stats_tool_*`").
  - b4f516a1 (F63): ubuntu-22.04 removed from the matrix (user's choice). It also lacks `<format>` (GCC 11).
  - 523adf7b (F64): `string(static_cast<const string&>(name.get()))`. `util/ci-style.sh origin/main` was clean locally before and after; only CI's xsdcxx 4.0 headers trigger the check.
  - fd5a52bd: the Catch2 test comparing `standard_normal` with `gsl_ran_gaussian` allows an absolute 1e-12 (was `!=`). Under `-ffp-contract=off`, against Homebrew's GSL, draw 87938 of 200000 differs by 1.3e-14 (r2 near 1), and the streams stay in step.
  - d29f4df8 (F62): `-ffp-contract=off` for GCC and Clang in `CMakeLists.txt`, next to the Intel `-fp-model strict`. G-exact fails for LifeNet1/2 only, until the next commit.
  - b3bf778f: `test/expected/outputLifeNet{1,2}.txt` regenerated (user approved). Only measure 36 moves, in the last printed digit (13 and 18 rows). The files are byte-identical to the b6-diag outputs of macos-15 and ubuntu-24.04-arm with contraction off.
  - 32f8a5ce: FINDINGS F65, F66.
  - 773f2154 (F61): the workflow sets `CMAKE_PROGRAM_PATH` to setup-python's `bin`. Run 39881602220 at 32f8a5ce passed every job, but the macOS jobs ran only 58 tests: `build.sh` exports `PATH=/usr/bin:$PATH` (F67), so CMake still found Python 3.9 and the new check skipped the statistical tests. Checked locally with `PATH=/usr/bin:$PATH CMAKE_PROGRAM_PATH=/opt/homebrew/bin`.
- Diagnosis of F62:
  - Local macOS build with `-ffp-contract=off`, strict: 54 of 56 scenarios byte-identical, LifeNet1/2 differ, with exactly the sums the Linux x86-64 CI job had printed.
  - b6-diag run 39879605135, a workflow on the scratch branch only: {macos-15, ubuntu-24.04, ubuntu-24.04-arm} × {default, `-ffp-contract=off`}, `run.py -C`, outputs uploaded. Contraction off: macOS = arm64 byte for byte, and x86-64 differs only in printing `-nan` for `nan` (ModelNameModelOptionOverrides output, SimpleMPDTest ctsout; F65). Default flags: arm64 GCC differs from macOS in EffectiveDrug and LifeNet1/2, and x86-64 equals its own contraction-off build. All builds use `-std=gnu++20` (no `CMAKE_CXX_EXTENSIONS OFF`), so GCC's default is `fast`.
  - The branch and its worktree were deleted at the end.
- Gates:
  - G-exact / G-unit: `build-rework` (strict) at b3bf778f: 58 of 58.
  - G-stat: `ctest -L statistical -j 4`: 53 passed, 5 disabled (no reference), as before.
  - G-style: `util/style.sh check 7d115055` clean.
  - G-perf: candidate b3bf778f against 523adf7b (`../om-bench-base`, same flags but without `-ffp-contract=off`): median per-pair ratio 1.0183 (+1.83%), spreads 9.3% / 7.3%, load ~5.3 at the start.
  - CI: at 773f2154, C/C++ CI 39882681731 (macos-14, macos-15, ubuntu-24.04, ubuntu-24.04-arm: 110 of 110 tests each) and Style 39882681637 pass. GitHub Pages also passes.
- Findings: F61–F64 moved to § Closed. New: F65 (NaN in expected outputs; the printed sign depends on the platform), F66 (`scenario12a` has an expected output but no test), F67 (`build.sh` puts `/usr/bin` first on PATH).
- Surprises:
  - FMA, not libm, explained every cross-platform difference.
  - Building only `--target openMalaria` leaves out `scenario_current.xsd` (known; it bit the first local run and the first diag push, 934c20ec, cancelled).
  - Every CI job had stayed green after the stats tests went out of macOS, so the gate's message is the only signal. Check the macOS test count (110) when changing the workflow.
- Follow-ups:
  - F65: if CI should ever compare strictly, the output writers must print NaN one way.
  - F67: making `build.sh`'s PATH export Cygwin-only would change tool choice for users; ask first.
  - G-style can still pass locally and fail in CI on generated XSD types (F64's note).
- Next: D6c.

### D6c: decision-tree library → `DecisionTreeLibrary` owned by `Simulation` (done, 2026-10-23)
- Base: eada1455 (B6 done). The G-perf and A/B base worktree `../om-bench-base` was moved to eada1455 and rebuilt.
- Commits:
  - 38f78eda: `util/ab_scenarios.py` group `decision-trees` (28 variants, below), run base against base first.
  - a7c68da3: **logic.**
    - `Clinical::DecisionTreeLibrary` (`CMDecisionTree.h`) replaces the file-static `decision_library` and `save_decision`. `create` replaces the static `CMDecisionTree::create`; each node's static `create` takes the library. The dedupe search, the comparisons and the build order are unchanged.
    - The library holds what the nodes reached through bridges: the `DiagnosticRegistry` (for `CMDTDiagnostic`), the within-host model's `PkPdParams` (for `CMDTTreatPKPD`'s course lookups, the same functions the `LSTMTreatments` facades call) and the health-system memory (for `CMDTUncomplicated`'s check; `ClinicalModel::hs_memory()` went). The memory is read once, in `ClinicalParams`' constructor, and nothing changes it later.
    - A `Simulation` member declared after the `DiagnosticRegistry` and the within-host model and before `ClinicalParams` (which holds pointers into it), built right after `ClinicalParams`. It reads no XML, so no error moves.
    - `ClinicalParams::set_health_system` takes the library. `TimedChangeHSDeployment` and `DecisionTreeComponent` (in the static `InterventionManager`) reach it through `bridge::mutable_decision_tree_library()` until D8.
    - Every node keeps its new-expression with its arguments in the old order, so which error a tree with two bad parts reports is unchanged on each compiler (F70).
    - `CMDecisionTreeSuite` builds each tree in a library local to `propTreatmentsNReps` (deviation from the plan's suite-wide library). No test relied on a node from an earlier test: no pair of nodes in the suite differs only in a field the dedupe ignores. A suite-wide library would also have kept nodes pointing into a `DiagnosticRegistry` that each `setUp` replaces.
  - 9e34f002 + 6e9038b9: **rename**, 61 by the tool (38 parameter, 10 variable, 7 member, 3 typedef, 1 macro, 1 method, 1 local constant), over `CMDecisionTree.{h,cpp}`, which D6b left out. Map: `Hmod_ESDecisionTree=HMOD_CM_DECISION_TREE`. Collisions reviewed: the local `InfectionOrigin` (it shadowed the enum type; the method of that name is called through `.`) and `CMHostData`'s `member(param)` initialisers.
  - 9cf1ff5a + bb46b13d: **modernise, tool:** 30 `use-override`, 15 `use-nullptr`, 3 `use-using`, 3 `use-equals-default`, 1 `concat-nested-namespaces`, 1 `loop-convert` (over `CMDTMultiple`'s children, which its body does not change), 1 `use-emplace`, 1 `unnecessary-value-param`. No `prefer-member-initializer` (no F58 hazard).
  - c78608b6: **modernise, hand:** `save` takes a `unique_ptr`, which ends F69's leaks. Each node is still built by its new-expression, now owned at once by a `unique_ptr`, rather than by `make_unique`, to keep F70's evaluation order. Also `const auto*` casts, const locals, and `CMDTMultiple() = default`. Left: the nodes' reference members (built once, never assigned; a reference says the child exists) and the dedupe semantics (F68).
  - 0eb65ebb: FINDINGS F68, F70–F72, F69 (closed), and F53's decision-tree part; PLAN.md D6c row as built; README's bridge list and A/B row.
- Gates:
  - G-exact / G-unit: 58/58 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches the 5-day and event-scheduler trees, `changeHS` trees (scenario1, ESChangeHS, EffectiveDrug, Molineaux), decision-tree interventions (MSAT, MSAT_HRP2, TriggeredMSAT, MonitoringAll, Dielmo) and the multiple, caseType, diagnostic, uncomplicated, random, noTreatment, report, treatFailure, treatPKPD, treatSimple and deploy nodes, with checkpoint stop and resume.
  - G-stat: `ctest -L statistical -j4` at c78608b6: 53 passed, 5 disabled (no reference), as after B6.
  - G-style: `util/style.sh check eada1455` clean at c78608b6. Rename: format half clean, second dry run `nothing to rename`, token check passed, `check 9e34f002^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check 9cf1ff5a^` clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate c78608b6, load ~6 at the start: median per-pair ratio 0.9961 (−0.39%), spreads 3.2% / 1.2%. Only tree construction changed; `exec` did not.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base eada1455): 0 of 177 variants (all seven groups) differ after the logic commit, and again after the hand commit. The new group `decision-trees` covers what ctest can't:
    - the infectionOrigin, age-switch and cohort nodes, which no ctest scenario uses, each with a positive control (outputs move against the plain scenario; the age switch's two report numbers count 4629 and 24498)
    - `<severe>` in an event-scheduler complicated tree, and a `<caseType>` under it (F71)
    - the build errors: caseType in a complicated tree and in an intervention tree, a random node's sum, the age switch's first bound and order, an unknown diagnostic and cohort component, treatSimple's negative and malformed durations
    - two errors in one node (F70)
    - the pairs the dedupe merges (F68)
    - Dielmo plain and with `--checkpoint`
  - Planted bugs (then reverted): the infectionOrigin branches swapped → `decision-trees/infection_origin` differs; the `<uncomplicated>` memory check removed → `clinical/dt5_uncomplicated_memory` differs; the check made `>=` → 12 Dielmo-based variants differ. A first plant, `memory > health_system_memory + 1`, moved nothing: it adds one day to a bound of 5-day steps.
- Findings:
  - F68: the dedupe merges nodes that differ in an ignored field.
  - F69: the library leaked nodes; fixed in c78608b6.
  - F70: the reported error depends on the compiler's argument order.
  - F71: `<caseType>` under `<severe>` escapes its check and aborts a Debug build.
  - F72: `<multiple>` drops `screened`, so Dielmo's MSAT records no mass screenings.
  - F53's decision-tree part is settled.
- Surprises:
  - No ctest scenario has an age switch, an infectionOrigin node or a cohort node in a decision tree.
  - `nCMDTReport` counts only the report numbers the scenario monitors. The first age and dedupe variants reported to unmonitored numbers and so showed nothing; they now add the monitoring options.
  - F72 reaches three ctest scenarios: Dielmo's MSAT intervention, and ESTS's and ESChangeHS's event-scheduler trees (7 and 14 such `<multiple>` nodes), put a diagnostic inside `<multiple>` (checked by scanning `test/scenario*.xml`).
- Follow-ups:
  - D8: `DecisionTreeComponent` and `TimedChangeHSDeployment` use `bridge::mutable_decision_tree_library()`. `CMDTCohort` and `CMDTDeploy` still use the static `InterventionManager` (`getComponentId`, `HumanIntervention`). `LSTMTreatments::find_schedule`/`find_dosages` and `Diagnostics::get` remain for the intervention components.
  - The uncompiled `ESCaseManagementSuite.h` (F9) still uses `CMHostData`'s old names (`pgState`, `ageYears`) and constructor.
  - F68, F71 and F72 change outputs or turn scenarios into init errors; they wait for the user. **Fixed after the log entry, with the user's approval; see the next entry.**
- Next: D7.

### D6c follow-up: F68, F71 and F72 fixed, with tests (2026-10-23, after D6c)
The user asked for the three behaviour-changing D6c findings to be fixed. Each fix is its own commit with a test that fails without it. The base for the A/B runs and G-perf is the D6c head 15e8a223 (`../om-bench-base`, rebuilt).
- Commits:
  - ee622593: **F68 fix.** `CMDTUncomplicated`, `CMDTCohort` and `CMDTTreatSimple` compare `memory`, `component` and both duration lists in `operator==`, so the library merges only nodes that behave the same. CxxTest tests in `CMDecisionTreeSuite`: an `<uncomplicated>` pair differing in memory and `<treatSimple>` pairs differing in either duration give two nodes; equal pairs still give one. Without the fix, the memory test and the blood-duration test fail. The cohort pair needs the intervention manager, which the unit tests don't set up; its A/B variant covers it.
  - 5ec95acf: **F71 fix.** A top-level `<severe>` node passes the caller's `is_uc` to its branches, as one inside `<multiple>` already did. A `<caseType>` under `<severe>` in a complicated or intervention tree is now an init error. Test: the uncomplicated tree accepts it, the other is rejected (without the fix both accept). The test uses an explicit try/catch, because clang-tidy on the suite header can't parse CxxTest's `TS_ASSERT_THROWS` ("expected catch").
  - 942a1306: **F72 fix.** `CMDTMultiple::exec` ORs its children's `screened`. Test: a `<multiple>` with a diagnostic reports a screening (without the fix it doesn't), one with only a treatment does not. The test sets the mock's density; left NaN, the diagnostic's draw trips a Debug assert.
  - 137ce408: scenario test scenarioDecisionTreeLibrary (Dielmo plus an `<uncomplicated>` memory pair, a `<treatSimple>` blood-duration pair and `nMassScreenings` monitored), registered in `OM_BOXTEST_NAMES`. New files only, from the fixed build (Release and Debug identical). The build before the fixes differs in four measures: nMassScreenings 0 → 29127, report 90 0 → 460, reports 3 and 14 14660 → 14686 and 472 → 460.
- Deviation: the tests are CxxTest, not Catch2 (M1 says new tests are Catch2). The decision-tree fixtures (clock, diagnostics, PK/PD, a `WHMock` human) exist only in `CMDecisionTreeSuite`; M9 ports the suite.
- Gates:
  - G-exact / G-unit: after each fix 58/58 (strict) on Release and Debug, then 59/59 with the new scenario. No existing `test/expected/` file moves. Of the ctest scenarios with a diagnostic inside `<multiple>` (F72), Dielmo monitors no screening measure, and ESTS and ESChangeHS never reach the hospital branch that holds theirs (one severe case, no complicated-case treatment).
  - A/B (base 15e8a223), cumulative:
    - after F68, 3 of 177 differ: `decision-trees/dedupe_uncomplicated_memory_0_1`, `dedupe_treat_simple_blood_1_5` and `dedupe_cohort_cohort_mda`. Each now differs from the twin it used to equal.
    - after F71, also `es_case_type_under_severe`, which now exits 69 with the caseType error.
    - after F72, also `dedupe_cohort_cohort_cohort` and `dedupe_cohort_mda_mda`, which wrap Cohort's diagnostic tree in `<multiple>`.
  - G-stat: `ctest -L statistical -j4` at 137ce408: 53 passed, 6 disabled (the new scenario has no reference).
  - G-style: `util/style.sh check HEAD` clean before each commit.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate 137ce408 against 15e8a223: median per-pair ratio 0.9988 (−0.12%), spreads 4.0% / 5.1%.
- Findings: F68, F71 and F72 moved to FINDINGS § Closed.
- Next: D7.

### Plan amendment: D7b (2026-10-23, at the start of D7)
- Two global families are in no D chunk: `InfectionIncidenceModel`'s file globals and `ctsNewInfections` counter (`model/Host`, the human side of transmission), and `AgeStructure`'s statics (`model/PopulationAgeStructure`). The user chose to keep D7 to its row (`model/Transmission`: `PerHost`'s statics, plus the follow-ups injecting the transmission RNG and the genotype count) and to add D7b for the two families, with `Human` taking the population RNG by injection (D2's follow-up). Row in PLAN.md § Stage D; § Recommended order row 12 notes it.

### D7: transmission statics → `PerHostParams` owned by `Simulation` (done, 2026-10-23)
- Base: d8c583e9 (D6c's fixes). The G-perf and A/B base worktree `../om-bench-base` was moved to d8c583e9 and rebuilt.
- Scope: the user kept D7 to its plan row and added D7b for the two global families no chunk owned (amendment above, 3de6a425). `model/Transmission` had only three statics: `PerHost::relAvailAge`, `PerHostAnophParams::params` and `PerHostAnophParams::entoAvailabilityPercentiles`. The transmission model itself has been a `Simulation` member since C1.
- Commits:
  - 3de6a425: PLAN.md D7b row and § Recommended order note; the amendment entry above.
  - 31d61953: `util/ab_scenarios.py` group `transmission` (14 variants, below), run base against base first.
  - 8e9e674f: **logic.**
    - `Transmission::PerHostParams` (`PerHost.h`) holds the age availability interpolator, the per-species `PerHostAnophParams` and the availability percentiles. Its constructor is the old `PerHost::init`; `add_species`, `num_species`, `get`, `set_ento_availability_factor`, `calc_availability_percentiles` (same body and `MasterRng(0, 0)`), `ento_availability_percentile` (same error) and `adult_age` replace the static functions.
    - A `Simulation` member with a `PerHostParamsBinding`, declared before the population and the transmission model (both point into it), assigned where `PerHost::init` ran; the percentiles are computed at the same points of `start_new` and `resume`.
    - `create_transmission_model` takes it, the transmission master RNG and the `GenotypeRegistry`. Each species' parameters are added at the same point of the factory loop, before its `AnophelesModel` is built, so every read and error keeps its order. `AnophelesModel::init_availability` sets the availability factor through the reference it is given; each `AnophelesModel` keeps a pointer to the `PerHostParams` (not to its element: the list grows during the loop) for `deploy_vector_trap`, and one to the genotype registry.
    - Each `PerHost` holds a pointer, set by `initialise`. **Bridge:** `bridge::per_host_params()` for `Human` (D7b) and the intervention deployments (`Deployments.h`, D8), removed in D9.
    - Follow-ups done: `VectorModel` seeds its RNG from the injected transmission stream (D2b), and the genotype count comes from the registry (D4d). `TransmissionModel`'s constructor already took a count that it ignored; it now stores it (`num_genotypes()`). `NonVectorModel` passed 1 there while its `update_kappa` used `Genotypes::count()`; it now passes the registry's count, i.e. the value it used before.
    - The adult age of the non-vector model is now evaluated before the factory's first read instead of in the base constructor; only an interpolator never set can throw there, which init rules out.
    - Checkpoint format unchanged: the pointers are not checkpointed and the percentiles are recomputed on resume, as before. Unit tests bind a default `PerHostParams`.
  - 8b583553 + 529d46cf: **rename**, 668 by the tool (202 parameter, 178 variable, 127 member, 77 method, 35 local constant, 24 function, 8 macro, 4 constant parameter, 4 enum constant, 3 constant member, 2 type alias, 1 global constant, 1 struct, 1 protected and 1 public member). Maps: seven acronyms the case conversion split (`initEIR365=init_eir365`, `EIRdaily=eir_daily`, `transientEIRknown=TransientEirKnown`, `SvfromEIR=sv_from_eir`, `SvfromEIR_view=sv_from_eir_view`, `xmlSeqNNHs=xml_seq_nnhs`, `valaverageEIR=val_average_eir`) and four collisions: `fsCoeffic=xml_coeffic` (a local next to `FSCoeffic`), `EIR_i=genotype_eir_i` and `EIR_l=genotype_eir_l` (`calculate_eir`'s outputs against the locals `eir_i`/`eir_l` of its inner loop, V1's follow-up), `m_id=component_id` (`PerHostInterventionData` has a method `id()`). Skip: `checkpointIntervs` (two overloads called from a template, no fix-it). The hand commit renames it and the `#else` branch of `USE_DERIVATIVE`, which the file defines (checked by compiling a copy without the define). Both in `.git-blame-ignore-revs` (5d117152). The uncompiled `AnophelesModelSuite` and `MosqLifeCycleSuite` (F9) keep the old names.
  - f4c2f44c + e1f80857: **modernise, tool:** 19 `loop-convert`, 19 `use-override`, 14 `use-default-member-init`, 8 `concat-nested-namespaces`, 6 `init-variables`, 6 `pro-type-member-init`, 5 `deprecated-headers`, 5 `use-equals-default`, 4 `unnecessary-value-param`, 3 `prefer-member-initializer`, 2 `use-ranges`, 1 `use-nullptr`, 1 `unnecessary-copy-initialization`. Checked in the dry run: no converted loop changes its container; the member-initializer moves contain no throwing read (F58).
  - acec99a5: **modernise, hand:** the four factories return `unique_ptr`; `AnophelesModel` gets a virtual destructor (F74) and `TrapParams` the rule of zero; 19 `std::bind` callbacks → lambdas; `init_vector_trap` by `const&`; `VecModeErr` `constexpr`; `misc-const-correctness` (west const); `static_cast`s and explicit narrowing casts; designated initializers. Left: CalcNvO's GSL code (M5), the index narrowings into `mod_nn`/`modulo_steps` (int helpers), the GSL root-solver globals and `USE_DERIVATIVE` (M6), `SimulationMode` (checkpointed, H3) and `VecStat` (ctsout only, G), `ostringstream` titles (G), `endl` (F14), public data.
  - d5761b35: G-style on lines the chunk changed: `AnophelesModel.h` includes `Genotypes.h` instead of forward-declaring into the shared namespace `WithinHost`; one local in `Deployments.h`.
  - e18142b8: FINDINGS F73, F74; PLAN.md D7 row as built; README's bridge list and A/B row.
- Gates:
  - G-exact / G-unit: 59/59 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches vector (one to three species, simple-MPD, exact Nv0 solver, traps, availability filter) and non-vector models, `scaledAnnualEIR`, genotypes in both modes, all with checkpoint stop and resume.
  - G-stat: `ctest -L statistical -j4` at d5761b35: 53 passed, 6 disabled (no reference), as after D6c's fixes.
  - G-style: `util/style.sh check d8c583e9` clean at d5761b35. Rename: format half clean, second dry run `nothing to rename` after the hand rename, token check passed, `check 8b583553^` clean. Modernise tool commit: format half clean, second dry run applies nothing (only the two no-fix-it items, done by hand), `check f4c2f44c^` clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate d5761b35, load ~6.9 at the start: median per-pair ratio 1.0151 (+1.51%), spreads 8.0% / 5.1%. The benchmark is a vector scenario, so it covers the per-human availability lookups that now go through a pointer.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base d8c583e9): 0 of 191 variants (all eight groups) differ after the logic commit and again after the hand commit. The group `transmission` covers: checkpoint bytes of AvailabilityFilter (three species); `--print-EIR` stdout, vector and non-vector; the `VECTOR_LIFE_CYCLE_MODEL` error, and `VECTOR_SIMPLE_MPD_MODEL` with non-vector data; `PerHostAnophParams`' availability error in the first and the second species and together with the life-cycle error (read order); an availability percentile above 100; the Gaussian copula deployment moved into the run (outputs differ from the plain scenario, F73), with `--checkpoint`, and with two species (its error).
  - Planted bugs (then reverted): a species' parameters read only after its `AnophelesModel` → `transmission/mass_action_het_and_life_cycle` differs; the percentiles not computed on resume → ctest AvailabilityFilter fails and `vector_checkpoint`, `copula_deployed_checkpoint` differ.
- Findings: F73 (the copula ctest scenarios never deploy), F74 (closed: `SimpleMPDAnophelesModel` deleted through a base pointer without a virtual destructor).
- Surprises:
  - The two copula ctest scenarios end before their only deployment, so the copula code had no test until the A/B variants.
  - `VECTOR_LIFE_CYCLE_MODEL` is only an error ("not yet implemented"); no scenario can use it.
  - `TransmissionModel` took a genotype count and ignored it; the non-vector model's "1" there had no effect.
- Follow-ups:
  - D7b: `Human` uses `bridge::per_host_params()` and `bridge::master_rngs()`; `InfectionIncidenceModel` and `AgeStructure`.
  - D8: the intervention deployments (`Deployments.h`) use `bridge::per_host_params()`; GVI, IRS and ITN were touched only by the rename (`component_id`).
  - F73: moving the copula scenarios' deployment into the run changes their expected outputs; ask the user. **Done after the log entry as new scenarios, at the user's request; see the next entry.**
- Next: D7b.

### D7 follow-up: F73 checked, and scenario tests that run the Gaussian copula (2026-10-23, after D7)
The user asked to double-check F73 and, if it held, to add a test with the fix, compare it with the original, check that the change makes sense, and add it as a test the refactor must pass.
- Check: F73 holds, more strongly than first written. The deployment (`time="2030-01-01" repeatStep="3y" repeatEnd="2020-01-01"`) creates no deployment time at all: `InterventionManager`'s expansion is `while (date < end)`, and 2030 is after 2020. Each original scenario's output is byte-identical to the same scenario with the `<deployment>` deleted, and to its expected file; the deploy line has been this way since upstream added the copula (bc128719, #406).
- Commits:
  - 2e3ff85e: scenarioGaussianCopulaGammaDeployed and scenarioGaussianCopulaLognormalDeployed (new files; the originals untouched): deployment at 2005-01-01, five rounds to 2017, `nMassGVI` monitored. In `OM_BOXTEST_NAMES` (checkpoint stop and resume). Expected outputs from the D7 base d8c583e9 (`../om-bench-base`); the D7 head's Release and Debug builds give the same bytes. The covered count is 232 and 236 over five rounds.
  - 3409b9e9: `util/copula_check.py`, the plausibility check below, rerunnable from the repo.
- Comparison with the original and plausibility (`util/copula_check.py build-rework/openMalaria GaussianCopulaGammaDeployed GaussianCopulaLognormalDeployed`, seeds 2001–2020, four variants: no deployment, the scenario as is (copula, correlation 1), the copula at correlation 0, plain coverage 0.5):
  - before the first deployment, every variant's output equals the no-deployment output for every seed, i.e. the original scenario's behaviour;
  - covered per round: 48.9, 49.2, 50.3 (Gamma) and 49.3, 50.6, 50.6 (Lognormal) of 100, matching coverage 0.5;
  - after 2005, against no deployment: simulated EIR −96%, infections −77%, uncomplicated episodes −80% with the copula at correlation 1 (Lognormal −96%, −80%, −84%); plain coverage −75%, −39%, −41% (−82%, −51%, −53%); copula at correlation 0 −65%, −29%, −29% (−77%, −38%, −38%);
  - this is what the method should do: at correlation 1 a human's probability of receiving the GVI is the Beta(1/3, 1/3) quantile of their availability rank, so the GVI goes to the most-bitten people and protects more than random coverage of the same size; at correlation 0 each human's copula sample is drawn once and kept, so the same people are covered every round, which is less effective than plain coverage, which redraws each round.
- Gates:
  - G-exact / G-unit: 61/61 (strict) on `build-rework/` (Release) and `build-debug/` (Debug).
  - Planted bugs (then reverted): the correlation ignored (`yy = 0.0 * xx + …`) and the copula reading `anoph_ento_availability[0]` (after the availability factor) instead of the raw sample: both new tests fail, the two original copula tests pass.
  - G-stat: the new scenarios' `stat_` tests are disabled (no reference), as for the other scenarios added since S5.
  - No model code changed, so G-style and G-perf do not apply.
- Findings: F73 moved to § Closed with the corrected description and the evidence. New: F75 (the copula's per-human sample is keyed by the sub-population, not the component, so whole-population copula deployments of different components share it; not changed).
- Next: D7b.

### D7b: infection incidence and age structure statics → `Simulation` (done, 2026-10-23)
- Base: e55b3741 (D7's F73 follow-up; same C++ as d5761b35). The G-perf and A/B base worktree `../om-bench-base` was moved to e55b3741 and rebuilt.
- Scope: the plan row (added at the start of D7): `InfectionIncidenceModel`'s 11 file globals and its `ctsNewInfections` counter, `AgeStructure`'s statics (and the file-global function pointer its GSL fit called through), and `Human` taking the population master RNG and the `PerHostParams` by injection (D2's and D7's follow-ups).
- Commits:
  - f844263a: `util/ab_scenarios.py` group `host` (19 variants, below), base against base first.
  - 99a752ec: **logic.**
    - `Host::InfectionIncidenceParams` (`InfectionIncidenceModel.h`), a `Simulation` member built where `InfectionIncidenceModel::init` ran, from the `Parameters` and the `ModelOptionFlags` (init read the options through the bridge; same values). Its constructor is the old init body: parameters 16, 5, 1, 3, 2, 4, then the LogNormal NaN error or the heterogeneity-workaround warning. `Kind` replaces the three exclusive `opt_` flags with their old precedence; `create` replaces `createModel` and returns `unique_ptr`; each per-human model holds a pointer. Default-constructed params have the zero/false values of the never-initialised globals, which the unit tests' humans use.
    - `Host::NewInfections`, a `Simulation` member: `Host::update` passes it to `report_num_new_infections`; checkpointed at the old position (after neonatal mortality), so the format is unchanged; the "new infections" ctsout callback is registered right after the params are built, capturing the member.
    - `AgeStructure` is an instance class and a `Simulation` member built where `AgeStructure::init` ran; the fit's statics and `wCalcRSSFunc` became a file-local `DemographyFit` passed to GSL through its `params` pointer. Same arithmetic and the same final re-evaluation at the minimiser's point.
    - `Human(SimTime, const HumanParams&, util::MasterRng&)`, `HumanParams` holding the `PerHostParams` and `InfectionIncidenceParams`; member order unchanged, so its four streams are seeded in the old order. `Population` holds them with the `AgeStructure`. `Human` was `bridge::master_rngs()`'s last user, so the bridge and `MasterRngsBinding` were removed. The unit tests pass their static `MasterRngs(0)` directly (same draws, F30).
  - 6ac27849 + 4eaa9225: **rename**, 62 by the tool (28 parameter, 17 variable, 7 method, 4 member, 2 header guards, 2 function, 1 constexpr variable, 1 local constant). Map: `m_cumulativeEIRa=cumulative_eir_a` (the tool gave `cumulative_ei_ra`). Collisions reviewed: `expected_num_infections` and `cum_age_prop`, each in a different function or class. No uncompiled suite uses the module.
  - e1c53c02: **logic**, members of `InfectionIncidenceParams` declared in read order. The modernise dry run showed `prefer-member-initializer` moving the six throwing parameter reads into the initializer list, which runs in declaration order; `x_star_p_inv` (4) was declared before `e_star_inv` (2), so a scenario missing both would have reported 4 instead of 2 (F58's hazard).
  - 13d177cc + 804c0268: **modernise, tool:** 9 `prefer-member-initializer`, 8 `use-override`, 4 `use-equals-default`, 3 `init-variables`, 2 `use-default-member-init`, 1 `concat-nested-namespaces`. Run with `--passes 12`: each member-initializer move overlaps the next, so the tool takes one per pass, and the default 5 passes left four (that first output was reverted, not committed).
  - 9f0a66fe: **modernise, hand:** `misc-const-correctness` (24 locals, west const); the tool's `= NAN` followed by an assignment → one const initialisation (twice); the effectiveEIR error's `ostringstream` → `std::format` with `{:g}` (same text; no scenario can reach it); `static_cast`s for a C cast and a narrowing; `Kind` on `std::uint8_t`. Left: the GSL minimiser's allocations (M5/M6), protected data, the warning's `endl` (F14), `operator&` (H3), `scnXml::Demography` (E).
  - 2a18091d: G-style on lines the chunk changed: `params` private behind a protected `params()`, `Human`'s constructor parameter `date_of_birth`, `Population`'s `target_pop` and casts, `emplace_back`, designated initializers, `make_unique` in the unit tests.
  - b38f88e7: FINDINGS F76, F77; PLAN.md D7b row as built; README's bridge list (`util/random.h` gone) and A/B row.
- Gates:
  - G-exact / G-unit: 61/61 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches the default, LogNormal and TRIPLE_HET heterogeneity-workaround incidence models, `NO_PRE_ERYTHROCYTIC`, `VACCINE_GENOTYPE`, the "new infections" ctsout column (11 scenarios), and the age structure in every scenario, all with checkpoint stop and resume.
  - G-stat: `ctest -L statistical -j4` at 2a18091d: 53 passed, 8 disabled (no reference), as after D7's F73 follow-up.
  - G-style: `util/style.sh check e55b3741` clean at 2a18091d. Rename: format half clean, second dry run `nothing to rename`, token check passed, `check 6ac27849^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check 13d177cc^` clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate 2a18091d, load ~3.5 at the start: median per-pair ratio 1.0157 (+1.57%), spreads 12.0% (base) / 4.2%.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base e55b3741): 0 of 210 variants (all nine groups) differ after the logic commit, after the hand commit and after the G-style commit. The group `host` covers: `TRANS_HET`, `COMORB_TRANS_HET` and `TRANS_TREAT_HET` (the heterogeneity workaround and its warning; ctest has only `TRIPLE_HET`); the LogNormal parameter-16 error; the negative binomial model, which no ctest scenario uses, with scenario5's parameter 16 (F76's error) and with parameter 16 = 2 and `--checkpoint`; checkpoint bytes of scenarioNoInterv, which reports "new infections"; each incidence parameter dropped alone and each consecutive pair in the read order; an altered demography table.
  - Planted bugs (then reverted): parameters 3 and 2 read in swapped order → `host/incidence_no_params_3_2` differs; the counter not checkpointed → `host/lognormal_new_infections_checkpoint` and `host/neg_bin_checkpoint` differ, ctest passes (below); the counter not reset at the start of the main phase → 10 ctest scenarios fail; the fit's final re-evaluation at the minimiser's point skipped → 51 ctest scenarios fail and `host/demography_altered` differs. A fifth plant, restoring the age-group percentages after each fit evaluation, changed nothing (F77).
- Findings: F76 (negative binomial with a small parameter 16 reported as a code error), F77 (the fit's in-place update has no effect).
- Surprises:
  - ctest cannot see the counter's checkpointing: the only checkpoint is written right after the counter is reset at the start of the main phase, so it always holds 0 there. The A/B checkpoint-bytes variants do see it.
  - The modernise tool's default 5 passes were not enough for a constructor with nine member assignments.
- Follow-ups:
  - D8: the intervention deployments (`Deployments.h`) still use `bridge::per_host_params()`.
  - D9: `Human`'s constructor still reaches the within-host model (`WHInterface::create_within_host_model`), the clinical params (`bridge::clinical_params()`) and the model options (`hetSample`) through their bridges; `mon::Continuous`'s callback registry is still global.
  - Sweep before F0: `Human` and `Population` are not renamed or modernised (their constructors changed here; the rest is untouched).
  - E: F76.
- Next: D8.

### D8: static `InterventionManager` → a `Simulation` member (done, 2026-10-23)
- Base: 49e756fb (D7b done; same C++ as 2a18091d). The G-perf and A/B base worktree `../om-bench-base` was moved to 49e756fb and rebuilt.
- Scope: every static of `model/interventions`, not only the manager's: the manager's six (`identifierMap`, `humanComponents`, `continuous`, `timed`, `nextTimed`, `importedInfections`), the global `removeAtIds`, the ITN, IRS and GVI `componentsByIndex` registries, and `VaccineComponent::params` and `reportComponent`. Any of them left static would break G-two-sims (D9). Plus the D5, D6c and D7 follow-ups for this module: inject the `PerHostParams`, the decision-tree library and the clinical params, and stop using the `Diagnostics` and `LSTMTreatments` facades.
- Commits:
  - da06b67a: `util/ab_scenarios.py` group `interventions` (35 variants, below), base against base first. Positive controls: each behaviour variant moves outputs against its plain scenario; the vaccine report component shows as 0 EPI and mass vaccinations with TBV declared first and not deployed, 19 and 149 with PEV declared first.
  - 12449692: **logic.**
    - `interventions::InterventionManager` is an instance class and a `Simulation` member with an `InterventionManagerBinding`, declared after everything it points into (`PerHostParams`, `DiagnosticRegistry`, within-host model, `DecisionTreeLibrary`, `ClinicalParams`) and before the population, whose humans point into its components. `init` runs at the old point with the same body, so every XML read and error keeps its order. It stays a member function rather than a constructor: decision-tree cohort and deploy nodes, and treatments' triggered deployments, look components up through the bridge while `init` runs, so the bridge must point at the object being filled.
    - `removeAtIds` is a member (`remove_at_ids(code)`); the global was never cleared, and one simulation per process filled it once, so the member holds the same lists.
    - The per-human ITN, IRS and GVI parts and each `PerEffectPerHumanVaccine` hold a pointer to their component, set at deployment and, on checkpoint load, by `make_human_part` (which already had the component) or, for vaccines, through the bridge. Same lookups per human and step, through a member pointer instead of a static vector. `reportComponent` (the first vaccine component configured) became a flag the manager passes to the first `VaccineComponent` it builds (`std::exchange` on a local).
    - Injected: the `PerHostParams` (human deployments, addNonHumanHosts), the `ClinicalParams` and `DecisionTreeLibrary` (changeHS, decision-tree component), the `DiagnosticRegistry` (screen) and the `PkPdParams` (treatPKPD). Inside the module the manager is passed explicitly to `HumanIntervention` and `TriggeredDeployments`.
    - **Bridge:** `bridge::intervention_manager()` (const) for the modules D8 does not own: `Human::removeFirstEvent`, `PerHost::has_active_interv` and its checkpoint load, `CMDTCohort`, `CMDTDeploy`, `Treatments` (triggered deployments) and `mon::initCohorts`; and for the vaccine checkpoint load. Removed in D9. Bridges and facades that lost their last user went: `bridge::per_host_params()` with `PerHostParamsBinding`, `bridge::mutable_decision_tree_library()` with `DecisionTreeLibraryBinding`, `bridge::mutable_clinical_params()` (the clinical params bridge is const now), `Diagnostics::get`, and the `LSTMTreatments` facades (header deleted; `LSTMTreatments.cpp` keeps the `CourseRegistry` definitions).
    - Checkpoint format unchanged: the pointers are not written, and the manager is passed to the checkpoint functions and checkpointed at its old position. The unit tests bind a default manager (empty removal lists, as the global was there).
  - 2cee6da6 + 0ed86df8: **rename**, 374 by the tool (120 parameter, 109 member, 74 variable, 35 method, 15 local constant, 12 enum constant, 3 namespace, 2 header guards, 2 private member, 1 global constant, 1 function). Maps, each for a collision: `m_id=component_id` and `m_duration=cohort_duration` (methods `id()` and `duration()`), `removeAtIds=remove_at_lists` (method `remove_at_ids()`), `_relativeAttractiveness=rel_attractiveness` and `_fecundityEffect=rel_fecundity_effect` (IRS methods of those names; the new names match ITN's members), `Component=component_types` and `Vaccine=vaccines` (the tool's `component`/`vaccine` are the per-human parts' component pointer and `Human::vaccine`; `components` is `HumanIntervention`'s member). Skip: `checkpointHet` (overloads called from a template, no fix-it), renamed in the hand commit.
  - 118b811e: **logic**, `HumanITN`'s members built in draw order. The modernise dry run showed `prefer-member-initializer` moving the disposal-time and initial-insecticide draws into the initializer list, ahead of the handling draw in the constructor body, which would change every ITN deployment's draws (F58's hazard, with random draws instead of throwing reads). The constructor now draws the handling sample and delegates to a private constructor that builds every member in its initializer list; members are built in declaration order, so the draws stay handling, attrition, insecticide.
  - da9bfd0f + 7bdf5f73: **modernise, tool** (`--passes 12`): 77 `use-override`, 33 `loop-convert`, 10 `concat-nested-namespaces`, 8 `use-nullptr`, 8 `use-default-member-init`, 7 `unnecessary-value-param`, 7 `use-using`, 6 `pro-type-member-init`, 5 `use-ranges`, 4 `use-equals-default`, 4 `init-variables`, 4 `unnecessary-copy-initialization`, 2 `use-emplace`, 1 `prefer-member-initializer` (ITN's rip factor, a required XML element that cannot throw), 1 `make-shared`. Checked: no converted loop changes its container, and the deploy-time map still stores the XML elements' addresses.
  - 9f5e01f4: **modernise, hand:** `misc-const-correctness` (west const; not on the lines that read ITN's parameter unions); owning `new` → `make_unique` (timed deployments, components, per-human parts) and `unique_ptr` for the changeHS/changeEIR XML clones; `use-auto` on casts; public deleted copy and move operations; `VecModeErr` `constexpr`; the global comparator gone; explicit casts for narrowing; the vaccine load path's `dynamic_cast`; `checkpointHet` → `checkpoint_het`; `DeterrencyModel` an `enum class` on `std::uint8_t`; `std::format` for four messages (two more became literals). Left, with reasons in the commit message: ITN's parameter unions, the three namespace-scoped enums used as indices or orderings, `endl` (F14), public data, the intentional `SubList::deploy` shadowing, multi-condition `ostringstream` messages, the `#if 0` R_0 code.
  - e894fa4f: G-style on lines the chunk changed: `HumanDeploymentBase`'s `PerHostParams` pointer private behind a protected `per_host_params()`; `Human::removeFirstEvent`'s loop and locals; `mon/init.cpp`'s `comp_id`.
  - 9e31e86b: FINDINGS F78, F79, F53 closed; PLAN.md D8 row as built; README's bridge list and A/B row.
- Gates:
  - G-exact / G-unit: 61/61 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit. ctest reaches every component type but `clearImmunity`, timed, continuous, cumulative and sub-population deployments, all three removal options together, changeHS, changeEIR, uninfectVectors, vectorPop, non-human hosts, traps, imported infections and decision-tree interventions, with checkpoint stop and resume.
  - G-stat: `ctest -L statistical -j4` at e894fa4f: 53 passed, 8 disabled (no reference), as after D7b.
  - G-style: `util/style.sh check 49e756fb` clean at e894fa4f. Rename: format half clean, second dry run `nothing to rename` (bar the skipped `checkpointHet`), token check passed, `check 2cee6da6^` clean. Modernise tool commit: format half clean, second dry run applies nothing (only the two no-fix-it items, done by hand), `check da9bfd0f^` clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate e894fa4f, load ~1.9 at the start: median per-pair ratio 0.9994 (−0.06%), spreads 4.1% (base) / 2.1%.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base 49e756fb): 0 of 245 variants (all ten groups) differ after the logic commit, the tool commit, the hand commit and at e894fa4f. The group `interventions` covers: `--print-interventions` (MonitoringAll, 2ITNs, NonHumanHosts, TrapTest) and `--verbose` output; checkpoint bytes of MonitoringAll, 2ITNs and scenario12 (changeEIR); which vaccine component reports deployments; `clearImmunity`; a deployment triggered by a treatment; each removal option alone; `complement="false"`; continuous availability filters; a decision-tree cohort node naming its own component; a deployment after the last survey (warning); and the init errors (duplicate and unknown ids, an unknown cohort, repeatStep without repeatEnd and of 0, a duplicate date, a date before the start, an unknown cumulative-coverage component, target ages 0 and above the maximum, missing vaccine efficacy, a vector component with the non-vector model, an unknown triggered component, decision-tree cohort and deploy nodes naming a component defined later or themselves).
  - Planted bugs (then reverted): every removal code reading the onFirstBout list → the three `interventions/sub_pop_removal_*` variants differ; every vaccine component reporting → both `interventions/vaccine_report_*` differ; `initial_insecticide` declared before `disposal_time` → the 8 ctest scenarios that deploy ITNs fail.
- Findings: F78 (no test reaches the checkpoint read of intervention state; D8's changes there are covered by review only), F79 (ITN's holeIndexMax "required" checks test the value, not presence). F53 closed: the intervention components now live in a `Simulation` member declared after the registries they point into.
- Surprises:
  - The only checkpoint is written before any intervention is deployed, so the per-human intervention and vaccine checkpoint read paths, and the changeHS/changeEIR redeploy on resume, never run in any test (F78).
  - `modernize-loop-convert` named a loop variable over `continuous` `continuou`.
  - A `git add` that named the already-staged deleted header failed, so the first logic commit held only the deletion; it was amended before anything else was committed.
- Follow-ups:
  - D9: the `bridge::intervention_manager()` users above; `Human` and `PerHost` could hold the manager (or the component type in each per-human part) instead.
  - H3: a checkpoint round-trip test with deployed interventions (F78).
  - E: F79; `TimedChangeHSDeployment` and `TimedChangeEIRDeployment` still hold `scnXml` clones (E1, E2).
  - Sweep before F0: `Human`, `Population` and `mon/` are not renamed or modernised; D8 touched a few of their lines.
- Next: D9.

### D8 follow-up: F79 fixed, with a test (2026-10-23, after D8)
The user asked for F79 to be fixed (and agreed that F78's round-trip test belongs in H3). The base for the A/B runs and G-perf is the D8 base 49e756fb (`../om-bench-base`), so the A/B shows the fix's change and nothing else.
- Commits:
  - 0b7567cf: `util/ab_scenarios.py`, three LifeNet2 variants (the only ctest scenario with logit ITN effects) in group `interventions`: holeIndexMax removed (`itn_logit_no_hole_index_max`, the "required" error, which must stay), 1e-300 (`itn_hole_index_max_tiny`, runs) and 0 (`itn_hole_index_max_zero`, rejected as missing on the base build). Base against base first.
  - 8d9a79a6: **fix.** The four "holeIndexMax required when using logit …" checks test `hole_index_max_present` instead of `hole_index_max == 0.0`. `init_two_stage` already took the flag and never read it; its default argument is dropped (the one caller passes it).
  - 0f28d92a: scenario test ITNHoleIndexMaxZero (LifeNet2 with holeIndexMax 0), in `OM_BOXTEST_NAMES`. New files only, expected output from the fixed build (Release and Debug identical). Without the fix it fails: the run stops with the error.
- Why 0 is valid: the logit models use `min(log(holeIndex + 1), log(holeIndexMax + 1))`, so 0 caps the hole term at 0 and holes have no effect; nothing divides by the value. Check: LifeNet2 with holeIndexMax 0 on the fixed build gives byte-identical `output.txt` to LifeNet2 with 1e-300 on the base build (and on the fixed build), and differs from plain LifeNet2. Rerun: `util/ab_scenarios.py` group `interventions` runs the 0 and 1e-300 variants; compare their candidate outputs with `--keep`.
- Gates:
  - G-exact / G-unit: 62/62 (strict) on Release and Debug with the new scenario; no existing expected file moves.
  - A/B (base 49e756fb): 1 of 248 variants differs, `interventions/itn_hole_index_max_zero` (exit 69 with the error before, runs after).
  - G-stat: `ctest -L statistical -j4` at 0f28d92a: 53 passed, 9 disabled (the new scenario has no reference).
  - G-style: `util/style.sh check HEAD` clean before the fix commit.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate 0f28d92a: median per-pair ratio 1.0002 (+0.02%), spreads 6.3% (candidate).
- Findings: F79 moved to FINDINGS § Closed.
- Next: D9.

### Plan amendment: D9 split (2026-10-23, at the start of D9)
D9 (monitoring state into `Simulation`, remove every bridge, G-two-sims) is far beyond one session. An inventory at b8c20803: the clock bridge (`Sim::` accessors that read the clock) has 371 call sites in 54 files, many on per-infection hot paths; Genotypes 28 sites in 7 files (`is_hrp2_deficient` per infection per day); the other six bridges about 60 sites in about 30 files; the monitoring record functions about 160 sites in 26 files. Of the remaining process-wide state only `mon::internal::runtime` and `mon::Continuous` break simulations run one after another (F81); the bridges' bindings restore the previous pointer, so they only break simulations alive at the same time. Split, with the user's approval of the plan: D9a (the two monitoring globals, sequential G-two-sims, `model/mon` rename and modernisation), D9b (cold bridges), D9c (Genotypes, Diagnostics), D9d (D9a's monitoring bridge), D9e (clock), D9f (remove the bridge namespace, interleaved G-two-sims). Rows in PLAN.md.

### D9a: monitoring state → `Simulation`, and G-two-sims for simulations run one after another (done, 2026-10-23)
- Base: b8c20803 (D8's F79 follow-up). The G-perf and A/B base worktree `../om-bench-base` was moved to b8c20803 and rebuilt.
- Commits:
  - 95266baa: `util/ab_scenarios.py` group `monitoring` (24 variants, below), base against base first (0 of 24 differ). The first try of the non-vector ctsout variant edited scenario5's intervention `<continuous>` and failed XSD; it uses scenario1's monitoring `<continuous>` instead.
  - a5f91dd1: **G-two-sims driver**, registered disabled. `test/two_sims.cpp` (target `openMalaria-two-sims`) runs one `Simulation` per group of command-line arguments separated by `--next`, one after another, as `main()` does for one; `test/two_sims.py` byte-compares each one's `output.txt` and `ctsout.txt` with `test/expected/` (a missing ctsout where one is expected fails). ctest `two_sims` (label `two-sims`): NoInterv, 1, TriggeredMSAT, TriggeredMSAT, 9, 1, SimpleMPDTest (vector then non-vector ctsout, a condition scenario twice and then one without its measure, survey counts both ways, duringInit). On the base code (Release): NoInterv then 1, and 1 then 1, segfault in the second run; TriggeredMSAT then 9 stops with "no valid option selected in a switch statement" (exit 83); 9 then TriggeredMSAT and 9 then 5 give different output; TriggeredMSAT twice and 5 then 9 pass (F81).
  - 2560b4a4: **logic.**
    - `mon::internal::RuntimeState` and its global → `mon::MonitoringState`, a `Simulation` member with a `MonitoringBinding`, declared before the clock (`Clock::init` → `readSurveyDates` fills it). The free functions keep their signatures and take a local reference from `bridge::monitoring()`; D9d passes it instead. Defaults are the global's, so a new `Simulation` starts as a new process did; conditions no longer accumulate.
    - `mon::Continuous` (a namespace with six file globals) → a class with the same members and defaults (`streamOffset` 0, as the zero-initialised global), a `Simulation` member declared before the population and the transmission model. Its stream closes with the `Simulation`. Passed where callbacks are registered (`Simulation`'s "new infections", the population callbacks, `TransmissionModel` and `VectorModel` through `create_transmission_model`) and to the checkpoint functions, which call it at its old position: registrations, XML reads and errors keep their order, and the checkpoint format is unchanged. No bridge.
    - Unit tests bind a default `MonitoringState`. `two_sims` enabled.
  - 442d4c81 + 39a676fc: **rename**, 152 by the tool (44 parameter, 39 function, 23 member, 18 variable, 13 local constant, 6 constexpr variable, 4 header guards, 4 method, 1 namespace `Deploy`). Maps: `ctsOStream=cts_stream` (the tool gave `cts_o_stream`), `_f=wrapped` (the prefix strip gave `f`, the name of the parameter its lambda captures). Skips: `writeRows`, `writeBinaryValue`, whose calls inside a generic lambda get no fix-it (F82); renamed in the hand commit. Collisions reviewed: namespace `deploy` beside the `deploy()` methods (a name before `::` finds only namespaces and types); `Human`'s own `cohortSet`/`updateCohortSet` keep their names.
  - 05f577dc + 8313fa51: **modernise, tool:** 9 `unnecessary-value-param`, 5 `concat-nested-namespaces`, 3 `use-ranges`, 2 `loop-convert`. Checked: neither converted loop changes its container; the ranges sort orders by output number, which is unique.
  - 5255cf06: **modernise, hand:** the two skipped renames and the loop variable `sub_pop`; `deploy::Method` → `enum class DeployMethod : std::uint8_t` at its 34 uses (not checkpointed or from XML); `Defs`, `is_valid_condition`'s list and the output.bin magic → `std::array`; designated initializers for `Condition` and `SurveyDate`; explicit casts for the output column number (same arithmetic, the cohort term still a `uint32_t` product); `std::cmp_greater` for the cohort bound; `ValueType` on `std::uint8_t`; `misc-const-correctness` (west const; not the checkpoint's `stored_size`, which the read instantiation writes). Left: designated initializers for the 90-row `Defs` table (rows read as columns), `Dim` (bit flags), `write_binary_value`'s `reinterpret_cast`, the `SurveyTime` slicing (E), `endl` (F14).
  - 7dddb242: G-style on lines the chunk changed: `registerContinousPopulationCallbacks` → `register_continuous_population_callbacks` (`Population` is not yet renamed).
  - 1d5dcf74: FINDINGS F80, F82, F81 closed; PLAN.md D9 split and D9a as built; README's bridge list, G-two-sims row and A/B row.
- Gates:
  - G-exact / G-unit: 63/63 (62 strict scenarios and unit suites, plus `two_sims`) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit.
  - G-two-sims (sequential): `two_sims` passes on Release and Debug from 2560b4a4. Planted bugs (then reverted): `conditions` a static member again → the run stops at scenario 9 (exit 83); the ctsout stream a static member (never closed between simulations) → three ctsout files missing.
  - G-stat: `ctest -L statistical -j3` at 7dddb242: 53 passed, 9 disabled (no reference), as after D8's F79 follow-up.
  - G-style: `util/style.sh check b8c20803` clean at 7dddb242. Rename: format half clean, second dry run `nothing to rename` (with the two skips), token check passed, `check 442d4c81^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check 05f577dc^` clean. The new driver (`test/`, outside the gate's paths) is clang-format and clang-tidy clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate 1d5dcf74 (same C++ as 7dddb242), load ~1.3 at the start: median per-pair ratio 1.0061 (+0.61%), spreads 2.1% (base) / 1.3%.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base b8c20803): 0 of 272 variants (all eleven groups) differ after the logic commit and at 7dddb242; the `monitoring` group also 0 of 24 after the hand commit. The group `monitoring` covers: ctsout with `duringInit` across a checkpoint (SimpleMPDTest) and `duringInit` on (MonitoringAll); a `--ctsout` file name; the continuous-output errors (unknown option, `N_v0` in a non-vector scenario, period 0); `--print-survey-times` plain and with repeated and an unreported last survey (the warning); the survey-time errors (repeatStep without repeatEnd, repeatStep 0, a bad date); the survey-option errors (unknown, obsolete, duplicate output number, an unsupported category); binary and compressed output; two cohorts across a checkpoint and the two cohort errors; TriggeredMSAT's condition across a checkpoint, on an unreported measure (the hidden condition store) and its two errors.
- Findings: F80 (a condition's value is not checkpointed; cannot matter with today's single checkpoint), F81 (closed: process-wide monitoring state), F82 (the rename tool and calls in generic lambdas).
- Surprises:
  - The base code crashed outright with two simulations in one process; the dangling ctsout callbacks were the first failure, before any survey-state difference showed.
  - `misc-const-correctness` proposed `const` for a variable that one instantiation of a template (the checkpoint read) writes; the build caught it.
- Follow-ups:
  - D9b–D9f as split above; D9d also removes D9a's `MonitoringBinding`.
  - H3: F80 if checkpoints are written later in a run; a G-two-sims case with checkpoint and resume in the same process.
  - Sweep before F0: `Human` and `Population` are still not renamed or modernised.
- Next: D9b.

### Plan amendment: D9b split (2026-10-23, at the start of D9b)
An inventory at 09eb5134 put D9b at about 35 files of non-mechanical change. The object bridges (`InterventionManager`, `ClinicalParams`, `WithinHostModel`) interlock: `Human` reaches all three, and the health-system setup (`ClinicalParams::set_health_system` → `ImmediateOutcomes`/`DecisionTree5Day` → `WHInterface::add_treatment` → `TreatmentRegistry` → `TriggeredDeployments`, which takes `bridge::intervention_manager()`) needs the within-host model and the manager together. The flag bridges are independent of them: `ModelOptions::option` (20 reads in 8 files), the `CommandLine` statics (15 reads in 9 files) and `bridge::infant_mortality()` (one read, in the survey writer). UnitParse's three `DeprecationWarnings` reads (32 callers in 11 files) and `AgeGroupInterpolation::outputSamples`' `SampleInterpolations` read the clock too, so D9e has to thread a context through their callers anyway. Split: D9b1 (flags and infant mortality), D9b2 (object bridges, with the rename and modernisation of `Human` and `Population`); the two clock-reading `CommandLine` users and the removal of the CommandLine bridge move to D9e. Rows in PLAN.md.

### D9b1: model options, command-line reads and infant mortality → passed in (done, 2026-10-23)
- Base: 09eb5134 (D9a done). The G-perf and A/B base worktree `../om-bench-base` was moved to 09eb5134 and rebuilt.
- Commits:
  - 428b10a9: the plan amendment above.
  - 6191be63: `util/ab_scenarios.py`, three variants in group `interventions` that put one component into scenarioVivax, which no ctest scenario does: a BSV vaccine (`vivax_bsv`, the "cannot be used with vivax model" error), `treatSimple` with the durations the vivax model supports (`vivax_treat_simple`, runs) and with a liver-stage duration (`vivax_treat_simple_liver`, its error). Base against base: 0 of 41.
  - e691b2a5: **logic, model options.** `Host::HumanParams` holds the `ModelOptionFlags`, and `het_sample` reads its seven options from them in the old order; `PerHostAnophParams` and `PerHostParams::add_species` take them; `create_transmission_model` takes them and passes them to the vector and Anopheles factories (life cycle, SimpleMPD, exact Nv0 solver), and `VACCINE_GENOTYPE` as a bool to the `TransmissionModel`, `NonVectorModel` and `VectorModel` constructors; `InterventionManager::init` takes them and passes them to `VaccineComponent` and `TreatSimpleComponent`. `util::ModelOptions`, `bridge::model_options` and `ModelOptionsBinding` went; the unit tests' humans get the test options.
  - fa502292: **logic, command line and infant mortality.** `--print-EIR` through the transmission factories (a bool into `AnophelesModel::init_eir`); `--print-interventions` and `--verbose` through `InterventionManager::init` (`verbose` a member); `--print-survey-times` through `Clock::init` into `read_survey_dates`; the ctsout name into `Continuous::init`; output name, format and compression into `write_survey_data`, which also takes the `InfantMortality` and passes it through `write_text`/`write_binary` to `write_rows`, where `all_cause()` is read as before. CommandLine's `output_name`, `output_format`, `ctsout_name` and `lookup_resource` statics, `bridge::infant_mortality` and `InfantMortalityBinding` went. `CommandLine::option` and its bridge remain for UnitParse and AgeGroupInterpolation (D9e).
  - d099a440: G-style on lines the chunk changed: `hetSample` → `het_sample`; NOLINT on `Monitoring.h`'s forward declaration of namespace `Clinical`, as in `ClinicalParams.h`.
  - 8a0f1831: FINDINGS F42 note; PLAN.md D9b1 row as built; README's bridge list (also adds `Genotypes.h` and `Diagnostic.h`, missing since D4d) and A/B row.
- Gates:
  - G-exact / G-unit / G-two-sims: 63/63 (62 strict scenarios and unit suites, plus `two_sims`) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit.
  - G-stat: `ctest -L statistical -j4` at d099a440: 53 passed, 9 disabled (no reference), as after D9a.
  - G-style: `util/style.sh check 09eb5134` clean at d099a440 (two items before it, fixed in that commit).
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate d099a440, load ~2.7 at the start: median per-pair ratio 0.9913 (−0.87%), spreads 1.4% (base) / 0.5%.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base 09eb5134): 0 of 275 variants (all eleven groups) differ after each logic commit and after the G-style commit. Existing variants cover the moved reads: `--print-EIR` (vector and non-vector), `VECTOR_LIFE_CYCLE_MODEL`, SimpleMPD with non-vector data, the mass-action / availability-distribution error, the heterogeneity options, `--print-interventions`, `--verbose`, `--print-survey-times`, `--ctsout`, binary and compressed output (MonitoringAll reports the IMR).
  - Planted bugs (then reverted): `het_sample` reading `ComorbHet` in place of `TransHet` → `host/het_workaround_trans_het` differs; `verbose` never set → `interventions/verbose_monitoring_all` differs; the survey-time print skipped → `monitoring/print_survey_times` and `survey_repeat_unreported_last` differ; the binary writer given a fresh `InfantMortality` → `monitoring/output_binary` differs.
- Findings: F42 note only; nothing new.
- Surprises: none in the code. The pre-commit clang-format hook reformatted each logic commit's long argument lists, so each was staged twice (whitespace only).
- Follow-ups:
  - D9b2: the object bridges (`InterventionManager`, `ClinicalParams`, `WithinHostModel` with `mutable_within_host_model`, and `WHVivax::set_hs_parameters`' `bridge::command_line()`), and the rename and modernisation of `Human` and `Population`.
  - D9e: UnitParse's three `DeprecationWarnings` reads and `AgeGroupInterpolation::outputSamples`' `SampleInterpolations` read, then `CommandLine::option`, its bridge and `CommandLineBinding`.
  - The commented-out penalisation code in `WHFalciparum.cpp` still names `util::ModelOptions::option`, which no longer exists; it is a comment and was left.
- Next: D9b2.

### D9b2: the object bridges → passed in; `Human` and `Population` renamed and modernised (done, 2026-10-26)
- Base: 70cb03c3 (D9b1 done). The G-perf and A/B base worktree `../om-bench-base` was moved to 70cb03c3 and rebuilt (Ninja).
- Inventory at the base: 13 uses of `bridge::intervention_manager()`, `clinical_params()`, `within_host_model()` / `mutable_within_host_model()` and WHVivax's `bridge::command_line()` in `model/` and `unittest/`.
- Commits:
  - 18ea267a: **logic, within-host model and clinical params.** `HumanParams` holds the `WithinHostModel` and `ClinicalParams`, whose `create` `Human`'s constructor calls at the old points (host_rng draws in the old order). Each per-human within-host model (`WHFalciparum`, `WHVivax`, through them `CommonWithinHost` and `DescriptiveWithinHostModel`) holds a pointer to the `TreatmentRegistry`, passed by `WithinHostModel::create`. The health-system setup takes the mutable within-host model and the manager: `set_health_system`, `set_immediate_outcomes`, `set_decision_tree_5day` and `get_health_system_treatment_by_name` call `within_host.treatments.add(desc, interventions)` and `within_host.vivax.set_health_system(elt)` where the statics were called, so reads and errors keep their order; `TreatmentRegistry::add` and `Treatments` take the manager for their triggered deployments. `TimedChangeHSDeployment` holds the within-host model and its owning manager; `InterventionManager::init` takes the within-host model in place of its `PkPdParams`. `VivaxParams` stores the deprecation-warnings setting at construction instead of reading the command line in `set_health_system` (the command line is fixed per simulation; the warning is printed in the same cases and at the same point, and changeHS needs no command line). Gone: both bridges, `WithinHostModelBinding`, `ClinicalParamsBinding`, `WHInterface::add_treatment`, `WHInterface::create_within_host_model`, `WHVivax::set_hs_parameters`.
  - 69af0838: **logic, intervention manager.** `HumanParams` holds the manager and `Human` keeps a pointer to it (`intervention_manager`) for `removeFirstEvent` and its checkpoint read. `PerHost::checkpoint(istream&, manager)` and `PerHumanVaccine::checkpoint(istream&, manager)` replace the template `operator&` on the read side; the shared fields are a `checkpoint_fields` template used by both directions, and the vaccine list read is `checkpoint_containers.h`'s (size, `validateListSize`, resize, each element). `has_active_interv` and `mon::init_cohorts` take the manager. `DecisionTreeLibrary` holds it (passed by `Simulation`, which assigns the library in its constructor body, when the manager member is already constructed) and passes it to the `CMDTDeploy` and `CMDTCohort` constructors; the cohort node still looks its component up after building both children. Gone: the bridge and `InterventionManagerBinding`. Now included in `HumanComponents.h`: `checkpoint_containers.h`, which the non-template read and write need.
  - f995cf66 + f5d211ec: **rename** of `model/Host/Human.{h,cpp}` and `model/Population.{h,cpp}`, 62 by the tool (21 variable, 13 member, 12 method, 11 function, 2 header guards, 2 parameter, 1 global constant). Maps: `dateOfBirth=dob` (the tool's `date_of_birth` is the constructor's parameter; `dob` matches `get_dob`), `ctsImmunityh=cts_immunity_h`. Collisions reviewed: `within_host_model`, `next_cts_dist` (locals elsewhere; the member is always reached by member access), `exp_it` (another function's local), `update_cohort_set` (the `mon::` function, always called qualified). The disabled `AnophelesModelSuite` still says `perHostTransmission`; it is not compiled and was already stale (F9).
  - dd43e68a + 5b5ddc61: **modernise, tool:** 3 `use-default-member-init` (HumanHet's constant defaults), 1 `concat-nested-namespaces`, 1 `use-ranges` (the median sort, over doubles), 1 `init-variables`, 1 `use-equals-default`.
  - bfd60dbb: **modernise, hand:** `misc-const-correctness` (21, west const); `Human` follows the rule of zero (its `unique_ptr` members already make it move-only); `CtsDemogAgeGroups` a `constexpr std::array`; the host-demography ctsout title with `std::format("{:g}")` (same text for 1, 5, 10, 15, 25; ctest's BSV, PEV and TBV compare it); explicit conversions with the same values (`from_ts(static_cast<int>(iage))`, `recent_births += static_cast<int>(size) - cum_pop`, `std::cmp_less(cum_pop, size)` since popSize is an `xml_schema::Int`, `static_cast<double>` divisions, `size % 2` for `mod_nn`). Left: public data members (F6), the checkpoint idiom (H3).
  - da0d7b74: G-style on lines the chunk changed: an explicit `static_cast<long>` for the vaccine read's `validateListSize`.
  - 722cf695: FINDINGS F78 note; PLAN.md D9b2 row as built; README's bridge list.
- Gates:
  - G-exact / G-unit / G-two-sims: 63/63 (strict) on `build-rework/` (Release) and `build-debug/` (Debug) after each C++ commit.
  - A/B (`util/ab_scenarios.py <base> <candidate>`, base 70cb03c3): 0 of 275 variants differ after each logic commit and after the hand modernise commit. No variants added: the existing ones reach the moved paths (health-system errors and changeHS under each clinical model, vivax liverStageDrug with pUseUncomplicated with and without deprecation warnings, treatment-triggered deployments, decision-tree cohort and deploy nodes and their errors, each sub-population removal option, the cohort errors, MonitoringAll's ITN, IRS and GVI coverage ctsout). Not reached by any test: the PerHost and vaccine checkpoint reads with deployed interventions (F78), covered by review only.
  - Planted bugs (then reverted): the vivax deprecation flag ignored → `vivax/lsd_puse`, `lsd_puse_conflict`, `changehs_same`, `changehs_same_checkpoint` differ; the blood-stage effect dropped from the looked-up treatment → 39 ctest scenarios fail; `removeFirstEvent` always reading the onFirstBout list together with `has_active_interv` always matching ITN → 2ITNs and VecFullTest fail and 22 of the 65 `interventions` and `monitoring` variants differ, including `sub_pop_removal_onFirstTreatment` and `_onFirstInfection`.
  - G-stat: `ctest -L statistical -j4` at da0d7b74: 53 passed, 9 disabled (no reference), as after D9b1.
  - G-style: `util/style.sh check 70cb03c3` clean at da0d7b74 (one item before it, fixed in that commit). Rename: format half clean, second dry run `nothing to rename`, token check passed, `check f995cf66^` clean. Modernise tool commit: format half clean, second dry run applies nothing, `check dd43e68a^` clean.
  - G-perf: `util/bench.sh -n 7 --compare ../om-bench-base/build-rework`, candidate da0d7b74, load ~8 at the start (desktop apps, not a job): median per-pair ratio 0.9944 (−0.56%), spreads 13.5% (base) / 4.3%. `Human` and each per-human within-host model gained one pointer.
- Findings: F78 note only; nothing new.
- Surprises:
  - Making the PerHost and vaccine reads non-templates made the container `operator&` a non-dependent call, so `HumanComponents.h` now includes `checkpoint_containers.h` (before, the template found it where `Human.cpp` instantiated it).
  - The bridge-free lifetimes: `DecisionTreeLibrary` is declared before the manager in `Simulation` and so outlives it during destruction; nothing in the library's destruction reads the manager.
- Follow-ups:
  - D9c: `Genotypes` and `Diagnostics`; D9d: the monitoring bridge; D9e: the clock and `CommandLine::option`.
  - `InterventionManager.h` forward-declares `class ClinicalParams`, which is a struct (harmless; tidy it when the file is next touched).
- Next: D9c.

### Tooling after D9b2: chunk briefing, gate runner, planted bugs, bridge inventory, drift checks, hooks (2026-10-26)
A retrospective on D9b2 (at the user's request) found where a chunk session spends tokens and turns: reading PLAN.md and PROGRESS.md at the start (~35k tokens), hand-written build/ctest/A/B pipelines (several broke on this shell's `grep` and `ls` replacements), planted bugs done by hand (one A/B run tested a stale binary after the planted build failed), repeated bridge inventories, and docs drift (the module status table lacked `model/mon` until D9b2 and `CMDecisionTree` until now). The user asked for all of the proposed changes. No model code changed.
- Commits:
  - 82eac7c0: `util/bridges.py`, the remaining bridges, their facades (including facades of facades in the bridge's own files, e.g. `mon::record_event` through `record_value`) and qualified call sites. At 08e18352: monitoring 158 sites in 30 files, clock 408 in 61 (with `unittest/` and `test/`), Genotypes 28, Diagnostics 8, `CommandLine::option` 4.
  - ceaf2483: `util/gates.sh`, the gates with one line each; `--all <base>` adds A/B, G-stat, G-style and G-perf (with the load average); lines logged in `build-rework/gates.log`.
  - 361323d6: `util/plant.py`, plant (`--replace FILE OLD NEW` or `--patch`), build (a build error stops it, exit 2), ctest and A/B groups, restore and rebuild; "caught" or "NOT CAUGHT". Tried on D9b2's plants (4 vivax variants; 40 of 63 ctest tests, where D9b2's message counted 39 by hand), a non-compiling plant and a no-op.
  - f5048569: `util/chunk.py` (`next`, `section`, `commits`, `gates`, `check-docs`). `next` for D9c prints ~13 kB. `check-docs` checks README's bridge list against the code, the module status table against the tool commits in `.git-blame-ignore-revs`, and the status table against the log entries.
  - 0fe5a0af: the module status row for `CMDecisionTree` (D6c), found by `check-docs`.
  - b23671a2: skills `/next-chunk` and `/close-chunk`, built on the tools above.
  - 732343be: `util/rename_module.py` prints the source line under each possible collision; `util/modernize_module.py --report` groups `misc-const-correctness` by file and prints the standard treatment under the const, narrowing, sign-comparison and cast checks.
  - b0580c6d: hooks: `guard-bash.sh` (PreToolUse) denies staging everything and `rm -r` on variable paths, asks before push, stash, clean, `reset --hard` and directory-wide checkouts, and ignores heredoc bodies; `format-after-bash.sh` (PostToolUse) clang-formats C++ changed by in-place Bash edits. The guard denied `git add -A --dry-run` live.
  - (the next commit): PLAN Part 1 (step 2 and the practical notes) and README's re-run section name the tools; this entry.
- Not done as proposed: moving PLAN Part 3 to its own file. `util/chunk.py section NAME` gives the same saving without breaking the "Part 3 §" references in PLAN, the skills and this log.
- Gates: no model code changed. `util/gates.sh --ab vivax --style 70cb03c3` and `--release-only --stat` ran at 82eac7c0 (63/63 on both builds, 0 of 18 vivax variants differ, G-style clean, G-stat 53 passed and 9 disabled). `util/chunk.py check-docs` clean after 0fe5a0af.
- Next: D9c, started with `/next-chunk`.

