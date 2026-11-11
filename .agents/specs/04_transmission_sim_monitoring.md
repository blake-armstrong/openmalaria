# Exact Mathematical & Algorithmic Specification: `om-transmission`, `om-sim`, and `om-monitoring`

Derived directly from C++ headers and implementations in `model/Transmission/**`, `model/Population*`, `model/sim*`, `model/main.cpp`, `model/checkpoint*`, `model/util/*`, and `model/mon/*`.

---

## 1. `crates/om-transmission` Specification

### 1.1 Fourier Series, Log-DFT/Exp-IDFT & Seasonality (`util/vectors.h`, `Transmission/transmission.h`)

Fourier coefficient vectors `FC` always have odd length $2N - 1$ ordered $[a_0, a_1, b_1, \dots, a_{N-1}, b_{N-1}]$.

1. **`logDFT(iArray, FC)`**: Let $T = \text{iArray.size()}$, $N = (\text{FC.size()} + 1)/2$, $\omega = \frac{2\pi}{T}$:
   $$a_0 = \text{FC}[0] = \frac{1}{T}\sum_{t=0}^{T-1} \ln(\text{iArray}[t])$$
   For $n = 1 \dots N-1$:
   $$\text{FC}[2n - 1] = \frac{1}{T}\sum_{t=0}^{T-1} \ln(\text{iArray}[t])\cos(\omega n t), \qquad \text{FC}[2n] = \frac{1}{T}\sum_{t=0}^{T-1} \ln(\text{iArray}[t])\sin(\omega n t)$$
2. **`logFourierCoefficients(iArray, FC)`**: Calls `logDFT(iArray, FC)`, then doubles all harmonic terms ($i \ge 1$):
   $$\text{FC}[i] \leftarrow 2 \cdot \text{FC}[i] \quad \forall i \in \{1, \dots, 2N-2\}$$
3. **`expIDFT(tArray, FC, rAngle)`**: Let $T_2 = \text{tArray.size()}$ (typically $365$), $\omega = \frac{2\pi}{T_2}$, $N = (\text{FC.size()} + 1)/2$. Require $N \le T_2$. Zero-fill `tArray`, then for $t = 0 \dots T_2 - 1$, let $wt = \omega t - \text{rAngle}$:
   $$\text{tArray}[t] = \exp\!\left(\text{FC}[0] + \sum_{n=1}^{N-1}\big(\text{FC}[2n-1]\cos(n \cdot wt) + \text{FC}[2n]\sin(n \cdot wt)\big)\right)$$

#### Species Ordering & Seasonality Input Modes (`Transmission/transmission.h`)
- **CRITICAL SPECIES SORT**: Before creating `AnophelesModel` instances in `VectorModel`, XML `<anopheles>` elements are sorted **descending by `seasonality.annualEIR`** using `std::sort` (`a1.annualEIR > a2.annualEIR`). If `annualEIR == 0` for all species, throws `xml_scenario_error` unless `interventionMode == forcedEIR`. `speciesIndex[mosquitoName] = i` records this sorted index $i \in \{0, \dots, S-1\}$.
- Let `targetEIR = seasonality.annualEIR` ($\ge 0$). `initEIR365` has length $365$ (daily adult EIR):
  1. **`<fourierSeries>`**:
     - `FSCoeffic = [0.0, a_1, b_1, a_2, b_2, ...]`, `EIRRotateAngle = fourierSeries.EIRRotateAngle` (default `0.0`).
     - If `targetEIR == 0`: `FSCoeffic = [ -infinity ]`, `initEIR365 = [0.0; 365]`.
     - Else: call `expIDFT(initEIR365, FSCoeffic, EIRRotateAngle)`, update
       $$\text{FSCoeffic}[0] \mathrel{+}= \ln\!\left(\frac{\text{targetEIR}}{\sum_{d=0}^{364} \text{initEIR365}[d]}\right)$$
       and call `expIDFT(initEIR365, FSCoeffic, EIRRotateAngle)` again.
  2. **`<monthlyValues>` (`smoothing="fourier"`, `"none"` is unimplemented and throws)**:
     - Require 12 monthly values $m_0, \dots, m_{11}$.
     - **Exact C++ floor quirk**: `maxEIR = max(0.0, max_i m_i)` (initialized from `months[0]` before filling, which is `0.0`). Let $\text{min} = \text{maxEIR} / 100.0$. For $i = 0 \dots 11$:
       $$\text{if } m_i < \text{min}: \quad m_i \leftarrow m_i + \text{min} \quad \text{(NOTE: } += \text{min, NOT } = \text{min!)}$$
     - Set `FSCoeffic.assign(5, 0.0)` ($N = 3$, 2 harmonics), `EIRRotateAngle = 0.0`.
     - Call `logFourierCoefficients(months, FSCoeffic)`, then normalize `FSCoeffic[0]` and compute `initEIR365` via `expIDFT` exactly as in `<fourierSeries>`.
  3. **`<dailyValues>`**:
     - Only allowed if `interventionMode == forcedEIR` (otherwise throws). Require `daily.size() >= 365`.
     - $\text{minEIR} = 0.01 \times \frac{1}{L}\sum_{k=0}^{L-1} \text{daily}[k]$.
     - Accumulate for $k = 0 \dots L-1$: $d = k \bmod 365$, `nDays[d]++`, `initEIR365[d] += max(daily[k], minEIR)`. Divide `initEIR365[d] /= nDays[d]`, then scale `initEIR365` by `targetEIR / sum(initEIR365)`.
- **Aggregating `initialisationEIR`** (length `stepsPerYear = 365 / interval`):
  - Per species: `partialInitEIR[step] = sum_{d : (d / interval) % stepsPerYear == step} initEIR365[d]`.
  - Total: `initialisationEIR[step] = sum_{s=0}^{S-1} partialInitEIR_s[step]`. `annualEIR = sum(initialisationEIR)`.
- **Optional `scaledAnnualEIR`**:
  - Let `scaleFactor = scaledAnnualEIR / annualEIR`. Scale `initialisationEIR *= scaleFactor`, `annualEIR = scaledAnnualEIR`, and for `VectorModel` each species `s`: `FSCoeffic[0] += ln(scaleFactor)`, `partialInitEIR *= scaleFactor`, `speciesEIR *= scaleFactor`.

---

### 1.2 `PerHost`, `PerHostAnophParams` & Vector Interventions (`PerHost.*`, `AgeGroupInterpolation.*`, `ITN.*`, `IRS.*`, `GVI.*`)

#### Age-Based Relative Availability (`AgeGroupInterpolator`)
Initialized from `<availabilityToMosquitoes>` groups $(l_k, v_k)_{k=0}^{M-1}$ with $l_0 = 0$:
- **`interpolation="linear"` (default)**: Builds piecewise-linear knot map $(x_j, y_j)$:
  - Knot $0$: $(0.0, v_0)$
  - Knots $k = 1 \dots M-1$: $\big(\tfrac{1}{2}(l_{k-1} + l_k),\, v_{k-1}\big)$
  - Knot $M$: $\big(\tfrac{1}{2}(l_{M-1} + \text{maxHumanAgeYears}),\, v_{M-1}\big)$
  - Knot $M+1$: $(+\infty,\, v_{M-1})$
  - Evaluation at `ageYears`: find first knot with $x_j > \text{ageYears}$ (`upper_bound`); let $(a_0, b_0) = (x_{j-1}, y_{j-1})$ and $(a_1, b_1) = (x_j, y_j)$. If $a_1 == +\infty$, return $b_1$; else return $b_0 + \frac{\text{ageYears} - a_0}{a_1 - a_0}(b_1 - b_0)$.
- **`interpolation="none"`**: Piecewise constant $v_k$ for $\text{ageYears} \in [l_k, l_{k+1})$ (with $l_M = +\infty$).
- **`PerHost::adultAge()`**: Let $x_{\max}$ be the first knot/group $x$ whose value strictly exceeds all preceding values (`val > max` while scanning from $0$). Return `sim::fromYearsD(x_max)`.

#### Species Base Parameters (`PerHostAnophParams`)
For each species $s$:
- `entoAvailability`: `LognormalSampler::fromMeanCV(1.0, CV)` or `GammaSampler` or `ConstSampler(1.0)` from `mosq.availability`.
- `probMosqBiting` ($P_{B}$), `probMosqFindRestSite` ($P_{C}$), `probMosqSurvivalResting` ($P_{D}$): `BetaSampler(mean, variance)`:
  - If $\text{variance} == 0$, sample returns `mean`.
  - Else $c = \frac{\text{mean}(1 - \text{mean})}{\text{variance}} - 1$ (require $c > 0$), $a = \text{mean}\cdot c$, $b = c - a$; draws $\text{Beta}(a, b)$.
- `entoAvailabilityFactor`: Set by `AnophelesModel::initAvailability` to $\alpha_{0,s} = \frac{P_{A1,s}}{N_{\text{pop}}} \cdot \text{availFactor}_s$.
- **`calcAvailabilityPercentiles()`**:
  - Creates a deterministic local RNG `MasterRng master(0, 0); MasterRng seed(0, 0); LocalRng rng(seed);` (does **NOT** touch `util::master_RNG`!).
  - Draws $100{,}000$ samples: $X_k = \sum_{s=0}^{S-1} \alpha_{0,s} \cdot \text{entoAvailability}_s.\text{sample}(\text{rng})$, sorts ascending.
  - Sets `entoAvailabilityPercentiles[0] = 0.0`, `entoAvailabilityPercentiles[p] = X[p * 1000]` for $p = 1 \dots 99$, and `getEntoAvailabilityPercentile(100)` returns `DBL_MAX`.

#### Per-Human Initialization & Runtime Queries (`PerHost`)
When a `Human` is constructed (`Human::Human(dob)`):
1. Sample `HumanHet het = hetSample(human.rng)` (handles `TRANS_HET`, `COMORB_HET`, `TREAT_HET`, `TRANS_TREAT_HET`, `COMORB_TREAT_HET`, `COMORB_TRANS_HET`, `TRIPLE_HET`, setting factors to `0.2` or `1.8` via `bernoulli(0.5)`).
2. `iiFactor = infIncidence->getAvailabilityFactor(rng, 1.0)` (`1.0` default; `Gamma(k, 1/k)` for `NEGATIVE_BINOMIAL_MASS_ACTION`; `LogNormal(-0.5*sigma^2, sigma)` for `LOGNORMAL_MASS_ACTION`).
3. `availabilityFactor = het.availabilityFactor * iiFactor`.
4. `PerHost::initialise(rng, availabilityFactor)`:
   - `relativeAvailabilityHet = availabilityFactor`
   - For each species $s = 0 \dots S-1$:
     - `anophEntoAvailabilityRaw[s] = base[s].entoAvailability.sample(rng)`
     - `anophEntoAvailability[s] = base[s].entoAvailabilityFactor * anophEntoAvailabilityRaw[s] * availabilityFactor`
     - `anophProbMosqBiting[s] = base[s].probMosqBiting.sample(rng)`
     - `pRest1 = base[s].probMosqFindRestSite.sample(rng)`
     - `pRest2 = base[s].probMosqSurvivalResting.sample(rng)`
     - `anophProbMosqResting[s] = pRest1 * pRest2`
5. **Per-host runtime queries** (given active human vector intervention components $\mathcal{C}_h$ where `deployTime != sim::never()`):
   - $\text{relativeAvailabilityAge}(a) = \begin{cases} 0.0 & \text{if } \text{outsideTransmission} \\ \text{relAvailAge.eval}(a) & \text{otherwise} \end{cases}$
   - $\text{relativeAvailabilityHetAge}(a) = \text{relativeAvailabilityHet} \times \text{relativeAvailabilityAge}(a)$
   - $\text{entoAvailabilityHetVecItv}(s) = \text{anophEntoAvailability}[s] \times \prod_{c \in \mathcal{C}_h} c.\text{relativeAttractiveness}(s)$
   - $\text{entoAvailabilityFull}(s, a) = \text{entoAvailabilityHetVecItv}(s) \times \text{relativeAvailabilityAge}(a)$
   - $\text{probMosqBiting}(s) = \text{anophProbMosqBiting}[s] \times \prod_{c \in \mathcal{C}_h} c.\text{preprandialSurvivalFactor}(s)$
   - $\text{probMosqResting}(s) = \text{anophProbMosqResting}[s] \times \prod_{c \in \mathcal{C}_h} c.\text{postprandialSurvivalFactor}(s)$
   - $\text{relMosqFecundity}(s) = \prod_{c \in \mathcal{C}_h} c.\text{relFecundity}(s)$
   - $\text{availBite}(s) = \text{entoAvailabilityHetVecItv}(s) \times \text{probMosqBiting}(s)$
   - Note on ITN/IRS/GVI protection blending: each component computes raw effect $x$ at `age = sim::nowOrTs1() - deployTime` and returns `x * proportionProtected + proportionUnprotected` (where `proportionProtected = usage * propActive`, `proportionUnprotected = 1.0 - proportionProtected`). `PerHost::update(human)` is called inside `VectorModel::calculateEIR` and updates ITN disposal (`sim::ts0() >= disposalTime` sets `deployTime = sim::never()`) and hole accumulation (`newHoles = rng.poisson(holeRate)`, `nHoles += newHoles`, `holeIndex += newHoles + ripFactor * rng.poisson(nHoles * ripRate)`).

