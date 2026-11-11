# OpenMalaria Safe Rust Migration — Cloud VM Handoff Runbook

This repository is mid-migration from legacy C++20 (`model/`) to a modern, library-first, Python-callable, 100% Safe Rust workspace (`Cargo.toml`, `crates/*`, `bindings/openmalaria-py`) following the [`.agents/skills/migrate-to-rust/SKILL.md`](skills/migrate-to-rust/SKILL.md) workflow.

## 1. Completed Milestones & Verified Crates

- **Stage 0 & Stage 1 (`migration_telemetry.json` initialized)**:
  - Legacy `before` census (`150` C++ files, `17,548` code LOC, `11,127` comment LOC) and all `8` discovered C++ latent bugs + `4` architectural/performance/safety opportunities are logged in [`migration_telemetry.json`](../migration_telemetry.json).
  - Full 6-section Architectural Blueprint is saved at [`.agents/MIGRATION_PLAN.md`](MIGRATION_PLAN.md).
  - Exhaustive C++ bit-exact parity specifications (every equation, constant, RNG draw order, edge case, CSV schema, and `mon::record*` call) are saved in:
    - [`.agents/specs/01_schema_xml.md`](specs/01_schema_xml.md)
    - [`.agents/specs/02_pkpd_within_host.md`](specs/02_pkpd_within_host.md)
    - [`.agents/specs/03_clinical_interventions.md`](specs/03_clinical_interventions.md)
    - [`.agents/specs/04_transmission_sim_monitoring.md`](specs/04_transmission_sim_monitoring.md)
- **Completed & Verified Crates (`#![forbid(unsafe_code)]`, `0` clippy warnings, `<= 364` non-blank LOC/file)**:
  1. `crates/om-rng`: Bit-exact `MasterRng` (`ChaCha<8>` with OpenMalaria IV), `LocalRng` (`Xoshiro256+`), and pure-Rust GSL 2.8 probability distributions & inverse CDFs (`9/9` unit/proptest/FFI-parity tests + `3/3` doc-tests pass).
  2. `crates/om-core`: Instance-owned `SimTime`/`SimClock`, `Parameters`, `ModelOptions`, `AgeGroupInterpolator`, `DecayFunction`, `UnitParse`, Gaussian copula samplers, Fourier/IDFT, QAG Gauss-Kronrod 15-pt quadrature, secant root finder, Nelder-Mead 2D simplex, and LU linear solver (`6/6` tests + `4/4` doc-tests pass).
  3. `crates/om-schema`: Pure-Rust `roxmltree` + `serde` Schema v50 XML parser, `XMLChecker` post-validator, and programmatic builder (`4/4` tests + `1/1` doc-test pass; parses and bit-exact roundtrips all `53/53` `test/scenario*.xml` files).
  4. `crates/om-pkpd`: 1-compartment, 2/3-compartment (Monolix cubic roots + GK15 quadrature), and parent-metabolite conversion PK/PD models, schedules, and dosage tables (`21/21` unit/compliance/scenario/proptest tests + `2/2` doc-tests pass, matching all 11 C++ `PkPdComplianceSuite` drug models).

## 2. Cloud VM Environment & Hardware Baseline Calibration

> [!IMPORTANT]
> `migration_telemetry.json` currently stores a `legacy_baseline` benchmark measured on Apple Silicon (`6241.08 ms`). Because the Cloud VM runs on different hardware, **re-build `./build/openMalaria` in the Cloud VM and overwrite `legacy_baseline` on the Cloud VM CPU** so all 4 benchmark stages (`legacy_baseline`, `safe_rust`, `soa_simd`, `pgo_lto`) and `migration_infographic.html` compare identical hardware!

1. **Ensure C++, Rust, LLVM PGO, and Python tools are present**:
   - C++ reference build (`cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`): requires `cmake`, `g++`/`clang++`, `libgsl-dev`, `libboost-dev`, `xsdcxx`, `libxerces-c-dev`, `zlib1g-dev`.
   - Rust (`rustup component add clippy llvm-tools-preview`).
   - Python venv (`.venv` with `maturin`, `pytest`, `numpy`).
2. **Re-benchmark `legacy_baseline` on the Cloud VM** across the exact 5-scenario workload (`scenarioVecFullTest.xml`, `scenario2ITNs.xml`, `scenarioMolineaux.xml`, `scenarioVivax.xml`, `scenarioEffectiveDrug.xml` with `--resource-path test`, median wall-clock ms and max RSS MB over 3 runs) via:
   ```bash
   python3 .agents/skills/migrate-to-rust/scripts/telemetry.py log-benchmark \
     --telemetry migration_telemetry.json \
     --stage legacy_baseline \
     --wall-ms <CLOUD_MEDIAN_MS> \
     --peak-rss-mb <CLOUD_PEAK_RSS_MB>
   ```

## 3. Remaining Implementation & Verification Roadmap

Keep `_active_bucket = "rewrite"` in `migration_telemetry.json` and implement the remaining crates in strict dependency order, consulting `.agents/specs/01..04_*.md` and `model/` C++ sources for every equation and RNG draw order:

1. **Tier 1B — `crates/om-monitoring`** (depends on `om-core`, `om-schema`, `flate2`):
   - All 89 `OutMeasure`s (`model/mon/OutMeasures.h`), `DeployMethod`, `SubPopRemoveType`, `CohortRegistry` & `HostCohorts`, `MonAgeGroups` (allocating `K + 1` bins with overflow bin `K` included in `surveySum` for `<condition>` and excluded on output write!), `Monitoring` survey stores, `allCauseIMR` trailing row `(1, 1, 21, val)`, `output.txt`, `OMOUTBIN` v2 `output.bin`, `.gz` compression, structured column tables for Python, and all 31 `Continuous` (`ctsout.txt`) callbacks.
