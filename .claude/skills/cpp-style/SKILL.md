---
name: cpp-style
description: OpenMalaria C++ style rules (naming, Core Guidelines, clang-format) and how to format changed files. Use when writing or editing C++ under model/ or unittest/, and before committing C++ changes.
---

# OpenMalaria C++ style

Applies to C++ under `model/` and `unittest/`. Never restyle `contrib/` (third-party) or
`schema/` (XSD; the `scnXml` C++ is generated). Full rationale and every tidy exclusion:
`docs/refactor/STYLE.md`. Tool runner: `util/style.sh` (never call clang-format or
clang-tidy by raw name; the script pins LLVM 22.1.8 and the macOS sysroot fix).

## Naming

| Kind | Case | Example |
|---|---|---|
| classes, structs, unions, enums, type aliases, typedefs, template parameters, concepts | `PascalCase` | `Population`, `WHFalciparum` |
| functions, methods | `snake_case` | `create_initial_humans()` |
| variables, parameters, members, local and non-static const members | `snake_case`, **no `m_` or `_` prefix** | `cumulative_pop` |
| namespaces | `snake_case` | `om::within_host` |
| global, static, class-static and `constexpr` constants | `PascalCase` | `MaxDurationDays` |
| enumerators (scoped and unscoped) | `PascalCase` | `Parameter::SigmaISq` |
| macros, header guards | `UPPER_CASE` | `HMOD_POPULATION` |

The C++ names are what the Python bindings expose, so following this table is what keeps
them PEP 8 without a renaming layer.

### Legacy code

Most existing code still has legacy names (`getSize()`, `m_foo`, `_bar`, `Hmod_X`,
namespace `OM`). They are renamed per module, by tool, in separate commits listed in
`.git-blame-ignore-revs` (`/rename-module`, from chunk B4 on).

- Every identifier you **declare** follows the table, even inside a legacy-named class.
- **Use** legacy names as they are. Do not rename an existing identifier by hand in a
  logic change.
- An override keeps its base class's name (`readability-identifier-naming` skips
  overrides).
- If your change must touch the declaration line of a legacy name, `/cpp-lint` will
  flag it. Handle it as `/cpp-lint` says; do not silence it with `NOLINT`.
- Do not reformat or restyle lines you did not otherwise change.

## Core Guidelines rules that matter here

The `.clang-tidy` config enforces `bugprone-*`, `cppcoreguidelines-*`, `modernize-*` and
`performance-*` on changed lines. Write code that passes it first time:

- **Ownership (R.11, R.20):** values or `std::unique_ptr`; no naked `new`/`delete`.
  The project does not use the GSL (`gsl::owner`, `gsl::at`).
- **Initialise every variable (ES.20).** The checkpoint read idiom becomes
  `T x = 0; x & stream;`.
- **Narrowing (ES.46):** make intended conversions explicit with `static_cast`.
- **const (Con.1–4):** `const` locals, references and methods by default.
- **Parameters (F.16):** cheap types by value, others by `const&`.
- **Modern forms:** `nullptr`, `override`, `= default` / `= delete`, `using` over
  `typedef`, `enum class` for new enums (Enum.3).
- **No new mutable globals or file statics (I.2).** Removing them is stage D's purpose.
  The only exception is stage D's temporary "current simulation" bridge pointer, marked
  `// NOLINT(cppcoreguidelines-avoid-non-const-global-variables)`.
- **Pointers and references:** `T* p`, `T& r` (NL.18). clang-format does this.
- **Hot paths** (per-human or per-step code): `operator[]` is fine, so don't add `.at()`.
  Avoid per-human allocation and per-human virtual dispatch; G-perf gates at +3%.
- **Output streams:** new code uses `'\n'`. Leave legacy `endl` in output writers
  alone, since changing it changes flush timing.
- **`NOLINT`** only names one check, `// NOLINT(check-name)`, and only for a genuine
  false positive or a case STYLE.md lists under "Kept on, with expected legacy hits".

## New files

- Copy the GPL licence header from a neighbouring file.
- Header guard: `#ifndef HMOD_<PATH_AND_NAME>` in `UPPER_CASE` (not `#pragma once`, which
  nothing in the tree uses).
- Add each new `.cpp` to the explicit source list in `model/CMakeLists.txt`, then
  re-run the configure step so `compile_commands.json` includes it:
  `cmake -S . -B build-rework`.
- `git add -N <file>` so `util/style.sh` sees it. Its commands read `git diff`.

## Formatting

`.clang-format` is the authority. Don't hand-format; after editing, run:

```sh
export OM_BUILD_DIR=build-rework
util/style.sh format-changed <base>     # base: the chunk's base commit, or HEAD
```

This runs `clang-format -i` on the files changed since `<base>`. The G-style gate checks
those whole files, so a formatted file always passes its format half. Pass paths after the
base to limit it to those files. The pre-commit hook (`pre-commit install`, once per
clone) does the same on commit.

Then run `/cpp-lint` for the clang-tidy half of the gate.

## Commits

Keep mechanical commits (formatting, renames, file moves) separate from logic commits.