---

### 1.3 `TransmissionModel` Base Class (`TransmissionModel.h`)

- **`updateKappa(population)`** (called once per time step after `population.update()` while `in_update == true`):
  - Initialize `sumWt_kappa = 0.0`, `sumWeight = 0.0`, `numTransmittingHumans = 0`.
  - For each `human` in `population`:
    - `avail = human.perHostTransmission.relativeAvailabilityHetAge(sim::inYears(human.age(sim::ts1())))`
    - `sumWeight += avail`
    - `pTransmit = human.withinHostModel->probTransmissionToMosquito(probTransGenotype_i, probTransGenotype_l)`
    - If `!opt_vaccine_genotype`:
      $$\text{riskTrans} = \text{avail} \cdot p_{\text{Transmit}} \cdot \text{human.vaccine.getFactor}(\text{TBV})$$
      Else (`opt_vaccine_genotype == true`):
      $$\text{riskTrans} = \text{avail} \cdot \sum_{g=0}^{G-1} \big(\text{probTransGenotype\_i}[g] + \text{probTransGenotype\_l}[g]\big) \cdot \text{human.vaccine.getFactor}(\text{TBV}, g)$$
    - If `riskTrans > 0.0`, `++numTransmittingHumans`.
    - `sumWt_kappa += riskTrans`.
  - Index `lKMod = sim::moduloSteps(sim::ts1(), laggedKappa.size())`.
  - If `population.empty()`, `laggedKappa[lKMod] = 0.0`; else check `sumWeight > 10 * DBL_MIN` and set `laggedKappa[lKMod] = sumWt_kappa / sumWeight`.
  - Annual average kappa update: let `tmod = sim::moduloYearSteps(sim::ts0())`.
    $$\text{\_sumAnnualKappa} \mathrel{+}= \text{laggedKappa}[\text{lKMod}] \times \text{initialisationEIR}[\text{tmod}]$$
    If `tmod == sim::stepsPerYear() - 1`:
    $$\text{\_annualAverageKappa} = \frac{\text{\_sumAnnualKappa}}{\text{annualEIR}}, \qquad \text{\_sumAnnualKappa} = 0.0$$
  - Returns `laggedKappa[lKMod]`.
- **`getEIR(human, age0, ageYears1, EIR_i, EIR_l)`**:
  - Calls `calculateEIR(human, ageYears1, EIR_i, EIR_l)`.
  - `sum_EIR_i = sum(EIR_i)`, `sum_EIR_l = sum(EIR_l)`, `allEIR = sum_EIR_i + sum_EIR_l`.
  - If `age0 >= adultAge`: `tsAdultEntoInocs_i += sum_EIR_i`, `tsAdultEntoInocs_l += sum_EIR_l`, `tsAdultEntoInocs += allEIR`, `tsNumAdults += 1`.
  - Returns `allEIR`.
- **`surveyEIR()`** (called at end of step before `end_update()`):
  - `tsAdultEIR = tsAdultEntoInocs / tsNumAdults`, `tsAdultEIR_i = tsAdultEntoInocs_i / tsNumAdults`, `tsAdultEIR_l = tsAdultEntoInocs_l / tsNumAdults`.
  - Reset `tsAdultEntoInocs = tsAdultEntoInocs_i = tsAdultEntoInocs_l = 0.0`, `tsNumAdults = 0`.
  - `surveyInputEIR += initialisationEIR[sim::moduloYearSteps(sim::ts0())]`.
  - `surveySimulatedEIR += tsAdultEIR`, `surveySimulatedEIR_i += tsAdultEIR_i`, `surveySimulatedEIR_l += tsAdultEIR_l`.
- **`summarize()`**:
  - Records `nTransmit` = `laggedKappa[sim::moduloSteps(sim::now(), laggedKappa.size())]`.
  - Records `annAvgK` = `_annualAverageKappa`.
  - If `!mon::isReported()`, returns early (preserving `survey*EIR` and `lastSurveyTime` across unreported surveys!).
  - Let `duration = sim::inSteps(sim::now() - lastSurveyTime)`. If `duration > 0.0`:
    records `inputEIR = surveyInputEIR / duration`, `simulatedEIR = surveySimulatedEIR / duration`, `simulatedEIR_Introduced = surveySimulatedEIR_i / duration`, `simulatedEIR_Indigenous = surveySimulatedEIR_l / duration`.
  - Resets `surveyInputEIR = surveySimulatedEIR = surveySimulatedEIR_i = surveySimulatedEIR_l = 0.0`, `lastSurveyTime = sim::now()`.
  - *(Note: `main.cpp` calls `transmission->summarize()` right BEFORE `mon::initMainSim()` at `s_interv = 0`. Because `runtime.isInit == false` at that instant, `statSurveyNumber() == NOT_USED` so `recordStat` is a no-op, while `isReported() == true`, which cleanly zeroes `survey*EIR` and sets `lastSurveyTime = sim::now()` for the main phase!)*

---

### 1.4 `NonVectorModel` (`NonVectorModel.h`)

- **Parameters & Buffers**:
  - `nYearsWarmupData = 5`.
  - `nSpore = sim::fromDays(nonVectorData.eipDuration)` (require `nSpore == (nSpore / interval) * interval` implicitly via `inSteps(nSpore)`).
  - `laggedKappa.resize(sim::inSteps(nSpore) + 1, 0.0)`.
  - `initialKappa.resize(sim::inSteps(sim::fromYearsI(5)), 0.0)`.
  - `initialisationEIR` (size `stepsPerYear`): given `daily = nonVectorData.EIRDaily` (size $\ge 365$), let $\text{minEIR} = 0.01 \cdot \text{averageEIR}(\text{daily})$. For day $d = 0 \dots \text{daily.size()}-1$:
    $$\text{step} = (d / \text{interval}) \bmod \text{stepsPerYear}, \quad \text{nDays}[\text{step}]\mathrel{++}, \quad \text{initialisationEIR}[\text{step}] \mathrel{+}= \max(\text{daily}[d], \text{minEIR})$$
    Then for each `step`: `initialisationEIR[step] *= interval / double(nDays[step])`. `annualEIR = sum(initialisationEIR)`.
- **`updateKappa(population)`**:
  - `currentKappa = TransmissionModel::updateKappa(population)`.
  - If `simulationMode == forcedEIR`: `initialKappa[sim::moduloSteps(sim::ts1(), initialKappa.size())] = currentKappa`.
- **`initIterate()`**:
  - `simulationMode = interventionMode`.
  - If `simulationMode == dynamicEIR`:
    - Collapse 5-year `initialKappa` to 1 year (`stepsPerYear`): for $i = \text{stepsPerYear} \dots \text{initialKappa.size()}-1$, `initialKappa[i % stepsPerYear] += initialKappa[i]`; resize to `stepsPerYear`; multiply all entries by $\frac{1}{5}$.
    - Validate every `initialKappa[i] > 0.0` (else throw ` initialKappa is invalid`).
  - Returns `sim::zero()` (no iterative calibration phase in `NonVectorModel`).
- **`changeEIRIntervention(nonVectorData)`**:
  - Sets `simulationMode = transientEIRknown`. Checks `nSpore == sim::fromDays(nonVectorData.eipDuration)`.
  - Requires `daily.size() >= (sim::endDate() - sim::startDate()) + 1`.
  - `nSteps = sim::inSteps(sim::fromDays(daily.size() - 1)) + 1`. Accumulates `max(daily[d], 0.01 * averageEIR(daily))` into `interventionEIR[d / interval]`, then scales each bin by `interval / double(nDays[istep])`.
  - Sets `annualEIR = quiet_NaN()`.
- **`calculateEIR(human, ageYears, EIR_i, EIR_l)`**:
  - `EIR_i.assign(1, 0.0)` (`resize(1)` on fresh empty vector), `EIR_l.resize(1)`.
  - Base adult step EIR $E_0$:
    - `forcedEIR`: $E_0 = \text{initialisationEIR}[\text{sim::moduloYearSteps}(\text{sim::ts0}())]$
    - `transientEIRknown`: $E_0 = \text{interventionEIR}[\text{sim::inSteps}(\text{sim::intervTime}())]$
    - `dynamicEIR`: $E_0 = \text{initialisationEIR}[\text{sim::moduloYearSteps}(\text{sim::ts0}())]$, and if `sim::intervTime() >= 0`:
      $$t = \text{sim::inSteps}(\text{sim::ts1}() - n_{\text{Spore}}), \qquad E_0 \leftarrow E_0 \times \frac{\text{laggedKappa}[t \bmod \text{laggedKappa.size()}]}{\text{initialKappa}[t \bmod \text{stepsPerYear}]}$$
  - Host adjustment:
    $$\text{EIR\_l}[0] = E_0 \times \text{human.perHostTransmission.relativeAvailabilityHetAge}(\text{ageYears})$$
  - Monitoring: `mon::recordStat(innoculationsPerAgeGroup, human, EIR_l[0])`, `mon::recordStat(innoculationsPerVector, EIR_l[0], 0)`.
- **`uninfectVectors()`**: If `simulationMode != dynamicEIR`, logs warning; zeroes all elements of `laggedKappa`.

---

### 1.5 `VectorModel` & `AnophelesModel` Chitnis Mosquito Difference Equations (`VectorModel.*`, `AnophelesModel.*`)

Let $\tau = \text{mosqRestDuration}$ (days, $\ge 1$) and $\theta_s = \text{EIPDuration}$ (days, strictly $> 2\tau$, i.e. $\theta_s \ge 2\tau + 1$). Ring buffer length is $L_v = \text{N\_v\_length} = \theta_s + \tau$. Let $G = \text{Genotypes::N()}$.

#### 1.5.1 Baseline Availability & Seeking Death Rate Calibration (`AnophelesModel::initAvailability`)
From XML `<mosq>`:
- $A_0 = \text{laidEggsSameDayProportion}$, $P_f = \text{survivalFeedingCycleProbability}$, $\chi = \text{humanBloodIndex}$, $\theta_d = \text{mosqSeekingDuration}$, $P_{E1} = \text{mosqProbOvipositing}$, and mean human parameters $P_{B1}, P_{C1}, P_{D1}$.
- Probability of not finding a host in one night: $P_A^{(0)} = 1 - A_0$.
- Availability conversion factor:
  $$\text{availFactor} = \frac{-\ln P_A^{(0)}}{\theta_d (1 - P_A^{(0)})}$$
- **Case A: No Non-Human Hosts (`nhhs.empty()`)**:
  $$P_{A1} = \frac{A_0 P_f}{P_{B1} P_{C1} P_{D1} P_{E1}}, \qquad P_{Ah} = 0$$
- **Case B: With Non-Human Hosts** $\{( \xi_k, P_{B,k}, P_{C,k}, P_{D,k}, f_k )\}$ (require $\sum_k \xi_k \in [0.9999, 1.0001]$; else if $\chi == 1.0$, ignore NHHs as in Case A):
  $$v = \chi P_{D1} P_{E1}, \qquad \chi_1 = 1 - \chi, \qquad u_k = \xi_k P_{B,k} P_{C,k}, \qquad w_k = \chi_1 P_{D,k} P_{E1}$$
  $$P_{A1} = \frac{A_0 P_f \chi \sum_k u_k}{P_{B1} P_{C1} \sum_k u_k (v + w_k)}, \qquad P_{Ah} = \frac{A_0 P_f \chi_1}{\sum_k u_k (v + w_k)}$$
