---
name: close-chunk
description: Finish a refactor chunk - run every gate with util/gates.sh, check the docs for drift with util/chunk.py check-docs, and write the FINDINGS, PLAN as-built, README and PROGRESS commits from the logged gate results and commit list. Use when a chunk's code commits are in and its gates are due, before telling the user the chunk is done.
---

# Close a chunk

Arguments: the chunk ID and its base commit (the HEAD the chunk started from).

## 1. Gates

```sh
util/gates.sh --all <base>     # G-exact/G-unit/G-two-sims on both builds, A/B, G-stat, G-style, G-perf
```

Each line is also logged in `build-rework/gates.log` with HEAD. A failing gate is fixed
before any docs commit. For G-perf, the line records the load average at the start; if it
was high (above ~4) and the ratio is near the limit, rerun on an idle machine.

Planted bugs for the moved code, each in one call (the tool restores the code and
rebuilds; a plant that does not compile is reported, not run):

```sh
util/plant.py --name '<what>' --replace <file> '<old>' '<new>' [--no-ctest] --ab <groups>
```

## 2. Docs, in this order (one commit each where they apply)

1. **FINDINGS.md**: a new `F<n>` for each bug, surprise or odd design found; notes on
   open entries the chunk touched; entries the chunk settled move to § Closed with the commit.
2. **PLAN.md**: the chunk's row or section gets `*As built:*` where the result differs
   from the plan, ending "See the <ID> entry in `PROGRESS.md`".
3. **README.md**: the bridge list and the gate and A/B rows, if the chunk changed them.
4. Commit 1–3 as `FINDINGS: …; PLAN: <ID> as built; README: …`.
5. `util/chunk.py check-docs` must print `clean`. It checks README's bridge list against
   the code, the module status table against the tool commits in `.git-blame-ignore-revs`,
   and the status table against the log entries. Fix what it reports in the PROGRESS
   commit (step 6) or, for README, in the docs commit.
6. **PROGRESS.md**, commit `PROGRESS: <ID> done`:
   - the status row: `util/chunk.py commits <base>` prints the list, ending `(this log entry)`;
   - a module status row for each module renamed or modernised;
   - the log entry, in the shape of the previous ones: Base; Commits (what each did and
     why, every hand change with its reason); Gates (`util/chunk.py gates <base>` prints
     the logged lines; copy their numbers, and list the planted bugs and what caught each);
     Findings; Surprises; Follow-ups; Next.

## 3. Report to the user

What changed, the gate results in a short table, anything covered only by review, and
the next chunk. Say if a planted bug was not caught, or a gate was run under load.
