# Mathematical & Algorithmic Specification: `crates/om-pkpd` and `crates/om-within-host`

All equations, constants, state fields, CSV formats, RNG call orders, and `mon::record*` calls below are extracted directly from the C++ source in `model/PkPd/` and `model/Host/`.

---

## Global Time & RNG Conventions (`Global.h`, `sim.h`, `util/random.h`, `util/sampler.cpp`)
- `PI = 3.14159265358979323846`.
- `SimTime = i32` (always in **days**). `DAYS_IN_YEAR = 365`. `interval = sim::oneTS()` (`1` or `5` days).
  - `sim::inYears(d) = d as f64 * (1.0 / 365.0)`
  - `sim::inSteps(d) = d / interval` (integer division)
  - `sim::moduloSteps(d, denom) = ((d / interval) % denom)` (`mod_nn`, asserts `d >= 0 && denom > 0`)
  - `sim::roundToTSFromDays(days_f64) = interval * floor(days_f64 / interval as f64 + 0.5) as i32`
  - `sim::yearsPerStep() = interval as f64 / 365.0`; `sim::stepsPerYear() = 365 / interval` (`73` for 5d, `365` for 1d).
  - `sim::never() = -0x3FFFFFFF`, `sim::future() = 0x3FFFFFFF`, `sim::zero() = 0`.
  - During `start_update()..end_update()`: `ts0 = s_t0`, `ts1 = s_t0 + interval`, `nowOrTs0() = ts0`, `nowOrTs1() = ts1`, `latestTs0() = ts0`. Outside updates (monitoring/interventions): `s_t0 == s_t1 == now()`, `nowOrTs0() = now()`, `nowOrTs1() = now()`, `latestTs0() = now() - interval`.
- **`LocalRng` (`RNG<Xoshiro256P>`)**:
  - `uniform_01() -> f64` in `[0, 1)`
  - `bernoulli(p) -> bool`: `uniform_01() < p` (**strict `<`**)
  - `uniform(n: i32) -> i32`: `(uniform_01() * n as f64) as i32`
  - `gauss(mean, std)`: `gsl_ran_gaussian(std) + mean`
  - `log_normal(meanlog, stdlog)`: `gsl_ran_lognormal(meanlog, stdlog)`
  - `gamma(a, b)`: `gsl_ran_gamma(a, b)` (shape `a`, scale `b`)
  - `beta(a, b)`: `gsl_ran_beta(a, b)`; `betaWithMean(m, b) = beta(m * b / (1.0 - m), b)`
  - `poisson(lambda)`: `gsl_ran_poisson(lambda)` (throws if `!lambda.is_finite()`)
  - `weibull(lambda, k)`: `gsl_ran_weibull(lambda, k)` (scale `lambda`, shape `k`)
  - `max_multi_log_normal(start, n, meanlog, stdlog)`:
    - Consumes **1** `uniform_01()` call: `normp = uniform_01().powf(1.0 / n as f64)`
    - `zval = gsl_cdf_ugaussian_Pinv(normp)`
    - `multi_sample = exp(meanlog + stdlog * zval)`
    - returns `max(start, multi_sample)`.
- **`LognormalSampler` (`util/sampler.cpp`)**:
  - `fromMeanCV(mean, CV, truncate)`:
    - If `CV == 0.0`: `sigma = 0.0`, `mu = -inf` if `mean == 0.0` else `ln(mean)`.
    - Else (`mean > 0, CV > 0`): `a = 1.0 + CV * CV`, `mu = ln(mean / sqrt(a))`, `sigma = sqrt(ln(a))`.
  - `fromMeanVariance(mean, variance, truncate)`:
    - Note C++ formula on line 145 of `sampler.cpp`: `CV = variance / mean` (literally `variance / mean`, NOT `sqrt(variance)/mean`!), `a = 1.0 + CV * CV`, `mu = ln(mean / sqrt(a))`, `sigma = sqrt(ln(a))` (or `sigma = 0, mu = ln(mean)` if `variance == 0.0`).
  - `sample(rng)`:
    - **If `sigma == 0.0`: NO RNG CALL**, `value = exp(mu)` (`0.0` when `mu == -inf`).
    - **Else**: `value = rng.log_normal(mu, sigma)` (1 `gsl_ran_lognormal` call).
    - If `truncate` set and `value > truncate`: returns `truncate`.
  - `sample(NormalSample(x))`: `exp(sigma * x + mu)` (Note: does not apply `truncate`).
- **`NormalSample`**:
  - `generate(rng)`: `NormalSample(rng.gauss(0.0, 1.0))` (1 Gaussian draw).
  - `generate_correlated(base, corr, factor, rng)` where `factor = sqrt(1.0 - corr * corr)`:
    - **If `corr == 1.0`: NO RNG CALL**, returns `base`.
    - **Else**: `e = rng.gauss(0.0, factor)`, returns `NormalSample(base.x * corr + e)`.

---

## Section 1: `Human` Glue, `InfectionIncidenceModel`, `NeonatalMortality`, `ImportedInfections`

### 1.1 `Human` Construction & `Host::update` Glue (`Human.cpp`)
- **Birth RNG call order** (`Human::Human(dob)`):
  1. Seed `human.rng` from `master_RNG` (`gen_seed()` twice / Xoshiro jump/seed from ChaCha8).
  2. `hetSample(human.rng)` (defaults: `availabilityFactor = 1.0, comorbidityFactor = 1.0, treatmentSeekingFactor = 1.0`):
     - If `TRANS_HET`: `avail = if rng.bernoulli(0.5) { 1.8 } else { 0.2 }`
     - If `COMORB_HET`: `comorb = if rng.bernoulli(0.5) { 1.8 } else { 0.2 }`
     - If `TREAT_HET`: `treat = if rng.bernoulli(0.5) { 1.8 } else { 0.2 }`
     - Then an `if / else if` chain:
       - `if TRANS_TREAT_HET`: if `rng.bernoulli(0.5)` `{ treat = 1.8; avail = 0.2 }` else `{ treat = 0.2; avail = 1.8 }`
       - `else if COMORB_TREAT_HET`: if `rng.bernoulli(0.5)` `{ comorb = 1.8; treat = 0.2 }` else `{ comorb = 0.2; treat = 1.8 }`
       - `else if COMORB_TRANS_HET`: if `rng.bernoulli(0.5)` `{ avail = 0.2; comorb = 0.2 }` else `{ avail = 1.8; comorb = 1.8 }`
       - `else if TRIPLE_HET`: if `rng.bernoulli(0.5)` `{ avail = 0.2; comorb = 0.2; treat = 1.8 }` else `{ avail = 1.8; comorb = 1.8; treat = 0.2 }`
  3. `withinHostModel = WHInterface::createWithinHostModel(rng, het.comorbidityFactor)`
  4. `iiFactor = infIncidence.getAvailabilityFactor(rng, 1.0)`
  5. `perHostTransmission.initialise(rng, het.availabilityFactor * iiFactor)`
  6. `clinicalModel = ClinicalModel::createClinicalModel(het.treatmentSeekingFactor)` (no RNG).

- **Per-timestep `Host::update(human, transmission)` order**:
  1. `age0 = human.age(sim::ts0())`. If `clinicalModel.isDead(age0)` -> `human.kill(); return;`.
  2. `mon::updateAgeGroup(human.monitoringAgeGroup, age0)`; `human.updateCohortSet()`.
  3. `age1 = sim::inYears(human.age(sim::ts1()))`.
  4. `transmission.getEIR(human, age0, age1, EIR_per_genotype_i, EIR_per_genotype_l)`.
  5. `EIR_i = sum(EIR_per_genotype_i)`, `EIR_l = sum(EIR_per_genotype_l)`, `EIR = EIR_i + EIR_l`.
  6. `x = if EIR > 0.0 { EIR_i / EIR } else { 0.0 }`.
  7. `expectedNNewInfs = infIncidence.expectedNumNewInfections(human, EIR)`.
  8. `expectedNNewInfs_i = x * expectedNNewInfs`; `expectedNNewInfs_l = (1.0 - x) * expectedNNewInfs`.
  9. **RNG ORDER**:
     - First: `nNewInfs_l = infIncidence.numNewInfections(human, expectedNNewInfs_l)` (indigenous FIRST!)
     - Second: `nNewInfs_i = infIncidence.numNewInfections(human, expectedNNewInfs_i)` (introduced SECOND!)
  10. `withinHostModel.update(human, human.rng, &mut nNewInfs_i, &mut nNewInfs_l, EIR_per_genotype_i, EIR_per_genotype_l, age1)`.
  11. `infIncidence.reportNumNewInfections(human, nNewInfs_i, nNewInfs_l)`.
  12. `clinicalModel.update(human, age1, age0 == 0)`; `clinicalModel.updateInfantDeaths(age0)`.

### 1.2 `InfectionIncidenceModel` (`InfectionIncidenceModel.h/.cpp`)
- **Static Parameters (from `Parameters`)**:
  - `baseline_avail_shape_param = param[16: BASELINE_AVAILABILITY_SHAPE]`
  - `baseline_avail_offset = 0.5 * baseline_avail_shape_param^2`
  - `gamma_p = param[5: GAMMA_P]`
  - `Sinf = 1.0 - exp(-param[1: NEG_LOG_ONE_MINUS_SINF])`
  - `Simm = param[3: SIMM]`
  - `EstarInv = 1.0 / param[2: E_STAR]`
  - `Xstar_pInv = 1.0 / param[4: X_STAR_P]`
  - `r_square_Gamma = 0.649`
  - Options:
    - `opt_no_pre_erythrocytic = ModelOptions::option(NO_PRE_ERYTHROCYTIC)`
    - `opt_neg_bin_mass_action = ModelOptions::option(NEGATIVE_BINOMIAL_MASS_ACTION)`
    - `opt_vaccine_genotype = ModelOptions::option(VACCINE_GENOTYPE)`
    - If `opt_neg_bin_mass_action`:
      $$\text{inf\_rate\_shape\_param} = \max\!\left(\frac{\text{baseline\_avail\_shape\_param} + 1.0}{0.649 \cdot \text{baseline\_avail\_shape\_param} - 1.0},\ 0.0\right)$$
    - Else if `ModelOptions::option(LOGNORMAL_MASS_ACTION)`:
      - `opt_lognormal_mass_action = true`
      - `r_square_LogNormal = ln(1.0 + 0.649)`
      - $\text{inf\_rate\_shape\_param} = \max\!\left(\sqrt{r^2_{\text{LN}} - 1.86 \cdot \text{baseline\_avail\_shape\_param}^2},\ 0.0\right)$ (Note: in IEEE-754, `std::max(NaN, 0.0)` returns `NaN` when thefirst arg is `NaN`, which is then caught by `isnan(inf_rate_shape_param)` -> `xml_scenario_error`!)
      - $\text{inf\_rate\_offset} = 0.5 \cdot \text{inf\_rate\_shape\_param}^2$
    - Else if any of `TRANS_HET | COMORB_TRANS_HET | TRANS_TREAT_HET | TRIPLE_HET`: `opt_any_het = true`.
- **State Fields**:
  - Per-human: `m_pInfected: f64 = 0.0`, `m_cumulativeEIRa: f64 = 0.0`.
  - Static: `ctsNewInfections: i32 = 0` (reset to `0` in `preMainSimInit()` and when `ctsReportNewInfections` writes `\t{ctsNewInfections}`).
- **Variant Selection (`createModel`)**:
  - `NegBinomMAII` if `opt_neg_bin_mass_action`
  - `LogNormalMAII` if `opt_lognormal_mass_action`
  - `HeterogeneityWorkaroundII` if `opt_any_het`
  - `InfectionIncidenceModel` (baseline) otherwise.
- **`getAvailabilityFactor(rng, baseAvailability = 1.0)`**:
  - Baseline / `HeterogeneityWorkaroundII`: `baseAvailability` (0 RNG calls).
  - `NegBinomMAII`: `rng.gamma(baseline_avail_shape_param, baseAvailability / baseline_avail_shape_param)` (1 `gamma` call).
  - `LogNormalMAII`: throws if `baseAvailability != 1.0`; returns `rng.log_normal(ln(baseAvailability) - baseline_avail_offset, baseline_avail_shape_param)` (1 `log_normal` call).
- **`susceptibility()`**:
  - If `opt_no_pre_erythrocytic`: `0.702`.
  - Else: $S_2 = S_{\text{imm}} + \dfrac{1.0 - S_{\text{imm}}}{1.0 + (m\_\text{cumulativeEIRa} \cdot X^*_{p,\text{inv}})^{\gamma_p}}$.
- **`getModelExpectedInfections(rng, effectiveEIR, phTrans)`**:
  - Baseline: $\left(S_{\inf} + \dfrac{1.0 - S_{\inf}}{1.0 + (\text{effectiveEIR} / \text{oneTS}) \cdot E^*_{\text{inv}}}\right) \cdot S_2 \cdot \text{effectiveEIR}$ (0 RNG calls).
  - `HeterogeneityWorkaroundII`: $\left(S_{\inf} + \dfrac{1.0 - S_{\inf}}{1.0 + (\text{effectiveEIR} / (\text{oneTS} \cdot \text{phTrans.relativeAvailabilityHet})) \cdot E^*_{\text{inv}}}\right) \cdot S_2 \cdot \text{effectiveEIR}$ (0 RNG calls).
  - `NegBinomMAII`: `rng.gamma(inf_rate_shape_param, effectiveEIR * S_2 / inf_rate_shape_param)` (1 `gamma` call).
  - `LogNormalMAII`:
    - **If `effectiveEIR < 1e-10`: returns `0.0` (0 RNG calls!)**.
    - Else: `meanlog = ln(effectiveEIR * S_2) - inf_rate_offset`; returns `rng.log_normal(meanlog, inf_rate_shape_param)` (1 `log_normal` call).