- Baseline per-human availability rate & NHH aggregates:
  $$\text{initAvail} = \frac{P_{A1}}{N_{\text{pop}}} \cdot \text{availFactor} \quad (\text{registered into PerHostAnophParams}[s])$$
  For each NHH $k$: $\text{avail}_k = P_{Ah} \xi_k \cdot \text{availFactor}$, $\text{df}_k = \text{avail}_k P_{B,k} P_{C,k} P_{D,k}$, $\text{nhh\_avail} = \sum_k \text{avail}_k$, $\text{nhh\_sigma\_df} = \sum_k \text{df}_k$, $\text{nhh\_sigma\_dff} = \sum_k \text{df}_k f_k$ (stored in `nhhInstances[name]` with `expiry = sim::future()`).
- Mosquito host-seeking death rate $\mu_{vA}$:
  $$\mu_{vA} = \frac{1 - (P_A^{(0)} + P_{A1} + P_{Ah})}{1 - P_A^{(0)}} \cdot \frac{-\ln P_A^{(0)}}{\theta_d}$$
  (Must satisfy $\mu_{vA} > 0$, i.e. $P_A^{(0)} + P_{A1} + P_{Ah} < 1$, else throws `xml_scenario_error`.)

#### 1.5.2 Initial State at $t = 0$ (`VectorModel::init2`, `AnophelesModel::initEIR`, `AnophelesModel::init2`)
- Initial constants: $\rho_S = 0.021$, $\rho_O = 0.078 \implies \text{initNvFromSv} = \frac{1}{0.021}$, $\text{initOvFromSv} = \frac{0.078}{0.021}$.
- At $t = 0$, compute across `population`:
  $$\text{meanAvail} = \frac{1}{N_{\text{pop}}}\sum_{h} \text{relativeAvailabilityAge}(\text{ageYears}_h(0))$$
  For each species $s$:
  $$\text{sum\_avail} = \sum_h \alpha_h, \quad \sigma_f = \sum_h \alpha_h P_{B,h}, \quad \sigma_{df} = \sum_h \alpha_h P_{B,h} P_{C,h} P_{D,h}, \quad \sigma_{dff} = \sum_h \alpha_h P_{B,h} P_{C,h} P_{D,h} f_h$$
  where $\alpha_h = \text{entoAvailabilityFull}(s, \text{ageYears}_h(0))$.
- In `AnophelesModel::init2`:
  $$\nu_A = \text{sum\_avail} + \text{nhh\_avail} + \mu_{vA}, \qquad P_A = e^{-\nu_A \theta_d}, \qquad \alpha_d = \frac{1 - P_A}{\nu_A}$$
  $$P_{A\mu} = (1 - P_A)\frac{\mu_{vA}}{\nu_A}, \qquad P_{A1} = (1 - P_A)\frac{\text{sum\_avail}}{\nu_A}, \qquad P_{Ah} = (1 - P_A)\frac{\text{nhh\_avail}}{\nu_A}$$
  $$\text{sumPFindBite} = \sigma_f \alpha_d, \qquad P_{df} = (\sigma_{df} + \text{nhh\_sigma\_df}) \alpha_d P_E, \qquad P_{dff} = (\sigma_{dff} + \text{nhh\_sigma\_dff}) \alpha_d P_E$$
  $$\text{initNv0FromSv} = \text{initNvFromSv} \cdot (1 - P_A - P_{df}), \qquad \text{initSvFromEIR} = \frac{N_{\text{pop}} \cdot \text{meanAvail}}{\text{sumPFindBite}}$$
  - For $d = 0 \dots 364$: `forcedS_v[d] = speciesEIR[d] * initSvFromEIR`, `mosqEmergeRate[d] = forcedS_v[d] * initNv0FromSv`.
  - Working caches: `fArray` length $\theta_s - \tau + 1$ with `fArray[0] = 1.0`; `ftauArray` length $\theta_s$ with `ftauArray[0..tau-1] = 0.0`, `ftauArray[tau] = 1.0`; `uninfected_v` length $L_v$.
  - Ring buffers (for $t = 0 \dots L_v - 1$, where $L_v \le 365$):
    - `P_A[t] = P_A`, `P_Amu[t] = P_Amu`, `P_A1[t] = P_A1`, `P_Ah[t] = P_Ah`, `P_df[t] = P_df`, `P_dff[t] = P_dff`, `P_dif_i = 0`, `P_dif_l = 0`.
    - `N_v[t] = forcedS_v[t] * initNvFromSv`.
    - For genotype $g = 0 \dots G-1$: `S_v_i[t*G + g] = 0`, `O_v_i[t*G + g] = 0`, `S_v_l[t*G + g] = forcedS_v[t] * Genotypes::initialFreq(g)`, `O_v_l[t*G + g] = S_v_l[t*G + g] * initOvFromSv`.

#### 1.5.3 Per-Time-Step `VectorModel::vectorUpdate` & `AnophelesModel::advancePeriod`
 *(Skipped completely if `simulationMode == transientEIRknown`; otherwise runs with `isDynamic = (simulationMode == dynamicEIR)`.)*

1. **Human aggregation (`VectorModel::vectorUpdate`)**:
   For each human $h$, evaluate `pTransmit = human.withinHostModel->probTransmissionToMosquito(probTrans_i, probTrans_l)` and `ageYears1 = sim::inYears(human.age(sim::ts1()))`. For each species $s$:
   - $\alpha_h = \text{host.entoAvailabilityFull}(s, \text{ageYears1})$
   - $\text{sum\_avail}[s] \mathrel{+}= \alpha_h$
   - $\text{df} = \alpha_h \cdot \text{host.probMosqBiting}(s) \cdot \text{host.probMosqResting}(s)$
   - $\sigma_{df}[s] \mathrel{+}= \text{df}$, $\quad \sigma_{dff}[s] \mathrel{+}= \text{df} \cdot \text{host.relMosqFecundity}(s)$
   - If `!opt_vaccine_genotype`: for $g = 0 \dots G-1$, $\sigma_{\text{dif},\{i,l\}}[s,g] \mathrel{+}= \text{df} \cdot \text{probTrans}_{\{i,l\}}[g] \cdot \text{vaccine.getFactor}(\text{TBV})$.
     Else: use genotype-specific `vaccine.getFactor(TBV, g)`.

