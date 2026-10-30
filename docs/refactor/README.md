# Reading the refactor

How to understand, after the fact, what this refactor changed in OpenMalaria, why, and what evidence
shows that it did not change the model's behaviour. The branch is `throwaway_ai_rework`, started from
`upstream/main` (chunk A1); every commit since then is part of the refactor.

## The documents

| File | What it holds |
|---|---|
| `PLAN.md` | The goal, the chunks in order, the gates each chunk must pass, and the decisions taken with the user. Each chunk's row or section has an *As built* note where the result differs from the plan. |
| `PROGRESS.md` | One log entry per chunk, in the order done: base commit, every commit with what it did, gate results with numbers, surprises, follow-ups. Plan amendments and reviews are entries of their own. The table at the top maps chunks to commits. |
| `FINDINGS.md` | Bugs, surprises and odd design found along the way (`F<n>`), whether fixed or not. Behaviour-changing fixes were made only with the user's agreement, in their own commits; § Closed says where. |
| `STYLE.md` | The naming and formatting rules, the clang-tidy configuration with the reason for every disabled check, and how the rename and modernise tools work. |
| `DEPENDENCIES.md` | Every third-party library: version, licence, why, and which code may use it. |
| `test/stats/README.md` | The statistical tests: method, false-failure rates, and the smallest change each scenario can detect. |

## How a change is made, and how to trace it

Each chunk moves one family of global state into an object owned by `Simulation`, or does one
modernisation step. Its commits come in a fixed pattern, so each kind can be read on its own:

1. **Logic commit**, subject `D<n>: …` (or the chunk's name). The only commits that change how the
   code is structured. The message says what moved where and that every read, draw and error happens
   in the old order.
2. **Rename commit**, `Rename <module> identifiers to the naming rules`, made by `util/style.sh rename`
   and marked `Mechanical:` in its message. A token check in the tool proves the diff is only the
   planned renames plus whitespace. Names the tool cannot see (inside disabled `#ifdef` blocks) are
   renamed in a separate small hand commit.
3. **Modernise tool commit**, `Modernize <module> with the safe fix-its`, made by
   `util/style.sh modernize`, also `Mechanical:`, with the count per clang-tidy check.
4. **Modernise hand commit**, `Modernize <module> by hand`: the judgement items, each listed in the
   message with what was left and why.
5. **Docs commits**: FINDINGS, PLAN as built, then `PROGRESS: <chunk> done`.

The tool-made commits (2, 3) and hand renames are listed in `.git-blame-ignore-revs`, which this
clone's `blame.ignoreRevsFile` points at, so `git blame` shows the logic commit that last changed a
line rather than a rename. Useful commands:

```sh
git log --oneline upstream/main..throwaway_ai_rework           # every refactor commit
git log upstream/main..throwaway_ai_rework --grep '^Mechanical:' --oneline   # tool-made commits
git log --oneline upstream/main..throwaway_ai_rework -- model/Host/WithinHost/WHVivax.cpp
git diff upstream/main throwaway_ai_rework -- model/            # the whole change to the model
```

During the D stage, old static accessors were a *bridge*: each delegated through a global pointer to the
object `Simulation` owns, so their call sites could move one chunk at a time. D9a–D9e5 moved every call site and
removed the bridges; `util/bridges.py` finds none.

## The evidence that behaviour is unchanged

| Gate | What it shows | What it does not show |
|---|---|---|
| **G-exact** (`ctest` with `-DOM_BOXTEST_STRICT=ON`) | Every scenario in `test/` gives `output.txt` and `ctsout.txt` byte-identical to `test/expected/`, including after a checkpoint stop and resume (except the scenarios in `OM_BOXTEST_NC_NAMES`, currently PkPdModels, whose resume is broken by F56). Strict since b7b057e1. Before that the comparison allowed 1e-6 (F49): the chunks since the last regeneration of `test/expected/` (M4, 1856ab43), i.e. D3, D4a, D4b and D4c, are known to be byte-exact because the outputs at b7b057e1 are byte-identical to it; for earlier chunks only the 1e-6 comparison is on record. | Code paths no scenario reaches; stdout, stderr and exit codes; checkpoint file contents. |
| **G-unit** | The CxxTest and Catch2 unit tests pass. | Several suites pin values tied to draw order (F39) or depend on test order (F30). |
| **G-two-sims** (`two_sims` since D9a, `two_sims_interleaved` since D9f, `two_sims_parallel` and the ThreadSanitizer run since M7a) | Scenarios run in one process (`openMalaria-two-sims`) each give `output.txt` and `ctsout.txt` byte-identical to `test/expected/`: `two_sims` runs them one after another, so no state leaks from one `Simulation` to the next (on the code before D9a it crashed or differed, F81); `two_sims_interleaved` keeps them alive together, taking turns one time step at a time, so no state is shared between live simulations (a planted construction-bound clock pointer, the old bridges' failure mode, fails it and not `two_sims`); `two_sims_parallel` runs 18 simulations (two scenarios twice) on 4 threads at once, and `util/gates.sh --tsan` runs all three tests in a ThreadSanitizer build (`build-tsan/`), which fails on any data race, including on state that reaches no output (a planted write-only static, M7a). | Races on paths those scenarios do not take, and on code not built with the sanitizer (GSL, Xerces); the per-human update on several threads (M8); checkpoint resume in the same process. |
| **G-stat** (`stat_*`, `statfull_*`) | For chunks that change random draws on purpose (D2b, M4, M8), results stay within the statistical spread of 500 reference runs per scenario (`test/reference/`). | Changes smaller than each scenario's detectable effect (`test/stats/README.md`); changes in spread rather than mean. |
| **G-cli** (`util/ab_cli.sh`, since M3a) | The `cli` group of `util/ab_scenarios.py`: scenario5 and scenarioVecTest with default, `--verbose --progress`, `--checkpoint` and `--checkpoint-stop` (each run twice, so checkpoints resume), every other flag and `--print-*` option, command-line, schema and missing-file errors, and a truncated and an extended checkpoint resumed. Base and candidate give byte-identical stdout, stderr (including `\r`), exit codes, outputs and checkpoint files. A chunk that changes console output on purpose (F1 and F2 in M3a; F94, F55 and F97 in M3b) shows with `util/ab_expected.py` that every variant changed exactly as the named transform says; a transform may compare with the base's run of the same variant without one flag (`f97`). | The interleaving of stdout and stderr (they are compared separately; the logger flushes after each message, as `endl` did). |
| **G-style**, **G-perf** | Code style on changed lines; run time within 3% of the base. | Behaviour. |
| **CI** (`.github/workflows/`, since B6) | Each push builds and runs `ctest` (scenarios in binary-output mode, unit and statistical tests) on macOS 14 and 15 (arm64) and Ubuntu 24.04 (x86-64 and arm64), and runs G-style against `main` with Ubuntu's `xsd` headers. Builds use `-ffp-contract=off` (F62), so all three platforms compute the same outputs. | Byte-identity: CI compares scenarios within 1e-6, and x86-64 prints some NaNs as `-nan` (F65). |
| **A/B runs** (`util/ab_scenarios.py`) | For paths ctest cannot see, a base and a candidate build give identical stdout, stderr, exit codes, outputs, checkpoint files and the CSV files a run writes on crafted scenario variants: init error paths, vivax health-system and liver-stage paths and its `nTransmit` report, the genotype, diagnostic and treatment registries (including `--print-genotypes` with a one-allele locus, F51, and its table layout with 200 genotypes and long or unnamed phenotypes, F86, the log-density measures with a monitoring diagnostic that allows false positives), the PK/PD models no ctest scenario uses, vivax non-malaria fevers, the order in which the pathogenesis and clinical parameters are read, the event scheduler with non-malaria fevers, health-system errors and `changeHS` under each clinical model, checkpoint bytes per clinical model, a decision-tree intervention that reaches newborns before their first clinical update, the decision-tree nodes and build errors no scenario reaches (infectionOrigin, age and cohort nodes, caseType checks, errors in two parts of one node, and the node pairs the library merges, F68), and the transmission paths (checkpoint bytes of a three-species vector scenario, `--print-EIR`, the vector-model and availability init errors in their read order, an availability percentile above 100, the Gaussian copula deployment, which ctest reaches only since F73's new scenarios, and `changeEIR` with the vector model and at the exact number of days each model requires, F92), and the host paths (the infection-incidence heterogeneity workaround, the negative binomial model and its checkpoint bytes, the LogNormal and negative binomial parameter-16 errors, the read order of the incidence parameters, the "new infections" counter in checkpoint bytes, and an altered demography for the age-structure fit), and the intervention paths (`--print-interventions` and `--verbose` output, checkpoint bytes with interventions, which vaccine component reports deployments, `clearImmunity`, deployments triggered by a treatment (a vaccine, with age limits, and an ITN, IRS and GVI, F87, also a GVI with an effect), a decision tree deploying a cohort that expires, the VACCINE_GENOTYPE phenotype errors and multi-locus vaccine phenotypes (F83), a vaccine on two genotypes without that option across a checkpoint, BSV and `treatSimple` components in a vivax scenario, a timed liver-stage clear between steps in a common and a descriptive model, each sub-population removal option alone, continuous availability filters and `begin`/`end` windows, and the intervention init errors, including decision-tree cohort and deploy nodes that name a component defined later or themselves), and the monitoring paths (ctsout with `duringInit` across a checkpoint, the continuous-output and survey errors, `--print-survey-times` with repeated and unreported surveys, binary and compressed output, two cohorts across a checkpoint, and deployment conditions across a checkpoint and on an unreported measure), and the unit-parsing paths (`--sample-interpolations` and its CSVs, also rewritten by a `changeHS`, `--no-deprecation-warnings` through the `changeHS` and decay parsing, a `changeHS` date long after the start, and a decay `L` in steps across a checkpoint), and the console messages no other variant prints (group `logging`, M3b: an old schema version, too few non-vector `changeEIR` days, `uninfectVectors` under forced EIR, the ITN and IRS advisories, F98, and `--print-genotypes` with `--print-EIR`, F97). | Only the variants listed in the script. The source file and line a traced exception prints, which move with any edit. |

Where a chunk found that ctest did not reach the code it changed, it added a scenario test whose
expected output was generated by the build before the chunk, and showed the test fails on planted
bugs (e.g. the three vivax liver-stage scenarios, 9315b35d). The `PROGRESS.md` entry of each chunk
records which of these checks ran and their results.

Chunks that change outputs on purpose (D2b, M4, V1's warmup fixes, B6's `-ffp-contract=off`), and the behaviour-changing bug fixes
listed under FINDINGS § Closed (e.g. F59, F60, F68, F71, F72), say so in their commit messages,
were agreed with the user first, and regenerate `test/expected/` or the statistical references in
commits of their own that list which measures moved.

## Re-running the evidence

```sh
cmake -S . -B build-rework -DCMAKE_BUILD_TYPE=Release -DOM_BOXTEST_STRICT=ON
cmake --build build-rework -j4
cd build-rework && ctest -j4                      # G-exact, G-unit, G-stat (single-run)
ctest -C full -L statistical-full -j1             # G-stat group test, ~6 min

# G-two-sims under ThreadSanitizer (configures build-tsan with -DOM_SANITIZE=thread if missing):
util/gates.sh --release-only --tsan

# Or every gate at once, one line each (also builds build-debug and build-tsan; logged in build-rework/gates.log):
util/gates.sh --all <base commit> --base ../om-base/build-rework   # A/B and G-perf against that build

# A/B of the crafted variants against any earlier commit:
git worktree add ../om-base <commit>
cmake -S ../om-base -B ../om-base/build-rework -DCMAKE_BUILD_TYPE=Release
cmake --build ../om-base/build-rework -j4 --target openMalaria inlined_xsd
python3 util/ab_scenarios.py ../om-base/build-rework/openMalaria build-rework/openMalaria
util/ab_cli.sh ../om-base/build-rework                          # G-cli
python3 util/ab_expected.py ../om-base/build-rework/openMalaria build-rework/openMalaria progress,f2
                                                                # a deliberate console change
```
