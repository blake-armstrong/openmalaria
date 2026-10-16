# Findings register

Bugs, surprises and strange design decisions found in the code, tests or build while working through the plan. `PROGRESS.md` says what each chunk did; this file keeps the issues themselves in one place, so they are not lost in old log entries.

Rules (also in `PLAN.md` § Session protocol):
- Add an entry when you find something, even if it is out of scope for the chunk. Give it the next free ID; never reuse IDs.
- Kinds: **bug** (wrong behaviour), **surprise** (correct but unexpected, and likely to trip someone up), **design** (a strange or questionable decision worth revisiting).
- Don't fix a finding that changes observable output (stdout, stderr, outputs, exit codes) inside a refactor chunk. Record it, and ask the user before fixing it in its own commit.
- When an entry is fixed or settled, move it to § Closed with the commit or chunk.

## Open

- **F1** (bug, found C2): `--progress` prints one `\r N%` line per step instead of one per percent. `print_progress` takes `last_percent` by value, so the caller's copy never changes (`model/Simulation.cpp`). Fixing it changes stderr only. Planned for M3 if the user agrees.
- **F2** (bug, found S1): after any caught exception, `main()` calls `perror("OpenMalaria")` whenever `errno != 0` (`model/main.cpp:119`). `errno` is often left over from an earlier, harmless failed file lookup, so error reports end with an unrelated `OpenMalaria: No such file or directory`. Changes stderr; candidate for M3 or I2.
- **F3** (surprise, found S1): invalid input can be reported as a C++ bug. `popSize="-5"` in `scenario1` gives `Code error: initialKappa is invalid … This is likely an error in the C++ code. Please report!` (from `NonVectorModel.h:107`), with exit 75, instead of an input-validation error. Scenario values aren't checked before they reach model code. Relevant to E (config boundary validation).
- **F4** (bug, found S1): `test/run.py` `findFile` returns from inside its loop, so it gives an arbitrary existing candidate from a `set`, not the newest as intended. Harmless while only one `openMalaria` exists in the build dir.
- **F5** (bug, found S1): `test/run.py` prints "No output 'output.txt'; error messages:" and then reads `stderr.txt` but throws the content away (`se.read()` is never printed). openMalaria doesn't write `stderr.txt` anyway, so the branch is dead.
- **F6** (design, found S1): `test/run.py` replaces `sys.path[0]` with `util/` at import time, so any script that imports it loses its own directory from the import path. `run_replicates.py` restores `sys.path` around the import.
- **F7** (surprise, found S1): binary survey output decoded by `binaryToText` lists all integer measures before the double ones. The rows match text output only when matched by (survey, group, measure), not line by line. `compareOutput.py` matches by key, so ctest's `--binary` mode is unaffected.
- **F8** (surprise, found A2): 6 of the 57 `test/scenario*.xml` files are not registered in `test/CMakeLists.txt` (2, 3, IPT, VT, MultiVecPopInterventions, VecLC*), some disabled for unknown reasons ("TODO: since when?"). They are not covered by any gate.
- **F9** (surprise, found B4): 4 CxxTest suites (AnophelesModel, DescriptiveInfection, ESCaseManagement, MosqLifeCycle) are not compiled, so renames and refactors can't reach them and they may have rotted. M9 ports or deletes them.
- **F10** (surprise, found M0): GSL is GPL-3.0-or-later and Xerces-C is Apache-2.0, so a distributed `openMalaria` binary is effectively GPLv3-only already, although the project says GPL-2.0-or-later. Bears on Decision 6.
- **F11** (surprise, found M0): `contrib/README.txt` is stale (CxxTest 3.10.1 under LGPL-2.1; the tree has 4.4 under LGPL-3.0). `DEPENDENCIES.md` is the source of truth.
- **F12** (surprise, found M0): `docker/Dockerfile` uses Ubuntu 22.04's CMake 3.22, below the new 3.24 minimum. It builds upstream's `schema-50` branch, so it is unaffected for now.
- **F13** (design, found M1): the `OM_CXXTEST_ENABLE` option now also enables the Catch2 tests. Rename it in M9.
- **F14** (design, found C2): stdout uses `std::endl` (flush on every line) in many places, and changing it would change the interleaving of stdout and stderr. It is kept with `NOLINT(performance-avoid-endl)` until M3 moves console output to spdlog.

## Closed

(none yet)