2. **Vector population interventions & step rates (`AnophelesModel::advancePeriod` at `ts0 = sim::ts0()`)**:
   - Seeking death rate with interventions:
     $$\mu_{v,\text{interv}} = \mu_{vA} \prod_{j} \big(1 + \text{seekingDeathRateIntervs}[j].\text{current\_value}(ts_0)\big)$$
   - Active NHHs: erase any NHH in `nhhInstances` with `ts0 >= expiry`. Copy remaining NHHs and multiply `avail_i`, `P_B_I`, `P_C_I`, `P_D_I`, `rel_fecundity` by $\prod (1 - \text{decay.current\_value}(ts_0))$ from `reduceNhhAvailability`, `reduceP_B_I`, `reduceP_C_I`, `reduceP_D_I`, `reduceFecundity`. Sum into `modified_nhh_avail`, `modified_nhh_sigma_df`, `modified_nhh_sigma_dff`.
   - Start $\nu_A = \mu_{v,\text{interv}} + \text{sum\_avail} + \text{modified\_nhh\_avail}$; add `modified_nhh_sigma_df` to $\sigma_{df}$ and `modified_nhh_sigma_dff` to $\sigma_{dff}$.
   - **Baited traps**: erase traps with `ts0 > expiry` (strict `>`); for remaining traps, $\nu_A \mathrel{+}= \text{initialAvail} \times \text{availHet->eval}(ts_0 - \text{deployTime})$. (When deployed, `initialAvail = N_pop * ratioToHumans * baseAvail * relAvail`, `expiry = nowOrTs1() + lifespan`).
   - **ATSB (`probAdditionalDeathSugarFeedingIntervs`)**:
     Let $p_{\text{sugar}} = \sum_j p_j.\text{current\_value}(ts_0)$. If $p_{\text{sugar}} > 1.0$, throw `xml_scenario_error`. If $p_{\text{sugar}} > 0.0$, solve for $a_t \ge 0$ in:
     $$f(x) = \big(1 - e^{-(x + a_h + a_n + \mu_{v,\text{interv}})\theta_d}\big)\frac{x}{x + a_h + a_n + \mu_{v,\text{interv}}} - p_{\text{sugar}} = 0$$
     where $a_h = \text{sum\_avail}$, $a_n = \text{modified\_nhh\_avail}$.
     *(Exact C++ solver detail: uses GSL `gsl_root_fdfsolver_secant` starting at $x_0 = 0.5$, max 20 iterations, stopping when `|a_t - x_prev| < 1e-4`. Note that on iteration 1 GSL's secant solver uses `quadratic_fdf`, which in C++ computes initial derivative `df = -a + b + c` with `a = (1 - exp(-tmp))*x/tmp^2`, `b = (1 - exp(-tmp))/tmp`, `c = exp(-tmp*theta_d)*x*theta_d/tmp` where `tmp = x + ah + an + mu_v_interv`, omitting `* theta_d` inside `a` and `b`! Subsequent secant steps only evaluate `quadratic_f`.)* Add $\nu_A \mathrel{+}= a_t$.
   - Step probabilities:
     $$P_A^{\text{ts}} = e^{-\nu_A \theta_d}, \qquad \alpha_d = \frac{1 - P_A^{\text{ts}}}{\nu_A}, \qquad \alpha_E = \alpha_d P_E \prod_j \big(1 - \text{probDeathOvipositingIntervs}[j].\text{current\_value}(ts_0)\big)$$
     $$P_{df}^{\text{ts}} = \sigma_{df} \alpha_E, \qquad P_{dff}^{\text{ts}} = \sigma_{dff} \alpha_E, \qquad P_{\text{dif},\{i,l\}}^{\text{ts}}[g] = \sigma_{\text{dif},\{i,l\}}[g] \alpha_E$$
   - Diagnostic outputs (note denominator uses baseline `mosq.seekingDeathRate`, not `leaveRate`):
     $$D_{\text{out}} = \mu_{vA} + \text{sum\_avail} + \text{modified\_nhh\_avail}$$
     $$P_{A\mu}^{\text{ts}} = (1 - P_A^{\text{ts}})\frac{\mu_{vA}}{D_{\text{out}}}, \qquad P_{A1}^{\text{ts}} = (1 - P_A^{\text{ts}})\frac{\text{sum\_avail}}{D_{\text{out}}}, \qquad P_{Ah}^{\text{ts}} = (1 - P_A^{\text{ts}})\frac{\text{modified\_nhh\_avail}}{D_{\text{out}}}$$
   - Zero `partialEIR_i[0..G-1] = 0`, `partialEIR_l[0..G-1] = 0`, `timeStep_N_v0 = 0`.

3. **Daily Difference Equation Update (`AnophelesModel::update`)**:
   Loop daily $d_0 = ts_0, ts_0 + 1, \dots, ts_0 + \text{interval} - 1$:
   - Emergence survival: $S_{\text{emerg}} = \prod_j \big(1 - \text{emergenceReduction}[j].\text{current\_value}(ts_0)\big)$.
   - Indices: $d_1 = d_0 + 1$, $d_{1\text{Mod}} = d_1 + L_v$, $t_1 = d_1 \bmod L_v$, $t_0 = d_0 \bmod L_v$, $t_\tau = (d_{1\text{Mod}} - \tau) \bmod L_v$.
   - Store step probabilities at $t_1$: $P_A[t_1] = P_A^{\text{ts}}$, $P_{A\mu}[t_1] = P_{A\mu}^{\text{ts}}$, $P_{A1}[t_1] = P_{A1}^{\text{ts}}$, $P_{Ah}[t_1] = P_{Ah}^{\text{ts}}$, $P_{df}[t_1] = P_{df}^{\text{ts}}$, $P_{dff}[t_1] = P_{dff}^{\text{ts}}$, $P_{\text{dif},\{i,l\}}[t_1, g] = P_{\text{dif},\{i,l\}}^{\text{ts}}[g]$.
   - **EIP survival recurrences** ($t(n) \equiv (d_{1\text{Mod}} - n) \bmod L_v$):
     - `fArray` ($f[0] = 1$):
       - For $n = 1 \dots \tau$: $f[n] = f[n-1] \cdot P_A[t(n)]$; then $f[\tau] \mathrel{+}= P_{df}[t_\tau]$.
       - For $n = \tau + 1 \dots \theta_s - \tau$: $f[n] = P_{df}[t(n)] f[n - \tau] + P_A[t(n)] f[n - 1]$.
     - `ftauArray` ($f_\tau[0\dots \tau-1] = 0, f_\tau[\tau] = 1$):
       - For $n = \tau + 1 \dots 2\tau$: $f_\tau[n] = f_\tau[n-1] \cdot P_A[t(n)]$; then $f_\tau[2\tau] \mathrel{+}= P_{df}[t(2\tau)]$.
       - For $n = 2\tau + 1 \dots \theta_s - 1$: $f_\tau[n] = P_{df}[t(n)] f_\tau[n - \tau] + P_A[t(n)] f_\tau[n - 1]$.
   - **Uninfected seeking mosquitoes**:
     For $d = 1 \dots L_v - 1$:
     $$U[d] = N_v[t(d)] - \sum_{g=0}^{G-1} \big(O_{v,i}[t(d), g] + O_{v,l}[t(d), g]\big)$$
   - **Infected ($O_v$) and Infectious ($S_v$) seeking mosquitoes** (for each $g = 0 \dots G-1$, and origin $o \in \{i, l\}$):
     $$O_{v,o}[t_1, g] = P_{\text{dif},o}[t_\tau, g] \cdot U[\tau] + P_A[t_0] \cdot O_{v,o}[t_0, g] + P_{df}[t_\tau] \cdot O_{v,o}[t_\tau, g]$$
     $$\text{sum}_o = \sum_{l=1}^{\tau - 1} P_{\text{dif},o}[t(\theta_s + l), g] \cdot P_{df}[t_\tau] \cdot U[\theta_s + l] \cdot f_\tau[\theta_s + l - \tau]$$
     $$S_{v,o}[t_1, g] = P_{\text{dif},o}[t(\theta_s), g] \cdot f[\theta_s - \tau] \cdot U[\theta_s] + \text{sum}_o + P_A[t_0] \cdot S_{v,o}[t_0, g] + P_{df}[t_\tau] \cdot S_{v,o}[t_\tau, g]$$
     - **Low-transmission cutoff**: if `isDynamic` and $S_{v,i}[t_1, g] + S_{v,l}[t_1, g] \le \text{mosq.minInfectedThreshold}$:
       set $S_{v,i}[t_1, g] = 0.0$ and $S_{v,l}[t_1, g] = 0.0$.
     - Accumulate step EIR factor & $S_v$:
       $$\text{partialEIR}_o[g] \mathrel{+}= S_{v,o}[t_1, g] \cdot \alpha_d, \qquad \text{total\_S\_v} \mathrel{+}= S_{v,i}[t_1, g] + S_{v,l}[t_1, g]$$
   - Store `quinquennialS_v[d_1 % (5 * 365)] = total_S_v`.
   - **Host-seeking total ($N_v$) & Emergence**:
     $$n_{\text{ovip}} = P_{dff}[t_\tau] \cdot N_v[t_\tau]$$
     $$\text{newAdults} = \text{getEmergenceRate}(d_0, \text{mosqEmergeRate}, n_{\text{ovip}}) \cdot S_{\text{emerg}}$$
     (Base `AnophelesModel::getEmergenceRate` returns `mosqEmergeRate[d_0 % 365]`.)
     $$N_v[t_1] = \text{newAdults} + P_A[t_0] \cdot N_v[t_0] + n_{\text{ovip}}, \qquad \text{timeStep\_N\_v0} \mathrel{+}= \text{newAdults}$$

4. **Per-Human EIR (`VectorModel::calculateEIR`)**:
   - First call `host.update(human)`.
   - If `simulationMode == forcedEIR`:
     - `EIR_i.assign(1, 0.0)`, `EIR_l.assign(1, 0.0)`.
     - $\text{ageFactor} = \text{host.relativeAvailabilityAge}(\text{ageYears})$.
     - $\text{EIR\_l}[0] = \sum_{s=0}^{S-1} \frac{\text{partialInitEIR}_s[\text{moduloYearSteps}(ts_0)]}{\text{initAvail}_s} \cdot \text{ageFactor} \cdot \text{host.entoAvailabilityHetVecItv}(s)$.
   - Else if `simulationMode == transientEIRknown`:
     - Same formula as `forcedEIR`, replacing `partialInitEIR_s[...]` with `interventionEIR_s[inSteps(intervTime())]`. *(Note: C++ has a stale debug `assert(simulationMode == forcedEIR)` in that branch; do not panic in Rust!)*
   - Else (`simulationMode == dynamicEIR`):
     - `EIR_i.assign(G, 0.0)`, `EIR_l.assign(G, 0.0)`.
     - $\text{ageFactor} = \text{host.relativeAvailabilityAge}(\text{ageYears})$.
     - For each species $s = 0 \dots S-1$:
       - $\text{entoFactor} = \text{ageFactor} \cdot \text{host.availBite}(s)$.
       - For $g = 0 \dots G-1$:
         - $e_{i} = \text{partialEIR}_{i,s}[g] \cdot \text{entoFactor}$, $\quad e_{l} = \text{partialEIR}_{l,s}[g] \cdot \text{entoFactor}$.
         - Record `mon::recordStat(innoculationsPerAgeGroup, human, e_i + e_l, 0, g)` and `mon::recordStat(innoculationsPerVector, e_i + e_l, s, g)`.
         - $\text{EIR\_i}[g] \mathrel{+}= e_i$, $\quad \text{EIR\_l}[g] \mathrel{+}= e_l$.

---

### 1.6 `SimpleMPDAnophelesModel` (`SimpleMPDAnophelesModel.h`)

Selected when `<simpleMPD>` is present in `<anopheles>`. Parameters: $\theta_j = \text{developmentDuration}$ (days, $> 0$), $P_j = \text{probPreadultSurvival} \in [0, 1]$, $b = \text{fEggsLaidByOviposit} > 0$.
- **`init2`**: Calls `AnophelesModel::init2`, recomputes initial $P_{dff}^{\text{ts}}$, and initializes:
  - Ring buffer `nOvipositingDelayed` (length $\theta_j$): for $t = 0 \dots \theta_j - 1$,
    $$\text{nOvipositingDelayed}[(t + \tau) \bmod \theta_j] = P_{dff}^{\text{ts}} \cdot \text{initNvFromSv} \cdot \text{forcedS\_v}[t]$$
  - Inverse larval resources $\gamma[t] = \text{invLarvalResources}[t]$ (length $365$): for $t = 0 \dots 364$,
    $$y_t = b \cdot P_{dff}^{\text{ts}} \cdot \text{initNvFromSv} \cdot \text{forcedS\_v}[(t + 365 - \tau - \theta_j) \bmod 365], \qquad \gamma[t] = \frac{P_j y_t - N_{v0}[t]}{N_{v0}[t] \cdot y_t}$$
- **`scale(factor)`**: Calls `AnophelesModel::scale(factor)` (scales `N_v, O_v_i, O_v_l, S_v_i, S_v_l` by `factor`) AND scales `nOvipositingDelayed` by `factor`.
- **`initIterate()`**: Recomputes $\gamma[t]$ for $t = 0 \dots 364$ from 5-year `quinquennialOvipositing` (length $5 \times 365$):
  $$y_t = b \cdot \frac{1}{5}\sum_{k=1}^{5} \text{quinquennialOvipositing}\big[(t - \theta_j + 365 k) \bmod (5 \times 365)\big], \qquad \gamma[t] = \frac{P_j y_t - N_{v0}[t]}{N_{v0}[t] \cdot y_t}$$
- **`getEmergenceRate(d0, mosqEmergeRate, nOvipositing)`**:
  Let $d_1 = d_0 + 1$:
  $$y_t = b \cdot \text{nOvipositingDelayed}[d_1 \bmod \theta_j], \qquad E = \frac{P_j y_t}{1.0 + \gamma[d_0 \bmod 365] \cdot y_t}$$
  $$\text{nOvipositingDelayed}[d_1 \bmod \theta_j] = n_{\text{ovip}}, \qquad \text{quinquennialOvipositing}[d_1 \bmod 1825] = n_{\text{ovip}}$$
  If `sim::intervTime() > 0 && E < 0.0`, throw `base_exception`. Return $\max(E, 0.0)$.

---

### 1.7 `EmergenceRateEstimator` & `CalcNvO` Periodic Solver (`EmergenceRateEstimator.h`, `CalcNvO.*`)

#### Calibration Driver (`VectorModel::initIterate()`)
- If `interventionMode != dynamicEIR`: sets `simulationMode = interventionMode`, returns `0`.
- If `initIterations < 0`: sets `simulationMode = dynamicEIR`, returns `0`.
- Increments `++initIterations`; if `initIterations > 30`, throws `Emergence rate calculation failed to converge!`.
- Loops $i = 0 \dots S-1$:
  - `needIterate = emergenceRateEstimators[i]->estimate(*species[i], laggedKappa, meanAvail)`
  - `species[i]->initIterate()`
  - `if (needIterate) break;` *(stops at first species needing another iteration!)*
- If `needIterate`: returns `sim::oneYear() + sim::fromYearsI(5)` ($6$ years = $2190$ days: 1 yr stabilization + 5 yr `quinquennialS_v` collection).
- Else: sets `initIterations = -1` and returns `sim::oneYear()` ($365$ days final stabilization before switching to `dynamicEIR`).

#### Mode A: Default `EmergenceRateAdaptiveFitter` (`USE_EXACT_NV0_SOLVER` = false)
- Initial state: `scaleFactor = 1.0`, `shiftAngle = m.EIRRotateAngle - (m.mosq.EIPDuration + 10) / 365.0 * 2.0 * M_PI`, `rotated = false`, `scaled = false`.
- On `estimate(m, laggedKappa, meanAvail)`:
  1. Extract 5th year of `quinquennialS_v`: for $i = 4 \times 365 \dots 5 \times 365 - 1$, `avgAnnualS_v[i % 365] = m.quinquennialS_v[i]`.
  2. $\text{factor} = \frac{\sum \text{forcedS\_v}}{\sum \text{avgAnnualS\_v}}$. (If $\sum \text{forcedS\_v} == 0$, return `false`; if $\neg(10^{-6} < \text{factor} < 10^6)$, throw `VectorFitting` error.)
  3. If $|\text{factor} - 1.0| > 0.1$: `scaled = false`, `scaleFactor *= factor`; else `scaled = true`.
  4. **`findAngle(EIRRotateAngle, FSCoeffic, avgAnnualS_v)`**:
     - Let $\delta = \frac{2\pi}{365}$. Loop `for (double angle = -M_PI; angle < M_PI; angle += delta)`:
       - Call `expIDFT(temp, FSCoeffic, EIRRotateAngle + angle)` (length 365).
       - Compute `sum = sqrtf(sum_{d=0}^{364} (temp[d] - avgAnnualS_v[d])^2)` *(note `sqrtf` single-precision cast in C++!)*.
       - Track `minAngle` achieving strict `< min`.
  5. `shiftAngle += rAngle`, `rotated = true`.
  6. Reconstruct emergence: `expIDFT(m.mosqEmergeRate, m.FSCoeffic, -shiftAngle)` *(note minus sign!)*, then `vectors::scale(m.mosqEmergeRate, scaleFactor * m.initSvFromEIR * m.initNv0FromSv)`.
  7. Scale state buffers: `m.scale(factor)`.
  8. Return `!(scaled && rotated)`.

#### Mode B: Exact Periodic Solver `EmergenceRateSolver` / `CalcInitMosqEmergeRate` (`USE_EXACT_NV0_SOLVER` = true)
1. **Inputs constructed in `EmergenceRateSolver::estimate`**:
   - $\theta_p = 365$, $\tau = \text{restDuration}$, $\theta_s = \text{EIPDuration}$.
   - Host type $0$ (humans, malaria-susceptible, $m = 1$): $N_0 = N_{\text{pop}}$, $\alpha_0 = \text{initAvail} \cdot \text{meanAvail}$, $P_{B,0}, P_{C,0}, P_{D,0}, P_{E,0}$ from `m.mosq`.
   - Additional host types $1 \dots n-1$ from `m.nhhInstances` (non-susceptible): $N_i = 1.0$, $\alpha_i = \text{nhh.avail\_i}$, $P_{B,i}, P_{C,i}, P_{D,i}$, $P_{E,i} = P_{E,0}$.
   - Daily `humanKappa[0..364]` via 5-day block linear interpolation of `laggedKappa` (length $L_\kappa = \text{stepsPerYear}$):
     For $i = 0 \dots L_\kappa - 1$, $s = 5i$, $e = \min(s + 5, 365)$, $v_0 = \text{laggedKappa}[i]$, $v_1 = \text{laggedKappa}[(i+1) \bmod L_\kappa]$:
     for $d = s \dots e - 1$: $\text{humanKappa}[d] = \big(1 - \frac{d-s}{5}\big)v_0 + \frac{d-s}{5}v_1$.
     *(Note: when `interval == 1`, $L_\kappa = 365$, so only $i = 0 \dots 72$ have $s < 365$, interpolating `laggedKappa[0..73]` over 5-day blocks—preserve if matching C++ verbatim!)*
   - $K_{vi}[k, 0] = \text{humanKappa}[k]$ for $k = 0 \dots 364$, $K_{vi}[k, i > 0] = 0$.
2. **State-Space Evolution Matrices $\Upsilon[k] \in \mathbb{R}^{\eta \times \eta}$ (`CalcUpsilon`)**:
   - Order: $m_t = \theta_s + \tau - 1$, $\eta = 2 m_t + \tau$.
   - Scalar probabilities: $\Lambda_{\text{tot}} = \sum_{i=0}^{n-1} \alpha_i N_i + \mu_{vA}$, $P_A = e^{-\Lambda_{\text{tot}}\theta_d}$, $w_i = (1 - P_A)\frac{\alpha_i N_i}{\Lambda_{\text{tot}}} P_{B,i} P_{C,i} P_{D,i} P_{E,i}$, $P_{df} = \sum_{i=0}^{n-1} w_i$.
   - Daily infection probabilities ($k = 0 \dots 364$): $P_{\text{dif}}[k] = \text{clamp}\!\big(\sum_{i=0}^{m-1} w_i K_{vi}[k, i],\, 0,\, P_{df}\big)$, $P_{\text{duf}}[k] = P_{df} - P_{\text{dif}}[k]$.
   - **Binomial EIP survival (`CalcPSTS`)**:
     - $k_+ = \lfloor \theta_s / \tau \rfloor - 1$:
       $$S_{k_+} = \sum_{j=0}^{k_+} \binom{\theta_s - (j+1)\tau + j}{j} P_A^{\,\theta_s - (j+1)\tau} P_{df}^{\,j}$$
     - For $l = 1 \dots \tau - 1$, let $k_{l+} = \lfloor (\theta_s + l)/\tau \rfloor - 2$:
       $$S_{kl+}[l - 1] = \sum_{j=0}^{k_{l+}} \binom{\theta_s + l - (j+2)\tau + j}{j} P_A^{\,\theta_s + l - (j+2)\tau} P_{df}^{\,j+1}$$
   - **Entries of $\Upsilon[k]$** ($k = 0 \dots 364$, zero-initialized):
     - Shift subdiagonal: for all $r \in \{0, \dots, \eta-1\} \setminus \{0, m_t, 2m_t\}$, set $\Upsilon[k]_{r,\, r-1} = 1$.
     - Row $0$ ($N_v$): $\Upsilon[k]_{0, 0} = P_A$, $\Upsilon[k]_{0,\, \tau - 1} \mathrel{+}= P_{df}$ *(accumulates onto $(0,0)$ if $\tau = 1$!)*.
     - Row $m_t$ ($O_v$): let $k_\tau = (k + \theta_p - \tau) \bmod \theta_p$:
       $\Upsilon[k]_{m_t,\, \tau - 1} = P_{\text{dif}}[k_\tau]$, $\Upsilon[k]_{m_t,\, m_t} = P_A$, $\Upsilon[k]_{m_t,\, m_t + \tau - 1} \mathrel{+}= P_{\text{duf}}[k_\tau]$.
     - Row $2 m_t$ ($S_v$): let $k_s = (k + \theta_p - \theta_s) \bmod \theta_p$:
       - $\Upsilon[k]_{2m_t,\, \theta_s - 1} = P_{\text{dif}}[k_s] S_{k_+}$, $\quad \Upsilon[k]_{2m_t,\, m_t + \theta_s - 1} = -P_{\text{dif}}[k_s] S_{k_+}$.
       - For $l = 1 \dots \tau - 1$, let $k_{sl} = (k + \theta_p - \theta_s - l) \bmod \theta_p$, $c_l = P_{\text{dif}}[k_{sl}] S_{kl+}[l - 1]$:
         $\Upsilon[k]_{2m_t,\, \theta_s + l - 1} = c_l$, $\quad \Upsilon[k]_{2m_t,\, m_t + \theta_s + l - 1} = -c_l$.
       - $\Upsilon[k]_{2m_t,\, 2m_t} = P_A$, $\quad \Upsilon[k]_{2m_t,\, 2m_t + \tau - 1} \mathrel{+}= P_{df}$.
3. **Periodic Orbit & Solving for $N_{v0}[0..364]$**:
   - Monodromy matrix: $X_{\theta_p} = \Upsilon[\theta_p - 1] \Upsilon[\theta_p - 2] \cdots \Upsilon[0]$.
   - Check spectral radius $\rho(X_{\theta_p}) < 1.0$; else throw `VectorFitting`.
   - Compute $M = (I - X_{\theta_p})^{-1}$.
   - Given candidate $N_{v0} \in \mathbb{R}^{365}$, let $\Lambda[t] = N_{v0}[t] \mathbf{e}_0 \in \mathbb{R}^\eta$:
     $$x_p[0] = M \sum_{s=0}^{\theta_p - 1} \Big(\prod_{j=s+1}^{\theta_p - 1} \Upsilon[j]\Big) \Lambda[s], \qquad x_p[t] = \Upsilon[t - 1] x_p[t - 1] + \Lambda[t - 1] \quad (t = 1 \dots \theta_p - 1)$$
     Extract $S_v(N_{v0})[t] = x_p[t]_{2 m_t}$.
   - Note $N_{v0} \mapsto S_v(N_{v0})$ is **strictly linear**! C++ solves $S_v(N_{v0}) - \text{forcedS\_v} = 0$ via `gsl_multiroot_fsolver_hybrids` (`EpsAbs = 1e-7`, `maxiter = 1000`, starting from `m.mosqEmergeRate`) or equivalently via the exact analytic $365 \times 365$ linear system $J \cdot N_{v0} = \text{forcedS\_v}$ (`CalcSvJacobian` + LU decomposition).
   - Check $N_{v0}[t] \ge 0.0$ for all $t = 0 \dots 364$; if any $< 0$, throw `VectorFitting`. Return `false` (converges in 1 step).

---

## 2. `crates/om-sim`: Time, Demography, Main Loop & Checkpointing

### 2.1 `SimTime`, Date Parsing & Phase Architecture (`sim.*`, `UnitParse.*`, `main.cpp`)
- `SimTime = i32` (days). `DAYS_IN_YEAR = 365`, `never() = -0x3FFFFFFF`, `future() = 0x3FFFFFFF`.
- `interval` (`oneTS()`): `5` days if `modelName == "base"`, overridden by `<parameters interval="..."/>` (`1` or `5`).
- `stepsPerYear = 365 / interval` (`73` or `365`).
- Exact conversions:
  - `inYears(d) = d / 365.0`, `inSteps(d) = d / interval`.
  - `fromYearsI(y) = 365 * y`, `fromYearsD(y) = interval * int(floor(stepsPerYear * y))`.
  - `roundToTSFromDays(d) = interval * int(floor(d / interval + 0.5))`, `fromYearsN(y) = roundToTSFromDays(365.0 * y)`.
  - `mod(a, b)`: `r = a % b; r >= 0 ? r : r + b`.
  - Date string `YYYY-MM-DD`: require `year >= 0`, `month in 1..12`, `day in 1..monthLen[month-1]` (`monthLen = [31,28,31,30,31,30,31,31,30,31,30,31]`, `monthStart = [0,31,59,90,120,151,181,212,243,273,304,334,365]`). Returns `365 * year + roundToTSFromDays(monthStart[month - 1] + day - 1)`. Relative dates (`steps`/`days`/`years`) add `s_start`.
- **`humanWarmupLength`**:
  $$L_{\text{warm}} = \text{maxHumanAge}, \quad \text{if } \text{interventionMode} \ne \text{forcedEIR}: L_{\text{warm}} = \max(L_{\text{warm}}, 365 \times 55)$$
  $$L_{\text{warm}} \leftarrow 365 \times \lceil L_{\text{warm}} / 365.0 \rceil$$
- **Simulation Phases**:
  1. **Warmup**: `s_t0 = s_t1 = 0`, `s_interv = never()`, `endTime = humanWarmupLength`, `estEndTime = humanWarmupLength + (s_end - s_start) + oneTS()`, call `run(...)`.
  2. **EIR Calibration**: `iterate = transmission.initIterate()`; while `iterate > 0`: `endTime += iterate`, `estEndTime = endTime + (s_end - s_start) + oneTS()`, call `run(...)`, `iterate = transmission.initIterate()`.
  3. **Transition to Main Phase**:
     - `estEndTime = endTime = endTime + (s_end - s_start) + oneTS()`
     - `sim::s_interv = 0` (so `intervTime() == 0` and `intervDate() == s_start`)
     - `InfectionIncidenceModel::preMainSimInit()` (`ctsNewInfections = 0`)
     - `InfantMortality::preMainSimInit()`
     - `Genotypes::preMainSimInit()` (`current_mode = interv_mode`)
     - `population.resetRecentBirths()`
     - `transmission.summarize()` (resets `survey*EIR` and sets `lastSurveyTime = sim::now()`)
     - `mon::initMainSim()` (`surveyIndex = 0`, `isInit = true`, `updateSurveyNumbers()`)
     - Optional checkpoint write.
  4. **Main Phase ("Intervention period")**:
     - Call `run(...)`.
     - For each human in `population.humans`: `human.clinicalModel->flushReports()`.
     - `mon::writeSurveyData()`.

#### Exact Per-Time-Step Order Inside `run()` (`while sim::now() < endTime`)
1. `mon::Continuous::update(population)` *(at `sim::now() == s_t0`, `in_update == false`)*.
2. **Survey check**: if `sim::intervDate() == mon::nextSurveyDate()`:
   - For each `human` in `population.humans`: `Host::summarize(human, surveyOnlyNewEp)`.
   - `transmission.summarize()`.
   - `mon::concludeSurvey()`.
3. **Interventions**: `InterventionManager::deploy(population.humans, transmission)` *(returns immediately if `sim::intervTime() < 0`; else imports infections, runs `timed` deployments with `date <= intervDate()`, then per-human `continuous` deployments)*.
4. `sim::start_update()` (`s_t1 += oneTS(); in_update = true`, so `ts0() = s_t0`, `ts1() = s_t0 + oneTS()`).
5. `transmission.vectorUpdate(population.humans)`.
6. `Host::NeonatalMortality::update(population.humans)`.
7. **Human updates**: for each `human` in `population.humans`:
   - **Warmup skip optimization**: if `human.getDOB() + sim::maxHumanAge() >= humanWarmupLength`, call `Host::update(human, transmission)`.
8. `population.update()` *(removes dead/outmigrated humans, births replacements at `DOB = sim::ts1()`)*.
9. `transmission.updateKappa(population.humans)`.
10. `transmission.surveyEIR()`.
11. `sim::end_update()` (`in_update = false; s_t0 = s_t1; s_interv += oneTS()`).

---

### 2.2 `PopulationAgeStructure` Curve Fitting & `Population` Dynamics (`PopulationAgeStructure.*`, `Population.*`)

#### Step 1: Age Group Array Construction (`estimateRemovalRates`)
Given `<demography maximumAgeYrs="...">` with $K$ `<group upperbound="u_k" poppercent="p_k"/>` elements ($k = 0 \dots K-1$, where $u_0 = 1.0$ year):
- `s_max_human_age = sim::fromYearsD(maximumAgeYrs)`.
- `ngroups = K + 1`.
- `ageGroupBounds` (length `ngroups + 1`):
  - `ageGroupBounds[0] = 0.0`, `ageGroupBounds[1] = 1.0 / 12.0`, and `ageGroupBounds[k + 2] = u_k` for $k = 0 \dots K - 1$.
- `ageGroupPercent` (length `ngroups`):
  - `ageGroupPercent[0] = 0.0`, `ageGroupPercent[k + 1] = p_k` for $k = 0 \dots K - 1$.
  - Normalize: let $S_p = \sum_{i=0}^{\text{ngroups}-1} \text{ageGroupPercent}[i]$; multiply every entry by $100.0 / S_p$.

#### Step 2: Nelder-Mead Curve Fitting (`setDemoParameters` & `minimizeCalc_rss`)
- Growth rate constant $\rho = 0.0$ (`growthRate` XML attribute is rejected if present).
- For candidate parameters $(\theta_1, \theta_2)$:
  1. Constants: $\text{IMR} = 0.1$, $M_{\text{inf}} = -\ln(1 - \text{IMR}) = -\ln(0.9)$, $\alpha_0 = 4.0$.
  2. $\mu_1 = \frac{e^{\theta_1}}{100.0}, \qquad \alpha_1 = \frac{e^{\theta_2}}{100.0}$.
  3. **Exact C++ $\mu_0$ formula** (preserve exact parenthesization/factors):
     $$\mu_0 = \frac{M_{\text{inf}} - \mu_1 (e^{0.5 \alpha_1} - 1)\alpha_0}{\alpha_1 (1 - e^{-0.5 \alpha_0})}$$
  4. For $i = 0 \dots \text{ngroups} - 2$ *(NOTE: excludes final group $i = \text{ngroups} - 1$!)*:
     - $m_i = \frac{1}{2}(\text{ageGroupBounds}[i+1] + \text{ageGroupBounds}[i])$
     - $M_1(m_i) = \frac{\mu_0 (1 - e^{-\alpha_0 m_i})}{\alpha_0}, \qquad M_2(m_i) = \frac{\mu_1 (e^{\alpha_1 m_i} - 1)}{\alpha_1}, \qquad M[i] = M_1(m_i) + M_2(m_i)$
     - $\text{pred}[i] = (\text{ageGroupBounds}[i+1] - \text{ageGroupBounds}[i]) \cdot e^{-\rho m_i - M[i]}$
  5. Normalize `pred[0 .. ngroups-2]` so $\sum_{i=0}^{\text{ngroups}-2} \text{pred}[i] = 100.0$.
  6. **Dynamic split of infant percentage** between `[0, 1/12]` ($i=0$) and `[1/12, 1]` ($i=1$):
     $$L_{\text{inf}} = e^{-0.5\rho - M[1]}, \qquad M_{nn} = -\ln\!\big(1 - 0.4(1 - e^{-M[1]})\big), \qquad L_1 = \frac{1}{12} e^{-\rho/24 - M_{nn}}$$
     $$p_{\text{inf}} = \text{ageGroupPercent}[0] + \text{ageGroupPercent}[1]$$
     $$\text{ageGroupPercent}[0] = p_{\text{inf}} \frac{L_1}{L_{\text{inf}}}, \qquad \text{ageGroupPercent}[1] = p_{\text{inf}} - \text{ageGroupPercent}[0]$$
  7. Residual sum of squares:
     $$\text{RSS}(\theta_1, \theta_2) = \sum_{i=0}^{\text{ngroups}-2} \big(\ln(\text{pred}[i]) - \ln(\text{ageGroupPercent}[i])\big)^2$$
- **Optimizer**: GSL `gsl_multimin_fminimizer_nmsimplex` (original Nelder-Mead simplex, where simplex size is mean Euclidean distance from vertices to centroid—NOT `nmsimplex2` which uses RMS!), starting at $(\theta_1, \theta_2) = (0.371626412, 0.841209593)$ with initial step vector $(0.1, 0.1)$ (so initial simplex vertices are $x_0$, $x_0 + (0.1, 0)$, $x_0 + (0, 0.1)$).
  - Iterate up to `iter < 100`, stopping early when `gsl_multimin_test_size(size, 1e-2) == GSL_SUCCESS` (`size < 1e-2`).
  - Call `setDemoParameters(best_theta1, best_theta2)` once more at termination to finalize $(\mu_0, \alpha_0, \mu_1, \alpha_1)$.

#### Step 3: Cumulative Age Curve (`calcCumAgeProp` & `targetCumPop`)
- Let $N_{\text{steps}} = \text{sim::inSteps}(\text{sim::maxHumanAge}())$, array length $N_{\text{cells}} = N_{\text{steps}} + 1$.
- `cumAgeProp[0] = 0.0`.
- For $j = 1 \dots N_{\text{cells}} - 1$:
  - $\text{ageDays} = \text{sim::fromTS}(N_{\text{cells}} - j - 1)$, $\quad a = \text{ageDays} / 365.0$.
  - $M_s = \frac{\mu_0 (1 - e^{-\alpha_0 a})}{\alpha_0} + \frac{\mu_1 (e^{\alpha_1 a} - 1)}{\alpha_1}$.
  - $\text{predperc} = (\text{ageDays} \ge \text{maxHumanAge}) \mathbin{?} 0.0 : e^{-\rho a - M_s}$.
  - `cumAgeProp[j] = cumAgeProp[j - 1] + predperc`.
- Normalize: divide `cumAgeProp[1 .. N_cells - 1]` by `cumAgeProp[N_cells - 1]` (so `cumAgeProp[N_cells - 1] == 1.0` and `cumAgeProp[0] == 0.0`).
- **`targetCumPop(ageTSteps, targetPop)`**:
  $$\text{index} = N_{\text{cells}} - 1 - \text{ageTSteps}, \qquad \text{return } \lfloor \text{cumAgeProp}[\text{index}] \times \text{targetPop} + 0.5 \rfloor$$
  *(At `ageTSteps == N_steps`, `index == 0`, returning `0`; at `ageTSteps == 0`, `index == N_cells - 1`, returning `targetPop`.)*

#### Step 4: `t = 0` Sampling & Per-Step `Population::update()`
- **`Population` invariant**: `humans` has fixed target size $N_{\text{pop}}$ and is **strictly sorted oldest-to-youngest** (ascending `dateOfBirth`).
- **`createInitialHumans()` (at $t = 0$)**:
  - `cumulativePop = 0`.
  - Loop `iage` from $N_{\text{steps}}$ down to $0$ inclusive:
    - `targetPop = targetCumPop(iage, size)`
    - `while (cumulativePop < targetPop)`: push `Human(dob = -fromTS(iage))`, `++cumulativePop`.
- **`Population::update()` (each time step during `in_update`)**:
  - `cumPop = 0`.
  - Scan `humans` from front (oldest) to back (youngest):
    - `isDead = human.isDead()` (set inside `Host::update` if `clinicalModel->isDead(age(ts0()))`, which also triggers if `age(ts0()) >= maxHumanAge()`).
    - `outmigrate = (cumPop >= targetCumPop(sim::inSteps(human.age(sim::ts1())), size))`.
    - If `isDead || outmigrate`: remove `human` (preserving order).
    - Else: `++cumPop`.
  - Replacement births:
    - `recentBirths += (size - cumPop)`.
    - `while (cumPop < size)`: push `Human(dob = sim::ts1())` at the back, `++cumPop`.

#### Checkpointing (`checkpoint.cpp`, `util/checkpoint.*`)
- Alternates `<name>0.gz` / `<name>1.gz` (`zlib` gzip stream) with `<name>` holding ASCII `"0"` or `"1"`.
- Header (`header`): little-endian `u32 = 0x50434D4F` (`"OMCP"`), `u8 = 1` (`bool true`), `u8 = 0xA5`, `f64 = -0.0`, `f64 = quiet_NaN` (checked bitwise via `memcmp` on read).
- Containers prefix `usize` (`u64` on 64-bit) length (`validateListSize(len, max=2000)` on generic containers; `Population` writes `size`, `recentBirths`, then `size` humans without the 2000 cap).

---

## 3. `crates/om-monitoring` Specification

### 3.1 Complete `OutMeasures.h` Table (`Measure` Index `0..65`)

Flags: `A = Age (1)`, `C = Cohort (2)`, `S = Species (4)`, `G = Genotype (8)`, `D = Drug (16)`.
Recording API:
- **Stat**: `recordStat` during `Host::summarize` or `TransmissionModel::summarize` / `AnophelesModel::summarize` (written to `statSurveyNumber()`, i.e. point-in-time or inter-survey model accumulator at survey time).
- **Event**: `recordEvent` / `recordDeploy` during time-step updates (accumulated into `eventSurveyNumber()`, the next *reported* survey $\ge \text{surveyIndex}$; note some expected-probability clinical measures like `expectedDirectDeaths` call `recordStat` during `doClinicalUpdate`? **WAIT — IMPORTANT**: check `EventScheduler.cpp` / `ImmediateOutcomes.cpp`: `expectedDirectDeaths`, `expectedHospitalDeaths`, `expectedIndirectDeaths`, `expectedSequelae`, `expectedSevere`, `expectedSevereWithoutComorbidities`, `innoculationsPerAgeGroup`, `innoculationsPerVector` actually call `mon::recordStat(...)` or `mon::recordEvent(...)`? In `EventScheduler.cpp` lines 177, 286, 288, 373, 375 and `VectorModel.cpp` lines 489-490 / `NonVectorModel.h` lines 234-235, they call **`mon::recordStat(...)`** during regular time steps! Because `statSurveyNumber()` equals `surveyDates[surveyIndex].num` **throughout the entire interval** while `isInit == true` (until `concludeSurvey()` increments `surveyIndex`), `recordStat` and `recordEvent` accumulate into the **exact same** reported survey when `surveyDates[surveyIndex]` is reported, and **only differ** when `surveyDates[surveyIndex]` has `reported="false"`: `statSurveyNumber()` is `NOT_USED` (dropping values during an unreported survey period), whereas `eventSurveyNumber()` looks ahead to the next *reported* survey!).

| `Measure` idx | XML `name` | `outId` | Type | Dims | Valid Cond? | Semantics & Reset Notes |
|---:|---|---:|---|---|:---:|---|
| 0 | `nHost` | `0` | `int` | `A\|C` | Yes | Survey stat: `+1` per human (skips existing cases if `surveyOnlyNewEp`) |
| 1 | `nInfect` | `1` | `int` | `A\|C` | Yes | Survey stat: `+1` if human has $\ge 1$ infection |
| 2 | `nInfect_Imported` | `1001` | `int` | `A\|C` | Yes | Survey stat: `+1` if infected & host origin == `Imported` |
| 3 | `nInfect_Introduced` | `2001` | `int` | `A\|C` | Yes | Survey stat: `+1` if infected & host origin == `Introduced` |
| 4 | `nInfect_Indigenous` | `3001` | `int` | `A\|C` | Yes | Survey stat: `+1` if infected & host origin == `Indigenous` |
| 5 | `nExpectd` | `2` | `double` | `A\|C` | Yes | Survey stat: sums `m_pInfected` across hosts. **C++ note**: `m_pInfected` is updated each step as $1 - e^{-h}(1 - m_{\text{pInfected}})$ and is **never reset** after birth! |
| 6 | `nPatent` | `3` | `int` | `A\|C` | Yes | Survey stat: `+1` if `monitoringDiagnostic.isPositive(totalDensity)` |
| 7 | `nPatent_Imported` | `1003` | `int` | `A\|C` | Yes | Survey stat: patent & host origin == `Imported` |
| 8 | `nPatent_Introduced` | `2003` | `int` | `A\|C` | Yes | Survey stat: patent & host origin == `Introduced` |
| 9 | `nPatent_Indigenous` | `3003` | `int` | `A\|C` | Yes | Survey stat: patent & host origin == `Indigenous` |
| 10 | `sumLogPyrogenThres` | `4` | `double` | `A\|C` | Yes | Survey stat: $\sum \ln(1 + Y^*)$ across hosts |
| 11 | `sumlogDens` | `5` | `double` | `A\|C` | Yes | Survey stat: $\sum \ln(\text{totalDensity})$ across patent hosts (error if diagnostic allows false positives; `Descriptive` requires `totalDensity > 1e-10`, `Common` records `0.0` if `0.0`) |
| 12 | `totalInfs` | `6` | `int` | `A\|C\|G` | Yes | Survey stat: total infections/broods (blood + liver) |
| 13 | `totalInfs_Imported` | `1006` | `int` | `A\|C\|G` | Yes | Survey stat: `Imported` infections |
| 14 | `totalInfs_Introduced` | `2006` | `int` | `A\|C\|G` | Yes | Survey stat: `Introduced` infections |
| 15 | `totalInfs_Indigenous` | `3006` | `int` | `A\|C\|G` | Yes | Survey stat: `Indigenous` infections |
| 16 | `nTransmit` | `7` | `double` | `None` | Yes | Survey stat: `laggedKappa[moduloSteps(now(), laggedKappa.size())]` |
| 17 | `totalPatentInf` | `8` | `int` | `A\|C\|G` | Yes | Survey stat: count of individual infections positive on monitoring diagnostic |
| 18 | `totalPatentInf_Imported` | `1008` | `int` | `A\|C\|G` | Yes | Survey stat: patent individual `Imported` infections |
| 19 | `totalPatentInf_Introduced` | `2008` | `int` | `A\|C\|G` | Yes | Survey stat: patent individual `Introduced` infections |
| 20 | `totalPatentInf_Indigenous` | `3008` | `int` | `A\|C\|G` | Yes | Survey stat: patent individual `Indigenous` infections |
| 21 | `sumPyrogenThresh` | `10` | `double` | `A\|C` | Yes | Survey stat: $\sum Y^*$ across hosts |
| 22 | `nTreatments1` | `11` | `int` | `A\|C` | Yes | Event (`recordEvent`): 1st-line blood-stage treatments |
| 23 | `nTreatments2` | `12` | `int` | `A\|C` | Yes | Event (`recordEvent`): 2nd-line blood-stage treatments |
| 24 | `nTreatments3` | `13` | `int` | `A\|C` | Yes | Event (`recordEvent`): inpatient/complicated treatments |
| 25 | `nUncomp` | `14` | `int` | `A\|C` | **No** | Event (`Episode` report): uncomplicated malaria episodes |
| 26 | `nUncomp_Imported` | `1014` | `int` | `A\|C` | Yes | Event (`Episode` report): uncomplicated `Imported` episodes |
| 27 | `nUncomp_Introduced` | `2014` | `int` | `A\|C` | Yes | Event (`Episode` report): uncomplicated `Introduced` episodes |
| 28 | `nUncomp_Indigenous` | `3014` | `int` | `A\|C` | Yes | Event (`Episode` report): uncomplicated `Indigenous` episodes |
| 29 | `nSevere` | `15` | `int` | `A\|C` | **No** | Event (`Episode` report): severe episodes (incl. coinfection) |
| 30 | `nSeq` | `16` | `int` | `A\|C` | **No** | Event (`Episode` report): sequelae cases |
| 31 | `nHospitalDeaths` | `17` | `int` | `A\|C` | **No** | Event (`Episode` report): direct deaths in hospital |
| 32 | `nIndDeaths` | `18` | `int` | `A\|C` | Yes | Event (`recordEvent`): indirect malaria + neonatal deaths |
| 33 | `nDirDeaths` | `19` | `int` | `A\|C` | **No** | Event (`Episode` report): direct malaria deaths |
| 34 | `nEPIVaccinations` | `20` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): doses of first defined vaccine component |
| 35 | `allCauseIMR` | `21` | `double` | `None` | **No** | Special post-sim row `(1, 1, outId, InfantMortality::allCause())` |
| 36 | `nMassVaccinations` | `22` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): mass doses of first defined vaccine |
| 37 | `nHospitalRecovs` | `23` | `int` | `A\|C` | **No** | Event (`Episode` report): hospital recoveries |
| 38 | `nHospitalSeqs` | `24` | `int` | `A\|C` | **No** | Event (`Episode` report): hospital sequelae |
| 39 | `annAvgK` | `26` | `double` | `None` | Yes | Survey stat: `_annualAverageKappa` |
| 40 | `nNMFever` | `27` | `int` | `A\|C` | **No** | Event (`Episode` report): non-malaria fever episodes |
| 41 | `innoculationsPerAgeGroup` | `30` | `double` | `A\|C\|G` | **No** | Step stat (`recordStat`): per-step EIR summed over steps in survey |
| 42 | `Vector_Nv0` | `31` | `double` | `S` | Yes | Survey stat: `timeStep_N_v0 / oneTS()` (mean daily emergence on last TS) |
| 43 | `Vector_Nv` | `32` | `double` | `S` | Yes | Survey stat: mean of `N_v` over last `oneTS()` days |
| 44 | `Vector_Ov` | `33` | `double` | `S\|G` | Yes | Survey stat: mean of `O_v_i + O_v_l` over last `oneTS()` days |
| 45 | `Vector_Sv` | `34` | `double` | `S\|G` | Yes | Survey stat: mean of `S_v_i + S_v_l` over last `oneTS()` days |
| 46 | `inputEIR` | `35` | `double` | `None` | **No** | Survey stat (if `isReported`): `surveyInputEIR / durationSteps`, then reset |
| 47 | `simulatedEIR` | `36` | `double` | `None` | **No** | Survey stat (if `isReported`): `surveySimulatedEIR / durationSteps` (adults $\ge \text{adultAge}$), then reset |
| 48 | `simulatedEIR_Introduced` | `2036` | `double` | `None` | Yes | Survey stat (if `isReported`): `surveySimulatedEIR_i / durationSteps`, then reset |
| 49 | `simulatedEIR_Indigenous` | `3036` | `double` | `None` | Yes | Survey stat (if `isReported`): `surveySimulatedEIR_l / durationSteps`, then reset |
| 50 | `Clinical_FirstDayDeaths` | `41` | `int` | `A\|C` | **No** | Event (`Episode` report): 1st-day direct deaths |
| 51 | `Clinical_HospitalFirstDayDeaths` | `42` | `int` | `A\|C` | **No** | Event (`Episode` report): 1st-day hospital direct deaths |
| 52 | `nNewInfections` | `43` | `int` | `A\|C` | Yes | Event (`recordEvent`): `nNewInfs_i + nNewInfs_l` |
| 53 | `nNewInfections_Imported` | `1043` | `int` | `A\|C` | Yes | Event (`recordEvent`): `+1` per imported infection |
| 54 | `nNewInfections_Introduced` | `2043` | `int` | `A\|C` | Yes | Event (`recordEvent`): `nNewInfs_i` |
| 55 | `nNewInfections_Indigenous` | `3043` | `int` | `A\|C` | Yes | Event (`recordEvent`): `nNewInfs_l` |
| 56 | `nMassITNs` | `44` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): ITN deployments |
| 57 | `nEPI_ITNs` | `45` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): ITN deployments |
| 58 | `nMassIRS` | `46` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): IRS deployments |
| 59 | `nMDAs` | `52` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): TreatSimple / TreatPKPD deployments |
| 60 | `nNmfDeaths` | `53` | `int` | `A\|C` | **No** | Event (`Episode` report): NMF deaths |
| 61 | `nMassScreenings` | `55` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): Screen component deployments |
| 62 | `nMassGVI` | `56` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): GVI deployments |
| 63 | `nCtsIRS` | `57` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): IRS deployments |
| 64 | `nCtsGVI` | `58` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): GVI deployments |
| 65 | `nCtsMDA` | `59` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): TreatSimple / TreatPKPD deployments |
| 66* | `nCtsScreenings` | `60` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): Screen component deployments |
| 67* | `nSubPopRemovalTooOld` | `61` | `int` | `A\|C` | Yes | Event (`recordEvent`): cohort expiry removals |
| 68* | `nSubPopRemovalFirstEvent` | `62` | `int` | `A\|C` | **No** | Event (`recordEvent`): cohort `onFirstBout`/`Treatment`/`Infection` removals |
| 69* | `nLiverStageTreatments` | `63` | `int` | `A\|C` | Yes | Event (`recordEvent`): liver-stage treatments (Primaquine) |
| 70* | `nTreatDiagnostics` | `64` | `int` | `A\|C` | Yes | Event (`recordEvent`): CM diagnostics used |
| 71* | `nMassRecruitOnly` | `65` | `int` | `A\|C` | Yes | Event (`recordDeploy` TIMED): RecruitOnly deployments |
| 72* | `nCtsRecruitOnly` | `66` | `int` | `A\|C` | Yes | Event (`recordDeploy` CTS): RecruitOnly deployments |
| 73* | `nTreatDeployments` | `67` | `int` | `A\|C` | Yes | Event (`recordDeploy` TREAT): CM-triggered component deployments |
| 74* | `sumAge` | `68` | `double` | `A\|C` | Yes | Survey stat: $\sum \text{inYears}(\text{age}(\text{now}))$ |
| 75* | `nInfectByGenotype` | `69` | `int` | `A\|C\|G` | Yes | Survey stat: `+1` per host carrying $\ge 1$ infection of genotype $g$ |
| 76* | `nPatentByGenotype` | `70` | `int` | `A\|C\|G` | Yes | Survey stat: `+1` per host whose summed density of genotype $g$ is diagnostic-positive |
| 77* | `logDensByGenotype` | `71` | `double` | `A\|C\|G` | Yes | Survey stat: $\ln(\text{dens}_g)$ for hosts positive for genotype $g$ |
| 78* | `nHostDrugConcNonZero` | `72` | `int` | `A\|C\|D` | Yes | Survey stat: `+1` if host drug $d$ conc $> \text{negligible\_concentration}$ |
| 79* | `sumLogDrugConcNonZero` | `73` | `double` | `A\|C\|D` | Yes | Survey stat: $\ln(\text{conc}_d)$ if host drug $d$ conc $> \text{negligible\_concentration}$ |
| 80* | `expectedDirectDeaths` | `74` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{directDeath}}$ over severe bouts |
| 81* | `expectedHospitalDeaths` | `75` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{hospDeath}}$ over hospital severe bouts |
| 82* | `expectedIndirectDeaths` | `76` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{indDeath}}$ over non-doomed bouts |
| 83* | `expectedSequelae` | `77` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{seq}}$ |
| 84* | `expectedSevere` | `78` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{severe}}$ (incl. coinfection) |
| 85* | `innoculationsPerVector` | `79` | `double` | `S` | **No** | Step stat (`recordStat`): per-step host EIR summed by species |
| 86* | `nCMDTReport` | `80` | `int` | `A\|C` | Yes | Event (`recordEvent`, optional `outId` filter): custom CM decision-tree report |
| 87* | `nSevereWithoutComorbidities` | `81` | `int` | `A\|C` | **No** | Event (`Episode` report): severe episodes excluding comorbidity-only |
| 88* | `expectedSevereWithoutComorbidities` | `82` | `double` | `A\|C` | Yes | Step stat (`recordStat`): $\sum p_{\text{severeNoComorb}}$ |
| 89* | `nNMFTreatments` | `-1` | `int` | `A\|C` | Yes | Internal (`outId = -1`, rejected by `findOutMeasure` since `outId < 0`!) |

