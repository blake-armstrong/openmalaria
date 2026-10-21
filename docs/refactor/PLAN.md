# OpenMalaria → OpenMM-style library + CLI: chunked refactor plan

## Context

Today OpenMalaria is a single executable, not a library:
- `model/main.cpp` is the driver. It runs a hand-ordered chain of about 35 static `init()` calls, then the warmup, EIR-calibration and main phases, then the time-step loop (`run()`).
- Simulation state is global:
  - `sim::` time statics (`model/sim.h`)
  - `util::master_RNG` (`util/random.h`)
  - `ModelOptions` and `CommandLine` bitsets
  - file-static `opt_*` flags and parameter copies, e.g. `WHFalciparum.cpp:55-79`
  - a fully static `InterventionManager`
  - `mon::internal::runtime`
  - PK/PD `schedules` and `dosages`
  - `PerHost::params`
- The CodeSynthesis-generated `scnXml::` types reach 85 of about 150 model files. Some are still held and used at runtime, e.g. `TimedChangeHSDeployment` and `changeEIRIntervention`.
- Model variants are picked by hard-coded if/else chains. There is no registry.

Consequences:
- Only one simulation can run per process.
- There is no stable API to embed in Python/R or calibration frameworks.
- Adding a model variant means editing factories and the schema together.

The goal is a layered design like OpenMM's:
- a small, stable C++ core API
- pluggable model components
- explicit simulation objects that own their state
- a thin CLI and nanobind Python bindings on top
- the scenario XML kept as one input format, not the internal data model
- one enforced code style
- statistical tests, so that changes which alter RNG draw order can still be verified
- legacy C++ practices migrated to modern C++20, and hand-rolled or dated infrastructure replaced with maintained libraries where that pays off (Stage M, § Modernisation policy)

The work is split into **small, self-contained chunks**. Each chunk fits in one Claude Code session and ends in a committed, tested state, so the context can be cleared between chunks.

---

# Part 1: How to work through the chunks

## Session protocol (every chunk)

1. **Start fresh** in `/Users/blaarm/git/openmalaria-fork` on branch `throwaway_ai_rework`. Never work in `/Users/blaarm/git/openmalaria`.
   - If the user names a chunk, do that chunk.
   - If the user just says to continue (e.g. "do the next chunk"), do the first chunk in **§ Recommended order** below whose status in `PROGRESS.md` is not `done`.
2. **Read only:**
   - `docs/refactor/PLAN.md` (a copy of this file, made by chunk A1): Part 1, the one chunk being done, and the Part 3 reference sections that chunk names
   - `docs/refactor/PROGRESS.md`
   - the open entries in `docs/refactor/FINDINGS.md` that concern the files or modules the chunk touches
3. **Check the chunk's prerequisites** against `PROGRESS.md`. If any is missing, stop and say so.
4. **Do only that chunk.** If it is clearly growing beyond one session (more than ~20 files of non-mechanical change), stop at a green point. Split the rest into a new chunk ID (e.g. `D4b`) and record the split in both `PLAN.md` and `PROGRESS.md`.
5. **Pass the gates the chunk lists** (below).
6. **Commit** on `throwaway_ai_rework`:
   - Keep mechanical commits (formatting, renames, file moves) separate from logic commits.
   - Stage files by name, never with `git add -A`, because `.cache/` and `python/` are untracked user files.
   - Never push unless the user asks.
7. **Append to `PROGRESS.md`:**
   - the chunk ID and status
   - the commit hashes
   - the gate results
   - surprises and deviations from the plan
   - follow-ups for later chunks
8. **Record findings** in `docs/refactor/FINDINGS.md` as you go, not only at the end: every bug, surprise or strange design decision found in the code, tests or build, even if it is outside the chunk's scope. Give each a new ID (`F<n>`) and refer to that ID from the `PROGRESS.md` entry. Don't fix a finding that changes observable output inside a refactor chunk: record it and ask the user. When a later chunk fixes or settles one, move it to § Closed with the commit.
9. **Clear the context.** The next chunk starts again at step 1.

**Stop and ask the user** before:
- discarding or stashing tracked changes
- editing `test/expected/*`
- answering any of the open decisions in Part 3
- touching the untracked `python/` directory

## Gates

