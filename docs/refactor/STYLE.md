# C++ style tooling

Config: `.clang-format`, `.clang-tidy` (repo root). Runner: `util/style.sh`.

## Pinned LLVM version

**LLVM 22.1.8** for both `clang-format` and `clang-tidy`, taken from the PyPI wheels
(`clang-format==22.1.8`, `clang-tidy==22.1.8`) and run through `uvx`, so nothing is
installed globally. The version is set once, in `LLVM_VERSION` at the top of
`util/style.sh`. The `clang-tidy` wheel also ships `clang-tidy-diff.py`,
`run-clang-tidy.py` and `clang-apply-replacements`. The `clang-format` wheel ships
`git-clang-format`. Pre-commit (B5) should pin the same version.

## Usage

```sh
cmake -S . -B build-rework ...          # compile_commands.json is now exported by default
export OM_BUILD_DIR=build-rework        # default is build/

util/style.sh format -i model/Foo.cpp   # clang-format, project config
util/style.sh tidy model/Foo.cpp        # clang-tidy -p $OM_BUILD_DIR
util/style.sh check [base]              # G-style gate, lines changed since base (default HEAD)
util/style.sh changed [base]            # model/ and unittest/ C++ files changed since base
util/style.sh format-changed [base [paths...]]  # git-clang-format the changed lines
util/style.sh fix [base] [checks]       # apply clang-tidy fix-its on changed lines
util/style.sh rename <dir> [--dry-run] [--map OLD=NEW] [--skip OLD]  # rename one module
```

`format-changed` uses `git-clang-format`, so it formats exactly the lines `check`
verifies. Whole-file `clang-format -i` is not quite equivalent: on a function appended
before a namespace's closing `}`, it kept a blank line that the line-range format in
`check` removes.

`fix` defaults to a safe list of checks whose fix-its are local and don't change
behaviour (`util/style.sh -h` prints it). Pass a `-checks` filter to apply others one at
a time. The fixes are exported and applied once with `clang-apply-replacements`, because
`clang-tidy-diff.py` runs one clang-tidy per file in parallel, and two of them can fix
the same header. After any fix, it runs `format-changed`.

The Claude Code skills `/cpp-style` (rules for writing code), `/cpp-lint` (fix,
rebuild, triage, report) and `/rename-module` (one module's rename commit) in
`.claude/skills/` wrap these commands.

`check` runs `git-clang-format --diff` and `clang-tidy-diff.py` on the
`model/` and `unittest/` C++ lines changed since `base`. It looks only at changed lines
because legacy names are not fixed until each module's rename. (Formatting alone could
be checked whole-file since the B2 reformat.) It exits non-zero on any finding. It reads `git diff`, so new files
must be staged (or `git add -N`) to be seen.

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

**G-style for a rename commit.** `util/style.sh check` cannot pass on a rename commit:
every call site the rename touches counts as changed, and still holds other legacy names
and legacy hits. On the PkPd pilot it reported about 70 findings, none of them a naming hit for
a PkPd symbol. A rename commit therefore passes G-style when (1) the git-clang-format
half of `check` is clean, (2) `rename <dir> --dry-run` prints `nothing to rename`, and
(3) the token check passed, which proves every other finding was there before.

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