*(Note: Total rows in `defs[]` = **89** (`Measure` indices `0..88`; rows labelled `66*..89*` above are `defs[65..88]`). Obsolete names throwing `"obsolete survey option: <name>"`: `contrib`, `nIPTDoses`, `innoculationsPerDayOfYear`, `kappaPerDayOfYear`, `Clinical_RDTs`, `Clinical_DrugUsage`, `nMassVA`, `Clinical_Microscopy`, `Clinical_DrugUsageIV`, `nAddedToCohort`, `nRemovedFromCohort`, `nAntibioticTreatments`.)*

---

### 3.2 Survey Indexing, Dimensions, `col2` Encoding & Output Formats (`mon/init.cpp`, `mon/Monitoring.cpp`)

1. **Survey Dates (`readSurveyDates`)**:
   - Parse `<surveyTime reported="true|false" [repeatStep="..." repeatEnd="..."]>date</surveyTime>`:
     - Trim whitespace, `cur = UnitParse::readDate(s, STEPS)`.
     - If `repeatStep` & `repeatEnd` present (both or neither required): `step = readDuration(repeatStep, NONE)` ($\ge \text{oneTS}$), `end = readDate(repeatEnd, NONE)`; loop `while (cur < end) { addSurvey(cur, reporting); cur += step; }`.
     - Deduplicate & sort ascending in `map<SimTime, bool>` where `reporting == true` overrides `false`.
     - Build `surveyDates`: if `reporting`, assign `num = nSurveys++` (`0 .. nSurveys - 1`), else `num = NOT_USED`.
     - Return `surveyDates.back().date` (`sim::s_end`).
   - **Active Survey Indices**:
     - Before `initMainSim()`: `isInit = false`, `statSurveyNumber() = NOT_USED`, `eventSurveyNumber() = NOT_USED`, `nextSurveyDate() = future()`.
     - At `initMainSim()` and after each `concludeSurvey()`: `survNumEvent` is set to the first `surveyDates[i].num != NOT_USED` for $i \ge \text{surveyIndex}$ (or `NOT_USED` if none remain).
     - `concludeSurvey()`: first updates deployment conditions (`for cond: val = surveySum(cond.measure, statSurveyNumber()); cond.value = (val >= cond.min && val <= cond.max)` if `statSurveyNumber() != NOT_USED`), then `++surveyIndex` and `updateSurveyNumbers()`.