- **`expectedNumNewInfections(human, effectiveEIR)`**:
  1. Throws `Error::EffectiveEIR` if `!effectiveEIR.is_finite()`.
  2. `expected = getModelExpectedInfections(human.rng, effectiveEIR, human.perHostTransmission)`.
  3. **PEV Vaccine**: If `!opt_vaccine_genotype`:
     - `expected *= human.vaccine.getFactor(Vaccine::PEV, 0)`.
     - (`PerHumanVaccine::getFactor(type, genotype)` multiplies `1.0 - effect.perGenotypeInitialEfficacy[genotype] * effect.hetSample.eval(sim::ts1() - effect.timeLastDeployment)` across all active vaccine effects of `type`; **0 RNG calls** here — `betaWithMean` and decay `hetSample` are drawn at vaccination deployment time).
  4. `m_cumulativeEIRa += effectiveEIR`.
  5. `m_pInfected = (1.0 - exp(-expected) * (1.0 - m_pInfected)).clamp(0.0, 1.0)`.
  6. Returns `expected`.
- **`numNewInfections(human, expectedNumInfections)`**:
  - If `expectedNumInfections > 0.0000001` (`1e-7`):
    - `n = human.rng.poisson(expectedNumInfections)` (1 `poisson` call).
    - returns `min(n, 21 /* MAX_INFECTIONS */)`.
  - If `expectedNumInfections.is_nan()`: throws `Error::NumNewInfections`.
  - Else: returns `0` (**0 RNG calls**).

### 1.3 `NeonatalMortality` (`NeonatalMortality.h/.cpp`)
- **Constants**:
  - `gEst = 0.011`, `critPrev2025 = 0.25`, `critPrevPrim = 0.19`, `pBirthPrim = 0.3`
  - `y = pBirthPrim * gEst = 0.0033`, `z = -1.0 / critPrev2025 = -4.0`
  - `ageLb = 20 * 365 = 7300`, `ageUb = 25 * 365 = 9125`.
- **Static State**:
  - `riskFromMaternalInfection: f64 = 0.0`
  - `prevByGestationalAge: Vec<f64>` of length `sim::inSteps(5 * 30)` (`150 / interval`: `30` for 5d TS, `150` for 1d TS), initialized to `0.0`.
  - `neonatalDiagnostic: &Diagnostic` (from `clinical.neonatalMortality.diagnostic` if present, else `diagnostics::monitoringDiagnostic()`).
- **`NeonatalMortality::update(population)`** (called once per TS after `vectorUpdate` and before `Host::update`):
  1. `nCounter = 0`, `pCounter = 0`.
  2. Iterate `human` in `population` (ordered oldest-first in OpenMalaria):
     - `age = human.age(sim::ts0())`
     - If `age >= ageUb`: `continue`
     - If `age < ageLb`: **`break`** (early exit!)
     - `nCounter += 1`
     - If `human.withinHostModel.diagnosticResult(human.rng, *neonatalDiagnostic)`: `pCounter += 1` (consumes 1 `bernoulli` on `human.rng` iff `neonatalDiagnostic` is stochastic & non-Vivax).
  3. `prev2025 = if nCounter > 0 { pCounter as f64 / nCounter as f64 } else { 0.25 }`.
  4. `index = sim::moduloSteps(sim::ts0(), prevByGestationalAge.len())`.
  5. `prevByGestationalAge[index] = prev2025`.
  6. `maxPrev = prevByGestationalAge.iter().fold(prev2025, |acc, &v| if v > acc { v } else { acc })`.
  7. `prevPG = maxPrev / (0.19 + maxPrev)`.
  8. `riskFromMaternalInfection = 0.0033 * (1.0 - exp(prevPG * (-4.0)))`.
- **`NeonatalMortality::eventNeonatalMortality(rng)`** (called in `ClinicalModel::update` when `newBorn`, i.e., `age0 == 0`):
  - Returns `rng.uniform_01() <= riskFromMaternalInfection` (**uses `<=`, NOT `<`, and ALWAYS consumes 1 `uniform_01()` draw** even if `riskFromMaternalInfection == 0.0`).

### 1.4 `ImportedInfections` (`ImportedInfections.h`)
- **State Fields**:
  - `period: SimTime` (days; `0` if non-periodic)
  - `lastIndex: u32 = 0` (checkpointed)
  - `rate: Vec<Rate { time: SimTime, value: f64 }>`:
    - Loaded from XML: `time = readDate(xmlTime, STEPS) - sim::startDate()`, `value = xmlRatePer1000PerYear * sim::yearsPerStep() * (1.0 / 1000.0)`.
    - Sorted ascending by `time`. If `period != 0 && rate[0].time != 0` -> error. Consecutive entries with equal `value` are erased.
- **`import(population)`** (called during intervention deployment at `sim::now()`):
  1. If `rate.is_empty()` return.
  2. `now = sim::intervTime()`; if `period > 0`, `now = mod_nn(now, period)`.
  3. If `rate[lastIndex].time > now`: `lastIndex = 0`.
  4. If `now > rate[lastIndex].time && rate.len() > lastIndex + 1 && rate[lastIndex + 1].time <= now`: `lastIndex += 1`.
  5. `rateNow = rate[lastIndex].value`.
  6. **If `rateNow > 0.0`** (if `rateNow <= 0.0`, **0 RNG calls**):
     - For each `human` in `population`:
       - If `human.rng.bernoulli(rateNow)` (1 `uniform_01()` per human):
         - `human.withinHostModel.importInfection(human.rng)`
         - `mon::recordEvent("nNewInfections", human)`
         - `mon::recordEvent("nNewInfections_Imported", human)`

---

## Section 2: Within-Host Models (`crates/om-within-host`)

### 2.1 `Genotypes` (`Genotypes.h/.cpp`)
- **Types & Static State**:
  - `Genotype { alleles: BTreeSet<u32>, init_freq: f64, fitness: f64, hrp2_deficient: bool }`.
  - `SampleMode`: `SAMPLE_FIRST` (default when `<parasiteGenetics>` absent), `SAMPLE_INITIAL`, `SAMPLE_TRACKING`.
  - `current_mode: SampleMode` (checkpointed), `interv_mode: SampleMode`, `N_genotypes: usize`.
  - `cum_initial_freqs: BTreeMap<f64, u32>` (cumulative `init_freq` -> `genotype_id`), with failsafe `cum_initial_freqs[1.0] = N_genotypes - 1`.
- **Initialization (`init`)**:
  - If `<parasiteGenetics>` absent: `N_genotypes = 1`, single genotype `(allele=0, init_freq=1.0, fitness=1.0, hrp2_deficient=false)`, `current_mode = interv_mode = SAMPLE_FIRST`, `cum_initial_freqs[1.0] = 0`.
  - If `<parasiteGenetics>` present:
    - `current_mode = SAMPLE_INITIAL` during warmup! `interv_mode = SAMPLE_INITIAL` if `samplingMode == "initial"`, `SAMPLE_TRACKING` if `"tracking"`. (`preMainSimInit()` sets `current_mode = interv_mode`).
    - Each locus validates `0.999 <= sum(allele.initialFrequency) <= 1.001`, then adjusts `alleles[0].init_freq += 1.0 - cum_p`.
    - Cartesian product across loci (`LocusSet::include`): outer loop over existing combinations `i`, inner loop over new locus alleles `j`:
      `cross`: `alleles = union`, `init_freq = a.init_freq * b.init_freq`, `fitness = a.fitness * b.fitness`, `hrp2_deficient = a.hrp2_deficient || b.hrp2_deficient`.
    - Validates total `0.999 <= cum_p <= 1.001`; inserts `(cum_p, id)` into `BTreeMap` (note: in C++ `std::map::insert`, if an allele has `init_freq == 0.0`, duplicate key `cum_p` is NOT overwritten, except final `cum_initial_freqs[1.0] = N - 1` overwrites key `1.0`).
- **`sampleGenotype(rng, genotype_weights)`**:
  - If `current_mode == SAMPLE_FIRST`: returns `0` (**0 RNG calls**).
  - Else if `current_mode == SAMPLE_INITIAL || genotype_weights.is_empty()`:
    - `sample = rng.uniform_01()` (1 `uniform_01()` call).
    - Lookup first entry in `cum_initial_freqs` with key **strictly `> sample`** (`upper_bound(sample)`), return its `genotype_id`.
  - Else (`SAMPLE_TRACKING` and `!genotype_weights.is_empty()`):
    - `weight_sum = sum(genotype_weights)`.
    - `sample = rng.uniform_01() * weight_sum` (1 `uniform_01()` call).
    - `cum = 0.0`; for `g` in `0..N_genotypes`: `cum += genotype_weights[g]`; if `sample < cum` return `g as u32`. Fallback returns `0`.

### 2.2 `Diagnostic` & `diagnostics` (`Diagnostic.h/.cpp`)
- **Fields**: `specificity: f64` (`NaN` = deterministic), `dens_lim: f64`, `uses_hrp2: bool` (`mechanism == "HRP2"`).
- **Initialization**:
  - If `VIVAX_SIMPLE_MODEL`: `dens_lim = NaN`, `specificity = NaN`.
  - If `<deterministic minDensity="..."/>`: `dens_lim = minDensity`, `specificity = NaN`.
  - If `<stochastic dens_50="..." specificity="..."/>`: `dens_lim = dens_50`; if `dens_lim == 0.0`, `specificity = NaN` (collapses to deterministic!), else `specificity = specificity` (`[0, 1]`).
  - Unit bias multiplier on `dens_lim`:
    - `"Other"` (or omitted when `!GARKI_DENSITY_BIAS`): `dens_lim *= param[15: DENSITY_BIAS_NON_GARKI]`
    - `"Garki"`: `dens_lim *= param[20: DENSITY_BIAS_GARKI]`
    - `"Malariatherapy"`: `dens_lim *= 1.0`.
  - `monitoringDiagnostic`: if `<surveys detectionLimit="..."/>`, deterministic with `detectionLimit * (GARKI_DENSITY_BIAS ? param[20] : param[15])`; else named `<surveys diagnostic="..."/>` (must not have `uses_hrp2`).
- **`isPositive(rng, dens, densHRP2) -> bool`**:
  - `d = if uses_hrp2 { densHRP2 } else { dens }`.
  - If `specificity.is_nan()` (deterministic): returns `d >= dens_lim` (**0 RNG calls**).
  - Else (stochastic):
    $$p_{\text{pos}} = 1.0 + \text{specificity} \cdot \left(\frac{d}{d + \text{dens\_lim}} - 1.0\right)$$
    returns `rng.bernoulli(pPositive)` (**1 `uniform_01()` call**).

### 2.3 `Treatments` (`WithinHost/Treatments.h/.cpp`)
- Simple non-PKPD treatment descriptor:
  - `Stages`: `NONE = 0, LIVER = 1, BLOOD = 2, BOTH = 3`.
  - `timeLiver: SimTime` (`0` = none, `-interval` from `timesteps="-1"` = immediate retroactive clearance, `>= 1` = proactive clearance for `len` days).
  - `timeBlood: SimTime` (same encoding).
  - Plus `TriggeredDeployments` (deployed when `WH::treatment(human, treatId)` is called).

### 2.4 `Infection` & `CommonInfection` Base (`Infection.h`, `CommonInfection.h`)
- `InfectionOrigin`: `Imported = 0, Introduced = 1, Indigenous = 2`.
- **`get_infection_origin(infections)`**:
  - If `infections.is_empty()` -> `Indigenous`.
  - Else if any infection has `Introduced` -> `Introduced`.
  - Else if any infection has `Indigenous` -> `Indigenous`.
  - Else (all `Imported`) -> `Imported`.
- **`Infection` fields**:
  - `m_startDate: SimTime = sim::nowOrTs0()`
  - `m_density: f64 = 0.0`
  - `m_cumulativeExposureJ: f64 = 0.0`
  - `m_genotype: u32`
  - `m_origin: i32` (`InfectionOrigin`)
  - Static `s_latentP: SimTime` (default `15` days if `<parameters>` absent, else parsed `latentp`).
  - `bloodStage() -> bool`: `sim::latestTs0() - m_startDate > 5` (strictly `> 5` days!).
  - `isHrp2Deficient() -> bool`: `Genotypes::getGenotypes()[m_genotype].hrp2_deficient`.
- **`CommonInfection` adds**:
  - `Kn: BTreeMap<usize, f64>` (`IC50^slope` cached per `drug_type_index` for this infection; note: NOT checkpointed in C++!).
  - `update(rng, survivalFactor, now, body_mass) -> bool`:
    - `bsAge = now - m_startDate - s_latentP`
    - If `bsAge < 0`: returns `false` (still in liver/latent stage; **0 RNG calls**).
    - Else: calls `updateDensity(rng, survivalFactor, bsAge, body_mass)`.

