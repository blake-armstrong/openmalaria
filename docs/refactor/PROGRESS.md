# Refactor progress

See `PLAN.md` for the session protocol, chunks and gates. Append one log entry per chunk.

## Status

| Chunk | Status | Commits |
|---|---|---|
| A1 | done | cfcb16e3, (this log entry) |
| A2 | done | (this log entry) |
| B1 | todo | |
| B2 | todo | |
| B3 | todo | |
| B4 | todo | |
| B5 | todo | |
| P1 | todo | |
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