2. **Measure Stores & Dimension Overrides (`initReporting`)**:
   - Each XML `<option name="..." value="true"/>` looks up `findOutMeasure(name)`. Optional XML attributes `byAge`, `byCohort`, `bySpecies`, `byGenotype`, `byDrugType` can **disable** (`false`) a supported `Dim` flag (setting `true` on an unsupported flag throws `xml_scenario_error`). Optional `outputNumber` overrides `outId` (must be $\ge 0$ and unique across all enabled options).
   - If `om.measure == allCauseIMR`: sets `runtime.reportIMR = om.outId` and does NOT add a `MeasureStore`.
   - Remaining enabled `OutMeasure`s are **sorted ascending by `outId`**.
   - If a deployment `Condition` uses a measure not in `storesByMeasure`, `ensureConditionStore` adds a hidden store with `forceNoCategories = true` (`outId = -1`, all dims = `1`).
   - **Age Groups**: `<ageGroup lowerbound="0.0">` ($\le 0$ required) with $K$ `<group upperbound="u_i"/>` entries creates `ageGroupUpperBound` of size $K + 1$: `[fromYearsD(u_0), ..., fromYearsD(u_{K-1}), sim::future()]`. Human `monitoringAgeGroup` starts at `0` and advances `while (age(ts0()) >= ageGroupUpperBound[idx]) ++idx`.
     - Store `nAges = (dims & Age) ? (K + 1) : 1`.
     - **CRITICAL**: `outputAgeGroups(store) = (nAges == 1) ? 1 : (nAges - 1)` ($= K$). The overflow bin `idx == K` (`age >= u_{K-1}`) IS allocated in `store.reports` (and included in `surveySum` for conditions!), so events/stats for humans older than $u_{K-1}$ land in bin $K$ and are **excluded** when writing `output.txt`/`output.bin`!
   - **Cohorts**: `<cohorts>` defines $M$ `<subPop id="..." number="..."/>` where each `number` must be a power of 2 in $[1, 2^{21}]$. `nCohorts = 1 << M` (`1` if `<cohorts>` absent). `cohortSetOutputId(cohortSet)` sums `cohortSubPopNumbers[b]` for each set bit $b \in \{0, \dots, M-1\}$.
   - **Store Flat Index**:
     $$\text{store.index}(a, c, sp, g, d) = (d \bmod n_D) + n_D\Big((g \bmod n_G) + n_G\big((sp \bmod n_S) + n_S((c \bmod n_C) + n_C (a \bmod n_A))\big)\Big)$$
     Survey offset: `survey * store.size() + store.index(...)`.