### 2.5 `WHFalciparum` Base (`WHFalciparum.h/.cpp`)
- **Static Parameters**:
  - `sigma_i = sqrt(param[6: SIGMA_I_SQ])`
  - `immPenalty_22 = 1.0 - exp(param[22: IMMUNITY_PENALTY])` (unused, commented out in C++)
  - `immEffectorRemain = exp(-param[23: IMMUNE_EFFECTOR_DECAY])`
  - `asexImmRemain = exp(-param[27: ASEXUAL_IMMUNITY_DECAY])`
  - `invCumulativeYstar = 1.0 / param[7: CUMULATIVE_Y_STAR]`
  - `invCumulativeHstar = 1.0 / param[8: CUMULATIVE_H_STAR]`
  - `alpha_m = 1.0 - exp(-param[9: NEG_LOG_ONE_MINUS_ALPHA_M])`
  - `decayM = param[10: DECAY_M]`
  - `y_lag_len = sim::inSteps(20) + 1` (`5` for 5d TS, `21` for 1d TS).
  - Gametocyte transmission constants:
    - `PTM_beta1 = 1.0`, `PTM_beta2 = 0.46`, `PTM_beta3 = 0.17`
    - `PTM_tau = 0.066`, `PTM_tau_prime = 1.0 / sqrt(1.0 / 0.066)`
    - `PTM_mu = -8.1`.
- **Per-Human State Fields**:
  - `numInfs: i32 = 0` (`MAX_INFECTIONS = 21`)
  - `_innateImmSurvFact: f64` — sampled in constructor: `exp(-rng.gauss(0.0, sigma_i))` (**1 `gauss` call**; note the minus sign `-rng.gauss(...)`!).
  - `m_cumulative_h: f64 = 0.0`, `m_cumulative_Y: f64 = 0.0`, `m_cumulative_Y_lag: f64 = 0.0`
  - `totalDensity: f64 = 0.0`, `hrp2Density: f64 = 0.0`, `timeStepMaxDensity: f64 = 0.0`
  - `m_y_lag_i: Vec<f64>`, `m_y_lag_l: Vec<f64>`, both of length `y_lag_len * Genotypes::N()`, zero-initialized.
  - `pathogenesisModel: Box<PathogenesisModel>`
  - `treatExpiryLiver: SimTime = 0`, `treatExpiryBlood: SimTime = 0`.
- **Immunity Equations**:
  - `updateImmuneStatus()` (called **before** updating infections each time step):
    - If `immEffectorRemain < 1.0`:
      `m_cumulative_h *= immEffectorRemain`; `m_cumulative_Y *= immEffectorRemain`.
    - If `asexImmRemain < 1.0`:
      $$m\_\text{cumulative\_h} \mathrel{*}= \frac{\text{asexImmRemain}}{1.0 + m\_\text{cumulative\_h} \cdot (1.0 - \text{asexImmRemain}) \cdot \text{invCumulativeHstar}}$$
      $$m\_\text{cumulative\_Y} \mathrel{*}= \frac{\text{asexImmRemain}}{1.0 + m\_\text{cumulative\_Y} \cdot (1.0 - \text{asexImmRemain}) \cdot \text{invCumulativeYstar}}$$
    - `m_cumulative_Y_lag = m_cumulative_Y`.
  - `immunitySurvivalFactor(ageInYears, cumulativeExposureJ) -> f64`:
    - `dH = 1.0`, `dY = 1.0`.
    - If `m_cumulative_h > 1.0`:
      $$d_H = \frac{1.0}{1.0 + (m\_\text{cumulative\_h} - 1.0) \cdot \text{invCumulativeHstar}}, \quad d_Y = \frac{1.0}{1.0 + (m\_\text{cumulative\_Y} - \text{cumulativeExposureJ}) \cdot \text{invCumulativeYstar}}$$
    - $d_A = 1.0 - \alpha_m \cdot \exp(-\text{decayM} \cdot \text{ageInYears})$
    - returns `min(dY * dH * dA, 1.0)`.
- **`probTransmissionToMosquito(probTransGenotype_i, probTransGenotype_l) -> f64`** (0 RNG calls):
  - Let `n = Genotypes::N()`.
  - Indices into `m_y_lag_*`:
    - `d10 = mod_nn(y_lag_len + sim::inSteps(sim::ts1() - 10), y_lag_len)`
    - `d15 = mod_nn(y_lag_len + sim::inSteps(sim::ts1() - 15), y_lag_len)`
    - `d20 = mod_nn(y_lag_len + sim::inSteps(sim::ts1() - 20), y_lag_len)`
  - For each array (`m_y_lag_i` and `m_y_lag_l`), for `g in 0..n`:
    - `y10 = y_lag[d10 * n + g]`, `y15 = y_lag[d15 * n + g]`, `y20 = y_lag[d20 * n + g]`
    - `y_lag_g[g] = 1.0 * y10 + 0.46 * y15 + 0.17 * y20`
    - `y_lag_sum = 1.0 * sum_g(y10) + 0.46 * sum_g(y15) + 0.17 * sum_g(y20)`.
  - `y_lag_sum = y_lag_sum_i + y_lag_sum_l`.
  - If `y_lag_sum < 0.001`: returns `0.0` (leaves `probTransGenotype_*` untouched/zero).
  - `zval = (ln(y_lag_sum) + (-8.1)) * PTM_tau_prime`
  - `pone = gsl_cdf_ugaussian_P(zval)` (standard normal CDF $\Phi(\text{zval}) = \frac{1}{2}\text{erfc}(-\text{zval}/\sqrt{2})$)
  - `pTransmit = (pone * pone).clamp(0.0, 1.0)`.
  - If `pTransmit <= 0.0`: returns `pTransmit`.
  - For `g in 0..n`:
    - `probTransGenotype_i[g] = pTransmit * y_lag_g_i[g] / y_lag_sum`
    - `probTransGenotype_l[g] = pTransmit * y_lag_g_l[g] / y_lag_sum`
  - Returns `pTransmit`. (Caller in `TransmissionModel` / `VectorModel` multiplies by `human.vaccine.getFactor(TBV, ...)`).
- **`treatSimple(human, timeLiver, timeBlood) -> bool`**:
  - If `timeLiver != 0`:
    - If `timeLiver < 0`: `clearInfections(LIVER)` (clears `!inf.bloodStage()`)
    - Else: `treatExpiryLiver = max(treatExpiryLiver, sim::nowOrTs1() + timeLiver)`
    - `mon::recordEvent("nLiverStageTreatments", human)`
  - If `timeBlood != 0`:
    - If `timeBlood < 0`: `clearInfections(BLOOD)` (clears `inf.bloodStage()`)
    - Else: `treatExpiryBlood = max(treatExpiryBlood, sim::nowOrTs1() + timeBlood)`
    - returns `true`
  - returns `false`.

### 2.6 `CommonWithinHost` (`CommonWithinHost.h/.cpp`)
Used when `DUMMY_WITHIN_HOST_MODEL`, `EMPIRICAL_WITHIN_HOST_MODEL`, `MOLINEAUX_WITHIN_HOST_MODEL`, or `PENNY_WITHIN_HOST_MODEL` is active (`1d` or `5d` TS).
- **Static State**:
  - `massByAge: AgeGroupInterpolator` (from `model.human.weight`), `hetMassMultStdDev = weight.multStdDev`, `minHetMassMult = 0.5 / massByAge.eval(0.0)`.
- **Per-Human State (in addition to `WHFalciparum`)**:
  - `hetMassMultiplier: f64` — sampled in constructor **after** `WHFalciparum` constructor:
    `do { hetMassMultiplier = rng.gauss(1.0, hetMassMultStdDev); } while (hetMassMultiplier < minHetMassMult);`
  - `pkpdModel: LSTMModel`
  - `infections: Vec/List<Box<CommonInfection>>`.
- **`importInfection(rng)`**:
  - If `numInfs < 21`: `m_cumulative_h += 1.0`, `numInfs += 1`, `g = Genotypes::sampleGenotype(rng, &[])`, push `createInfection(rng, g, Imported)`.
- **`clearImmunity()`**:
  - For each `inf`: `inf.m_cumulativeExposureJ = 0.0`. Sets `m_cumulative_h = 0.0`, `m_cumulative_Y_lag = 0.0` (note: `m_cumulative_Y` is NOT reset in C++!).
- **`treatPkPd(schedule, dosage, age, delay_d)`**:
  - `mass = massByAge.eval(age) * hetMassMultiplier`; `pkpdModel.prescribe(schedule, dosage, age, mass, delay_d)`.
- **`update(human, rng, &mut nNewInfs_i, &mut nNewInfs_l, weights_i, weights_l, ageInYears)` — Exact Order**:
  1. Cap counts:
     - `nNewInfs_l = min(nNewInfs_l, 21 - numInfs)`
     - `nNewInfs_i = min(nNewInfs_i, 21 - numInfs - nNewInfs_l)`
     - (Note: C++ computes `nNewInfsIgnored = nNewInfs_i + nNewInfs_l - (21 - numInfs)` *after* clamping, so `nNewInfsIgnored <= 0` always!).
  2. **Add `Introduced` (`_i`) infections FIRST** (`0..nNewInfs_i`):
     - `g = Genotypes::sampleGenotype(rng, weights_i)`
     - If `opt_vaccine_genotype`:
       - `vf = human.vaccine.getFactor(PEV, g)`
       - If `vf == 1.0 || human.rng.bernoulli(vf)` -> push `createInfection(rng, g, Introduced)`, else `discarded_i += 1`.
     - Else -> push `createInfection(rng, g, Introduced)`.
     - `nNewInfs_i -= discarded_i`; `numInfs += nNewInfs_i`.
  3. **Add `Indigenous` (`_l`) infections SECOND** (`0..nNewInfs_l`):
     - Same logic with `weights_l` and `Indigenous`; `nNewInfs_l -= discarded_l`; `numInfs += nNewInfs_l`.
  4. `updateImmuneStatus()`.
  5. `totalDensity = 0.0; hrp2Density = 0.0; timeStepMaxDensity = 0.0;`
  6. `treatmentLiver = treatExpiryLiver > sim::ts0()`; `treatmentBlood = treatExpiryBlood > sim::ts0()`.
  7. `body_mass = massByAge.eval(ageInYears) * hetMassMultiplier`.
  8. **Daily loop**: `for now in (sim::ts0() .. sim::ts0() + sim::oneTS())` (1 iteration for 1d TS, 5 iterations for 5d TS):
     - `pkpdModel.medicate(rng)`
     - Iterate `inf` in `infections`:
       - `expires = if inf.bloodStage() { treatmentBlood } else { treatmentLiver }` (Note: `bloodStage()` uses `sim::latestTs0() - m_startDate > 5`, NOT `now`!).
       - If `!expires`:
         - `drugFactor = pkpdModel.getDrugFactor(rng, inf, body_mass)`
         - `immFactor = immunitySurvivalFactor(ageInYears, inf.cumulativeExposureJ())`
         - `bsvFactor = human.vaccine.getFactor(BSV, if opt_vaccine_genotype { inf.genotype() } else { 0 })`
         - `survivalFactor = bsvFactor * _innateImmSurvFact * immFactor * drugFactor`
         - `expires = inf.update(rng, survivalFactor, now, body_mass)`
       - If `expires`: remove `inf`, `numInfs -= 1`.
       - Else:
         - `d = inf.getDensity()`
         - `totalDensity += d` (Note: on 5d TS, `totalDensity` accumulates across all 5 days!)
         - if `!inf.isHrp2Deficient()` { `hrp2Density += d` }
         - `timeStepMaxDensity = max(timeStepMaxDensity, d)`
     - `pkpdModel.decayDrugs(body_mass)`
  9. `m_cumulative_h += (nNewInfs_i + nNewInfs_l) as f64`; `m_cumulative_Y += totalDensity`.
  10. Update `m_y_lag_i` and `m_y_lag_l` at slot `y_lag_idx = sim::moduloSteps(sim::ts1(), y_lag_len)`:
      - Zero `m_y_lag_i[y_lag_idx * N .. (y_lag_idx + 1) * N]` and `m_y_lag_l[...]`.
      - For each surviving `inf`: if `inf.origin() == Imported`, add `inf.getDensity()` to `m_y_lag_i[y_lag_idx * N + inf.genotype()]`; **else** (`Introduced` or `Indigenous`), add to `m_y_lag_l[y_lag_idx * N + inf.genotype()]`.