| Gate | Command / check | Available from |
|---|---|---|
| **G-exact** | `ctest` in the build dir (or `./build.sh -t`): every existing scenario, including checkpoint-stop/resume, is byte-identical to `test/expected/` | A2 |
| **G-unit** | the CxxTest suites in `unittest/` pass | A2 |
| **G-style** | `OM_BUILD_DIR=build-rework util/style.sh check <chunk's base commit>`: `git-clang-format --diff` and `clang-tidy-diff` report nothing on changed lines (see `docs/refactor/STYLE.md`) | B1 |
| **G-perf** | `OM_BUILD_DIR=build-rework util/bench.sh --compare <base build>`, where the base build is the chunk's base commit configured the same way (recipe in `util/bench.sh -h`): the median per-pair candidate/base wall-time ratio is ≤ 1.03. The stored baseline in `PROGRESS.md` is a sanity check only | P1 |
| **G-stat** | `ctest -L statistical` passes. For a chunk that changes draws on purpose (D2b, M4, M8), `ctest -C full -L statistical-full -j1` (S7's group test, 20 runs per scenario, ~6 min) must pass too | S4 (full coverage after S5; `statistical-full` after S7) |
| **G-two-sims** | two `Simulation`s in one process match two separate runs exactly | D9 |
| **G-diff** | for a chunk that replaces a numerical routine (GSL, R_nmath, a distribution): a Catch2 differential test compares the new routine with the old one over a grid of inputs, within a tolerance stated in the test. The old routine stays callable from the test until the chunk ends | M1 |
| **G-cli** | `util/ab_cli.sh <base build>`: a few scenarios run with default, `--verbose --progress`, `--checkpoint` and `--checkpoint-stop` flags (twice, so the second run resumes) give byte-identical stdout, stderr, outputs, checkpoint files and exit codes. Required for any chunk that touches console output or command-line handling | M3 |
| **G-threads** | the parallel human update gives identical output for 1, 2 and all cores | M8 |

Every chunk must pass every gate that is available at that point. One exception applies: a chunk whose *purpose* is to change RNG draw order or floating-point results (the numerical library swaps in stage M) may fail G-exact, but must then pass G-stat, plus G-diff where it applies. It must say so in its commit message and in `PROGRESS.md`. Batch such chunks where the order allows, so `test/expected/` is regenerated as few times as possible.

## Dependency overview

```
A1 → A2 ─┬─ B1 → B2 → B3 → B4 → B5
         ├─ P1
         ├─ S1 → S2 → S3 → S4 → S5 → S6 → S7
         └─ C1 → C2 → D1 … D9 → E1 … E6 → F0 … F6 → G1 … G3 → H1 … H3 → I1 … I3 → J1 … J4
B5 → M0 → M1;  B4 → M2 (before D1);  S5 → V1 → D2b;  S7 → D2b;  D9 → M3, M7;  D2b + S7 → M4;  F6 → M5 → M6;  G3 + M4 + M7 → M8;  M1 → M9
```
- The B, P and S tracks can run in any order relative to each other and to C/D.
- D2b (splitting the RNG streams) is the first chunk allowed to change draw order, so it requires S6 and S7 (a test with enough power to verify it), and V1, so that no regenerated scenario seed aborts in vector warmup.
- Every chunk from D onward requires B4, so that each module's rename can run right after it is refactored.
- Every chunk from D onward also requires M2, so that each module's modernisation pass runs right after its rename.

## Recommended order

The dependency graph allows many orders. Follow this one unless the user says otherwise. Work down the list. The next chunk is the first one not marked `done` in `PROGRESS.md`.

| # | Chunk(s) | Notes |
|---|---|---|
| 1 | A1, A2, B1 | Done. |
| 2 | **B2** | Before any other chunk touches C++, so that every later diff is format-clean. **First ask the user** whether they want to merge new upstream commits into this branch. If so, they should do it before B2, because the reformat makes later upstream merges painful. |
| 3 | **P1** | Needed before C1, which gates on G-perf. Measure on an idle machine under `caffeinate -i`, with no S5 job running. |
| 4 | **B3**, then **B4** | B4's pilot (`model/PkPd/`) shows the real cost of the `m_`/`_` member prefixes and header guards before the bigger modules are renamed. |
| 5 | **B5** | So that pre-commit catches format slips throughout C and D. |
| 6 | **C1**, then **C2** | The first structural refactor. Gates: G-exact, G-unit, G-style, G-perf. |
| 7 | **M0 → M2** | **First ask the user Decision 5.** Dependency mechanism, Catch2 and the `modernize-module` skill must exist before D1, so every D chunk can use them. |
| 7b | **S1 → S4** | Have the statistical tooling working before the D track gets going. |
| 8 | **S5** | A long compute job (≈1 h at `-j10` for N = 500 if every scenario is run; more with stragglers). Run it overnight or while no G-perf measurement is being taken, because it saturates the CPU. Every chunk up to D2 keeps outputs byte-identical, so it doesn't matter which commit generates the references. |
| 9 | **D1**, **D2** | These can overlap with S5/S6 in calendar time, but don't take G-perf measurements while S5 is running. |
| 10 | **S6** | Must be done before D2b. |
| 10a | **S7** | Added after S6 (F31). Must be done before D2b: G-stat is the only gate for draw-order changes, and S6 showed the current check misses shifts of about 2 SD. Confirm its three choices with the user first. Independent of V1. |
| 10b | **V1** | Added after S5 (F16, F24, F26, F27). Must be done before D2b: with new draws, a scenario's own seed could start aborting in vector warmup. Each fix changes outputs, so ask the user first. |
| 11 | **D2b**, then **M4** | **First ask the user Decision 1.** The first chunk allowed to change draw order; it is gated by G-stat, not G-exact. M4 also changes draws, so do it straight after and regenerate `test/expected/` once for both. |
| 12 | **D3 → D9** | |
| 12b | **M3**, **M7** | Logging and replicate-level parallelism, once `Simulation` owns all its state. **Ask the user Decision 6 before M7.** |
| 13 | **E1 → E6** | **Ask the user Decision 3 before E1.** |
| 14 | **F0 → F6** | |
| 14b | **M5 → M6** | **Ask the user Decision 7 before M5.** Numerical code is now isolated in components, so each swap stays local. |
| 14c | **G1 → G3**, then **M8** | M8 needs the per-`Simulation` `Monitor` from G1 for deterministic accumulation. |
| 15 | **H1 → H3** | **Ask the user Decision 2 before H1.** If Python is a primary goal, do stage J immediately after H2 (before H3 and I). |
| 16 | **I1 → I3** | **Ask the user Decision 4 before I2.** |
| 17 | **J1 → J4** | |
| 18 | **M9** | Any time after M1; at the latest here. |

Practical notes for every chunk:
- Use `build-rework/` as the build dir (see the A2 entry in `PROGRESS.md`). Set `OM_BUILD_DIR=build-rework` for `util/style.sh`.
- Wrap long `ctest` or benchmark runs in `caffeinate -i`. During B1, idle sleep stalled a test run for 990 s.
- Commit each logical step as it lands, in its own small commit, not one commit per chunk.

---

# Part 2: Chunks

Each chunk has the same fields:
- **Needs:** chunks that must be done first
- **Read:** the Part 3 reference sections to read
- **Do:** the work
- **Done when:** the exit criteria
- **Gates:** the gates it must pass

## Stage A: Workspace

### A1: Create the branch and put the plan in the repo
- **Needs:** nothing. **Read:** Part 3 § Workspace facts.
- **Do:**
  ```sh
  cd /Users/blaarm/git/openmalaria-fork
  git status                                   # must show no tracked changes on core-only
  git fetch upstream
  git switch --no-track -c throwaway_ai_rework upstream/main
  git rev-parse HEAD upstream/main             # the two hashes must be identical
  git diff --quiet upstream/main && echo clean
  git status --short                           # only ?? .cache/ and ?? python/
  ```
  - If `git status` shows tracked changes, stop and ask.
  - Then copy this plan file to `docs/refactor/PLAN.md` and create `docs/refactor/PROGRESS.md` with a table of all chunk IDs (status `todo`). Commit both.
- **Done when:** the branch HEAD equals `upstream/main` plus the one docs commit.
- **Gates:** none.

### A2: Baseline build and tests
- **Needs:** A1.
- **Do:**
  - Build with `./build.sh` (or CMake with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`) and run `ctest`.
  - Record in `PROGRESS.md`: compiler and version, the pass/fail list, the total test time, and the **wall time of each scenario**, which S5 needs for planning.
  - If any test fails on clean upstream, record it as a known failure. Do not fix it.
- **Done when:** the baseline is recorded.
- **Gates:** G-exact and G-unit establish the baseline.

## Stage B: Style tooling and Claude Code skills

### B1: Style configs
- **Needs:** A2. **Read:** § Code style.
- **Do:**
  - Add `.clang-format` (matching the current 4-space style) and `.clang-tidy` (Core Guidelines checks plus `readability-identifier-naming` per § Code style).
  - Exclude `schema/` (generated code) and `contrib/`.
  - Turn on `CMAKE_EXPORT_COMPILE_COMMANDS` by default.
  - Record the pinned LLVM version.
  - Do not reformat anything yet.
- **Done when:** both tools run on `model/Population.cpp` with the project config. The tidy exclusions are listed in `docs/refactor/STYLE.md`, each with its reason.
- **Gates:** G-exact.

### B2: Whole-tree format
- **Needs:** B1.
- **Do:** run clang-format over `model/`, `unittest/` and `util/` C++ in one commit. Add that commit's hash to `.git-blame-ignore-revs` in a second commit.
- **Done when:** `clang-format --dry-run -Werror` is clean on the whole tree.
- **Gates:** G-exact, G-unit.

### B3: `cpp-style` and `cpp-lint` skills
- **Needs:** B1. **Read:** § Code style.
- **Do:**
  - Write `.claude/skills/cpp-style/SKILL.md`: the naming and Core Guidelines rules, plus running clang-format on changed files.
  - Write `.claude/skills/cpp-lint/SKILL.md`: clang-tidy on changed files, apply the safe fix-its, rebuild, and report anything left that needs judgement.
  - Optionally add a `PostToolUse` hook in `.claude/settings.json` that runs clang-format on any edited `*.cpp`/`*.h`.
- **Done when:** both skills have been tried on a throwaway edit, and the edit has been reverted.
- **Gates:** G-style.

### B4: `rename-module` skill plus a pilot
- **Needs:** B3.
- **Do:**
  - Write `.claude/skills/rename-module/SKILL.md`. It runs clang-tidy `readability-identifier-naming` fix-its over one directory and the call sites that use it, builds, runs G-exact, makes one commit, and appends that commit to `.git-blame-ignore-revs`.
  - Pilot it on one small leaf module, e.g. `model/PkPd/`.
- **Done when:** the pilot commit contains only renames and passes the gates.
- **Gates:** G-exact, G-unit, G-style.

### B5: Pre-commit and CI
- **Needs:** B2.
- **Do:**
  - Add `.pre-commit-config.yaml` for clang-format.
  - Add a GitHub Actions job in `.github/workflows/` that runs clang-format and `clang-tidy-diff` against the merge base.
- **Done when:** the job definition passes locally (e.g. with `act`, or by running its script by hand).
- **Gates:** G-style.

## Stage P: Performance baseline

### P1: Benchmark
- **Needs:** A2.
- **Do:**
  - Choose or add a benchmark scenario with a large population and many steps.
  - Add `util/bench.sh`, which runs it 5 times and reports the median wall time.
  - Record the baseline in `PROGRESS.md`.
- **Done when:** the baseline is recorded and the run-to-run spread is under 3%.
- **Gates:** G-exact.

## Stage S: Statistical regression tests

### S1: Seeded replicate runner
- **Needs:** A2. **Read:** § Statistical tests.
- **Do:** add `test/stats/run_replicates.py`. It:
  - copies a scenario into a temporary directory
  - rewrites `<computationParameters iseed>`
  - runs `openMalaria`, reusing what `test/run.py` already does (paths, schema link, binary-to-text conversion)
  - collects `output.txt` and `ctsout.txt` per seed
  - takes a `--jobs` option to run in parallel
- **Done when:** running the same seed twice gives byte-identical output, and two different seeds give different output (shown on `scenario1`).
- **Gates:** G-exact.

### S2: Reference builder
- **Needs:** S1. **Read:** § Statistical tests (1).
- **Do:** add `test/stats/generate_reference.py`. It computes, per cell, the mean, the SD, q2.5 and q97.5. It also computes the leave-one-out miss-count threshold and the aggregate intervals, and writes `test/reference/<scenario>.csv` with a metadata header. Add unit tests that use synthetic data.
- **Done when:** a reference for `scenario1` at N = 50 is produced and the unit tests pass.
- **Gates:** G-exact.

### S3: Checker with reruns
- **Needs:** S2. **Read:** § Statistical tests (1)–(2).
- **Do:** add `test/stats/check_scenario.py`, implementing § Statistical tests (2):
  - the per-run judgement: miss-count p-value against the leave-one-out counts, the aggregates, and fixed cells
  - the fixed rerun rule
  - a report sorted by |z|
  - a `--replicates K` mode
  - Move the reference format to version 2 (full leave-one-out counts, 99% aggregate family) and regenerate `scenario1` at N = 500.
- **Done when:**
  - The checker passes on unchanged code.
  - It fails on a deliberately perturbed copy of `scenario1`, e.g. EIR × 1.5.
- **Gates:** G-exact.

### S4: CTest integration
- **Needs:** S3.
- **Do:**
  - In `test/CMakeLists.txt`, register `stat_<scenario>` with label `statistical` for every scenario that has a reference file.
  - Scenarios without one are skipped with a message.
- **Done when:** `ctest -L statistical` runs `stat_scenario1`. A plain `ctest -LE statistical` still runs only the old tests.
- **Gates:** G-exact, G-stat.

### S5: Generate the full references
- **Needs:** S4.
- **Do:**
  - Use the per-scenario timings from A2 to batch the scenarios. Default N = 500; for slow scenarios use a lower N and record it.
  - This is a long compute job. Run it in the background or on a cluster. One chunk session may cover only some batches; record which are done in `PROGRESS.md`.
  - Commit the CSVs.
- **Done when:** every scenario has a reference.
- **Gates:** G-stat.

### S6: Validate the method
- **Needs:** S5. **Read:** § Statistical tests (4).
- **Do:**
  - Measure the per-run failure rate with 200 fresh-seed runs, then the false-failure rate of the whole procedure, reruns included; it must be ≤ 0.5%.
  - Run the power study with parameter perturbations of 5%, 10% and 20%, and with an intervention switched off, for both the default rerun rule and `--replicates K`.
  - Retune the per-run α, K and the rerun rule if the targets are not met.
  - Rule (c) (fixed cells) gives ~10% per-run false failures in 2ITNs, IRS30 and Rach5IC, from the nearly constant ctsout `recent births` column (F28). Candidate: a cell counts as fixed only if every cell of its measure or column is constant in every replicate. Changing the rule changes each reference's `loo_run_failures`, so regenerate the references from the S5 replicates, which are kept for this (see S5 in PROGRESS.md).
  - Write `test/stats/README.md` with the minimum detectable effect for each scenario.
  - Optionally add a nightly random-seed job.
- **Done when:** the README exists and the false-failure target is met. If it isn't, retune the thresholds and record why.
- **Gates:** G-stat.

### S7: Group-mean test for `--replicates`
Added after S6, with the user's agreement (F31). S6 found that the check has little power against small real changes: a 20% higher force of infection moves mean prevalence by about 2 single-run SDs in ten scenarios, yet the default rerun rule detects it in only 2 of 44 scenarios and `--replicates 20` in 10. Each run is judged against intervals that hold 99% of single runs, and `--replicates` counts failing runs instead of testing where the K runs lie as a group. Since D2b, M4 and M8 change draws on purpose, G-stat is their only gate.
- **Needs:** S6. **Read:** § Statistical tests; `test/stats/README.md`; FINDINGS F17, F31.
- **First confirm with the user** the three choices below (recommended values given). They change § Statistical tests (2), which the user settled in S3.
  - K = 20.
  - Tested quantities: the aggregates plus each survey measure's total over surveys and age groups (not aggregates only), so changes that don't reach the five aggregates (e.g. treatment counts) are seen too.
  - A family false-failure rate of 0.01% per scenario, so the 44-test suite raises a false alarm about 0.4% of the time instead of S6's ~9%.
- **Do:**
  - Reference format 4: store every replicate's value of every tested quantity. Regenerate all references from the kept replicates: `/Users/blaarm/git/om-s5/build-rework/stats-replicates/` (43 scenarios) and `build-rework/stats-s6/ref1` (scenario1). No new replicates are needed.
  - `--replicates K` judges the group: for each tested quantity, z = (mean of the K runs − reference mean) / (reference SD · √(1/K + 1/N)). The check fails if the largest |z| exceeds a threshold, or if any single run fails rule (c) (fixed cells). Keep the per-run rules for reporting.
  - Calibrate the threshold per scenario by leave-K-out on the reference replicates, not from the normal distribution: repeatedly take K of the N replicates as the "new build", score them against the other N − K, and use the (1 − α) quantile of the largest |z|. This allows for skewed counts and for correlation between quantities, as rule (a)'s leave-one-out does for misses.
  - Add a ctest label (e.g. `statistical-full`) that runs `check_scenario.py --replicates K` for every referenced scenario. Plain `ctest` keeps the cheap single-run check.
  - Extend `validate_method.py` and re-validate on the kept S6 runs in `build-rework/stats-s6/`, with no new runs: false failures from the 200 fresh seeds per scenario as disjoint groups of K (440 groups), and power and MDE from groups of K resampled from the 50 runs of each perturbation. Update the README tables.
  - Update § Statistical tests (2) and the G-stat gate text.
  - Afterwards, ask the user whether to remove `/Users/blaarm/git/om-s5` and `build-rework/stats-s6/`, kept for this chunk.
- **Not in scope:** tests of spread or distribution shape. A change that keeps every mean but changes a variance (e.g. a sampler with the wrong spread) still passes; record it as a known limitation.
- **Done when:** the group test's false-failure rate per scenario meets the chosen α (calibration plus the fresh-group check), the README has the new power table, and `ctest -L statistical-full` runs.
- **As built:** the label's tests need `-C full` (see § Statistical tests (3)); results in `test/stats/README.md` and the S7 entry in `PROGRESS.md`.
- **Gates:** G-stat (both labels), G-exact.

## Stage V: Vector warmup robustness

Added after S5, with the user's agreement. In S5, some scenarios aborted in vector warmup for a share of seeds, so they got no statistical reference: ModelName* (F16; 10 people), AvailabilityFilter (F26; 13% of seeds, 300 people) and ExactNv0 (F27; 18%, 150 people). SimpleMPDTest got none either, because its warmup ctsout length varies with the number of calibration rounds (F29). LifeNet1/2 lost one seed (F24). Every abort is a model safety check tripped by sampling noise in a small population; the scenario's own seed happens to pass.

### V1: Make vector warmup converge reliably
- **Needs:** S5. **Read:** § Statistical tests (5); FINDINGS F16, F24, F26, F27.
- **Do:**
  - Instrument `EmergenceRateAdaptiveFitter` (per-round measured S_v ratio and scale factor) on AvailabilityFilter and a ModelName* scenario. Find why passing seeds need ~25 rounds of 6 simulated years each instead of a few, and fix the cause. Candidates: a damped correction step; a convergence test on an averaged measurement instead of one noisy round. Raising the 30-round cap alone only hides the problem.
  - ExactNv0 (`CalcNvO.cpp`): stop noise in the measured human infectiousness (`laggedKappa`) from producing negative emergence. Candidates: smooth `laggedKappa` before the solver; clamp small negatives to zero. Keep the error for inputs that are genuinely infeasible.
  - Look at the SimpleMPD negative-emergence check (F24) in the same way.
  - **Every fix changes outputs: ask the user before each one.** Regenerate `test/expected/` for the affected scenarios only with approval, and batch it with D2b/M4's regeneration where the order allows. Regenerate the statistical references of the affected scenarios in their own commit (§ Statistical tests (5)).
  - SimpleMPDTest writes ctsout during warmup (`duringInit`), so its length follows the number of calibration rounds (F29). Either V1 makes the round count fixed, or the stats tooling keys such rows by time from the end of the run and ignores the warmup rows.
  - Then build references for ModelName*, AvailabilityFilter, ExactNv0 and SimpleMPDTest and enable their `stat_*` tests.
- **Done when:** seeds 1001–1500 and 2001–2005 run without an abort for every registered scenario, and every registered scenario has a reference.
- **As built:** six fitter changes (every species fitted each round; 5-year mean S_v; 5-year mean kappa for the exact solver; SimpleMPD negative-resources guard; a rotation check in the stop test; `find_angle` keeps no rotation unless another fits better), a duringInit ctsout reader and NaN-only-sometimes measures in the stats tooling. Details in the V1 entry of `PROGRESS.md` and FINDINGS F16, F24, F26, F27, F29, F35–F37.
- **Gates:** G-unit, G-style, G-perf. G-exact and G-stat on the scenarios the fix does not touch. For the ones it touches, the change is intended: show which measures moved, as for a reference regeneration.

## Stage C: Extract the Simulation (Phase 1)

### C1: Move the initialisation into `Simulation`
- **Needs:** A2. **Read:** § Mapping.
- **Do:**
  - Create `model/Simulation.{h,cpp}`.
  - Move the ordered `init()` chain from `main.cpp` into its constructor, keeping the exact call order.
  - The globals stay as they are.
- **Done when:** `main.cpp` constructs a `Simulation` and calls into it.
- **Gates:** G-exact, G-unit, G-perf (once available).

### C2: Move the phases and the step loop
- **Needs:** C1.
- **Do:**
  - Move the warmup, EIR calibration, main phase and `run()` into `Simulation::step()`, `run_until()` and `run_phase()`.
  - Keep the checkpoint read/write behaviour identical.
  - `main.cpp` becomes argument parsing plus a call to `Simulation`.
- **Done when:** `main.cpp` is under ~100 lines.
- **Gates:** G-exact (including the checkpoint tests), G-unit, G-perf.

## Stage D: De-globalise (Phase 2)

**Bridge pattern used by every D chunk**, so that each chunk stays small:
- Each global family moves into an object owned by `Simulation`.
- The old static accessors are kept temporarily. They delegate through a single "current simulation" pointer, so call sites can migrate gradually.
  - As built in D1: one pointer per family in `namespace bridge` (`bridge::current_clock`), set for `Simulation`'s lifetime by a binding member (`ClockBinding`) declared right after the family's object. A single `Simulation*` would need `Simulation`'s full definition in every inline accessor, which `Global.h` can't include. Later D chunks add their pointer and binding next to it; `Simulation` sets them all, so they still act as one current simulation.
- Each chunk:
  1. converts one family
  2. migrates the call sites in the modules it owns
  3. runs `/rename-module` on those modules as a separate commit
  4. runs `/modernize-module` (from M2) on the same modules: a tool-made fix-it commit, then a hand commit for the judgement items (§ Modernisation policy)
- D9 removes the bridge.
- `PROGRESS.md` keeps a table of modules with their rename and modernisation status. Any module the D chunks did not reach is swept before F0.

For each D chunk, **Needs:** the previous D chunk plus B4 and M2. **Gates:** G-exact, G-unit, G-style and G-perf, except where a chunk says otherwise.

| Chunk | Global family → new owner |
|---|---|
| **D1** | `sim::` time statics in `model/sim.h` → `Clock`, a member of `Simulation` |
| **D2** | `util::master_RNG` → an RNG owned by `Simulation`, with the same single stream and draw order |
| **D2b** | Split into per-purpose RNG streams. **Changes draw order on purpose.** Needs S6, S7, V1 and the user's answer to Decision 1. Gate: G-stat (including `statistical-full`) instead of G-exact. Regenerate `test/expected/` only with the user's approval. Agreed design (2026-10-21): separate ChaCha master streams for the population and for transmission; each `Human` gets four Xoshiro streams, seeded from the population master in a fixed order: host (heterogeneity, infection incidence, within-host, infections, pathogenesis, imported infections), clinical (case management, decision trees run by the clinical model, neonatal mortality), intervention (coverage, component sampling, decision trees run by interventions) and monitoring (survey and ctsout diagnostics). The statistical references are kept, as the yardstick for this chunk; `test/expected/` is regenerated at the end of D2b and again after M4. |
| **D3** | `ModelOptions`, `CommandLine` bitsets, `Parameters` → a `ModelOptions` object built once and owned by `Simulation` |
| **D4** | Within-host and infection file statics (`WHFalciparum.cpp:55-79`, `CommonWithinHost`, the `Infection` subclasses) → component instance members. Split by subdirectory if it grows past ~20 files. |
| **D5** | PK/PD `schedules`/`dosages` and `PerHost::params` → instance state |
| **D6** | Pathogenesis and clinical statics → instance members |
| **D7** | Transmission and vector statics → instance members |
| **D8** | Static `InterventionManager` → an instance per `Simulation` |
| **D9** | `mon::internal::runtime` → owned by `Simulation`. Remove the bridge pointer. Add the G-two-sims test. Gates add G-two-sims. |

## Stage E: Config boundary (Phase 3)

**Pattern for every E chunk:**
- Define plain `om::config::<Area>` structs.
- Add the XML → config translation under `model/io/xml/` (or `src/io/xml/` after F0).
- Make the area's components consume config, never `scnXml::`.

For each E chunk, **Needs:** the previous chunk; E1 needs D9. **Gates:** G-exact, G-unit, G-style.

| Chunk | Area |
|---|---|
| **E1** | Health system, including replacing the runtime XML uses: `ClinicalModel::setHS` and `TimedChangeHSDeployment` |
| **E2** | Transmission and vectors, including `changeEIRIntervention` |
| **E3** | Within-host, infection and PK/PD |
| **E4** | Demography and population |
| **E5** | Interventions |
| **E6** | Monitoring. Then enforce the boundary: `model` no longer links the `schema` target directly, and `grep scnXml` matches only `io/xml`. |

Decision 3 (whether XML stays the main format) must be answered before E1.

## Stage F: Layout and component registry (Phase 4)

| Chunk | Do |
|---|---|
| **F0** | Mechanical move into the target layout (§ Target layering) with `git mv`. Split CMake into per-layer targets whose link dependencies enforce the dependency rule. Make one commit and add it to `.git-blame-ignore-revs`. |
| **F1** | Registry core (name → factory, with typed config) plus unit tests. Nothing is migrated yet. |
| **F2** | Within-host and infection through the registry. Replaces `WHInterface::createWithinHostModel` and the `CommonWithinHost::createInfection` function pointer. |
| **F3** | Clinical and pathogenesis through the registry (`createClinicalModel`) |
| **F4** | Transmission through the registry (`createTransmissionModel`) |
| **F5** | Interventions through the registry (the if-chain at `InterventionManager.cpp:140-183`) |
| **F6** | An explicit host interface for interventions, instead of reaching into `Human`'s sub-objects |

Each F chunk needs the previous one; F0 needs E6. **Gates:** G-exact (registry dispatch must keep the creation order), G-unit, G-style, G-perf.

## Stage G: Monitoring as Reporters (Phase 5)

| Chunk | Do |
|---|---|
| **G1** | A per-`Simulation` `Monitor` plus a `Reporter` interface. The survey-txt/bin reporter reproduces the current output exactly. |
| **G2** | A ctsout reporter |
| **G3** | An in-memory reporter that returns arrays, with unit tests |

**Gates:** G-exact, G-unit, G-style.

## Stage H: Public API and packaging (Phase 6)

| Chunk | Do |
|---|---|
| **H1** | Headers under `include/openmalaria/`: `Model`, `Simulation`, `State`, `Reporter`, `Registry`, with pimpl and no internal types exposed |
| **H2** | A shared library, CMake `install`/`export` and a package config, plus a tiny out-of-tree consumer project used as a test |
| **H3** | A versioned `State`/checkpoint format, with a round-trip test |

**Gates:** all available gates.

## Stage I: CLI rewrite (Phase 7)

| Chunk | Do |
|---|---|
| **I1** | A CLI11 app at `apps/cli/` with a `run` subcommand on the public API and a `--seed` flag. Switch S1's runner from rewriting XML to `--seed`. |
| **I2** | The `validate`, `resume` and `print …` subcommands. The legacy flags become aliases (Decision 4). Remove the `CommandLine` reads from inside the model. |
| **I3** | Tests that every legacy invocation used in `test/` and `util/` still works |

**Gates:** all available gates.

## Stage J: Python bindings with nanobind (Phase 8)

Before J1, ask the user what the existing untracked `python/` directory in the fork is. Do not overwrite it. If needed, use a different directory name such as `bindings/python/`.

| Chunk | Do |
|---|---|
| **J1** | A nanobind module plus `pyproject.toml` (scikit-build-core, `STABLE_ABI`). Expose `Model.from_xml`, `Simulation`, `run` and `step`. |
| **J2** | Reporters returning NumPy (`nb::ndarray`) and pandas; `get_state`/`set_state` |
| **J3** | A pure-Python `openmalaria.app` layer, plus smoke tests that compare against the CLI output |
| **J4** | Wheel builds with cibuildwheel in CI |

**Gates:** all available gates, plus the Python smoke tests from J3 onward. Decision 2 (whether Python is a primary goal) may move this stage earlier, to directly after H2.

**Later and optional:** R bindings. (The threaded "Platform" and the move off CxxTest are now M8 and M9.)

## Stage M: Modernisation (libraries and practices)

Read § Modernisation policy before any M chunk. Each library enters in its own chunk, behind a thin `om::` wrapper wherever its types would otherwise spread, and is recorded in `docs/refactor/DEPENDENCIES.md` (version, licence, reason, which layers may link it). Public headers (stage H) never expose a third-party type.

| Chunk | Needs | Do | Gates |
|---|---|---|---|
| **M0** | B5, Decision 5 | Dependency mechanism: `cmake/Dependencies.cmake` using `FetchContent` with `FIND_PACKAGE_ARGS`, so an installed package is used first and a pinned tag and hash is fetched otherwise. Raise `cmake_minimum_required` to 3.24. Write `DEPENDENCIES.md`, listing the existing GSL, Xerces-C, xsd, zlib, gzstream, R_nmath and CxxTest too. Update the CI style job's install step. No new library is used yet. | G-exact, G-unit, G-style |
| **M1** | M0 | Catch2 v3 as a second unit-test target, registered with ctest, so G-unit covers both. Port one small CxxTest suite as the pilot. From here on, new tests (F1, G3, every G-diff) are written in Catch2. | G-exact, G-unit, G-style |
| **M2** | B4 | `.claude/skills/modernize-module/SKILL.md` plus `util/style.sh modernize <dir>`, modelled on `rename-module`: module-wide safe `modernize-*` and Core Guidelines fix-its as one tool-made commit (listed in `.git-blame-ignore-revs`), then a hand commit for the judgement items in § Modernisation policy. Pilot on `model/PkPd/`. | G-exact, G-unit, G-style, G-perf |
| **M3** | D9, M0 | Logging on spdlog (with `SPDLOG_USE_STD_FORMAT`, so no separate fmt). `Simulation` owns an `om::Logger`, handed to components like the clock and RNG are after D. The CLI sink uses the pattern `%v`, so console text is unchanged; Python (J) can attach its own sink. Replace the ~130 `cout`/`cerr` diagnostics in ~32 files; survey and ctsout writers are not logging and stay. Compile out debug levels with `SPDLOG_ACTIVE_LEVEL`. Add `util/ab_cli.sh` (G-cli) first. Fix the `print_progress` bug noted in C2 here, if the user agrees. May split into M3a (infrastructure + G-cli) and M3b (call sites). | G-exact, G-unit, G-style, G-perf, G-cli |
| **M4** | D2b, S7 | Portable random distributions. `random.h` wraps the RNG as a `gsl_rng` to use GSL's `gsl_ran_*` samplers. Replace them with in-tree samplers (or Boost.Random) on the `Simulation` RNG. **Not** the `std::` distributions, whose output differs between libc++, libstdc++ and MSVC, which would break cross-platform reproducibility. Changes draws on purpose. *As built:* in-tree samplers in `util/random_variates.h`; normal and Poisson (mean < 10) draws are unchanged, since GSL used the same algorithms; G-diff in `unittest/RandomVariatesTests.cpp`. See the M4 entry in `PROGRESS.md`. | G-stat, G-diff (moments and KS tests per distribution), G-unit, G-style, G-perf |
| **M5** | F6, Decision 7 | Linear algebra on Eigen: `gsl_vector`, `gsl_matrix` and `gsl_blas` in `util/vectors.h`, `CalcNvO`, `TransmissionModel` and `PopulationAgeStructure`. Use fixed-size Eigen types where sizes are known (vector life-cycle matrices). | G-diff, G-stat, G-unit, G-style, G-perf |
| **M6** | M5 | The rest of GSL and R_nmath: root finding, multiroot, minimisation, integration, CDFs and splines (`AnophelesModel`, `CalcNvO`, `WHFalciparum`, `AgeGroupInterpolation`, `sampler`, `LSTMDrug*`, `ITN`, `IRS`) on Boost.Math (header-only, BSL) or Eigen-based solvers. Split M6a (vector model calibration) and M6b (the rest) if needed. When the last use goes, drop GSL and `contrib/R_nmath` from the build. | G-diff, G-stat, G-unit, G-style, G-perf |
| **M7** | D9, M0, Decision 6 | Replicate-level parallelism with oneTBB (or the Decision 6 alternative): an `om::run_replicates` batch API built on `task_group`, used by the S1 runner and later by Python. Extend G-two-sims: N simulations in parallel equal N serial runs. No change inside a simulation. | G-exact, G-unit, G-style, G-two-sims |
| **M8** | G3, M4, M7 | Parallel human update, the OpenMM "Platform" (serial and threaded). Needs counter-based per-human RNG streams keyed by (human, step, purpose), using the ChaCha generator already in `contrib/`, so results do not depend on thread count; per-thread monitor accumulators reduced in a fixed order; no shared mutable state in `Host::update`. Changes draws on purpose. Record the speed-up curve in `PROGRESS.md`. | G-stat, G-threads, G-unit, G-style, G-perf (serial platform ≤ +3%) |
| **M9** | M1 | Port the remaining CxxTest suites to Catch2, including the 4 suites that are not compiled today, or delete them with a reason. Remove `contrib/cxxtest`. | G-exact, G-unit, G-style |

---

# Part 3: Reference (read the sections a chunk names)

## Workspace facts
- The fork has two remotes: `origin` (blake-armstrong/openmalaria) and `upstream` (OpenMalaria-Org/openmalaria).
- It is currently on `core-only`. `.cache/` and `python/` are untracked and not in `upstream/main`, so they carry over when switching branches untouched.
- The local `upstream/main` ref is stale (#430), so a fetch is required.
- `--no-track` means a bare `git push` can never target the upstream org repo.

## Mapping
| OpenMM | OpenMalaria equivalent |
|---|---|
| `System` | `om::Model`: an immutable description built from plain config types |
| `Force` / `ForceImpl` | Component definitions plus per-simulation `*Impl`, chosen through a name-keyed registry |
| `Integrator` | `Stepper`: the ordered per-step update (interventions → transmission → humans → population → survey) plus the phase logic |
| `Context` | `om::Simulation`: owns the clock, RNG, population, transmission, interventions and monitor; several can exist per process |
| `State` | `om::State`: a snapshot plus a versioned checkpoint blob |
| `Platform` | Later: serial vs. threaded updates |
| `app` / Reporters | `om::io` loaders and writers, plus `Reporter` observers |

## Target layering
```
include/openmalaria/   public C++ API
src/core/              clock, RNG, Simulation, Stepper, registry, errors
src/components/        within_host/, infection/, clinical/, transmission/, interventions/, pkpd/
src/monitoring/        Monitor, Reporters
src/io/xml/            the only place scnXml:: is used
src/io/checkpoint/     versioned State serialisation
apps/cli/              CLI (public API only)
bindings/python/       nanobind module + openmalaria.app   (name to be confirmed, see J)
.claude/skills/        cpp-style, cpp-lint, rename-module
```
Dependency rule: `components` → `core`. `io` → `core`, `components` and `schema`. CLI and Python → the public API only.
Third-party libraries follow the same rule (§ Modernisation policy): only `io/xml` sees Xerces-C and `scnXml`; CLI11 only in `apps/cli`; nanobind only in `bindings/python`; spdlog, Eigen and Boost.Math stay inside `src/`; TBB only in the platform and batch code. No public header includes any of them.

## Modernisation policy

**Language baseline.** The tree already builds as C++20. Use it: `std::span` for array views, `std::format` (never `printf`), `std::numbers` for `M_PI` and friends, concepts in place of SFINAE, designated initializers for the E-stage config structs, `std::optional`/`std::variant` for optional or alternative XML elements. Move to C++23 (`std::expected`, `std::print`, `std::mdspan`) only when every CI toolchain, including the manylinux one for J4, supports what is used; revisit at H1.

**Libraries.**

| Need | Library | Licence | Replaces | Chunk |
|---|---|---|---|---|
| CLI | CLI11 | BSD-3-Clause | `util/CommandLine` | I1 |
| Logging | spdlog (std::format backend) | MIT | `cout`/`cerr` diagnostics | M3 |
| Linear algebra | Eigen 3.4 | MPL-2.0 | `gsl_vector`, `gsl_matrix`, `gsl_blas` | M5 |
| Special functions, roots, quadrature, distributions | Boost.Math (standalone) | BSL-1.0 | the rest of GSL, `contrib/R_nmath` | M6 |
| Random variates | in-tree samplers (or Boost.Random) | BSL-1.0 | `gsl_ran_*` via the `gsl_rng` adapter | M4 |
| Parallelism | oneTBB (Decision 6) | Apache-2.0 | none (new) | M7, M8 |
| Unit tests | Catch2 v3 | BSL-1.0 | CxxTest | M1, M9 |
| Python | nanobind | BSD-3-Clause | none (new) | J1 |
| Checkpoint format | decided in H3 (own versioned binary, or e.g. zpp_bits/cereal) | | `operator&` idiom | H3 |

Licence notes: OpenMalaria is GPL-2.0-or-later. MIT, BSD, BSL and MPL-2.0 code may be combined with it. Apache-2.0 (oneTBB) is compatible with GPLv3 but not GPLv2, so a binary that links oneTBB can only be distributed under GPLv3. That is Decision 6. GSL is GPL, so dropping it is a packaging and API choice, not a licence one.

**Rules for adding a library:**
- One library per chunk, added through `cmake/Dependencies.cmake` (M0), pinned by tag and hash, and recorded in `DEPENDENCIES.md`.
- Behind a thin `om::` wrapper wherever its types would otherwise spread through the model (logger, linear-algebra aliases, task API). Header-only numeric code can be used directly inside one component.
- Swaps that change floating-point results or draws are gated by G-diff plus G-stat, never by "it looks the same".
- G-perf applies. A library that is slower on the hot path is not adopted there.
- Every new dependency must build in CI and, from J4, in cibuildwheel.

**Practices the per-module pass (M2, `/modernize-module`) migrates:**
- Tool-made fix-it commit: `nullptr`, `override`, `using` for `typedef`, `= default`/`= delete`, `make_unique`, range-for, default member initializers, `emplace`, redundant `void`, bool literals, `std::numbers`.
- Hand commit (judgement):
  - owning raw pointers and `new`/`delete` (~90 `new` sites in `model/`) → `std::unique_ptr` or values
  - C arrays → `std::array`, and pointer + length → `std::span`
  - `printf`-family and `ostringstream` formatting → `std::format`
  - macros → `constexpr` or `inline` functions
  - plain enums → `enum class`, except where the integer value is checkpointed or mapped from XML (keep those until H3/E)
  - output parameters → return values or structs
  - remaining `const` gaps (Con.1–4)
- Not in this pass: the checkpoint `operator&` idiom (H3), `scnXml` uses (E), and file statics and globals (D).

## Code style
- Follows the ISO C++ Core Guidelines (isocpp.github.io/CppCoreGuidelines).
- Classes, structs, enums, type aliases and template parameters: `PascalCase`.
- Functions and methods: `snake_case`.
- Variables, parameters, members and namespaces: `snake_case` (NL.10), with no member prefix.
- Constants and enumerators: `PascalCase`. This is assumed; adjust if you prefer otherwise.
- `.clang-tidy` enables `cppcoreguidelines-*`, `bugprone-*`, `modernize-*`, `performance-*` and `readability-identifier-naming`. Every disabled check is recorded with its reason, e.g. hot-loop or checkpoint `operator&` idioms.
- CI runs clang-tidy in diff mode, so legacy code never blocks a change.
- Renames happen per module, as separate commits made by tools, each listed in `.git-blame-ignore-revs`.
- Because the C++ names follow these rules, the nanobind bindings match PEP 8 with no renaming layer.

## Statistical tests

**Why.** The byte-for-byte tests fail on any change in RNG draw order, even a correct one. These tests instead check that a single run falls inside the range that many replicate runs of the reference code produce.

**Facts they build on:**
- The seed comes only from `<computationParameters iseed="N"/>` (`model/main.cpp:154`).
- `output.txt` rows are `survey, group, measure, value`; `output1.txt` has 792 rows.
- Reuse `test/run.py` and `util/compareOutput.py` rather than writing new parsers.

1. **References.** For each scenario:
   - Run N = 500 replicates with seeds 1001…1000+N.
   - For each cell, compute the mean, the SD and the empirical q2.5/q97.5. This is a 95% *prediction* interval for one new run.
   - Use empirical quantiles rather than mean ± 1.96·SD or a fitted distribution, because the outputs are skewed counts and often zero.
   - Leave-one-out: count each replicate's misses against the interval built from the other N − 1, and store all N counts. They are the yardstick for how many misses a correct run has, and they already include the correlation between cells (F17).
   - The key aggregates (clinical episodes, severe episodes, deaths, mean prevalence, mean EIR) get Bonferroni-widened intervals whose family covers 99%. Their quantiles are Hyndman–Fan type 6, whose expected coverage of one new run is the nominal one; type 7 (used for the cells) is too narrow at these far tails (F18, settled in S6).
   - Write a CSV with a metadata header: N, seeds, commit, schema version and date.
2. **Checking a build.** Settled with the user in S3, replacing the original exact-match rule for zero-width cells (F15).
   - **One run fails** if any of these hold:
     - (a) its miss count m is improbable: `p = (1 + #{leave-one-out counts ≥ m}) / (N + 1) ≤ 1%`
     - (b) an aggregate is outside its interval
     - (c) a *fixed* cell differs: a non-zero (or NaN) cell of a survey measure or ctsout column whose every cell was the same in every replicate, such as the input EIR echoed back. A cell that was 0 in every replicate is a rare event, and a non-zero value there is just a miss under (a). So is a constant cell in a measure or column that varies elsewhere, such as ctsout `recent births` (F28, settled in S6).
   - A correct run fails one of these about 2% of the time. Missing or extra cells are an error, not a statistical failure.
   - **Rerun rule, fixed in advance.** Run 1 uses the scenario's own `iseed`, which is outside the reference range. If it passes, the test passes. If it fails, run K = 5 fixed seeds (2001–2005); the test fails if at least one of them also fails. A correct build then fails about 0.02 × (1 − 0.98⁵) ≈ 0.2% of the time; a change that makes every run fail is always caught. Seeds are fixed, so the verdict for a build is deterministic.
   - **Group test** (`--replicates`, added in S7 after F31): K = 20 seeds (2001–2020) run up front. For every *group quantity* (each aggregate, and each survey measure's total over surveys and age groups, whole-population and cohort/genotype groups separately), z = (mean of the K runs − reference mean) / (reference SD · √(1/K + 1/N)). The check fails if the largest |z| exceeds the scenario's threshold, or if any run fails (c). The threshold is the 99.99% quantile of the largest |z| under leave-K-out on the reference replicates (10⁶ resamples of K against the other N − K), so each scenario's family false-failure rate is 0.01% whatever the skew or correlation of the quantities. A quantity is tested only if at least 10 replicates differ from its most common value. Reference format 4 stores every replicate's value of every tested quantity and the threshold. It is the test for changes expected to move outputs only slightly, and it tests means only, not spreads or shapes.
   - The report lists misses sorted by |z|.
3. **Integration.** Each scenario is a CTest test `stat_<scenario>` with label `statistical` (the single-run check with reruns), and `statfull_<scenario>` with label `statistical-full` (the group test), registered for the configuration `full` only, so plain `ctest` skips it: `ctest -C full -L statistical-full`.
   - The byte-for-byte tests stay the oracle for pure refactors.
   - Checkpoint-and-resume stays exact, comparing against the same build and seed.
4. **Validation.**
   - False failures of the whole procedure, reruns included, must be ≤ 0.5%, estimated from the per-run failure rate over 200 fresh seeds.
   - A power study (perturbations of 5%, 10% and 20%, and an intervention switched off) records each scenario's minimum detectable effect, with and without `--replicates`.
5. **Updates.** References are regenerated only for intended changes in behaviour. The regeneration goes in its own commit, with a summary of which measures moved.

## Key risks
- **Reproducibility.** Pure refactors keep G-exact. Deliberate draw-order changes rely on G-stat, which only catches shifts above the minimum detectable effect measured in S6 and S7 (`test/stats/README.md`), and only changes of means.
- **Statistical test cost.** About 50 scenarios × 500 runs is a large one-off job. Batch it using the A2 timings.
- **Performance.** The per-human loop is hot. Dispatch per component, not per human per step, and check G-perf.
- **Scale.** About 32k lines. The bridge pattern and chunk splitting keep each session small.
- **Compatibility.** Users depend on the XML schema and the output formats; keep both stable.
- **Rename churn.** Renames happen per module, in separate commits made by tools.

## Open decisions (ask the user before the chunk that needs them)
1. **Before D2b:** which changes may give up byte-for-byte equality and rely on G-stat instead? (Recommended: only chunks whose purpose is to change draw order.) **Answered (2026-10-21): only chunks whose purpose is to change draws: D2b, M4, M8.** Every other chunk, M5/M6 included, stays G-exact.
2. **Before H1:** are Python bindings a primary goal? If so, move stage J to directly after H2.
3. **Before E1:** does XML stay the primary user format, or is YAML/JSON or Python-built config added?
4. **Before I2:** must the existing CLI flags stay backward compatible, and for how long?
5. **Before M0:** how are third-party libraries obtained? (Recommended: CMake `FetchContent` with `FIND_PACKAGE_ARGS`, i.e. an installed package first, otherwise a pinned fetch. Alternatives: a vcpkg manifest, which also suits cibuildwheel and Windows; or system packages only.)
6. **Before M7:** oneTBB is Apache-2.0, so binaries that link it can only be distributed under GPLv3, not GPLv2. Is that acceptable? (Alternatives with no such issue: Taskflow (MIT), or OpenMP through the compiler.)
7. **Before M5:** how far does GSL go? (Recommended: all the way, i.e. Eigen + Boost.Math + in-tree samplers, then drop GSL, which simplifies wheels and a Windows build. Alternatives: only linear algebra; or keep GSL and only wrap it.)
