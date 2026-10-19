# Statistical regression tests

The byte-for-byte scenario tests (`test/expected/`) fail on any change in RNG draw order, even a correct one. These tools check instead that a run of the new code behaves like many runs of the reference code. The method is specified in `docs/refactor/PLAN.md` § Statistical tests; this file says how to use the tools and what they can and cannot detect (validated in chunk S6).

## Tools

| Script | Does |
|---|---|
| `run_replicates.py` | Runs a scenario for many seeds (rewriting `iseed`), one directory per seed. Resumable. |
| `generate_reference.py` | Builds `test/reference/scenario<NAME>.csv.gz` from ≥ 100 replicates (S5 used seeds 1001–1500). Needs numpy (via `uv`). |
| `check_scenario.py` | Judges a build against a reference. Registered as the ctest tests `stat_<NAME>` (label `statistical`). |
| `validate_method.py` | False-failure rates and power from directories of runs (this file's numbers). |
| `make_intervention_off.py`, `foi_scale.patch` | The perturbations of the S6 power study. |

## The check, in short

A run fails if (a) its number of cells outside their 95% reference interval is improbable (p ≤ 1% against the reference's leave-one-out counts), (b) one of the key aggregates (clinical episodes, severe episodes, deaths, mean prevalence, mean EIR) is outside its 99%-family interval, or (c) a *fixed* cell differs: a non-zero cell of a survey measure or ctsout column that was constant in every replicate, such as the echoed input EIR.

- **Default (ctest):** run the scenario's own seed. If it fails, run seeds 2001–2005; the test fails if any of them fails too.
- **`--replicates K`:** run seeds 2001… up front; fail if the number of failing runs is improbable (binomial tail ≤ 0.1% at a 2% per-run rate). For K = 20 that means 4 or more failing runs.

## What it can detect (S6)

All numbers come from `validate_method.py` on runs of the build of a1536e12 (S6 changed no C++; its outputs are identical to the S5 replicates', checked seed by seed).

**False failures (unchanged code).** 200 fresh seeds (3001–3200) per scenario, 8798 runs:
- Per-run failure rate 1.8% pooled: rule (a) 0.93%, rule (b) 1.10%, rule (c) 0 runs.
- Whole procedure (default rerun rule): 0.16% pooled. Per scenario at most 0.39% (NoInterv), using the per-run rate pooled over the 200 fresh runs and the reference's 500 leave-one-out runs. Target ≤ 0.5%: met. (The 200 fresh runs alone put a few scenarios at up to 0.74%, which is within sampling noise for 44 scenarios at 1.8%.)
- `--replicates 20`: 0.04% pooled.
- With 44 `stat_*` tests, a correct build that changes draws (e.g. D2b, M4) has about a **9% chance that at least one test fails falsely**. Look at the failing scenario's report and rerun it with `--replicates` before concluding anything. Pure refactors change no outputs, so their verdicts don't change.

**Power.** Two kinds of deliberate error, 50 runs each (seeds 4001–4050), judged against the references:
- **Force of infection × 1.05, 1.10, 1.20.** `foi_scale.patch`, applied in a throwaway build, multiplies the expected number of new infections of every human (`InfectionIncidenceModel::numNewInfections`, which every incidence model goes through) by `$OM_S6_FOI_SCALE`. It has the same meaning in every scenario and is not echoed into the output. Vector warmup still fits emergence to the input EIR, so the effect is not calibrated away.
- **One intervention switched off** (`make_intervention_off.py`; 33 scenarios have one).

Power is P(the check fails) computed from the per-run failure rate q: q(1 − (1 − q)⁵) for the rerun rule, P(Binomial(20, q) ≥ 4) for `--replicates 20`. The minimum detectable effect (MDE) is the smallest perturbation with power ≥ 80%.

In the table: false failure is the per-run rate r and the rerun rule's false-failure rate (fresh + leave-one-out), in %. Each power cell is "rerun rule / `--replicates 20`" power in %. The last column is how far the 20% perturbation moved the mean of the most-affected aggregate, in single-run standard deviations.

| scenario | false failure: r, rerun rule | 5% | 10% | 20% | intervention off | MDE rerun / --replicates 20 | largest aggregate shift at 20% |
|---|---|---|---|---|---|---|---|
| 1 | 1.9, 0.17 | 0 / 0 | 0 / 0 | 0 / 0 | 30 / 95 | >20% / >20% | +0.5 SD (prevalence) |
| 10 | 1.3, 0.08 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | +0.7 SD (episodes) |
| 11 | 2.0, 0.19 | 0 / 0 | 0 / 0 | 16 / 67 | – | >20% / >20% | +1.4 SD (episodes) |
| 12 | 1.4, 0.10 | 0 / 0 | 0 / 0 | 3 / 7 | 100 / 100 | >20% / >20% | +2.0 SD (prevalence) |
| 2ITNs | 1.4, 0.10 | 6 / 21 | 2 / 3 | 6 / 21 | 4 / 13 | >20% / >20% | +1.2 SD (prevalence) |
| 4 | 2.3, 0.25 | 3 / 7 | 1 / 1 | 46 / 100 | 100 / 100 | >20% / 20% | +2.2 SD (prevalence) |
| 5 | 1.7, 0.14 | 0 / 0 | 2 / 3 | 0 / 0 | 2 / 3 | >20% / >20% | +0.8 SD (prevalence) |
| 6 | 2.1, 0.22 | 2 / 3 | 3 / 7 | 2 / 3 | 0 / 0 | >20% / >20% | +0.6 SD (prevalence) |
| 9 | 1.7, 0.14 | 0 / 0 | 0 / 0 | 3 / 7 | 100 / 100 | >20% / >20% | -1.5 SD (episodes) |
| BSV | 1.7, 0.14 | 4 / 13 | 13 / 59 | 72 / 100 | 1 / 1 | >20% / 20% | +2.4 SD (prevalence) |
| Cohort | 2.1, 0.22 | 0 / 0 | 1 / 1 | 2 / 3 | 0 / 0 | >20% / >20% | +1.3 SD (episodes) |
| DecisionTree5DayDielmo | 1.9, 0.17 | 1 / 1 | 13 / 59 | 53 / 100 | 100 / 100 | >20% / 20% | +2.4 SD (prevalence) |
| EffectiveDrug | 1.3, 0.08 | 3 / 7 | 2 / 3 | 0 / 0 | 88 / 100 | >20% / >20% | -0.1 SD (prevalence) |
| Empirical | 1.4, 0.10 | 0 / 0 | 0 / 0 | 1 / 1 | – | >20% / >20% | +0.7 SD (prevalence) |
| ESTS | 1.6, 0.12 | 0 / 0 | 3 / 7 | 44 / 100 | – | >20% / 20% | +2.7 SD (episodes) |
| GaussianCopulaGamma | 2.6, 0.31 | 1 / 1 | 1 / 1 | 1 / 1 | 0 / 0 | >20% / >20% | +0.4 SD (prevalence) |
| GaussianCopulaLognormal | 2.3, 0.25 | 0 / 0 | 0 / 0 | 7 / 30 | 0 / 0 | >20% / >20% | +0.7 SD (prevalence) |
| Genotypes | 2.0, 0.19 | 98 / 100 | 100 / 100 | 100 / 100 | 100 / 100 | 5% / 5% | +17.2 SD (episodes) |
| HetVecDailyEIR | 2.4, 0.28 | 2 / 3 | 1 / 1 | 2 / 3 | – | >20% / >20% | +0.7 SD (prevalence) |
| InfectionOrigin | 1.0, 0.05 | 0 / 0 | 0 / 0 | 13 / 59 | 100 / 100 | >20% / >20% | +0.9 SD (prevalence) |
| IRS30 | 1.3, 0.08 | 2 / 3 | 1 / 1 | 4 / 13 | 84 / 100 | >20% / >20% | +1.0 SD (prevalence) |
| KK_20150612 | 2.3, 0.25 | 2 / 3 | 2 / 3 | 37 / 98 | 100 / 100 | >20% / 20% | +1.9 SD (prevalence) |
| LifeNet1 | 1.4, 0.10 | 0 / 0 | 2 / 3 | 2 / 3 | 100 / 100 | >20% / >20% | +0.9 SD (prevalence) |
| LifeNet2 | 1.7, 0.14 | 0 / 0 | 1 / 1 | 1 / 1 | 100 / 100 | >20% / >20% | +0.8 SD (prevalence) |
| Molineaux | 1.1, 0.06 | 0 / 0 | 0 / 0 | 1 / 1 | 100 / 100 | >20% / >20% | +1.7 SD (episodes) |
| MolPairwise | 2.0, 0.19 | 2 / 3 | 1 / 1 | 9 / 40 | – | >20% / >20% | +1.2 SD (prevalence) |
| MSAT | 1.4, 0.10 | 0 / 0 | 1 / 1 | 2 / 3 | 0 / 0 | >20% / >20% | +0.7 SD (episodes) |
| MSAT_HRP2 | 2.0, 0.19 | 1 / 1 | 23 / 85 | 74 / 100 | 0 / 0 | >20% / 10% | +3.4 SD (episodes) |
| NamawalaArabiensis | 1.6, 0.12 | 0 / 0 | 1 / 1 | 0 / 0 | – | >20% / >20% | +0.0 SD (prevalence) |
| NoInterv | 2.9, 0.39 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | +0.4 SD (EIR) |
| NoMPDLarviciding | 2.0, 0.19 | 0 / 0 | 1 / 1 | 0 / 0 | 0 / 0 | >20% / >20% | -0.3 SD (episodes) |
| NonHumanHosts | 1.6, 0.12 | 1 / 1 | 2 / 3 | 0 / 0 | 100 / 100 | >20% / >20% | -0.1 SD (EIR) |
| Penny | 2.0, 0.19 | 1 / 1 | 55 / 100 | 100 / 100 | – | 20% / 10% | +6.5 SD (episodes) |
| PEV | 1.3, 0.08 | 3 / 7 | 7 / 30 | 51 / 100 | 6 / 21 | >20% / 20% | +2.2 SD (prevalence) |
| Rach5IC | 1.4, 0.10 | 2 / 3 | 1 / 1 | 1 / 1 | 0 / 0 | >20% / >20% | +0.4 SD (prevalence) |
| SimpleMPDLarviciding | 2.4, 0.28 | 0 / 0 | 0 / 0 | 0 / 0 | 2 / 3 | >20% / >20% | -0.3 SD (prevalence) |
| SubPopRemoval | 1.1, 0.06 | 0 / 0 | 0 / 0 | 0 / 0 | 35 / 98 | >20% / >20% | – (no aggregates) |
| TBV | 1.6, 0.12 | 3 / 7 | 9 / 40 | 64 / 100 | 1 / 1 | >20% / 20% | +2.5 SD (prevalence) |
| TrapTest | 2.3, 0.25 | 2 / 3 | 0 / 0 | 1 / 1 | 100 / 100 | >20% / >20% | +0.5 SD (prevalence) |
| TriggeredMSAT | 1.7, 0.14 | 0 / 0 | 2 / 3 | 6 / 21 | 100 / 100 | >20% / >20% | +1.4 SD (prevalence) |
| VecFullTest | 2.6, 0.31 | 0 / 0 | 0 / 0 | 1 / 1 | 100 / 100 | >20% / >20% | +0.3 SD (prevalence) |
| VecMonthly | 2.0, 0.19 | 1 / 1 | 1 / 1 | 1 / 1 | 78 / 100 | >20% / >20% | +0.5 SD (prevalence) |
| VecTest | 2.3, 0.25 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | -0.2 SD (prevalence) |
| Vivax | 2.1, 0.22 | 0 / 0 | 4 / 13 | 0 / 0 | – | >20% / >20% | -0.0 SD (episodes) |

**What this means.**
- **A single run can't see small effects.** The rerun rule judges each run against intervals that hold 99% of single runs, so it reliably catches only shifts of about 3 single-run SDs or more. Its MDE is above 20% for 42 of 44 scenarios (Genotypes: 5%; Penny: 20%).
- **`--replicates 20` is better but still weak** (MDE 20% or less for 10 scenarios), because it counts failing runs instead of testing where the K runs lie as a group. A 20% higher force of infection moves the mean prevalence by 1.9–2.5 SD in ten scenarios; the mean of 20 runs would show that as z ≈ 8.5–11, yet the rerun rule's power there is 3–74% (Penny: 100%) (F31).
- **Some scenarios hardly respond at all** to a 20% higher force of infection (largest shift ≤ 0.4 SD): NamawalaArabiensis, NoInterv, NoMPDLarviciding, NonHumanHosts, SimpleMPDLarviciding, VecTest, Vivax. No output-based test with a practical number of runs will see such an effect there; other scenarios cover it.
- **Switching off an intervention** is caught with ≥ 80% power in 16 of 33 scenarios by the rerun rule and in 19 by `--replicates 20`. How much the other 14 removals move the outputs was not measured; some deploy little within the surveyed period.
- Rule (a) adds little power: Genotypes is caught through its aggregates, with the miss count unremarkable (p ≈ 0.2). For strongly correlated cells, the leave-one-out miss counts are too heavy-tailed for a coherent shift to stand out (F17).

**Using it for draw-order changes (D2b, M4, M8).** The stat tests guard against gross errors (a few SD in an aggregate). Treat a pass as "no large change", not as proof of equivalence.

## Reproducing the study

```sh
# fresh seeds, unchanged build
OM_BUILD_DIR=build-rework test/stats/run_replicates.py NAME -o fresh/NAME --first-seed 3001 -n 200 -j 4
# perturbed force of infection: a throwaway worktree with foi_scale.patch applied, built like build-rework
OM_S6_FOI_SCALE=1.1 OM_BUILD_DIR=<worktree>/build-rework test/stats/run_replicates.py NAME -o power/foi1.1/NAME --first-seed 4001 -n 50 -j 4
# intervention off
test/stats/make_intervention_off.py ivoff
OM_BUILD_DIR=build-rework test/stats/run_replicates.py ivoff/scenarioNAME.xml -o power/ivoff/NAME --first-seed 4001 -n 50 -j 4
# report
test/stats/validate_method.py --fresh fresh --effect 5%=power/foi1.05 --effect 10%=power/foi1.1 \
    --effect 20%=power/foi1.2 --other "intervention off=power/ivoff"
```

In S6 this took about 1 h for the fresh seeds and 1 h for the power runs at `-j 4` on an Apple M5, and the report about 5 minutes.

Not covered: the scenarios without a reference (ModelName*, AvailabilityFilter, ExactNv0, SimpleMPDTest; see V1). No nightly random-seed job was added.
