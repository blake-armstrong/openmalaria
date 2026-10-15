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
```

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

Notes for the rename work (B4):
- clang-tidy's case conversion does not strip the `m_` or `_` member prefixes
  (`m_cumulative_Y` → `m_cumulative_y`, `_innateImmSurvFact` →
  `_innate_imm_surv_fact`). Stripping them needs a separate scripted rename.
- Header guards such as `Hmod_Population` are flagged as macros, and become
  `HMOD_POPULATION`.
- CxxTest finds tests by the `test` prefix on suite methods, so `testFoo` → `test_foo`
  still works. The suite classes themselves are already PascalCase. The generated
  runner (`<build>/unittest/tests.cpp`) is never checked.
- `class sim` (`model/sim.h`) and the exception classes in `util/errors.h`
  (`base_exception`, `format_error`, …) are flagged and will become PascalCase.
