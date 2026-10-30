# Third-party dependencies

Every library and tool the build uses, with the version tested here, its licence, how
it is obtained, and which code may use it. A chunk that adds, upgrades or drops a
dependency updates this file in the same commit. Policy: PLAN.md § Modernisation policy.

## How libraries are obtained (Decision 5)

New libraries come through `om_dependency()` in `cmake/Dependencies.cmake`, which the root
`CMakeLists.txt` includes:

```cmake
om_dependency (Catch2 VERSION 3.x.y
  URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.x.y.tar.gz
  SHA256 <sha256 of that tarball>
  [FIND_PACKAGE_ARGS <extra find_package arguments>...])
```

- It first tries `find_package(<name> <VERSION> GLOBAL ...)`. An installed package
  (Homebrew, apt, conda, a prefix on `CMAKE_PREFIX_PATH`) wins if it satisfies the
  version, under that package's own version-compatibility rule.
- Otherwise it downloads the pinned tarball into `<build>/_deps/`, checks the SHA256 (a
  mismatch fails the configure) and adds it with `add_subdirectory`.
- It prints `Dependency <name>: using installed <v> from <dir>` or
  `Dependency <name>: fetched <v> into <dir>`.
- `VERSION`, `URL` and `SHA256` are required. Pin a release tarball, not a branch or a
  bare tag, so the hash means something. `VERSION` is the tarball's version, which is
  also the minimum accepted from an installed package.
- Set build options of a fetched project (e.g. `CATCH_INSTALL_DOCS`) as normal
  variables before the call.
- `NO_FIND_PACKAGE` skips `find_package` and always fetches the pinned tarball. Use it
  only when installed packages are built in a way the tree cannot use: spdlog's
  Homebrew, apt and conda packages are built against fmt, while this tree needs
  `SPDLOG_USE_STD_FORMAT`, which changes spdlog's ABI.

Standard FetchContent cache variables control it from the command line:

| Variable | Effect |
|---|---|
| `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` | always fetch the pinned version (reproducible, ignores installed packages) |
| `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` | no network; everything must be installed or already in `_deps/` |
| `-DFETCHCONTENT_SOURCE_DIR_<UPPERNAME>=<dir>` | use a local source tree for one dependency |
| `-DCMAKE_PREFIX_PATH=<prefix>` | where `find_package` looks first |

Tested on a scratch project with `fmt` 12.2.0 (Homebrew has it installed): default →
installed copy used; `TRY_FIND_PACKAGE_MODE=NEVER` → fetched and built; a minimum
version above the installed one → fetched; a wrong SHA256 → configure fails with
"Hash mismatch".

The pre-existing dependencies below still use the hand-written `cmake/Find*.cmake`
modules. They move to `om_dependency` or standard `find_package` only when a chunk has
a reason to touch them (e.g. M6 drops GSL).

## Toolchain

| Tool | Minimum | Tested here | Notes |
|---|---|---|---|
| CMake | 3.24 (since M0) | 4.4.2 | 3.24 for `FetchContent_Declare(FIND_PACKAGE_ARGS)`. Ubuntu 24.04 apt has 3.28. Ubuntu 22.04 apt has 3.22, which is too old: use `pip install cmake`, Kitware's apt repository or snap. GitHub's hosted runners preinstall a newer CMake in `/usr/local/bin`, ahead of apt's on `PATH`. |
| C++ compiler | C++20 | Apple clang 21.0.0 | CI style job: Ubuntu 24.04 clang |
| Python 3 | any 3.x | system | build-time only: `cxxtestgen`, `test/run.py`, schema inlining |
| LLVM clang-format / clang-tidy | 22.1.8, pinned | 22.1.8 | via `uvx`, see STYLE.md |

`docker/Dockerfile` builds on Ubuntu 22.04 with apt's CMake 3.22. It clones and builds
the upstream `schema-50` branch, not this tree, so it is not affected yet. It will
need a newer CMake before it builds code from this branch.

## Libraries

