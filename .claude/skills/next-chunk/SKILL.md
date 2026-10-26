---
name: next-chunk
description: Start a refactor chunk on throwaway_ai_rework - print the briefing for the next chunk (or a named one) with util/chunk.py next instead of reading PLAN.md and PROGRESS.md, check prerequisites, move the base worktree and take the bridge inventory. Use when the user says "do the next chunk", "continue the refactor" or names a chunk ID (e.g. D9c), before any planning.
---

# Start a chunk

The briefing replaces step 2 of PLAN's session protocol ("Read only: …"): it prints the
parts of PLAN.md, PROGRESS.md and FINDINGS.md that step names, and nothing else. Do not
read those files whole. It is ~3–4k tokens; the files are ~35k.

## 1. Briefing

```sh
util/chunk.py next          # the first chunk not done in PLAN's recommended order
util/chunk.py next D9c      # or the chunk the user named
```

It prints, in order:
- the chunk's PLAN text: its `###` section or table row, with its stage's rules and any
  parent row (e.g. `D9` for `D9c`), and the Part 3 sections they name;
- PLAN Part 1: the session protocol, the gates table, the practical notes;
- the previous log entry's follow-ups and `Next`, and plan amendments that name the chunk;
- open findings that name the chunk or a module in its row;
- HEAD, whether the tree is clean, and where the base worktree is.

Read a Part 3 section only if the chunk names it: `util/chunk.py section 'Modernisation policy'`.
Read further in PROGRESS.md only for a specific earlier entry you need (`grep -n '^### '`
then Read with offset).

## 2. Checks before planning

- **Prerequisites:** every chunk in the chunk's **Needs** is `done` in the status table
  (`grep -E '^\| (D9b2|M2) ' docs/refactor/PROGRESS.md`). If one is missing, stop and say so.
- **Open decisions:** if the briefing says to ask the user a Decision first, ask now,
  leading with the recommendation.
- **Tree:** `model/`, `unittest/`, `test/`, `util/` and `docs/` clean. If not, stop and ask
  (never stash or discard tracked changes without asking).

## 3. Base worktree

The chunk's base is HEAD at its start. If the briefing says the base worktree is elsewhere:

```sh
git -C ../om-bench-base checkout --detach <base>
cmake --build ../om-bench-base/build-rework -j4 --target openMalaria inlined_xsd
```

## 4. Inventory (stage D9)

```sh
util/bridges.py            # every bridge, its facades and their call sites
util/bridges.py --files    # per file
```

Use it for the chunk's file count (PLAN step 4: split above ~20 files of non-mechanical
change) and keep the numbers for the PROGRESS entry. D9f's end state is
`no bridges left`.

## 5. Then

Plan the chunk. While working, run the gates with `util/gates.sh` (not hand-written
build/ctest/A/B pipelines) and planted bugs with `util/plant.py`. Finish with
`/close-chunk`.
