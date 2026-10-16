# C++ style tooling

Config: `.clang-format`, `.clang-tidy` (repo root). Runner: `util/style.sh`.

## Pinned LLVM version

**LLVM 22.1.8** for both `clang-format` and `clang-tidy`, taken from the PyPI wheels
(`clang-format==22.1.8`, `clang-tidy==22.1.8`) and run through `uvx`, so nothing is
installed globally. The version is set once, in `LLVM_VERSION` at the top of
`util/style.sh`. The `clang-tidy` wheel also ships `clang-tidy-diff.py`,
`run-clang-tidy.py` and `clang-apply-replacements`. The `clang-format` wheel ships
`git-clang-format`. `.pre-commit-config.yaml` pins the same version (see § Pre-commit and CI).

## Usage

```sh
cmake -S . -B build-rework ...          # compile_commands.json is now exported by default
export OM_BUILD_DIR=build-rework        # default is build/

util/style.sh format -i model/Foo.cpp   # clang-format, project config
util/style.sh tidy model/Foo.cpp        # clang-tidy -p $OM_BUILD_DIR
util/style.sh check [base]              # G-style gate, lines changed since base (default HEAD)
util/style.sh changed [base]            # model/ and unittest/ C++ files changed since base
util/style.sh format-changed [base [paths...]]  # clang-format -i the changed files
util/style.sh fix [base] [checks]       # apply clang-tidy fix-its on changed lines
util/style.sh rename <dir> [--dry-run] [--map OLD=NEW] [--skip OLD]  # rename one module
util/ci-style.sh [base-ref]             # the CI job: check against the merge base with base-ref
```

`format-changed` runs `clang-format -i` on the whole of each changed file, and the format
half of `check` checks whole changed files, so a formatted file always passes. Both
worked on changed lines only (`git-clang-format`) until B5. That was dropped because
line-range results depend on which lines are in range: trailing comments that the PkPd
rename aligned for `HEAD^..HEAD` failed `check` against the branch's merge base, and a
blank line before a namespace's closing `}` that whole-file formatting keeps was flagged.
Since the B2 reformat every file is clean whole-file, so this costs nothing.

`fix` defaults to a safe list of checks whose fix-its are local and don't change
behaviour (`util/style.sh -h` prints it). Pass a `-checks` filter to apply others one at
a time. The fixes are exported and applied once with `clang-apply-replacements`, because
`clang-tidy-diff.py` runs one clang-tidy per file in parallel, and two of them can fix
the same header. After any fix, it runs `format-changed`.

The Claude Code skills `/cpp-style` (rules for writing code), `/cpp-lint` (fix,
rebuild, triage, report) and `/rename-module` (one module's rename commit) in
`.claude/skills/` wrap these commands.

`check` runs clang-format on the `model/` and `unittest/` C++ files changed since `base`
(printing a diff for each one that fails), then `clang-tidy-diff.py` on the changed lines.
clang-tidy looks only at changed lines because legacy names are not fixed until each
module's rename. It exits non-zero on any finding. It reads `git diff`, so new files must
be staged (or `git add -N`) to be seen.

Between `git diff` and `clang-tidy-diff.py`, `util/tidy_diff_filter.py` drops the
changed lines that only bulk mechanical commits touched. When `base..HEAD` contains a
commit listed in `.git-blame-ignore-revs`, it keeps only the added lines that
`git blame --ignore-revs-file .git-blame-ignore-revs` (on the working-tree contents)
attributes to another commit in the range or to uncommitted changes. Lines that blame
can't move off an ignored commit stay on it and are dropped too. With no ignored commit
in the range, the diff passes through unchanged. `fix` uses the same filter. Without it,
`check` against this branch's merge base with `upstream/main` (171 files changed by B2
and the PkPd rename) reported 3575 distinct legacy findings; with it, none.

On macOS, `util/style.sh` adds `-isysroot $(xcrun --show-sdk-path)` to clang-tidy.
CMake 4 no longer writes `-isysroot` into `compile_commands.json`, and the PyPI
clang-tidy cannot otherwise find the SDK's libc++ headers (`'cstdint' file not found`).