3. **Row Iteration Order & `col2` Formula (`writeRows` / `writeMeasure`)**:
   - Outer loops: `for survey in 0 .. nSurveys`: `for store in stores (sorted by outId, skipping outId < 0)`:
   - Inner loops (exact nesting order!):
     ```text
     for cohortSet in 0 .. store.nCohorts:
       for ageGroup in 0 .. outputAgeGroups(store):
         for species in 0 .. store.nSpecies:
           for genotype in 0 .. store.nGenotypes:
             for drug in 0 .. store.nDrugs:
     ```
   - `survey_out = survey + 1` (1-based!).
   - `col2`:
     - If `om.dims & Dim::Species`:
       $$\text{col2} = (\text{species} + 1) + 1{,}000{,}000 \times \text{genotype}$$
     - Else:
       $$\text{col2} = \text{ageGroup} + (\text{om.dims \& Age} \mathbin{?} 1 : 0) + 1{,}000 \times \text{cohortSetOutputId}(\text{cohortSet}) + 1{,}000{,}000 \times \big((\text{om.dims \& Drug}) \mathbin{?} (\text{drug} + 1) : \text{genotype}\big)$$
   - Final row (after all surveys/stores): if `reportIMR >= 0`, emit `(survey=1, col2=1, measure=reportIMR, value=InfantMortality::allCause(), isDouble=true)`.