2. **Tier 2 — `crates/om-within-host`** (depends on `om-core`, `om-rng`, `om-schema`, `om-pkpd`, `om-monitoring`):
   - `GenotypeRegistry`, `DiagnosticRegistry`, `SimpleTreatment`, `InfectionIncidenceModel` (baseline, NegBinom, LogNormal, heterogeneity, PEV), `WHFalciparum` (`DescriptiveWithinHost` + `CommonWithinHost` + `Dummy`, `Descriptive` `densities.csv`, `Empirical` `autoRegressionParameters.csv`, `Molineaux` **`f32` state fields** + pairwise sample, `Penny`), `WHVivax` (`VivaxBrood`), and `PathogenesisModel` (`Ross`, `Predetermined`, `Mueller`, NMF, indirect mortality).
3. **Tier 3 (Parallelizable) — `crates/om-clinical` & `crates/om-transmission`**:
   - `crates/om-clinical`: `Episode` bitflags & destructor/flush report quirks, `InfantMortality`, `NeonatalMortality` (`<=` comparison!), `CM5DayCommon`, `ClinicalImmediateOutcomes`, `DecisionTree5Day`, `CMDecisionTree` (matching `CMDTMultiple` tag-category ordering in `model/Clinical/CMDecisionTree.cpp:666-705`), `ClinicalEventScheduler`.
   - `crates/om-transmission`: `PerHostAnophParams` (isolated `(0,0)` RNG for 100k availability percentiles), `PerHost` (with `HumanItn`, `HumanIrs`, `HumanGvi` mosquito survival/attractiveness/fecundity hooks), `NonVectorModel`, `VectorModel` (**descending `annualEIR` species sort** & `<monthlyValues>` `+= min` floor), `AnophelesModel` (Chitnis equations + ATSB secant solver), `SimpleMPDAnophelesModel`, `EmergenceRateAdaptiveFitter`, and `CalcNvO` exact periodic solver.
4. **Tier 4 — `crates/om-interventions`**:
   - `ComponentType` `stable_sort`, `ImportedInfections`, `Vaccine` (`PEV`/`BSV`/`TBV` + first-defined `reportComponent` + double `nTreatDeployments` quirk), `Screen`, `TreatSimple`, `TreatPKPD`, `DecisionTreeComponent`, `ClearImmunity`, `TimedHumanDeployment` (Gaussian copula evaluated *before* availability-percentile and `subPop` filters!), `TimedCumulativeHumanDeployment`, `ContinuousHumanDeployment` (unscaled `anophEntoAvailabilityRaw`), `TriggeredDeployments` (0 RNG when `coverage >= 1.0`), and vector/system timed interventions (`changeHS`, `changeEIR`, `uninfectVectors`, `vectorPop`, `vectorTrap`, `addNonHumanHosts`, `nonHumanHostsModifications`).
5. **Tier 5 — `crates/om-sim`, `crates/openmalaria`, `crates/openmalaria-cli`, `bindings/openmalaria-py`**:
   - `om-sim`: `PopulationAgeStructure`, `Population` (`#[cfg(feature = "soa")]` SoA hot arrays vs AoS baseline), exact 11-step main loop (`model/main.cpp:86-125`), deterministic `bincode` + `.gz` checkpointing (`--checkpoint` / `--checkpoint-stop`), and `Rayon` `run_batch`.
   - `openmalaria`: Public safe library API (`Scenario`, `Simulation`, `SimulationBuilder`, `SimulationResult`, `CliArgs`, extension traits).
   - `openmalaria-cli`: Thin ~60 LOC `openMalaria` binary built strictly as sequential `openmalaria` library calls.
   - `bindings/openmalaria-py` + `python/openmalaria` + `python/tests/test_openmalaria.py`: `PyO3` + `maturin` Python package releasing the GIL during stepping and `run_batch`.
6. **Benchmarks, Crucible, Parity, PGO & Infographic (`migrate-to-rust` Stages 2.2 – 2.5)**:
   - Measure & log `safe_rust` (`--no-default-features`) and `soa_simd` (default `soa` + Fat LTO).
   - `test_heal` bucket (max 5 iterations): `cargo clippy --workspace --all-targets -- -D warnings`, `cargo test --workspace --all-targets`, `cargo test --workspace --doc`, and `maturin develop --release` + `pytest` (`0` warnings, `0` failures).
   - `parity` bucket (max 5 iterations): A/B differential parity comparing `./build/openMalaria` vs `./target/release/openMalaria` across `N >= 20` deterministic seeds (`1..=20`) and the `OM_BOXTEST_NAMES` scenarios in `test/CMakeLists.txt` using `util/compareOutput.py` (`1e-6`) and `util/compareCtsout.py`.
   - `pgo_bench` bucket: LLVM PGO (`-Cprofile-generate` -> 5-scenario workload -> `llvm-profdata merge` -> `-Cprofile-use` + Fat LTO), log `pgo_lto`, run `scan-codebase --phase after --dir crates` (verifying `0` `unsafe` blocks and `100.0%` of `.rs` files `<= 400` non-blank LOC), `log-spotlight`, `set-story`, and `render-infographic` -> `migration_infographic.html`.

## 4. Non-Negotiable Engineering Constraints

- **`#![forbid(unsafe_code)]`** in every crate under `crates/*` (`0` `unsafe` blocks). Keep `PyO3` proc-macros strictly inside `bindings/openmalaria-py`.
- **File Modularity**: `telemetry.py` counts **total non-blank lines (`code + comment`)** against `<= 400` LOC (`files_under_400_pct`). Keep every `.rs` file `<= 360` non-blank lines!
- **Documentation Integrity**: Include `//!` and `///` rustdoc comments and runnable doc-tests across every crate.