For a whole-tree tidy run, anchor the file regex to the repo root. Otherwise the
generated CxxTest runner `<build>/unittest/tests.cpp` matches too:

```sh
uvx --from clang-tidy==22.1.8 run-clang-tidy.py -p build-rework -j 10 -quiet \
    -extra-arg-before=-isysroot$(xcrun --show-sdk-path) "^$PWD/(model|unittest)/"
```

## clang-format

LLVM base with the following overrides. Each override was chosen to minimise churn
in the B2 reformat, measured over `model/` and `unittest/` (36.6k lines):

| Option | Value | Why |
|---|---|---|
| `IndentWidth` | 4 | current style |
| `ColumnLimit` | 120 | 100 reformats ~1.4k more lines; long lines are common |
| `AccessModifierOffset` | -4 | `public:` flush with `class`, as now |
| `NamespaceIndentation` | None | matches most files |
| `FixNamespaceComments` | false | would add a `// namespace X` line to every file |
| `PointerAlignment` | Left | Core Guidelines NL.18 (`T* p`) |
| `SortIncludes` | Never | include order may matter (e.g. `Global.h` first); not a formatting concern |
| `IncludeBlocks` | Preserve | same |
| `ReflowComments` | Never | saves ~2k lines of churn, and keeps comment text as written |
| `AllowShortIfStatementsOnASingleLine` | Never | LLVM default, made explicit |
| `AllowShortLoopsOnASingleLine` | false | LLVM default, made explicit |

Even so, the B2 reformat (0bf623e3) changed 171 files (+13,162 / -14,526 lines). The
old spacing was too inconsistent for any config to match it.

## Blame

Bulk mechanical commits (the B2 reformat, the per-module renames) are listed in
`.git-blame-ignore-revs`. Enable it once per clone:

```sh
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

### Format exclusions

| Path | How | Reason |
|---|---|---|
| `contrib/` | `contrib/.clang-format` with `DisableFormat: true` | third-party code (cxxtest, gzstream, R_nmath, chacha) |
| `schema/` | `schema/.clang-format` with `DisableFormat: true` | XSD sources; the generated `scnXml` C++ lives in the build dir |

## clang-tidy

Enabled: `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`,
`readability-identifier-naming`.

Headers are reported only when they match `.*/(model|unittest)/.*` and do not match
`.*/(contrib|schema)/.*`. This excludes the generated `<build>/schema/*.h`, the
`contrib/` headers and system headers.

### Disabled checks

| Check | Count* | Reason |
|---|---|---|
| `bugprone-easily-swappable-parameters` | 93 | Model functions take several `double`s by design. Not actionable without strong unit types. |
| `cppcoreguidelines-avoid-magic-numbers` | 1137 | Model coefficients from published fits, unit conversions and RNG bit shifts are literal by nature. Naming each one does not help readability. |
| `cppcoreguidelines-owning-memory` | 78 | Needs `gsl::owner<>` from the Guidelines Support Library, which the project doesn't use. Ownership is fixed with `unique_ptr` where it matters. |
| `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access` | 715 | Wants `.at()` in place of `operator[]`. Nearly all hits are in per-human / per-step hot loops, where the bounds checks would cost G-perf. |
| `cppcoreguidelines-pro-bounds-constant-array-index` | 110 | Same hot-loop indexing, and it needs `gsl::at`. |
| `modernize-use-nodiscard` | 197 | Would annotate every getter. Too noisy for the value. |
| `modernize-use-trailing-return-type` | 833 | Pure style preference, not a Core Guideline. It would rewrite every signature. |

\* Distinct diagnostics over `model/` and `unittest/` before disabling, LLVM 22.1.8.

### Kept on, with expected legacy hits

These stay enabled. CI runs in diff mode (B5), so legacy code doesn't block changes,
but new code must comply or carry a targeted `NOLINT(check)`.

- `cppcoreguidelines-avoid-non-const-global-variables` (167): removing the globals is
  stage D's purpose. The temporary D-stage bridge pointer should use `NOLINT`.
- `cppcoreguidelines-init-variables` (78): includes the checkpoint read idiom
  `T x; x & stream;`. The fix-it (`T x = 0;`) is safe.
- `cppcoreguidelines-pro-type-reinterpret-cast`: binary checkpoint I/O and RNG seeding
  (`util/checkpoint.cpp`, `util/random.h`, `util/xoshiro.h`). Use `NOLINT` there.
- `cppcoreguidelines-pro-type-union-access` (136): all in `interventions/ITN.cpp`.
- `cppcoreguidelines-pro-bounds-pointer-arithmetic` (51): mostly
  `Transmission/Anopheles/CalcNvO.cpp`, which works on GSL (GNU Scientific Library)
  C arrays.
- `performance-avoid-endl` (168): replacing `endl` changes flush timing of output files,
  so leave the legacy hits alone unless the change is meant.

### Options

- `cppcoreguidelines-special-member-functions.AllowSoleDefaultDtor: true`, so a
  lone `virtual ~X() = default;` does not demand all five special members.
- `cppcoreguidelines-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic: true`:
  plain structs are fine.
- `cppcoreguidelines-avoid-do-while.IgnoreMacros: true`: `do { ... } while (false)` is
  the standard way to make a macro body one statement. Every Catch2 assertion
  (`REQUIRE`, `CHECK_THAT`, ...) expands to it, so without this option every assertion
  in a Catch2 test is a finding (M1).

### Naming (`readability-identifier-naming`)

| Kind | Case | Example |
|---|---|---|
| classes, structs, unions, enums, type aliases, typedefs, template parameters, concepts | `PascalCase` (acronyms allowed) | `Population`, `WHFalciparum` |
| functions, methods | `snake_case` | `create_initial_humans()` |
| variables, parameters, members (no prefix), local and parameter constants, non-static const members | `snake_case` | `cumulative_pop` |
| namespaces | `snake_case` | `om::within_host` |
| global, static, class-static and `constexpr` constants | `PascalCase` | `MaxDurationDays` |
| enumerators (scoped and unscoped) | `PascalCase` | `Parameter::SigmaISq` |
| macros | `UPPER_CASE` | `OM_ASSERT` |

Legacy hits: 3176 naming diagnostics in `model/` and `unittest/`. These include 437
methods, 144 free functions, 426 members and 32 namespaces (e.g. `OM`, `WithinHost`).
They are fixed per module by the rename commits (B4 onward), not here.

## Module renames (`util/style.sh rename`)

Legacy names are fixed one module at a time by `util/style.sh rename <dir>`
(`util/rename_module.py`), driven by the `/rename-module` skill. Each rename is one
commit, listed in `.git-blame-ignore-revs`. The pilot was `model/PkPd` (B4).

How it works, and why:
- `readability-identifier-naming` runs over **every** TU in the compile database,
  including the generated CxxTest runner, because a symbol's uses in other modules and in
  `unittest/` headers are only seen from TUs that include them. The exported fixes are
  filtered to symbols declared in the module, merged, and applied once with
  `clang-apply-replacements`.
- Namespaces are renamed only if the module is the only place that declares them. A
  namespace's diagnostic lands on whichever declaration a TU sees first, so it can't be
  filtered by location. `OM`, `WithinHost`, `Host` and the like are left for a later
  rename of their own.
- clang-tidy's case conversion keeps `m_` and `_` prefixes (`m_cumulative_Y` →
  `m_cumulative_y`), and doesn't flag `m_drugs` at all, since it is valid lower case. A
  second pass with every member kind set to `UPPER_CASE` finds the locations of all
  members, and the tool strips the prefix from those that have one.
- clang-tidy misses two kinds of use, which the tool adds textually:
  - the out-of-line definition of an override (`LSTMDrugOneComp::getIndex`). The
    in-class override declaration is renamed with the base method, but the definition
    is not. The tool renames `Class::old` for every class declared in the module.
  - header guards: only the `#define` is renamed, not `#ifndef` or the `#endif` comment.
    The tool renames the old macro name on every preprocessor line.
- Before applying, it reports possible collisions: a new name that already appears in
  code in a touched file (comments and strings ignored), or two old names that become
  one. After applying, it lists old names still used in code. Each must be a different
  symbol, e.g. the enumerator `util::Error::PkPd`.
- A token check compares every changed file with `HEAD`. Apart from whitespace, the only
  differences allowed are the planned old → new pairs. So the commit provably contains
  only renames.

**G-style for a rename commit.** `util/style.sh check HEAD` cannot pass on a rename
commit before it is listed in `.git-blame-ignore-revs`: every call site the rename touches
counts as changed, and still holds other legacy names and legacy hits. On the PkPd pilot
it reported about 70 findings, none of them a naming hit for a PkPd symbol. A rename
commit therefore passes G-style when (1) the clang-format half of `check` is clean,
(2) `rename <dir> --dry-run` prints `nothing to rename`, (3) the token check passed,
which proves every other finding was there before, and (4) once the commit is listed in
`.git-blame-ignore-revs`, `check <rename commit>^` is clean, because the tidy filter then
skips the lines only the rename touched.

Name overrides (`--map`) handle poor case conversions (`IC50` → `i_c50`, mapped to
`ic50`) and collisions (member `PD` → `pd` would bind to a local `pd` in one
constructor, mapped to `pd_params`).

Other notes:
- Header guards such as `Hmod_Population` become `HMOD_POPULATION`. A guard is renamed
  by case conversion only, so a misspelt one (`Hmod_LSTMDrugOnComp`) stays misspelt.
- CxxTest finds tests by the `test` prefix on suite methods, so `testFoo` → `test_foo`
  still works. The suite classes themselves are already PascalCase. The generated
  runner (`<build>/unittest/tests.cpp`) is never edited.
- Unittest suites that `unittest/CMakeLists.txt` doesn't register are not compiled, so
  their uses are not renamed.
- `class sim` (`model/sim.h`) and the exception classes in `util/errors.h`
  (`base_exception`, `format_error`, …) are flagged and will become PascalCase.

## Pre-commit and CI

**Pre-commit.** `.pre-commit-config.yaml` runs the `clang-format` hook from
`pre-commit/mirrors-clang-format` at `v22.1.8` on `model/` and `unittest/` C++. It formats
whole staged files, like `format-changed`, and fails the commit if it changed anything
(restage and commit again). Enable it once per clone:

```sh
uv tool install pre-commit && pre-commit install
```

**CI.** `.github/workflows/style.yml` (job `g-style`, ubuntu-24.04) runs on every push and
pull request. All of its logic is in `util/ci-style.sh [base-ref]`, which runs the same
way by hand:

1. `git merge-base <base-ref> HEAD`. In CI, base-ref is `origin/<PR base branch>`, or
   `origin/<default branch>` for a push. The checkout uses `fetch-depth: 0`, which both
   the merge base and the blame filter need.
2. If `$OM_BUILD_DIR` (default `build-style/`) has no `compile_commands.json`, configure
   it (`Release`, `OM_CXXTEST_ENABLE=ON`). CI sets `CC=clang CXX=clang++`, so the compile
   database holds flags clang-tidy understands.
3. Build the `schema` target, so the generated `scnXml` headers exist for clang-tidy.
4. `util/style.sh check <merge base>`.

A rename commit in the range is handled by the tidy filter, so CI does not need the
rename-commit variant of G-style, as long as the rename's `.git-blame-ignore-revs` entry
is in the same push.

The fork's `origin/main` is still at upstream #430 (7b1d8919), while this branch is based
on `upstream/main` (#468). Until `origin/main` is moved up, push runs on this branch also
check upstream's #431–#468 changes. That is clean today: `util/ci-style.sh origin/main`
passes.
