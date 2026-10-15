# Refactor progress

See `PLAN.md` for the session protocol, chunks and gates. Append one log entry per chunk.

## Status

| Chunk | Status | Commits |
|---|---|---|
| A1 | done | cfcb16e3, (this log entry) |
| A2 | todo | |
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
