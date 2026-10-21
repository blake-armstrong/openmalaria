---
name: rename-module
description: Rename every identifier declared in one C++ module directory (e.g. model/PkPd) to the project naming rules, at every use in the tree, as one tool-made commit listed in .git-blame-ignore-revs. Use when a module's rename is due (after it is refactored in stages D on) or when asked to rename a module. Argument is the module directory.
---

# Rename one module

The rename is mechanical and must stay that way: the commit contains only renames plus
the formatting they cause, and is made by the tool, not by hand. Naming rules:
`/cpp-style`. Tool: `util/style.sh rename` (`util/rename_module.py`).

A module that shares its directory with others (e.g. the `model/` root files) is given as a
list of files instead of `<dir>`; everything below then applies to those files.

## What the tool does

- Runs `readability-identifier-naming` over every TU in `$OM_BUILD_DIR/compile_commands.json`,
  including the generated CxxTest runner, so uses in other modules and in `unittest/` are
  found.
- Keeps the symbols **declared** in the module directory, and namespaces declared **only**
  there. A namespace that other modules also open (`OM`, `WithinHost`, `Host`) is left for
  its own rename.
- A second pass finds members with `m_` or `_` prefixes (clang-tidy's case conversion
  keeps them) and strips the prefix.
- Adds what clang-tidy misses: out-of-line definitions of overrides (`Derived::oldName`,
  including derived classes outside the module)
  and the `#ifndef` / `#endif` lines of header guards.
- Applies all fixes once with `clang-apply-replacements`, then `format-changed HEAD`.
- Reports possible collisions before applying; after applying, reports old names still
  used in code and checks that the diff against `HEAD` is only the planned renames plus
  whitespace (the token check).

## 1. Preconditions

- `export OM_BUILD_DIR=build-rework`. The build must be configured with
  `-DOM_CXXTEST_ENABLE=ON` and built, so the compile database and the CxxTest runner are
  current. Re-run `cmake -S . -B build-rework` if `.cpp` files were added.
- `model/` and `unittest/` must have no uncommitted changes (the tool refuses otherwise),
  so the rename is its own commit.
- Unittest suites that `unittest/CMakeLists.txt` does not register are not compiled, so
  their uses are not renamed. `grep` them for the module's names first. If one uses the
  module, tell the user.

## 2. Dry run and review

```sh
util/style.sh rename <dir> --dry-run
```

It takes about a minute (two clang-tidy passes over the tree). Review the plan:

| Look for | Action |
|---|---|
| A poor name from case conversion, e.g. `IC50` → `i_c50` | `--map IC50=ic50` (applies to every symbol with that old name, so list each variant: `--map IC50_pow_slope=ic50_pow_slope`) |
| A collision: the new name already appears in a touched file | Read each one. A struct member access (`p.f`) or a different scope is fine. A local or parameter in the same scope as a renamed member (member `PD` → `pd` in a constructor with a local `pd`) would change which name binds: `--map PD=pd_params`. |
| A new name that equals a variable at its use sites (namespace `PkPd` → `pkpd` next to variables named `pkpd`) | Prefer the tool's name (`pk_pd`), which avoids it. |
| `NO FIX-IT` | clang-tidy refused (a use inside a macro, or a conflict). The tool will not apply. Use `--skip <name>` and list it in the report. |
| A namespace you expected but that is not listed | Other modules open it too. Leave it. |

## 3. Apply

```sh
util/style.sh rename <dir> [same --map/--skip options]
```

Then read the output:

- **Old names still used in code:** each must be a different symbol that shares the
  name (an enumerator `util::Error::PkPd`, a member of another module's class, a
  `scnXml` accessor, a local elsewhere). If one is a missed use of a renamed symbol, the
  tool has a gap: revert (`git checkout -- model unittest`), then fix the tool or the
  options and re-run. Do not patch the rename by hand.
- **Token check** must print that the diff is the planned renames plus whitespace only.
  It exits non-zero otherwise.

## 4. Build and test

```sh
cmake --build build-rework -j10
cd build-rework && caffeinate -i ctest -j10 --output-on-failure
```

A compile error means a missed use: revert and fix the tool or options, as above. The
only allowed warnings are the baseline ones (`sprintf` deprecated in `model/main.cpp`,
duplicate `-lc++`). G-exact and G-unit must pass.

## 5. G-style for a rename commit

Before the commit, `util/style.sh check HEAD` will report findings, because every call
site the rename touched now counts as changed and still holds legacy names and other
legacy hits. A renames-only commit cannot fix those. For a rename commit, G-style is
instead:

- the `== clang-format` half of `util/style.sh check HEAD` prints nothing;
- `util/style.sh rename <dir> --dry-run` prints `nothing to rename`;
- the token check passed, which proves every other `check` finding is pre-existing;
- after the `.git-blame-ignore-revs` commit (§ 6), `util/style.sh check <rename commit>^`
  is clean. The clang-tidy half skips lines that only blame-ignored commits touched, so
  this is also how CI sees a branch that contains the rename.

## 6. Commit

Stage the changed files by name, never with `git add -A`:

```sh
git add $(git diff --name-only -- model unittest)
git commit
```

Message: `Rename <dir> identifiers to the naming rules`, then a body with the counts by
kind, every `--map`/`--skip` used and why, and the line `Mechanical: made by util/style.sh
rename; no logic change.` Add the commit attribution lines.

Then a second commit appends the hash to `.git-blame-ignore-revs`, with a comment line in
the existing style:

```
# B4: rename model/PkPd identifiers (util/style.sh rename)
<full hash>
```

## 7. Report

- the module, the counts by kind, and the maps and skips with reasons
- the collisions reviewed and what was done about each
- the namespaces renamed, and those left for their own module
- build, ctest, and the three G-style checks above
- the two commit hashes
- that out-of-tree code using the old names (e.g. the untracked `python/` bindings)
  was not changed and may need updating
