# Statistical regression tests

The byte-for-byte scenario tests (`test/expected/`) fail on any change in RNG draw order, even a correct one. These tools check instead that runs of the new code behave like many runs of the reference code. The method is specified in `docs/refactor/PLAN.md` § Statistical tests; this file says how to use the tools and what they can and cannot detect (validated in chunks S6 and S7).

## Tools

| Script | Does |
|---|---|
| `run_replicates.py` | Runs a scenario for many seeds (rewriting `iseed`), one directory per seed. Resumable. |
| `generate_reference.py` | Builds `test/reference/scenario<NAME>.csv.gz` from ≥ 100 replicates (S5 used seeds 1001–1500), including the group test's calibration. Needs numpy (via `uv`). |
| `check_scenario.py` | Judges a build against a reference. Registered as the ctest tests `stat_<NAME>` (label `statistical`, single-run check) and `statfull_<NAME>` (label `statistical-full`, group test). |
| `validate_method.py` | False-failure rates, calibration and power from directories of runs (this file's numbers). |
| `make_intervention_off.py`, `foi_scale.patch` | The perturbations of the S6 power study. |

## The checks, in short

**Single run.** A run fails if (a) its number of cells outside their 95% reference interval is improbable (p ≤ 1% against the reference's leave-one-out counts), (b) one of the key aggregates (clinical episodes, severe episodes, deaths, mean prevalence, mean EIR) is outside its 99%-family interval, or (c) a *fixed* cell differs: a non-zero cell of a survey measure or ctsout column that was constant in every replicate, such as the echoed input EIR.

- **Default (`stat_<NAME>`, plain `ctest`):** run the scenario's own seed. If it fails, run seeds 2001–2005; the test fails if any of them fails too.

**Group test (`--replicates`, `statfull_<NAME>`).** Run K = 20 seeds (2001–2020) and compare their mean with the reference, for every *group quantity*: each aggregate, and each survey measure's total over surveys and age groups (whole-population groups and cohort or genotype groups totalled separately). For each,

  z = (mean of the K runs − reference mean) / (reference SD · √(1/K + 1/N)).

The test fails if the largest |z| exceeds the reference's threshold, or if any of the K runs fails rule (c). The per-run rules are still printed for each run.

- The threshold is calibrated per scenario by leave-K-out on the reference replicates: 10⁶ times, K of the N replicates are scored as a "new build" against the other N − K, and the threshold is the 99.99% quantile of the largest |z|. So each scenario's family false-failure rate is α = 0.01%, allowing for skewed counts and correlated quantities, and the 44-test suite raises a false alarm about 0.4% of the time.
- A quantity is tested only if at least 10 of the N replicates differ from its most common value. Rarer events (e.g. a measure that is 0 in 495 of 500 runs) have too few non-zero values for a stable leave-K-out tail; the single-run rules still see them.
- Run it with `ctest -C full -L statistical-full` (the tests are registered for the configuration `full` only, so plain `ctest` skips them), or `check_scenario.py NAME --replicates`. Each test runs its 20 seeds with `OM_STAT_FULL_JOBS` (default 4) parallel runs; all 44 take about 6 minutes at `-j1` on the M5. K is fixed by the reference; another K needs `generate_reference.py --group-k K`.

## What they can detect (S6, S7)

All numbers come from `validate_method.py` on runs of the build of a1536e12 (S6 changed no C++; its outputs are identical to the S5 replicates', checked seed by seed). S7 re-used the same runs (`build-rework/stats-s6/`, report in `report-s7.md` there); it made no new runs.

**False failures (unchanged code).** 200 fresh seeds (3001–3200) per scenario, 8798 runs:
- Single-run check: per-run failure rate 1.8% pooled (rule (a) 0.93%, rule (b) 1.10%, rule (c) 0 runs). The default rerun rule fails 0.16% pooled; per scenario at most 0.39% (NoInterv), using the per-run rate pooled over the 200 fresh runs and the reference's 500 leave-one-out runs. Target ≤ 0.5%: met. With 44 `stat_*` tests, a correct build that changes draws (e.g. D2b, M4) has about a **9% chance that at least one fails falsely**. Look at the failing scenario's report and run its group test before concluding anything.
- Group test: the fresh runs, as disjoint groups of 20 in seed order, give 438 groups (LifeNet1/2 have 9 each, as seed 3095 aborts). **None fails.** The calibration was also checked at levels where 438 groups can show something: the leave-K-out quantiles at 5% and 1% are exceeded by 15 and 1 groups, against 21.9 and 4.4 expected. So the group test is if anything slightly conservative (by about a third at those levels).

**Power.** Two kinds of deliberate error, 50 runs each (seeds 4001–4050), judged against the references:
- **Force of infection × 1.05, 1.10, 1.20.** `foi_scale.patch`, applied in a throwaway build, multiplies the expected number of new infections of every human (`InfectionIncidenceModel::numNewInfections`, which every incidence model goes through) by `$OM_S6_FOI_SCALE`. It has the same meaning in every scenario and is not echoed into the output. Vector warmup still fits emergence to the input EIR, so the effect is not calibrated away.
- **One intervention switched off** (`make_intervention_off.py`; 33 scenarios have one).

Rerun-rule power is computed from the per-run failure rate q as q(1 − (1 − q)⁵). Group-test power is the share of 1000 random groups of 20 of the 50 runs that fail the group test. The minimum detectable effect (MDE) is the smallest perturbation with power ≥ 80%.

In the table: false failure is the per-run rate r and the rerun rule's false-failure rate (fresh + leave-one-out), in %. Each power cell is "rerun rule / group test" power in %. The last column is how far the 20% perturbation moved the mean of the group quantity that moved most, in single-run standard deviations.

| scenario | false failure: r, rerun rule | group threshold | 5% | 10% | 20% | intervention off | MDE rerun / group | largest shift at 20% |
|---|---|---|---|---|---|---|---|---|
| 1 | 1.9, 0.17 | 4.60 | 0 / 0 | 0 / 0 | 0 / 0 | 30 / 100 | >20% / >20% | +0.5 SD |
| 10 | 1.3, 0.08 | 4.23 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | +0.5 SD |
| 11 | 2.0, 0.19 | 4.50 | 0 / 0 | 0 / 0 | 16 / 100 | – | >20% / 20% | +1.4 SD |
| 12 | 1.4, 0.10 | 4.23 | 0 / 2 | 0 / 52 | 3 / 100 | 100 / 100 | >20% / 20% | +1.8 SD |
| 2ITNs | 1.4, 0.10 | 4.85 | 6 / 0 | 2 / 5 | 6 / 98 | 4 / 0 | >20% / 20% | +1.6 SD |
| 4 | 2.3, 0.25 | 5.15 | 3 / 100 | 1 / 100 | 46 / 100 | 100 / 100 | >20% / 5% | +11.4 SD |
| 5 | 1.7, 0.14 | 4.74 | 0 / 0 | 2 / 75 | 0 / 100 | 2 / 100 | >20% / 20% | +2.7 SD |
| 6 | 2.1, 0.22 | 6.08 | 2 / 0 | 3 / 57 | 2 / 100 | 0 / 0 | >20% / 20% | +3.0 SD |
| 9 | 1.7, 0.14 | 4.32 | 0 / 0 | 0 / 1 | 3 / 100 | 100 / 100 | >20% / 20% | -1.5 SD |
| BSV | 1.7, 0.14 | 4.48 | 4 / 30 | 13 / 100 | 72 / 100 | 1 / 100 | >20% / 10% | +3.0 SD |
| Cohort | 2.1, 0.22 | 5.26 | 0 / 0 | 1 / 0 | 2 / 74 | 0 / 0 | >20% / >20% | +1.3 SD |
| DecisionTree5DayDielmo | 1.9, 0.17 | 4.24 | 1 / 5 | 13 / 100 | 53 / 100 | 100 / 100 | >20% / 10% | +2.5 SD |
| ESTS | 1.6, 0.12 | 7.46 | 0 / 0 | 3 / 88 | 44 / 100 | – | >20% / 10% | +3.3 SD |
| EffectiveDrug | 1.3, 0.08 | 17.41 | 3 / 0 | 2 / 0 | 0 / 0 | 88 / 100 | >20% / >20% | -0.1 SD |
| Empirical | 1.4, 0.10 | 4.88 | 0 / 0 | 0 / 0 | 1 / 0 | – | >20% / >20% | +0.6 SD |
| GaussianCopulaGamma | 2.6, 0.31 | 5.07 | 1 / 0 | 1 / 0 | 1 / 14 | 0 / 0 | >20% / >20% | +0.9 SD |
| GaussianCopulaLognormal | 2.3, 0.25 | 4.88 | 0 / 0 | 0 / 0 | 7 / 83 | 0 / 0 | >20% / 20% | +1.3 SD |
| Genotypes | 2.0, 0.19 | 4.72 | 98 / 100 | 100 / 100 | 100 / 100 | 100 / 100 | 5% / 5% | +17.6 SD |
| HetVecDailyEIR | 2.4, 0.28 | 5.15 | 2 / 74 | 1 / 100 | 2 / 100 | – | >20% / 10% | +5.2 SD |
| IRS30 | 1.3, 0.08 | 4.83 | 2 / 0 | 1 / 7 | 4 / 95 | 84 / 100 | >20% / 20% | +1.5 SD |
| InfectionOrigin | 1.0, 0.05 | 4.77 | 0 / 18 | 0 / 100 | 13 / 100 | 100 / 100 | >20% / 10% | +3.8 SD |
| KK_20150612 | 2.3, 0.25 | 4.44 | 2 / 14 | 2 / 100 | 37 / 100 | 100 / 100 | >20% / 10% | +3.4 SD |
| LifeNet1 | 1.4, 0.10 | 4.84 | 0 / 0 | 2 / 1 | 2 / 8 | 100 / 100 | >20% / >20% | +0.8 SD |
| LifeNet2 | 1.7, 0.14 | 4.83 | 0 / 0 | 1 / 0 | 1 / 6 | 100 / 100 | >20% / >20% | +0.8 SD |
| MSAT | 1.4, 0.10 | 5.35 | 0 / 4 | 1 / 98 | 2 / 100 | 0 / 100 | >20% / 10% | -3.1 SD |
| MSAT_HRP2 | 2.0, 0.19 | 4.86 | 1 / 100 | 23 / 100 | 74 / 100 | 0 / 100 | >20% / 5% | +7.7 SD |
| MolPairwise | 2.0, 0.19 | 4.81 | 2 / 0 | 1 / 0 | 9 / 84 | – | >20% / 20% | +1.2 SD |
| Molineaux | 1.1, 0.06 | 5.07 | 0 / 0 | 0 / 10 | 1 / 100 | 100 / 100 | >20% / 20% | +2.0 SD |
| NamawalaArabiensis | 1.6, 0.12 | 6.03 | 0 / 0 | 1 / 0 | 0 / 0 | – | >20% / >20% | -0.1 SD |
| NoInterv | 2.9, 0.39 | 5.97 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | -0.8 SD |
| NoMPDLarviciding | 2.0, 0.19 | 5.99 | 0 / 0 | 1 / 0 | 0 / 0 | 0 / 2 | >20% / >20% | -0.3 SD |
| NonHumanHosts | 1.6, 0.12 | 4.09 | 1 / 0 | 2 / 5 | 0 / 49 | 100 / 100 | >20% / >20% | -0.9 SD |
| PEV | 1.3, 0.08 | 4.43 | 3 / 11 | 7 / 91 | 51 / 100 | 6 / 100 | >20% / 10% | +2.5 SD |
| Penny | 2.0, 0.19 | 5.40 | 1 / 100 | 55 / 100 | 100 / 100 | – | 20% / 5% | +6.4 SD |
| Rach5IC | 1.4, 0.10 | 4.84 | 2 / 0 | 1 / 0 | 1 / 24 | 0 / 0 | >20% / >20% | +1.0 SD |
| SimpleMPDLarviciding | 2.4, 0.28 | 4.83 | 0 / 0 | 0 / 0 | 0 / 0 | 2 / 99 | >20% / >20% | -0.4 SD |
| SubPopRemoval | 1.1, 0.06 | 3.93 | 0 / 0 | 0 / 0 | 0 / 0 | 35 / 100 | >20% / >20% | +0.4 SD |
| TBV | 1.6, 0.12 | 4.48 | 3 / 6 | 9 / 100 | 64 / 100 | 1 / 100 | >20% / 10% | +2.7 SD |
| TrapTest | 2.3, 0.25 | 4.51 | 2 / 0 | 0 / 0 | 1 / 0 | 100 / 100 | >20% / >20% | +0.5 SD |
| TriggeredMSAT | 1.7, 0.14 | 4.64 | 0 / 1 | 2 / 70 | 6 / 100 | 100 / 100 | >20% / 20% | +2.4 SD |
| VecFullTest | 2.6, 0.31 | 4.88 | 0 / 0 | 0 / 0 | 1 / 0 | 100 / 100 | >20% / >20% | +0.3 SD |
| VecMonthly | 2.0, 0.19 | 5.43 | 1 / 0 | 1 / 0 | 1 / 0 | 78 / 100 | >20% / >20% | +0.5 SD |
| VecTest | 2.3, 0.25 | 6.03 | 0 / 0 | 0 / 0 | 0 / 0 | – | >20% / >20% | +0.3 SD |
| Vivax | 2.1, 0.22 | 4.48 | 0 / 0 | 4 / 0 | 0 / 0 | – | >20% / >20% | +0.6 SD |

**What this means.**
- **The group test is the one to use for draw-order changes.** At a 20% higher force of infection it has ≥ 80% power in 24 of 44 scenarios (S6's count-based `--replicates 20`: 10; the rerun rule: 2); at 10% in 13; at 5% in 4 (4, Genotypes, MSAT_HRP2, Penny). It catches a switched-off intervention in 26 of 33 scenarios (rerun rule 16, old `--replicates 20` 19). As a suite, even the 5% change fails at least one `statfull_*` test with near certainty.
- **A single run can't see small effects.** The rerun rule judges each run against intervals that hold 99% of single runs, so it reliably catches only shifts of about 3 single-run SDs or more. It stays as the cheap check in plain `ctest`, and for pure refactors, which change no outputs, its verdicts don't change.
- **Some scenarios hardly respond** to a 20% higher force of infection: no group quantity moves by more than 1 single-run SD in 1, 10, EffectiveDrug, Empirical, GaussianCopulaGamma, LifeNet1/2, NamawalaArabiensis, NoInterv, NoMPDLarviciding, NonHumanHosts, Rach5IC, SimpleMPDLarviciding, SubPopRemoval, TrapTest, VecFullTest, VecMonthly, VecTest and Vivax. With K = 20 the group test needs a shift of about 1.3 SD for 80% power (z = shift / √(1/20 + 1/500) ≈ 4.3 × shift, against thresholds of 4–5), so these scenarios only guard against larger changes; other scenarios cover the force of infection.
- **EffectiveDrug has heavy tails:** a few replicates have an outbreak (up to 2081 clinical episodes against a mean of 50), so its threshold is 17.4 and its group test sees only large changes. A rank-based group statistic (Wilcoxon rank-sum) was tried on the same data in S7 and did no better there, nor overall (F32).
- **Thresholds above ~5.5** (6, ESTS, NamawalaArabiensis, NoInterv, NoMPDLarviciding, VecTest) come from one or two rare-count quantities (e.g. ESTS measure 64: 14 of 500 replicates at 1, the rest 0), whose K-run mean is skewed. Their threshold applies to all of the scenario's quantities, which costs some power there.
- Rule (a) adds little power: Genotypes is caught through its aggregates, with the miss count unremarkable (p ≈ 0.2). For strongly correlated cells, the leave-one-out miss counts are too heavy-tailed for a coherent shift to stand out (F17).

**Known limitation: only means are tested.** A change that keeps every group quantity's mean but changes a spread or a distribution's shape (e.g. a sampler with the right mean and the wrong variance) passes the group test, and the single-run check sees it only if it is gross. Means of totals also hide changes that cancel across surveys or age groups.

**Using it for draw-order changes (D2b, M4, M8).** G-stat for these chunks is `ctest -L statistical` plus `ctest -C full -L statistical-full`. A pass means no change in any group quantity's mean beyond the MDE above, not proof of equivalence.

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

In S6 this took about 1 h for the fresh seeds and 1 h for the power runs at `-j 4` on an Apple M5. The report takes about 7 minutes on one core, mostly re-reading the GaussianCopula runs.

Not covered: the scenarios without a reference (ModelName*, AvailabilityFilter, ExactNv0, SimpleMPDTest; see V1). No nightly random-seed job was added.