| Library | Version tested | Licence | Obtained | Used by | Fate |
|---|---|---|---|---|---|
| GSL | 2.8 (Homebrew); 2.7.1 on Ubuntu 22.04/24.04 | GPL-3.0-or-later | system, `cmake/FindGSL.cmake` | `model/`: `util/random.h` (`gsl_cdf_ugaussian_Pinv`), `util/sampler.cpp` and `interventions/Deployments.h` (CDFs), `WHFalciparum`, `LSTMDrugConversion`, `LSTMDrugThreeComp`, `PopulationAgeStructure`, `AnophelesModel`, `CalcNvO`, `TransmissionModel.h`; `unittest/MolineauxInfectionSuite.h`; `unittest/RandomVariatesTests.cpp` (the old `gsl_ran_*` samplers and `gsl_cdf_*` as G-diff yardsticks) | samplers replaced by in-tree `util/random_variates.h` in M4; linear algebra → M5; the rest → M6, then dropped (Decision 7). M6 moves the G-diff test's CDFs to Boost.Math and drops its `gsl_ran_*` comparisons. |
| Xerces-C | 3.3.0 (Homebrew); 3.2.x on Ubuntu | Apache-2.0 | system, `cmake/FindXercesC.cmake` | the generated `schema` target, through the XSD runtime; no direct use in `model/` | only `io/xml` after E6 |
| CodeSynthesis XSD (`xsd` / `xsdcxx`) | 4.2.0 (Homebrew); 4.0.0 on Ubuntu | compiler GPL-2.0; header-only runtime GPL-2.0 with the FLOSS exception | system, `cmake/FindXSD.cmake` | generates `schema/` (`cxx-tree --std c++11`); runtime headers reach every file that includes a `scnXml` header | only `io/xml` after E6 |
| zlib | 1.2.12 (macOS SDK); 1.3 on Ubuntu 24.04 | Zlib | system, `cmake/FindZ.cmake` | `contrib/gzstream` only | stays while gzstream does |
| gzstream | 1.5, in tree (`contrib/gzstream/`) | LGPL-2.1-or-later | vendored | `model/checkpoint.cpp`, `model/mon/Monitoring.cpp` | revisit at H3 (checkpoint format) |
| R nmath `qnorm` | from R, in tree (`contrib/R_nmath/`) | GPL-2.0-or-later | vendored | `model/interventions/ITN.cpp`, `IRS.cpp` | Boost.Math → M6, then removed |
| ChaCha RNG | in tree (`contrib/chacha.h`), modified (see its header) | Zlib | vendored | `model/util/random.h`, `unittest/ChaChaSuite.h` | stays; per-human streams in M8 |
| xoshiro | in-tree port (`model/util/xoshiro.h`) of the Blackman/Vigna reference generator | reference code public domain (CC0); port GPL-2.0-or-later as part of OpenMalaria | in tree | `model/util/random.h`, `unittest/XoshiroSuite.h` | stays |
| Random variates | in tree (`model/util/random_variates.h`, M4): polar method (normal), Marsaglia–Tsang (gamma), Jöhnk or gamma ratio (beta), multiplication or Hörmann's PTRS (Poisson), inversion (Weibull) | GPL-2.0-or-later as part of OpenMalaria; written from the published algorithms, no code copied | in tree | `model/util/random.h` | stays; replaces `gsl_ran_*`, not `std::` distributions (their output differs between standard libraries) |
| spdlog | 1.17.0 | MIT | `om_dependency(NO_FIND_PACKAGE)`, always fetched (M3), built with `SPDLOG_USE_STD_FORMAT=ON`, so no fmt | `model/util/logging.cpp` only, linked `PRIVATE` to `model`; no header in `model/` includes it. `model` sets `SPDLOG_ACTIVE_LEVEL` (debug in Debug builds, info otherwise) `PUBLIC`, so `logging.h` compiles out debug and trace calls in Release | stays |
| CxxTest | 4.4, in tree (`contrib/cxxtest/`), modified | LGPL-3.0 | vendored | `unittest/*Suite.h` only (`cxxtestgen` needs Python) | new tests are Catch2 since M1; removed in M9 |

`contrib/README.txt` still says CxxTest 3.10.1 under LGPL-2.1. The tree has had 4.4,
under LGPL-3.0, since 826bd5a8 (2019; refreshed in #398, 58dae029), and `contrib/cxxtest/COPYING` is LGPL-3.0.

## Fetched libraries (`om_dependency`)

| Library | Pinned | Licence | Declared in | Used by | Notes |
|---|---|---|---|---|---|
| Catch2 | 3.16.0 (2026-08-25), `catchorg/Catch2` tag tarball, SHA256 `0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34` | BSL-1.0 | `unittest/CMakeLists.txt` (only with `OM_CXXTEST_ENABLE`) | the `unittest-catch2` test executable (`Catch2::Catch2WithMain`) | since M1; replaces CxxTest in M9 |

Catch2 notes:
- An installed Catch2 is accepted from 3.16.0 up within major version 3 (Catch2's own
  `SameMajorVersion` rule). Ubuntu 24.04's apt package is 3.4.0 and Homebrew has none
  installed here, so both the CI style job and local builds fetch it. That is one
  1.2 MB download per fresh build dir, at configure time. CI's install step is
  unchanged.
- Tested both ways: fetched (default here) and installed into a scratch prefix
  (`-DCMAKE_PREFIX_PATH=<prefix>`). Both build and pass.
- As a subproject Catch2 installs nothing (its `install()` rules need it to be the
  top-level project), so it does not leak into H2's install rules.
- `catch_user_config.hpp` is generated at configure time, so clang-tidy can parse the
  tests after a configure plus the `schema` build, which is what `util/ci-style.sh` does.

## Licence summary

OpenMalaria's own code is GPL-2.0-or-later. Two existing link dependencies already fix
the licence of a distributed `openMalaria` binary at **GPLv3**:
- GSL is GPL-3.0-or-later.
- Xerces-C is Apache-2.0, which the FSF regards as compatible with GPLv3 but not GPLv2.

So oneTBB (Apache-2.0, Decision 6) would not narrow the licence of today's binaries. It
would matter only for a build without GSL and Xerces-C, e.g. a Python wheel after M6
whose XML layer is optional. Record this when Decision 6 is asked.

CxxTest (LGPL-3.0) and Catch2 (BSL-1.0) link only into the test binaries, which are not
distributed.