4. **`output.txt` vs `output.bin` (`OMOUTBIN` v2)**:
   - **`output.txt`**: Tab-separated `"{survey}\t{col2}\t{outId}\t{value}\n"`. Integer values formatted as `i64`; double values formatted with default C++ `ostream` (`std::defaultfloat`, precision 6).
   - **`output.bin` (`--output-format=bin`)**: Little-endian **columnar** binary format, partitioning integer measures (`isDouble == false`) before double measures (`isDouble == true`):
     1. `magic`: `[u8; 8] = *b"OMOUTBIN"` (8 bytes)
     2. `version`: `u32 = 2` (4 bytes)
     3. `integerRows`: `u64` (8 bytes) — total rows across all surveys with `isDouble == false`
     4. `doubleRows`: `u64` (8 bytes) — total rows across all surveys with `isDouble == true` (including `+1` if `reportIMR >= 0`)
     5. `survey` column: `integerRows` × `i32`, immediately followed by `doubleRows` × `i32`
     6. `col2` column: `integerRows` × `i32`, immediately followed by `doubleRows` × `i32`
     7. `measure` (`outId`) column: `integerRows` × `i32`, immediately followed by `doubleRows` × `i32`
     8. `int_values`: `integerRows` × `i32`
     9. `double_values`: `doubleRows` × `f64` (IEEE-754)
   - If `--compress-output` (`-z`) is set, wrap the output stream in gzip (`ogzstream`).

---

### 3.3 Continuous Reporting (`ctsout.txt`, `mon/Continuous.*`)

- **Configuration**: Disabled if `<continuous>` is absent. Otherwise `ctsPeriod = UnitParse::readShortDuration(period, STEPS)` ($\ge \text{oneTS}$), `duringInit = options.duringInit` (default `false`). Enabled `<option name="..." value="true"/>` callbacks are stored in `toReport` **in the exact order listed in the XML**.
- **Header**:
  - Line 1: `"##\t##\n"`
  - Line 2: `[if duringInit: "simulation time\t"]` + `"timestep"` + concatenated `titles` of enabled callbacks + `"\n"` (flushed immediately; `streamOffset = tellp()`).
- **Update (`Continuous::update` at top of `run()` step when `now() == s_t0`)**:
  - If `!duringInit`: return unless `sim::intervTime() >= 0 && mod_nn(sim::intervTime(), ctsPeriod) == 0`. Write `sim::inSteps(sim::intervTime())`.
  - If `duringInit`: return unless `mod_nn(sim::now(), ctsPeriod) == 0`. Write `sim::inSteps(sim::now()) << "\t"` followed by `("nan" if sim::intervTime() < 0 else sim::inSteps(sim::intervTime()))`.
  - Invoke each callback in `toReport` (each writes leading `'\t'`), then write `'\n'` and flush, updating `streamOffset`.
- **Complete Registered Callbacks Table (all 31 across the codebase)**:
  *(For per-species `VectorModel` callbacks, columns iterate `i = 0 .. numSpecies - 1` in sorted species order, looking up `<name>` from `speciesIndex`, and `getLastVecStat` averages the ring buffer over the `oneTS()` days ending at `sim::now()`, i.e. indices `(sim::now() + 1 + N_v_length - oneTS() .. sim::now() + N_v_length) % N_v_length`.)*

| XML `option name` | Header `titles` | Value(s) Written (prefixed by `\t`) |
|---|---|---|
| `hosts` | `\thosts` | `population.humans.size()` |
| `host demography` | `\thost % ≤ 1\thost % ≤ 5\thost % ≤ 10\thost % ≤ 15\thost % ≤ 25` | 5 cumulative **counts** (not %!) of humans with `inYears(age(now())) < ubound` (iterating reverse from youngest) |
| `recent births` | `\trecent births` | `population.recentBirths`, then resets `recentBirths = 0` |
| `patent hosts` | `\tpatent hosts` | Count of humans positive on `monitoringDiagnostic` (uses `human.rng`!) |
| `immunity h` | `\timmunity h` | Mean `withinHostModel->getCumulative_h()` across population |
| `immunity Y` | `\timmunity Y` | Mean `withinHostModel->getCumulative_Y()` across population |
| `median immunity Y` | `\tmedian immunity Y` | Median `getCumulative_Y()` across population |
| `human age availability` | `\thuman age availability` | Mean `relativeAvailabilityAge(inYears(age(now())))` over humans with `!outsideTransmission` |
| `ITN coverage` | `\tITN coverage` | Fraction of humans with active `ITN` (`deployTime != never()`) |
| `IRS coverage` | `\tIRS coverage` | Fraction of humans with active `IRS` |
| `GVI coverage` | `\tGVI coverage` | Fraction of humans with active `GVI` |
| `new infections` | `\tnew infections` | `InfectionIncidenceModel::ctsNewInfections`, then resets to `0` |
| `input EIR` | `\tinput EIR` | `initialisationEIR[mod((now() - oneTS()) / oneTS(), stepsPerYear())]` |
| `simulated EIR` | `\tsimulated EIR` | `tsAdultEIR` (from last step's `surveyEIR()`) |
| `human infectiousness` | `\thuman infectiousness` | `laggedKappa[moduloSteps(now(), laggedKappa.size())]` |
| `num transmitting humans` | `\tnum transmitting humans` | `numTransmittingHumans` |
| `N_v0` | `\tN_v0(<sp>)` × $S$ | `species[i]->timeStep_N_v0 / oneTS()` |
| `P_A` | `\tP_A(<sp>)` × $S$ | Mean `P_A` over last `oneTS()` days |
| `P_Amu` | `\tP_Amu(<sp>)` × $S$ | Mean `P_Amu` over last `oneTS()` days |
| `P_A1` | `\tP_A1(<sp>)` × $S$ | Mean `P_A1` over last `oneTS()` days |
| `P_Ah` | `\tP_Ah(<sp>)` × $S$ | Mean `P_Ah` over last `oneTS()` days |
| `P_df` | `\tP_df(<sp>)` × $S$ | Mean `P_df` over last `oneTS()` days |
| `P_dif` | `\tP_dif(<sp>)` × $S$ | Mean $\sum_g (P_{\text{dif},i} + P_{\text{dif},l})$ over last `oneTS()` days |
| `N_v` | `\tN_v(<sp>)` × $S$ | Mean `N_v` over last `oneTS()` days |
| `O_v` | `\tO_v(<sp>)` × $S$ | Mean $\sum_g (O_{v,i} + O_{v,l})$ over last `oneTS()` days |
| `S_v` | `\tS_v(<sp>)` × $S$ | Mean $\sum_g (S_{v,i} + S_{v,l})$ over last `oneTS()` days |
| `alpha` | `\talpha_i(<sp>)` × $S$ | Mean `entoAvailabilityFull(i, inYears(age(now())))` across humans |
| `P_B` | `\tP_B(<sp>)` × $S$ | Mean `probMosqBiting(i)` across humans |
| `P_C*P_D` | `\tP_C*P_D(<sp>)` × $S$ | Mean `probMosqResting(i)` across humans |
| `resource availability` | `\tres avail(<sp>)` × $S$ | `NaN` in base `AnophelesModel`; in `SimpleMPD`, mean of `1.0 / invLarvalResources[d % 365]` for $d \in [\text{now} - \text{oneTS} + 365, \text{now} + 365)$ |
| `resource requirements` | `\tres req(<sp>)` × $S$ | `quiet_NaN()` |