### 2.7 `DescriptiveWithinHostModel` (`DescriptiveWithinHost.h/.cpp`)
Used when none of Dummy/Empirical/Molineaux/Penny/Vivax options are set (requires `sim::oneTS() == 5`).
- **Differences from `CommonWithinHost`**:
  1. Constructor does **not** sample `hetMassMultiplier` (only `WHFalciparum` constructor's 1 `gauss` call).
  2. Saves `nNewInfsToBeCreated_i = nNewInfs_i` and `nNewInfsToBeCreated_l = nNewInfs_l` **before** clamping to `MAX_INFECTIONS = 21`.
  3. Creates `DescriptiveInfection(rng, g, Introduced)` (`0..nNewInfs_i`) then `DescriptiveInfection(rng, g, Indigenous)` (`0..nNewInfs_l`).
  4. `updateImmuneStatus()`.
  5. Single 5-day pass over `infections`:
     - If `inf.expired()` (`sim::ts0() > m_startDate + m_duration`) or `(inf.bloodStage() ? treatmentBlood : treatmentLiver)`: erase `inf`, `numInfs -= 1`, `continue`.
     - `infStepMaxDens = timeStepMaxDensity` (passed in-out for historical `!MAX_DENS_CORRECTION` compatibility)
     - `immSurvFact = immunitySurvivalFactor(ageInYears, inf.cumulativeExposureJ())`
     - `bsvFactor = human.vaccine.getFactor(BSV, opt_vaccine_genotype ? inf.genotype() : 0)`
     - `inf.determineDensities(rng, m_cumulative_h, &mut infStepMaxDens, immSurvFact, _innateImmSurvFact, bsvFactor)`
     - If `bugfix_max_dens` (`MAX_DENS_CORRECTION`, default `true`): `infStepMaxDens = max(infStepMaxDens, timeStepMaxDensity)`.
     - `timeStepMaxDensity = infStepMaxDens`.
     - `totalDensity += inf.getDensity()`; if `!inf.isHrp2Deficient()`, `hrp2Density += inf.getDensity()`.
  6. `m_cumulative_h += (nNewInfs_i + nNewInfs_l) as f64`; **`m_cumulative_Y += 5.0 * totalDensity`** (`sim::oneTS() * totalDensity`).
  7. Updates `m_y_lag_i` and `m_y_lag_l` identically to `CommonWithinHost`.
  8. **Legacy bug-compatibility**: `if !opt_vaccine_genotype { nNewInfs_i = nNewInfsToBeCreated_i; nNewInfs_l = nNewInfsToBeCreated_l; }` (restores unclamped counts for `reportNumNewInfections`!).

---

### 2.8 The 5 Falciparum Infection Models

#### Model 1: `DummyInfection` (`DummyInfection.h/.cpp`)
- **Creation**: `m_density = 16.0` (0 RNG calls).
- **`updateDensity(rng, survivalFactor, bsAge, _)`** (0 RNG calls):
  - `GROWTH_RATE = 8.0`, `PARASITE_THRESHOLD = 1.0`.
  - `m_density = (mod_nn((m_density * 8.0) as i32, 20000) as f64) * survivalFactor`
  - `m_cumulativeExposureJ += m_density`
  - returns `m_density < 1.0` (`true` = extinct).

#### Model 2: `DescriptiveInfection` (`DescriptiveInfection.h/.cpp`) + `test/densities.csv`
- **CSV Format (`densities.csv`)**:
  - Header line (skipped): `5day,Duration,Meanlog(Georgia,self-cleared subset)`
  - 3,570 data rows (`i` = `1..=84`, `j` = `1..=84` with `i <= j`): `i,j,meanlogdens`.
  - Stored in `meanLogParasiteCount[84][84]`: `meanLogParasiteCount[i - 1][j - 1] = meanlogdens`, and if `j != i`, `meanLogParasiteCount[j - 1][i - 1] = 0.0`.
- **Static Parameters**:
  - `maxDurationTS = 83`, `numDurations = 84`, `maxDens = 2_000_000.0`.
  - `sigma0sq = param[11: SIGMA0_SQ]`, `xNuStar = param[12: X_NU_STAR]`.
  - `bugfix_innate_max_dens = ModelOptions::option(INNATE_MAX_DENS)` (default `true` in base options? Check XML/ModelOptions bit!).
  - `bugfix_max_dens = ModelOptions::option(MAX_DENS_CORRECTION)`.
- **Creation (`DescriptiveInfection::DescriptiveInfection`)**:
  - `dur_mean = 5.1300001144409179688_f64`, `dur_sigma = 0.80000001192092895508_f64`.
  - `dur = rng.log_normal(dur_mean, dur_sigma)` (**1 `log_normal` call**).
  - `m_duration = ((1.0 + dur) as i32) - 5` (in days; Note: `expired()` is `sim::ts0() > m_startDate + m_duration`).
- **`determineDensities(rng, cumulativeh, &mut timeStepMaxDensity, immSurvFact, innateImmSurvFact, bsvFactor)`**:
  - `infage = sim::ts0() - m_startDate - s_latentP`.
  - If `infage < 0`:
    - `m_density = 0.0`; if `bugfix_max_dens` { `timeStepMaxDensity = 0.0;` } (0 RNG calls).
  - Else (`infage >= 0`):
    - `timeStepMaxDensity = 0.0`
    - `infAge = min(sim::inSteps(infage), 83)`
    - `infDur = min(sim::inSteps(m_duration), 83)` (note: in C++, `m_duration` can be negative if `dur < 4.0`, though `expired()` already filtered `ts0 > startDate + m_duration` before reaching `infage >= 0` whenever `s_latentP >= 0`).
    - `m_density = max(meanLogParasiteCount[infAge][infDur], 0.0) * immSurvFact`
    - `varlog = sigma0sq / (1.0 + (cumulativeh / xNuStar))`; `stdlog = sqrt(varlog)`.
    - If `stdlog > 0.0000001` (`1e-7`):
      - `meanlog = m_density - stdlog * stdlog / 2.0`
      - `m_density = rng.log_normal(meanlog, stdlog)` (**1 `log_normal` call**)
      - `timeStepMaxDensity = rng.max_multi_log_normal(m_density, 4 /* sim::oneTS() - 1 */, meanlog, stdlog)` (**1 `uniform_01` call**).
    - `m_density = min(m_density, 2_000_000.0)`
    - `timeStepMaxDensity = min(timeStepMaxDensity, 2_000_000.0)`
    - `m_density *= innateImmSurvFact`
    - `m_density *= bsvFactor`
    - `m_cumulativeExposureJ += 5.0 * m_density`.
  - Outside the `if/else` (executes even when `infage < 0`!):
    - If `bugfix_innate_max_dens`: `timeStepMaxDensity *= innateImmSurvFact`
    - `timeStepMaxDensity *= bsvFactor`.

*(Note: `DescriptiveIPTInfection.*` has header comment `NOTE: this code is not used any more` and is not compiled in `CMakeLists.txt`.)*

#### Model 3: `EmpiricalInfection` (`EmpiricalInfection.h/.cpp`) + `test/autoRegressionParameters.csv`
- **CSV Format (`autoRegressionParameters.csv`)**:
  - Exact header (validated literally): `day,mub1,sigb1,mub2,sigb2,mub3,sigb3`
  - 418 data rows (`day = 0..=417`): populates `_mu_beta1[418]`, `_sigma_beta1[418]`, `_mu_beta2[418]`, `_sigma_beta2[418]`, `_mu_beta3[418]`, `_sigma_beta3[418]`.
- **Hardcoded Static Constants**:
  - `_maximumDurationInDays = 418`
  - `_alpha1 = 0.2647, _alpha2 = 2.976, _alpha3 = 0.9181`
  - `_mu1 = 6.08e-04, _mu2 = 0.624, _mu3 = 0.3064`
  - `_sigma0_res = 0.9998, _sigmat_res = 0.002528`
  - `_inflationMean = 1.09635, _inflationVariance = 0.172029`
  - `_extinctionLevel = 0.0100976, _overallMultiplier = 0.697581`
  - `_subPatentLimit = 10.0 / _overallMultiplier`
  - `_maximumPermittedAmplificationPerCycle = 1000.0`.
- **Helper Functions & RNG Order**:
  - `getInflatedDensity(rng, nonInflated)`: `exp(ln(_inflationMean) + rng.gauss(nonInflated, sqrt(_inflationVariance)))` (**1 `gauss` call**).
  - `sampleSubPatentValue(rng, alpha, mu, upperBound)`:
    1. `beta = alpha * (1.0 - mu) / mu`
    2. `nonInflated = upperBound + ln(rng.beta(alpha, beta))` (**1 `beta` call**)
    3. Loop `tries = 0..10`: `inflated = getInflatedDensity(rng, nonInflated)` (**1 `gauss` call per try**); `tries += 1`; repeat `while inflated > upperBound && tries < 10`.
    4. If `inflated > upperBound`, clamp `inflated = upperBound`. Return `inflated`.
  - `sigma_noise(ageDays) = _sigma0_res + _sigmat_res * (ageDays as f64)`.
- **Per-Infection State & Creation (`growthRateMultiplier = 1.0`)**:
  - `_laggedLogDensities: [f64; 3]`, `_patentGrowthRateMultiplier: f64 = 1.0`.
  - Creation RNG order (`upper = ln(_subPatentLimit)`):
    1. `L[0] = sampleSubPatentValue(rng, _alpha1, _mu1, upper)`
    2. `L[1] = sampleSubPatentValue(rng, _alpha2, _mu2, upper)`
    3. `L[2] = sampleSubPatentValue(rng, _alpha3, _mu3, upper)`
    4. `L[0] += ln(growthRateMultiplier)` (`0.0` when `1.0`).
- **`updateDensity(rng, survivalFactor, bsAge, _)`**:
  1. If `bsAge >= 418 || !(L[0] > -999999.9)` return `true` (extinct, 0 RNG calls).
  2. `upperLimit = ln(1000.0 * exp(L[1]) / _inflationMean)`; `d = bsAge as usize`.
  3. Outer loop `tries0 in 0..10`:
     - Inner loop `tries1 in 0..10`:
       1. `b_1 = rng.gauss(_mu_beta1[d], _sigma_beta1[d])`
       2. `b_2 = rng.gauss(_mu_beta2[d], _sigma_beta2[d])`
       3. `b_3 = rng.gauss(_mu_beta3[d], _sigma_beta3[d])`
       4. `expectedLogDens = b_1 * (L[0] + L[1] + L[2]) / 3.0 + b_2 * (L[2] - L[0]) / 2.0 + b_3 * (L[2] + L[0] - 2.0 * L[1]) / 4.0`
       5. `logDensity = rng.gauss(expectedLogDens, sigma_noise(bsAge)) + ln(_patentGrowthRateMultiplier)`
       6. If `logDensity <= upperLimit`: `break` inner loop.
     - If `!(logDensity <= upperLimit)`: `logDensity = upperLimit`.
     - `localDensity = getInflatedDensity(rng, logDensity) * survivalFactor` (**1 `gauss` call**).
     - If `d == 0 && localDensity < _subPatentLimit`: `localDensity = 0.0`.
     - `amp = localDensity / exp(L[1])`.
     - If `localDensity >= 0.0 && amp <= 1000.0`: `break` outer loop.
  4. If `!(localDensity >= 0.0 && amp <= 1000.0)`: `localDensity = 1000.0 * exp(L[1])`.
  5. Shift lags: `L[2] = L[1]; L[1] = L[0]; L[0] = ln(localDensity)` (note: `-inf` if `localDensity == 0.0`).
  6. `m_density = localDensity`; `m_cumulativeExposureJ += m_density`.
  7. Returns `!(m_density > _extinctionLevel)` (i.e. `false` if `m_density > _extinctionLevel`, else `true` extinct).

#### Model 4: `MolineauxInfection` (`MolineauxInfection.h/.cpp`)
**CRITICAL PRECISION DETAIL**: State fields in `MolineauxInfection` use **`f32` (`float`)**, while intermediate arithmetic in `updateDensity` uses `f64` (`double`) and casts back to `f32`!
- **Constants**:
  - `v = 50`, `taus = 4`.
  - `sigma = 0.02`, `sigma_decay = exp(-0.04)`, `beta = 0.01`, `s = 0.02`, `q = 0.3`, `qPow[i] = 0.3_f64.powi((i + 1) as i32)` for `i in 0..50`.
  - `mu_m = 16.0, sigma_m = 10.4`; `shape_m = 2.4, scale_m = 6.8`.
  - `k_c = 0.2, k_m = 0.04`, `inv_Pv_star = 1.0 / 30.0`, `kappa_c = 3, kappa_m = 1, kappa_v = 3`, `C = 1.0`.
  - `blood_vol_per_kg = 7e4`, `initial_dens = 0.1`, `elim_parasites = 50.0`.
  - `case_specific_data: [(f64, f64); 35]` (35 `(duration, max_dens)` pairs for `G131..G48` in lines 114-150 of `MolineauxInfection.cpp`).
- **Static Options & Params**:
  - `multi_factor_gamma = ModelOptions::option(PARASITE_REPLICATION_GAMMA)`
  - `pairwise_P_star_sample = ModelOptions::option(MOLINEAUX_PAIRWISE_SAMPLE)`
  - If `!pairwise_P_star_sample`:
    - `m_max = param[34: MEAN_LOCAL_MAX_DENSITY]`, `sd_max = param[35: SD_LOCAL_MAX_DENSITY]`
    - `m_dur = param[36: MEAN_DIFF_POS_DAYS]`, `sd_dur = param[37: SD_DIFF_POS_DAYS]`
    - Note C++ lines 181-182 if `FIRST_LOCAL_MAXIMUM_GAMMA`: `m_max = m_max^2 / sd_max^2; sd_max = sd_max^2 / m_max` (uses the ALREADY-overwritten `m_max`!). Same on lines 191-192 if `MEAN_DURATION_GAMMA`!
- **Per-Infection State (`f32`!)**:
  - `mi: [f32; 50]`, `Sm_summation: f32 = 0.0`, `lagged_Pc: [f32; 4] = [0.0; 4]`, `Pc_star: f32`, `Pm_star: f32`.
  - `variants: Vec<Variant>` where `Variant { Pi1: f32, Pi2: f32, Si_summation: f32, lagged_Pi: [f32; 4] }` (all `0.0`).
- **Creation RNG Order**:
  1. For `i in 0..50`: `do { mi[i] = (if multi_factor_gamma { rng.gamma(2.4, 6.8) } else { rng.gauss(16.0, 10.4) }) as f32; } while (mi[i] < 1.0_f32);`
  2. If `pairwise_P_star_sample`:
     - `patient = rng.uniform(35)` (**1 `uniform_01` call**)
     - `Pc_star = (0.2 * case_specific_data[2 * patient + 1]) as f32`
     - `Pm_star = (0.04 * case_specific_data[2 * patient + 0]) as f32`.
     Else:
     - `Pc_star = (0.2 * 10.0_f64.powf(if first_local_maximum_gamma { rng.gamma(m_max, sd_max) } else { rng.gauss(m_max, sd_max) })) as f32`
     - `Pm_star = (0.04 * 10.0_f64.powf(if mean_duration_gamma { rng.gamma(m_dur, sd_dur) } else { rng.gauss(m_dur, sd_dur) })) as f32`.
- **`updateDensity(_, survival_factor, age_BS, body_mass)`** (**0 RNG calls!**):
  1. `elim_dens = 50.0 / (7e4 * body_mass)`. `Pi = [0.0_f64; 50]`.
  2. If `age_BS == 0`:
     - `variants.resize(1, Variant::default())`; `Pi[0] = 0.1`; `m_density = 0.1`.
     Else:
     - `sum = 0.0`; for `i in 0..variants.len()`:
       `newP = survival_factor * (variants[i].Pi1 as f64)`; `Pi[i] = newP`;
       `variants[i].Pi1 = (survival_factor * (variants[i].Pi2 as f64)) as f32`; `sum += newP`;
     - `m_density = sum`.
  3. `m_cumulativeExposureJ += m_density`.
  4. If `m_density <= elim_dens`: return `true` (extinct).
  5. If `mod_nn(age_BS, 2) != 0`: return `false` (odd `age_BS` only interpolates).
  6. Even `age_BS`:
     - `base = m_density / (Pc_star as f64)`; `Sc = 1.0 / (1.0 + base * base * base)`.
     - `tau = mod_nn(age_BS / 2, 4) as usize`.
     - `Sm_summation = Sm_summation + lagged_Pc[tau]`.
     - `lagged_Pc[tau] = (if m_density < 1.0 { m_density } else { 1.0 }) as f32`.
     - `Sm = (1.0 - 0.01) / (1.0 + (Sm_summation as f64) / (Pm_star as f64)) + 0.01`.
     - `sum_qj_Sj = 0.0`; `Si = [0.0_f64; 50]`:
       For `i in 0..50`:
       - If `i < variants.len()`:
         `variants[i].Si_summation = ((variants[i].Si_summation as f64) * sigma_decay + (variants[i].lagged_Pi[tau] as f64)) as f32`;
         `variants[i].lagged_Pi[tau] = Pi[i] as f32`;
         `b = (variants[i].Si_summation as f64) * (1.0 / 30.0)`;
         `Si[i] = 1.0 / (1.0 + b * b * b)`;
       - Else: `Si[i] = 1.0`.
       - `sum_qj_Sj += qPow[i] * Si[i]`.
     - For `i in 0..50`:
       - `p_i = if Si[i] >= 0.1 { qPow[i] * Si[i] / sum_qj_Sj } else { 0.0 }`
       - `growth_factor = (mi[i] as f64) * Si[i] * Sc * Sm`
       - If `i < variants.len()`:
         `Pi_prime = ((1.0 - 0.02) * Pi[i] + 0.02 * p_i * m_density) * growth_factor`;
         if `Pi_prime < elim_dens` { `Pi_prime = 0.0;` }
         `variants[i].Pi1 = sqrt(Pi[i] * Pi_prime) as f32`; `variants[i].Pi2 = Pi_prime as f32`.
       - Else:
         `Pi_prime = (0.02 * p_i * m_density) * growth_factor`;
         if `Pi_prime >= elim_dens`: `variants.resize(i + 1, Variant::default())`; `variants[i].Pi2 = Pi_prime as f32`.
     - Return `false`.

#### Model 5: `PennyInfection` (`PennyInfection.h/.cpp`)
- **Constants**:
  - `delta_C = 7, delta_V = 6`
  - `beta_N = 0.5198, psi_N = 0.0946, kappa_N = 2.9506, sigma_epsilon = 1.4217`
  - `beta_C = 0.1872, psi_C = 0.2224, kappa_C = 1.9535, rho_C = 0.1292, exp_negRho_C = exp(-0.1292)`
  - `beta_V = 0.0427, kappa_V = 4.1529, rho_V = 2.5482, exp_negRho_V = exp(-2.5482), lambda_V = 4.2119, prob_lambda_V = 1.0 / 4.2119`
  - Initial/threshold params:
    - `mu_Y = 3.9700, sigma_Y = 1.3436, a_Y = 8.7305, b_Y = 0.4547`
    - `mu_X = 1.9969, sigma_X = 0.7424, a_X = 7.2350, b_X = 0.2760`
    - `mu_TN = 7.5872, sigma_TN = 2.8977, a_TN = 6.8558, b_TN = 1.1067`
    - `mu_TC = 5.5573, sigma_TC = 0.4068, a_TC = 186.6233, b_TC = 0.0297`
    - `mu_TV = 6.12898, sigma_TV = 1.3768, a_TV = 19.8167, b_TV = 0.3093`
    - `m_rep = 16.0, Omega = 0.00025`.
  - Options: `immune_threshold_gamma = IMMUNE_THRESHOLD_GAMMA`, `update_density_gamma = UPDATE_DENSITY_GAMMA`.
- **State & Creation RNG Order**:
  - `cirDensities: [f64; 7] = [0.0; 7]`, `seqDensities: [f64; 6] = [0.0; 6]`
  - `variantSpecificSummation: f64 = 0.0`, `clonalSummation: f64 = 0.0`
  - Loop `do { ... } while (threshold_N <= threshold_C || threshold_N <= threshold_V)`:
    - Draws **in order** `threshold_N`, `threshold_C`, `threshold_V` as `exp(rng.gamma(a, b))` if `immune_threshold_gamma` else `exp(rng.gauss(mu, sigma))`.
- **`updateDensity(rng, survivalFactor, bsAge, _)`**:
  - `ageDays = bsAge`.
  - If `ageDays == 0`:
    1. `cirDensities[0] = exp(update_density_gamma ? rng.gamma(a_Y, b_Y) : rng.gauss(mu_Y, sigma_Y))`; `m_density = cirDensities[0]`.
    2. `seqDensities[0] = exp(update_density_gamma ? rng.gamma(a_X, b_X) : rng.gauss(mu_X, sigma_X))`.
  - Else (`ageDays > 0`):
    1. `yesterdayV = mod_nn(ageDays - 1, 6)`, `seqDensityYesterday = seqDensities[yesterdayV]`.
    2. `yesterdayC = mod_nn(ageDays - 1, 7)`, `cirYesterday = cirDensities[yesterdayC]`.
    3. `base_Npow = (cirYesterday / threshold_N).powf(kappa_N)`;
       `R_Nx = (1.0 - beta_N) / (1.0 + base_Npow) + beta_N`;
       `R_Ny = (1.0 - psi_N) / (1.0 + base_Npow) + psi_N`.
    4. `clonalSummation = clonalSummation * exp_negRho_C + cirDensities[mod_nn(ageDays, 7)]`;
       `base_Cpow = (clonalSummation / threshold_C).powf(kappa_C)`;
       `R_Cx = (1.0 - beta_C) / (1.0 + base_Cpow) + beta_C`;
       `R_Cy = (1.0 - psi_C) / (1.0 + base_Cpow) + psi_C`.
    5. **RNG #1**: `if rng.bernoulli(prob_lambda_V)` (`1.0 / 4.2119`):
       `variantSpecificSummation = 0.0; seqDensities = [0.0; 6];`
       `variantSpecificSummation = variantSpecificSummation * exp_negRho_V + seqDensities[mod_nn(ageDays, 6)]`;
       `R_Vx = (1.0 - beta_V) / (1.0 + (variantSpecificSummation / threshold_V).powf(kappa_V)) + beta_V`.
    6. `cirDensity_new = seqDensityYesterday * 16.0 * R_Vx * R_Cx * R_Nx`;
       `seqDensity_new = cirYesterday * R_Cy * R_Ny`.
    7. If `cirDensity_new < Omega`: `cirDensity_new = 0.0` (NO RNG #2!).
       Else:
       - **RNG #2**: If `update_density_gamma`:
         `a_c = ln(cirDensity_new)^2 / sigma_epsilon^2`, `b_c = sigma_epsilon^2 / ln(cirDensity_new)`,
         `cirDensity_new = exp(rng.gamma(a_c, b_c)) * survivalFactor`
         Else:
         `cirDensity_new = exp(rng.gauss(ln(cirDensity_new), sigma_epsilon)) * survivalFactor`.
       - If `cirDensity_new < Omega`: `cirDensity_new = 0.0`.
    8. `seqDensity_new *= survivalFactor`.
    9. If `seqDensity_new < Omega`:
       - If `cirDensity_new == 0.0`: **return `true` (extinct; note `m_cumulativeExposureJ` is NOT incremented on extinction day)**.
       - `seqDensity_new = 0.0`.
    10. `cirDensities[mod_nn(ageDays, 7)] = cirDensity_new; m_density = cirDensity_new;`
        `seqDensities[mod_nn(ageDays, 6)] = seqDensity_new;`
  - `m_cumulativeExposureJ += m_density`; return `false`.

---

### 2.9 `WHVivax` & `VivaxBrood` (`WHVivax.h/.cpp`)
Enabled when `VIVAX_SIMPLE_MODEL` option is set.
- **Static Parameters (from `<vivax>` & `<liverStageDrug>`)**:
  - `latentP` (default `15d` or `model.parameters.latentp`)
  - `probBloodStageInfectiousToMosq`
  - `maxNumberHypnozoites: i32`, `baseNumberHypnozoites: f64` (`[0, 1]`):
    - Precomputes `nHypnozoitesProbMap: BTreeMap<f64, i32>` for `n = 0..=max`: `total = sum_{k=0..=max} base^k`, `cumP += base^n / total`, `map[cumP] = n`.
  - `latentRelapse1st: HypnozoiteReleaseDistribution { sampler: LognormalSampler, latentRelapse: f64 }`, optional `latentRelapse2nd` + `pSecondRelease: f64` (`NaN` if absent).
  - `bloodStageProtectionLatency: SimTime = sim::roundToTSFromDays(val)`
  - `bloodStageLength: WeibullSampler` (`scale, shape`)
  - `pPrimaryA, pPrimaryB, pRelapseOneA, pRelapseOneB, pRelapseTwoA, pRelapseTwoB, pEventIsSevere`
  - `vivaxClinOption: String` in `{"A1j", "A2j", "B1j", "B2j"}`
  - PQ params (`setHSParameters`): `ignoreNoPQ: bool` (default `false`), `pHetNoPQ: f64` (`0.0`), `pReceivePQ: f64` (`0.0`), `effectivenessPQ: f64` (`1.0`).
- **Host Creation (`WHVivax::WHVivax`)**:
  - Throws if `comorbidityFactor != 1.0`.
  - `noPQ = pHetNoPQ > 0.0 && rng.bernoulli(pHetNoPQ)` (1 `bernoulli` iff `pHetNoPQ > 0.0`).
  - State: `infections: List<VivaxBrood>`, `noPQ: bool`, `morbidity: Pathogenesis::State`, `cumPrimInf: u32 = 0`, `treatExpiryLiver: SimTime = 0`, `treatExpiryBlood: SimTime = 0`, `pSevere: f64 = 0.0`.
- **`VivaxBrood` Creation (`VivaxBrood(rng, origin)`) — Exact RNG Order**:
  1. `releases: BTreeSet<SimTime>`; insert `sim::nowOrTs0() + latentP` (primary release).
  2. Sample `numberHypnozoites`: `x = rng.uniform_01()`, `upper_bound(x)` in `nHypnozoitesProbMap` (**1 `uniform_01` call**).
  3. Loop `i = 0; while i < numberHypnozoites`:
     - `isFirstRelease = !(pSecondRelease == 1.0 || (pSecondRelease > 0.0 && rng.bernoulli(pSecondRelease)))` (1 `bernoulli` iff `0.0 < pSecondRelease < 1.0`; note `pSecondRelease` is `NaN` if no 2nd release, so `false` with 0 RNG calls).
     - `dist = if isFirstRelease { &latentRelapse1st } else { &latentRelapse2nd }`:
       - Loop up to `1000` tries: `delay = dist.sampler.sample(rng)` until `0.0 <= delay <= 480.0` (`16.0 * 30.0`).
       - `releaseDelay = sim::roundToTSFromDays(delay + dist.latentRelapse)`.
     - `timeToRelease = sim::nowOrTs0() + latentP + releaseDelay`.
     - Insert into `releases`; **only increment `i` if `timeToRelease` was NOT already in `releases`!** (Resamples on collision!).
  4. Store `releaseDates: Vec<SimTime>` in **descending** order (`releases.rbegin()..rend()`, so `.last()` / `.pop()` is earliest).
  5. `bloodStageClearDate = sim::never()`, `primaryHasStarted = false`, `relapseHasStarted = false`, `relapsebHasStarted = false`, `hadEvent = false`, `hadRelapse = false`.
- **`VivaxBrood::update(rng) -> UpdResult`**:
  - While `!releaseDates.is_empty() && *releaseDates.last() == sim::ts0()`:
    - `releaseDates.pop()`
    - If `(vivaxClinOption == "B1j" || vivaxClinOption == "B2j") && (bloodStageClearDate + bloodStageProtectionLatency >= sim::ts0())`: `continue` (suppressed by active/recent blood stage of same brood!).
    - Flags (checked in exact order!):
      - `if !relapsebHasStarted && relapseHasStarted { relapsebHasStarted = true; res.newRelapsebBS = true; }`
      - `if !relapseHasStarted && primaryHasStarted { relapseHasStarted = true; res.newRelapseBS = true; }`
      - `if !primaryHasStarted { primaryHasStarted = true; res.newPrimaryBS = true; }`
      - `res.newBS = true;`
    - `lengthDays = bloodStageLength.sample(rng)` (**1 `weibull` call**).
    - `bloodStageClearDate = sim::ts0() + sim::roundToTSFromDays(lengthDays)`.
  - `isPatent() = bloodStageClearDate > sim::latestTs0()`.
  - `res.isFinished = !isPatent() && releaseDates.is_empty()`.
- **`WHVivax::update(human, rng, nNewInfs_i, nNewInfs_l, ..., ageInYears)`**:
  1. `pSevere = 0.0`.
  2. Push `nNewInfs_i` `VivaxBrood(rng, Introduced)` (note: **no `MAX_INFECTIONS` cap** in `WHVivax`!), then `nNewInfs_l` `VivaxBrood(rng, Indigenous)`.
  3. `morbidity = NONE`, `oldCumInf = cumPrimInf` (**saved before brood loop!**).
  4. `treatmentLiver = treatExpiryLiver > sim::ts0()`, `treatmentBlood = treatExpiryBlood > sim::ts0()`.
  5. `matImmClin = 1.0 - 0.90 * exp(-2.53 * ageInYears)`.
  6. For each brood `inf` in `infections`:
     - If `treatmentLiver` -> `inf.releaseDates.clear()`
     - If `treatmentBlood` -> `inf.bloodStageClearDate = sim::never()`
     - `res = inf.update(rng)`
     - If `res.newPrimaryBS` -> `cumPrimInf += 1`
     - If `res.newBS`:
       - Compute `pClin` & sample `clinicalEvent = rng.bernoulli(pClin)` (**1 `bernoulli` call**):
         - If `res.newPrimaryBS`: `pClin = matImmClin * pPrimaryA * exp(-pPrimaryB * (oldCumInf as f64))`, `inf.hadEvent = clinicalEvent`
         - Else if `res.newRelapseBS`: `pClin = matImmClin * pRelapseOneA * exp(-pRelapseOneB * ((oldCumInf as f64) - 1.0))` (note: in C++, `oldCumInf - 1` is `uint32_t` subtraction! If `oldCumInf >= 1` it is `oldCumInf - 1`, if `oldCumInf == 0` — i.e., primary infection was cleared by `treatmentBlood` or suppressed? Wait: primary sets `cumPrimInf += 1` in an earlier step so `oldCumInf >= 1` unless two releases happened same step, which `set<SimTime>` prevents! So `oldCumInf >= 1` always!), `inf.hadRelapse = clinicalEvent`
         - Else if `res.newRelapsebBS`:
           - If `"A1j" | "A2j"`: `pClin = matImmClin * pRelapseOneA * exp(-pRelapseOneB * (oldCumInf - 1) as f64)`
           - If `"B1j" | "B2j"`: `pClin = matImmClin * pRelapseTwoA * exp(-pRelapseTwoB * (oldCumInf - 1) as f64)`
           - `inf.hadRelapse = clinicalEvent`
         - Else (3rd+ relapse): `pClin = matImmClin * pRelapseTwoA * exp(-pRelapseTwoB * (oldCumInf - 1) as f64)`.
       - If `clinicalEvent`:
         - `pSevere = pSevere + (1.0 - pSevere) * pEventIsSevere`
         - If `rng.bernoulli(pEventIsSevere)` (**1 `bernoulli` call**) -> `morbidity |= STATE_SEVERE`, else `morbidity |= STATE_MALARIA`.
     - If `res.isFinished` -> erase `inf`.
  7. If `morbidity == NONE`: `morbidity = PathogenesisModel::sampleNMF(rng, ageInYears)`.
- **`WHVivax::probTransmissionToMosquito`**: returns `probBloodStageInfectiousToMosq` if any brood `isPatent()`, else `0.0` (does NOT populate `probTransGenotype_*`; asserts `Genotypes::N() == 1`).
- **`WHVivax::diagnosticResult`**: returns `true` if any brood `isPatent()`, else `false` (ignores `diagnostic` parameter; **0 RNG calls**).
- **`WHVivax::optionalPqTreatment` & `treatSimple`**:
  - `optionalPqTreatment(human)`: if `pReceivePQ > 0.0 && (ignoreNoPQ || !noPQ) && human.rng.bernoulli(pReceivePQ)`:
    - if `human.rng.bernoulli(effectivenessPQ)` -> clear LS on all broods;
    - `mon::recordEvent("nLiverStageTreatments", human)`.
  - `treatSimple(human, timeLiver, timeBlood)`:
    - If `timeLiver != 0`: throws if `pReceivePQ > 0.0`; if `(ignoreNoPQ || !noPQ) && (effectivenessPQ == 1.0 || human.rng.bernoulli(effectivenessPQ))` -> if `timeLiver >= 0` `treatExpiryLiver = max(treatExpiryLiver, sim::nowOrTs1() + timeLiver)` else clear LS on all broods; `mon::recordEvent("nLiverStageTreatments", human)`.
    - If `timeBlood != 0`: if `timeBlood < 0` clear BS on all broods else `treatExpiryBlood = max(treatExpiryBlood, sim::nowOrTs1() + timeBlood)`; return `true`.

---

### 2.10 `PathogenesisModel` (`State.h`, `PathogenesisModel.*`, `Submodels.*`)
- **`Pathogenesis::State` bitflags (`State.h`)**:
  - `NONE = 0x0`, `SICK = 0x1`, `MALARIA = 0x2`, `COINFECTION = 0x4`, `SEVERE = 0x8`, `COMPLICATED = 0x20`
  - `STATE_NMF = 0x1` (`SICK`)
  - `STATE_MALARIA = 0x3` (`SICK | MALARIA`)
  - `STATE_COINFECTION = 0x27` (`SICK | MALARIA | COMPLICATED | COINFECTION`)
  - `STATE_SEVERE = 0x2B` (`SICK | MALARIA | COMPLICATED | SEVERE`)
  - `StatePair { state: State, indirectMortality: bool }`.
- **Static Parameters**:
  - `pg_indirRiskCoFactor = 1.0 - exp(-param[18: INDIRECT_RISK_COFACTOR])`
  - `pg_severeMalThreshold = param[21: SEVERE_MALARIA_THRESHHOLD] + 1.0`
  - `pg_comorbIntercept = 1.0 - exp(-param[24: COMORBIDITY_INTERCEPT])`
  - `pg_inv_critAgeComorb = 1.0 / param[30: CRITICAL_AGE_FOR_COMORBIDITY]`
  - `pg_NMF_incidence: AgeGroupInterpolator` (set iff `NON_MALARIA_FEVERS` option enabled).
- **3 Presentation Submodels (`getPEpisode(timeStepMaxDensity, totalDensity)`)**:
  1. **`PyrogenPathogenesis` (Ross / default)**:
     - Static: `initPyroThres = param[28: Y_STAR_0]`, `n = 11`, `delt = 1.0 / 11.0`,
       `smuY = -ln(0.5) / (sim::stepsPerYear() as f64 * param[25: Y_STAR_HALF_LIFE])`, `b = -smuY * delt`,
       `Ystar2_13 = param[13: Y_STAR_SQ]`, `a = param[14: ALPHA] * (sim::oneTS() as f64) * delt`, `Ystar1_26 = param[26: Y_STAR_1]`.
     - Per-human state: `_pyrogenThres: f64 = initPyroThres` (plus `_comorbidityFactor: f64`).
     - `updatePyrogenThres(totalDensity)` (11-step Euler integration):
       For `_ in 1..=11`:
       $$\_\text{pyrogenThres} \mathrel{+}= \frac{\text{totalDensity} \cdot a}{(Y^*_1 + \text{totalDensity}) \cdot (Y^*_2 + \_\text{pyrogenThres})} + b \cdot \_\text{pyrogenThres}$$
     - `getPEpisode`: calls `updatePyrogenThres(totalDensity)`, returns $\dfrac{\text{timeStepMaxDensity}}{\text{timeStepMaxDensity} + \_\text{pyrogenThres}}$.
  2. **`PredetPathogenesis` (`PREDETERMINED_EPISODES`)**:
     - Inherits `PyrogenPathogenesis`; calls `updatePyrogenThres(totalDensity)`, returns `if timeStepMaxDensity > _pyrogenThres { 1.0 } else { 0.0 }`.
  3. **`MuellerPathogenesis` (`MUELLER_PRESENTATION_MODEL`)**:
     - Static: `rateMultiplier_31 = param[31: MUELLER_RATE_MULTIPLIER] * sim::yearsPerStep()`, `densityExponent_32 = param[32: MUELLER_DENSITY_EXPONENT]`.
     - Stateless (only `_comorbidityFactor`).
     - `getPEpisode(_, totalDensity)`: `inc = rateMultiplier_31 * totalDensity.powf(densityExponent_32)`, returns `1.0 - exp(-inc)`.
- **`determineState(human, ageYears, timeStepMaxDensity, endDensity, isDoomed) -> StatePair` — Exact RNG Order**:
  1. `pMalariaFever = getPEpisode(timeStepMaxDensity, endDensity)`.
  2. **RNG #1**: `if human.rng.bernoulli(pMalariaFever)`:
     - `prSevereEpisode = timeStepMaxDensity / (timeStepMaxDensity + pg_severeMalThreshold)`
     - `comorb_factor = _comorbidityFactor / (1.0 + ageYears * pg_inv_critAgeComorb)`
     - `pCoinfection = pg_comorbIntercept * comorb_factor`
     - `exSevere = prSevereEpisode + (1.0 - prSevereEpisode) * pCoinfection`
     - Record `expectedSevere` and `expectedSevereWithoutComorbidities`.
     - **RNG #2**: `if human.rng.bernoulli(prSevereEpisode)` -> `result.state = STATE_SEVERE`
       Else **RNG #2b**: `if human.rng.bernoulli(pCoinfection)` -> `result.state = STATE_COINFECTION` else `STATE_MALARIA`.
     - `indirectRisk = pg_indirRiskCoFactor * comorb_factor`
     - If `!isDoomed`: record `expectedIndirectDeaths`.
     - **RNG #3** (always called even if `isDoomed`!): `if human.rng.bernoulli(indirectRisk)` -> `result.indirectMortality = true`.
  3. Else (no malaria fever):
     - `result.state = sampleNMF(human.rng, ageYears)`:
       - If `pg_NMF_incidence.isSet()`: `pNMF = pg_NMF_incidence.eval(ageYears)`; **RNG #2 (alt)**: `if rng.bernoulli(pNMF) { STATE_NMF } else { NONE }`.
       - Else: `NONE` (0 extra RNG calls).

---

## Section 3: PK/PD Models (`crates/om-pkpd`)

### 3.1 `LSTMDrugType` & `LSTMDrugPD` (`LSTMDrugType.h/.cpp`)
- **`LSTMDrugPD` (per phenotype)**:
  - Fields: `n: f64` (`slope`), `IC50: LognormalSampler`, `V: f64` (`max_killing_rate`, 1/day).
  - `IC50_pow_slope(rng, drug_index, inf)`:
    - If `inf.Kn.contains_key(&drug_index)`: returns `inf.Kn[drug_index]` (**0 RNG calls**).
    - Else: `Kn = IC50.sample(rng).powf(n)` (1 `log_normal` call iff `IC50.sigma > 0`), caches `inf.Kn[drug_index] = Kn`, returns `Kn`.
  - `calcFactor(Kn, neg_elim_rate, &mut conc, duration) -> f64` (Hastings & Winter 2011 analytic 1-comp integral):
    $$C_0 = *\text{conc}, \quad C_1 = C_0 \cdot \exp(\text{neg\_elim\_rate} \cdot \text{duration})$$
    $$*\text{conc} = C_1, \quad \text{factor} = \left(\frac{K_n + C_1^n}{K_n + C_0^n}\right)^{\frac{V}{-\text{neg\_elim\_rate} \cdot n}}$$
- **`LSTMDrugType` (per drug)**:
  - PK fields:
    - `negligible_concentration: f64` (mg/L)
    - Elimination: either `<half_life>` -> `elimination_rate = LognormalSampler::fromMeanCV(ln(2.0) / half_life, 0.0)`, `neg_m_exp = 0.0`; OR `<k>` + `<m_exponent>` -> `elimination_rate = createSampler(k)`, `neg_m_exp = -m_exponent`.
    - `vol_dist = createSampler(pk.vol_dist)` (L/kg).
    - Optional `<compartment2>` (`k12, k21` samplers) + optional `<compartment3>` (`k13, k31` samplers; if `<compartment2>` present without `<compartment3>`, `k13 = k31 = LognormalSampler::fromMeanCV(0.0, 0.0)` — 2-compartment model uses `LSTMDrugThreeComp` with `k13 = k31 = 0.0`!). Requires `<k_a>` (`absorption_rate`).
    - Optional `<conversion>` (incompatible with `<compartment2>`): `metabolite = findDrug(conv.metabolite)` (must be defined earlier), `conversion_rate = createSampler(conv.rate)`, `mwr = conv.molRatio`, `ic50_log_corr = conv.IC50_log_correlation` (`[0, 1]`), `ic50_corr_factor = sqrt(1.0 - corr^2)`. Also allows `<k_a>`.
  - PD genotype->phenotype mapping:
    - Maps each `genotype_id in 0..Genotypes::N()` to unique matching phenotype index `genotype_mapping[genotype_id]` (exact partition check: every phenotype restricts the same set of loci; each genotype must match **exactly one** phenotype).
  - `drugsInUse: Vec<usize>`: populated in order of `findDrug()` calls (plus `metabolite` index when parent uses conversion), deduplicated.
  - `createInstance(rng, index)`:
    - `LSTMDrugConversion` if `conversion_rate.is_some()`
    - `LSTMDrugThreeComp` if `k12.is_some()`
    - `LSTMDrugOneComp` otherwise.

### 3.2 `LSTMTreatments`, `Schedule`, `DosageTable` (`LSTMMedicate.h`, `LSTMTreatments.*`)
- `MedicateData`: `drug: usize`, `qty: f64` (`mg`), `time: f64` (`hour / 24.0`, in days).
- `Schedule`: `Vec<MedicateData>`.
- `DosageTable`:
  - Either `multMassKg = true, useMass = true` (`<multiply by="kg"/>`), where `getMultiplier(key) = key` (`body_mass`).
  - Or piecewise step table (`useMass = false` for `<age>`, `useMass = true` for `<bodymass>`):
    - Input `(lowerbound_0 = 0.0, mult_0), (lowerbound_1, mult_1), ..., (lowerbound_k, mult_k)` (strictly increasing lowerbounds).
    - Stored in `BTreeMap<f64, f64>` keyed by **upper** bound: `table[lowerbound_1] = mult_0`, ..., `table[lowerbound_k] = mult_{k-1}`, `table[+inf] = mult_k`.
    - Lookup `getMultiplier(key)` uses **`table.upper_bound(key)`** (strictly `> key`, i.e., intervals `[lb_i, lb_{i+1})`).

### 3.3 `LSTMDrug` Base (`LSTMDrug.h/.cpp`)
- Fields: `vol_dist: f64`, `doses: Vec<(f64, f64)>` (`(time_days, qty_mg)`).
- `medicate(time, qty)`: inserts `(time, qty)` at `lower_bound` comparing **only `lhs.first < rhs.first`** (preserves FIFO order for ties at identical `time`!).

### 3.4 `LSTMDrugOneComp` (`LSTMDrugOneComp.h/.cpp`)
- **Creation RNG Order**:
  1. `vol_dist = type.sample_Vd(rng)`
  2. `neg_elim_sample = -type.sample_elim_rate(rng)`
  - `concentration = 0.0`.
- **`calculateDrugFactor(rng, inf, body_mass) -> f64`**:
  1. If `concentration == 0.0 && doses.is_empty()` -> return `1.0` (**0 RNG calls**, does NOT sample `Kn`!).
  2. `conc_today = concentration`, `neg_elim_rate = neg_elim_sample * body_mass.powf(type.neg_m_exp)`.
  3. `drugPD = type.getPD(inf.genotype())`, `Kn = drugPD.IC50_pow_slope(rng, type.index, inf)`.
  4. `time = 0.0`, `totalFactor = 1.0`.
  5. For `(t_dose, qty)` in `doses`:
     - If `t_dose < 1.0`:
       - If `time < t_dose`: `totalFactor *= drugPD.calcFactor(Kn, neg_elim_rate, &mut conc_today, t_dose - time)`; `time = t_dose`.
       - `conc_today += qty / (vol_dist * body_mass)`.
     - Else: `break`.
  6. If `time < 1.0`: `totalFactor *= drugPD.calcFactor(Kn, neg_elim_rate, &mut conc_today, 1.0 - time)`.
  7. Return `totalFactor`.
- **`updateConcentration(body_mass)`**:
  1. If `concentration == 0.0 && doses.is_empty()` -> return.
  2. `neg_elim_rate = neg_elim_sample * body_mass.powf(type.neg_m_exp)`.
  3. `concentration *= exp(neg_elim_rate)`.
  4. For `(t_dose, qty)` in `&mut doses`:
     - If `t_dose < 1.0`: `concentration += qty / (vol_dist * body_mass) * exp(neg_elim_rate * (1.0 - t_dose))`; `doses_taken += 1`.
     - Else: `t_dose -= 1.0`.
  5. Erase first `doses_taken` elements of `doses`.
  6. If `concentration < type.negligible_concentration`: `concentration = 0.0`.

### 3.5 `LSTMDrugThreeComp` (`LSTMDrugThreeComp.h/.cpp`)
Covers both 2-compartment (`k13 = k31 = 0`) and 3-compartment models.
- **Creation RNG Order**:
  1. `vol_dist = type.sample_Vd(rng)`
  2. `elim_sample = type.sample_elim_rate(rng)`
  3. `k12 = type.sample_k12(rng)`
  4. `k21 = type.sample_k21(rng)`
  5. `k13 = type.sample_k13(rng)` (0 RNG calls when 2-comp since `CV == 0`)
  6. `k31 = type.sample_k31(rng)` (0 RNG calls when 2-comp since `CV == 0`)
  7. `nka = -type.sample_ka(rng)`
  - State: `concA = concB = concC = concABC = 0.0`; `conc() = concA + concB + concC - concABC`.
- **`updateCached(body_mass)`** (recomputed whenever `last_bm != body_mass`, Monolix cubic roots):
  - $k = \text{elim\_sample} \cdot \text{body\_mass}^{\text{neg\_m\_exp}}$
  - $a_0 = k k_{21} k_{31}$, $a_1 = k k_{31} + k_{21} k_{31} + k_{21} k_{13} + k k_{21} + k_{31} k_{12}$, $a_2 = k + k_{12} + k_{13} + k_{21} + k_{31}$
  - $p = a_1 - \frac{1}{3} a_2^2$, $q = \frac{2}{27} a_2^3 - \frac{1}{3} a_1 a_2 + a_0$
  - $a_t = \frac{1}{3} a_2$, $r_t = -\frac{1}{3} p$, $r_2 = 2\sqrt{r_t}$, $\phi = \frac{1}{3}\arccos\!\left(\dfrac{-q}{r_t \cdot r_2}\right)$, $\pi_{23} = \frac{2}{3}\pi$
  - $n_a = r_2 \cos(\phi) - a_t$, $n_b = r_2 \cos(\phi + \pi_{23}) - a_t$, $n_g = r_2 \cos(\phi + 2\pi_{23}) - a_t$
  - $\text{kaOv} = \dfrac{-n_{ka}}{\text{vol\_dist} \cdot \text{body\_mass}}$
  - $A = \dfrac{\text{kaOv} (k_{21} + n_a)(k_{31} + n_a)}{(n_a - n_{ka})(n_b - n_a)(n_g - n_a)}$, $B = \dfrac{\text{kaOv} (k_{21} + n_b)(k_{31} + n_b)}{(n_b - n_{ka})(n_a - n_b)(n_g - n_b)}$, $C = \dfrac{\text{kaOv} (k_{21} + n_g)(k_{31} + n_g)}{(n_g - n_{ka})(n_b - n_g)(n_a - n_g)}$.
- **Quadrature (`calculateFactor(p, duration)`)**:
  - Integrand on $t \in [0, \text{duration}]$:
    $$c(t) = c_A e^{n_a t} + c_B e^{n_b t} + c_C e^{n_g t} - c_{ABC} e^{n_{ka} t}, \quad f_C(t) = \frac{V \cdot c(t)^n}{c(t)^n + K_n}$$
  - Integrated via **GSL `gsl_integration_qag` with `qag_rule = 1` (15-point Gauss-Kronrod `GSL_INTEG_GAUSS15`)**, `abs_eps = 1e-4`, `rel_eps = 1e-4`, `max_iter = 1000` (throws if `err_eps > 5e-2`).
  - Returns `1.0 / exp(intfC)`.
- **`calculateDrugFactor(rng, inf, body_mass)`**:
  1. If `conc() == 0.0 && doses.is_empty()` -> return `1.0` (**0 RNG calls**).
  2. `updateCached(body_mass)`. Sample/fetch `Kn = pd.IC50_pow_slope(rng, type.index, inf)`.
  3. Step `p = (concA, concB, concC, concABC)` across `doses` with `t_dose < 1.0`:
     - On sub-interval `duration = t_dose - time > 0`: `totalFactor *= calculateFactor(p, duration)`; decay `p.cA *= exp(na * duration)` (and `cB, cC, cABC`); `time = t_dose`.
     - At `t_dose`: `p.cA += A * qty; p.cB += B * qty; p.cC += C * qty; p.cABC += (A + B + C) * qty;`.
  4. If `time < 1.0`: `totalFactor *= calculateFactor(p, 1.0 - time)`. Return `totalFactor`.
- **`updateConcentration(body_mass)`**:
  1. If `conc() == 0.0 && doses.is_empty()` -> return. `updateCached(body_mass)`.
  2. `concA *= exp(na); concB *= exp(nb); concC *= exp(ng); concABC *= exp(nka);`
  3. For `(t_dose, qty)` in `&mut doses`:
     - If `t_dose < 1.0`:
       `concA += A * qty * exp(na * (1.0 - t_dose))` (and `B/nb`, `C/ng`, `(A+B+C)/nka`); `doses_taken += 1`.
     - Else: `t_dose -= 1.0`.
  4. Erase `doses_taken` doses. If `conc() < type.negligible_concentration`: `concA = concB = concC = concABC = 0.0`.

### 3.6 `LSTMDrugConversion` (`LSTMDrugConversion.h/.cpp`)
- **Creation RNG Order**:
  1. `vol_dist = parent.sample_Vd(rng)`
  2. `nka = -parent.sample_ka(rng)` (`-x`)
  3. `nkP_sample = -parent.sample_elim_rate(rng)`
  4. `nconv_sample = -parent.sample_conv_rate(rng)`
  5. `nkM_sample = -metabolite.sample_elim_rate(rng)`
  6. `vol_dist_metabolite = metabolite.sample_Vd(rng)`
  - State: `qtyG = qtyP = qtyM = 0.0` (in mg), `last_bm: f64`.
  - `getConcentration(idx)`: `qtyP / (vol_dist * last_bm)` if `idx == parent.index`, `qtyM / (vol_dist_metabolite * last_bm)` if `idx == metabolite.index`, else `0.0`.
- **Analytic Conversion Quantities (`setConversionParameters`)**:
  - `nkP = nkP_sample * body_mass.powf(parent.neg_m_exp)` (`-y`)
  - `nconv = nconv_sample * body_mass.powf(parent.neg_m_exp)` (`-z`)
  - `nkM = nkM_sample * body_mass.powf(metabolite.neg_m_exp)` (`-k`)
  - `nl = nkP + nconv` (`-(y + z)`)
  - `f = nka / (nl - nka)`
  - `rz = parent.mwr * nconv` (`-u * z`)
  - `g = rz * nka / ((nka - nl) * (nka - nkM))`
  - `h = rz * nka / ((nka - nl) * (nkM - nl))`
  - `i = rz / (nl - nkM)`
  - `j = rz * nka / ((nkM - nl) * (nkM - nka))`
  - `invVdP = 1.0 / (vol_dist * body_mass)`, `invVdM = 1.0 / (vol_dist_metabolite * body_mass)`.
  - Over interval $t$ with `expAbsorb = exp(nka * t)`, `expPLoss = exp(nl * t)`:
    $$Q_P(t) = f \cdot Q_G e^{n_{ka} t} + (Q_P - f \cdot Q_G) e^{n_l t}$$
    $$Q_M(t) = g \cdot Q_G e^{n_{ka} t} + (h \cdot Q_G - i \cdot Q_P) e^{n_l t} + (j \cdot Q_G + i \cdot Q_P + Q_M) e^{n_{kM} t}$$
    $$Q_G(t) = Q_G e^{n_{ka} t}$$
- **Correlated `Kn` Sampling (`setKillingParameters`) — Exact RNG Order**:
  - If `inf.Kn.contains_key(&parent.index)`: reads `KnP = inf.Kn[parent.index]` and `KnM = inf.Kn[metabolite.index]` (**0 RNG calls**).
  - Else:
    1. `zscore = NormalSample::generate(rng)` (**1 `gauss(0, 1)` call — ALWAYS called even if `IC50` has `sigma == 0`!**)
    2. `KnP = pdP.IC50_pow_slope(zscore)`; `inf.Kn[parent.index] = KnP;`
    3. `metab_zscore = parent.IC50_correlated_sample(zscore, rng)` (**1 `gauss(0, ic50_corr_factor)` call iff `ic50_log_corr != 1.0`**)
    4. `KnM = pdM.IC50_pow_slope(metab_zscore)`; `inf.Kn[metabolite.index] = KnM;`.
- **Quadrature (`calculateFactor(p, duration)`)**:
  - Integrand on $t \in [0, \text{duration}]$:
    $$c_P = Q_P(t) \cdot \text{invVdP},\quad f_{CP} = \frac{V_P c_P^{n_P}}{c_P^{n_P} + K_{nP}}, \qquad c_M = Q_M(t) \cdot \text{invVdM},\quad f_{CM} = \frac{V_M c_M^{n_M}}{c_M^{n_M} + K_{nM}}$$
    $$f_{\text{conv}}(t) = \max(f_{CP},\ f_{CM})$$
  - Integrated via **GSL `gsl_integration_qag` with `qag_rule = 1` (`GAUSS15`)**, `abs_eps = 1e-5`, `rel_eps = 1e-2`, `max_iter = 1000`.
  - Returns `exp(-intfC)`.
- **`calculateDrugFactor` & `updateConcentration`**:
  - Early exit (`1.0` / `return`) if `qtyG == 0.0 && qtyP == 0.0 && qtyM == 0.0 && doses.is_empty()`.
  - Step `(qtyG, qtyP, qtyM)` across `doses` with `t_dose < 1.0` (updating `qtyM`, `qtyP`, `qtyG` in order so old `qtyG, qtyP` are used in `Q_M, Q_P`!), adding `qty` to `qtyG` at each dose, then advancing `1.0 - time`.
  - `updateConcentration` sets `last_bm = body_mass` at start, decrements `t_dose -= 1.0` for `t_dose >= 1.0`, erases taken doses, and zeroes `qtyG = qtyP = qtyM = 0.0` iff **both** `qtyP < parent.negligible_concentration * body_mass * vol_dist` **AND** `qtyM < metabolite.negligible_concentration * body_mass * vol_dist_metabolite`.

### 3.7 `LSTMModel` (`LSTMModel.h/.cpp`)
- Per-human state: `m_drugs: Vec<Box<LSTMDrug>>`, `medicateQueue: VecDeque/List<MedicateData>`.
- **Daily Call Order** (inside `CommonWithinHost::update` daily loop):
  1. `prescribe(schedule, dosage, age, body_mass, delay_d)` (called externally from Clinical/MDA):
     - `doseMult = dosages[dosage].getMultiplier(useMass ? body_mass : age)`
     - Appends each `med` in `schedules[schedule]` with `qty = med.qty * doseMult`, `time = med.time + delay_d` to `medicateQueue`.
  2. `medicate(rng)`:
     - Iterates `medicateQueue`: if `item.time < 1.0`, calls `medicateDrug(rng, item.drug, item.qty, item.time)` and erases `item`; else `item.time -= 1.0`.
     - `medicateDrug`: searches `m_drugs` for `drug.getIndex() == typeIndex`; if found calls `drug.medicate(time, qty)` (0 RNG); else pushes `LSTMDrugType::createInstance(rng, typeIndex)` (**samples PK params here on first use of this drug type by this human!**) and calls `medicate(time, qty)`.
  3. `getDrugFactor(rng, inf, body_mass)`:
     - Multiplies `drug.calculateDrugFactor(rng, inf, body_mass)` across `m_drugs` in insertion order.
  4. `decayDrugs(body_mass)`:
     - Calls `drug.updateConcentration(body_mass)` for each `drug` in `m_drugs` (note: `m_drugs` entries are never removed once created, only zeroed; thus PK parameters sampled per-human per-drug persist for life!).

---

## Section 4: Exact `mon::record*` Calls by Class

Signature reference (`mon/Monitoring.h`):
- `mon::recordStat(measure, human, val = 1.0, species = 0, genotype = 0, drug = 0)`
- `mon::recordEvent(measure, human, val = 1.0, outId = 0)`

1. **`Host::Human` (`Human.cpp`)**:
   - `removeFirstEvent`: `mon::recordEvent("nSubPopRemovalFirstEvent", *this)` (if `expIt->second >= sim::nowOrTs0()`)
   - `updateCohortSet`: `mon::recordEvent("nSubPopRemovalTooOld", *this)` (if `!(expIt->second >= sim::ts0())`)
   - `Host::summarize(human, surveyOnlyNewEp)` (skipped if `surveyOnlyNewEp && clinicalModel.isExistingCase()`):
     - `mon::recordStat("nHost", human)` (`val = 1.0`)
     - `mon::recordStat("sumAge", human, sim::inYears(human.age(sim::now())))`
     - calls `withinHostModel.summarize(human)` -> `patent`, then `infIncidence.summarize(human)`
     - if `patent && mon::isReported()` -> `human.removeFirstEvent(ON_FIRST_INFECTION)`.

2. **`InfectionIncidenceModel` (`InfectionIncidenceModel.cpp`)**:
   - `summarize`: `mon::recordStat("nExpectd", human, m_pInfected)`
   - `reportNumNewInfections(human, new_i, new_l)`:
     - `mon::recordEvent("nNewInfections", human, (new_i + new_l) as f64)`
     - `mon::recordEvent("nNewInfections_Introduced", human, new_i as f64)`
     - `mon::recordEvent("nNewInfections_Indigenous", human, new_l as f64)`
     - (plus `ctsNewInfections += new_i + new_l` for continuous callback `"new infections"`).

3. **`NeonatalMortality` (`NeonatalMortality.cpp` / `ClinicalModel.cpp`)**:
   - None inside `NeonatalMortality.cpp` directly; when `eventNeonatalMortality(human.rng)` returns `true` in `ClinicalModel::update`, `ClinicalModel` calls `mon::recordEvent("nIndDeaths", human)`.

4. **`ImportedInfections` (`ImportedInfections.h`)**:
   - On each imported infection (`bernoulli(rateNow) == true`):
     - `mon::recordEvent("nNewInfections", human)` (`val = 1.0`)
     - `mon::recordEvent("nNewInfections_Imported", human)` (`val = 1.0`).

5. **`LSTMModel` (`PkPd/LSTMModel.cpp`)**:
   - `summarize(human)`:
     For `index` in `LSTMDrugType::getDrugsInUse()`, for `drug` in `m_drugs`:
     `conc = drug.getConcentration(index)`; if `conc > 0.0`:
     - `mon::recordStat("nHostDrugConcNonZero", human, 1.0, 0, 0, index)`
     - `mon::recordStat("sumLogDrugConcNonZero", human, ln(conc), 0, 0, index)`.

6. **`WHFalciparum` (`WHFalciparum.cpp`)**:
   - `treatSimple`: if `timeLiver != 0`: `mon::recordEvent("nLiverStageTreatments", human)`.

7. **`CommonWithinHost::summarize(human)` (`CommonWithinHost.cpp`)**:
   - Calls `pathogenesisModel.summarize(human)` and `pkpdModel.summarize(human)`.
   - `infectionType = get_infection_origin(infections)`.
   - If `!infections.is_empty()`:
     - `mon::recordStat("nInfect", human)`
     - One of `"nInfect_Indigenous"` / `"nInfect_Introduced"` / `"nInfect_Imported"` (by `infectionType`).
     - If `reportInfectedOrPatentInfected` (`isUsed("totalInfs") || isUsed("totalPatentInf")` — **important**: gated because `isPositive` may consume `human.rng` if `monitoringDiagnostic` is stochastic!):
       For each `inf` in `infections` (`g = inf.genotype()`):
       - `mon::recordStat("totalInfs", human, 1.0, 0, g, 0)`
       - One of `"totalInfs_Indigenous"` / `"totalInfs_Introduced"` / `"totalInfs_Imported"` with `(human, 1.0, 0, g, 0)`
       - If `monitoringDiagnostic().isPositive(human.rng, inf.getDensity(), NaN)`:
         - `mon::recordStat("totalPatentInf", human, 1.0, 0, g, 0)`
         - One of `"totalPatentInf_Indigenous"` / `"totalPatentInf_Introduced"` / `"totalPatentInf_Imported"` with `(human, 1.0, 0, g, 0)`.
     - If `reportInfectionsByGenotype` (`isUsed("nInfectByGenotype") || isUsed("nPatentByGenotype") || isUsed("logDensByGenotype")`):
       Copy & sort `infections` by `genotype` ascending (do NOT mutate `infections` order!), group by `genotype`, sum `dens`:
       - `mon::recordStat("nInfectByGenotype", human, 1.0, 0, genotype, 0)`
       - If `monitoringDiagnostic().isPositive(human.rng, dens, NaN)`:
         - `mon::recordStat("nPatentByGenotype", human, 1.0, 0, genotype, 0)`
         - `mon::recordStat("logDensByGenotype", human, ln(dens), 0, genotype, 0)`.
   - Host patency check: if `monitoringDiagnostic().isPositive(human.rng, totalDensity, NaN)`:
     - `mon::recordStat("nPatent", human)`
     - One of `"nPatent_Imported"` / `"nPatent_Introduced"` / `"nPatent_Indigenous"` (by `infectionType`)
     - `mon::recordStat("sumlogDens", human, if totalDensity == 0.0 { 0.0 } else { ln(totalDensity) })`
     - returns `true` (else `false`).

8. **`DescriptiveWithinHostModel::summarize(human)` (`DescriptiveWithinHost.cpp`)**:
   - Calls `pathogenesisModel.summarize(human)` (no `pkpdModel`).
   - If `!infections.is_empty()`:
     - `"nInfect"` + `"nInfect_{Indigenous|Introduced|Imported}"`.
     - **Always** (ungated, genotype=0):
       - `mon::recordStat("totalInfs", human, infections.len() as f64)`
       - `mon::recordStat("totalInfs_Imported", human, nImported as f64)`
       - `mon::recordStat("totalInfs_Introduced", human, nIntroduced as f64)`
       - `mon::recordStat("totalInfs_Indigenous", human, nIndigenous as f64)`
     - If `reportPatentInfected` (any of `totalPatentInf{,_Imported,_Introduced,_Indigenous}` used):
       For each `inf`: if `monitoringDiagnostic().isPositive(human.rng, inf.getDensity(), NaN)` -> records `"totalPatentInf"` and `"totalPatentInf_{Origin}"` (**with `genotype = 0`**, unlike `CommonWithinHost`!).
     - If `reportInfectionsByGenotype`: same grouped-by-genotype `"nInfectByGenotype"`, `"nPatentByGenotype"`, `"logDensByGenotype"` as `CommonWithinHost`.
   - Host patency check: if `monitoringDiagnostic().isPositive(human.rng, totalDensity, NaN)`:
     - `"nPatent"` + `"nPatent_{Imported|Introduced|Indigenous}"`
     - **If `totalDensity > 1e-10`** (note threshold difference vs `CommonWithinHost`!): `mon::recordStat("sumlogDens", human, ln(totalDensity))`
     - returns `true` (else `false`).

9. **`WHVivax` (`WHVivax.cpp`)**:
   - `summarize(human)`: if `infections.is_empty()` return `false`;
     - `mon::recordStat("nInfect", human)`
     - `mon::recordStat("totalInfs", human, infections.len() as f64)`
     - For each `inf`: if `inf.isPatent()` -> `mon::recordStat("totalPatentInf", human)`, `patentHost = true`
     - If `patentHost` -> `mon::recordStat("nPatent", human)`; return `patentHost`.
   - `determineMorbidity`: `mon::recordStat("expectedSevere", human, pSevere)`.
   - `optionalPqTreatment` & `treatSimple`: `mon::recordEvent("nLiverStageTreatments", human)`.

10. **`PathogenesisModel` & `PyrogenPathogenesis` (`PathogenesisModel.cpp`, `Submodels.cpp`)**:
    - `PyrogenPathogenesis::summarize` (and `PredetPathogenesis`, NOT `MuellerPathogenesis`):
      - `mon::recordStat("sumPyrogenThresh", human, _pyrogenThres)`
      - `mon::recordStat("sumLogPyrogenThres", human, ln(_pyrogenThres + 1.0))`
    - `PathogenesisModel::determineState` (on malaria fever `bernoulli(pMalariaFever) == true`):
      - `mon::recordStat("expectedSevere", human, exSevere)`
      - `mon::recordStat("expectedSevereWithoutComorbidities", human, prSevereEpisode)`
      - If `!isDoomed`: `mon::recordStat("expectedIndirectDeaths", human, indirectRisk)`.
