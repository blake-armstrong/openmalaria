# Exact Mathematical & Algorithmic Specification: `crates/om-clinical` & `crates/om-interventions`

All specifications below are extracted directly from the C++ headers and implementations in `model/Clinical/`, `model/interventions/`, and their coupled callers (`Host/Human.*`, `Host/NeonatalMortality.cpp`, `Host/ImportedInfections.h`, `Host/WithinHost/Treatments.*`, `Transmission/PerHost.*`, `Transmission/VectorModel.cpp`, `Transmission/NonVectorModel.h`, `Transmission/Anopheles/AnophelesModel.*`, `mon/Monitoring.*`, `sim.h`, `main.cpp`). Every legacy C++ quirk/bug required for bit-for-bit PRNG and output parity is flagged with **[PARITY QUIRK]**.

---

## 0. Simulation Clock & Main-Loop Execution Order (`sim.h`, `main.cpp`)

- **`SimTime` (`int` in days)**:
  - `DAYS_IN_YEAR = 365`, `oneDay() = 1`, `oneTS() = interval` (`1` or `5`), `stepsPerYear() = 365 / interval`, `yearsPerStep() = interval / 365.0`.
  - `never() = -0x3FFFFFFF`, `zero() = 0`, `future() = 0x3FFFFFFF`.
  - `inYears(d) = d * (1.0 / 365.0)`, `inSteps(d) = d / interval`.
  - `roundToTSFromDays(d) = fromTS(floor(d / interval + 0.5))`, `fromYearsN(y) = roundToTSFromDays(365.0 * y)`, `fromYearsD(y) = fromTS(floor(stepsPerYear() * y))`.
- **Clock state across a step**:
  - **Between updates** (`in_update == false`): `s_t0 == s_t1 == sim::now() == sim::nowOrTs0() == sim::nowOrTs1()`; `intervTime() = s_interv`; `intervDate() = startDate + s_interv`.
  - **`sim::start_update()`**: `in_update = true; s_t1 += oneTS();` so `ts0() = nowOrTs0() = s_t0`, `ts1() = nowOrTs1() = s_t0 + oneTS()`.
  - **`sim::end_update()`**: `in_update = false; s_t0 = s_t1; s_interv += oneTS();`.
- **Main loop per timestep (`main.cpp:277-313`, `Population.cpp`)**:
  1. `mon::Continuous::update(population)`
  2. If `sim::intervDate() == mon::nextSurveyDate()`:
     - For each `human`: `Host::summarize(human, surveyOnlyNewEp)` (if `surveyOnlyNewEp && clinicalModel->isExistingCase()`, returns early without recording `nHost` or any survey measures; at end of `summarize`, if `patent && mon::isReported()`, calls `human.removeFirstEvent(SubPopRemove::ON_FIRST_INFECTION)`).
     - `transmission.summarize()`
     - `mon::concludeSurvey()` (evaluates `<condition>` triggers for interventions!).
  3. `InterventionManager::deploy(population.humans, transmission)` (runs between updates at `sim::now()`).
  4. `sim::start_update()`
  5. `transmission.vectorUpdate(population.humans)`
  6. `NeonatalMortality::update(population.humans)`
  7. For each `human` in `population` (if `DOB + maxHumanAge() >= humanWarmupLength`):
     - `age0 = human.age(sim::ts0())`.
     - If `clinicalModel->isDead(age0)` $\implies$ `human.kill(); return;` (removed later in step 8).
     - `mon::updateAgeGroup(human.monitoringAgeGroup, age0)` (`while (age0 >= upperBound[idx]) ++idx;`).
     - `human.updateCohortSet()` (expires time-limited sub-pops where `!(expiry >= sim::ts0())`).
     - `age1 = sim::inYears(human.age(sim::ts1()))`.
     - `EIR = transmission.getEIR(human, age0, age1, ...)` — note: `VectorModel::calculateEIR` calls `human.perHostTransmission.update(human)` (updates ITN attrition/holes); `NonVectorModel::calculateEIR` does **not**!
     - `withinHostModel->update(human, ..., age1, ...)`
     - `clinicalModel->update(human, age1, age0 == sim::zero())`
     - `clinicalModel->updateInfantDeaths(age0)`
  8. `population.update()`: erases humans with `dead || outmigrate` (**invokes `~Episode() -> Episode::report()`!**); creates new births with `DOB = sim::ts1()`.
  9. `transmission.updateKappa(...)`, `transmission.surveyEIR()`, `sim::end_update()`.
- **End of simulation (`main.cpp:294`)**: For all surviving humans, `human.clinicalModel->flushReports()`, then `mon::writeSurveyData()`.

---

## 1. Specification for `crates/om-clinical`

### 1.1 `ClinicalModel` Base, `doomed` State Machine, Community CFR, Neonatal & Infant Mortality

#### Parameters & Model Dispatch (`ClinicalModel.cpp`)
- `indirectMortBugfix = ModelOptions::option(INDIRECT_MORTALITY_FIX)`
- `healthSystemMemory = UnitParse::readShortDuration(clinical.getHealthSystemMemory(), UnitParse::STEPS)`
- `oddsRatioThreshold = exp(parameters[Parameter::LOG_ODDS_RATIO_CF_COMMUNITY])` (Param `17`)
- **Dispatch**:
  1. `ModelOptions::option(CLINICAL_EVENT_SCHEDULER)` $\implies$ `ClinicalEventScheduler` (`1`-day TS required).
  2. Else if `healthSystem.getImmediateOutcomes().present()` $\implies$ `ClinicalImmediateOutcomes` (subclass of `CM5DayCommon`).
  3. Else (`healthSystem.getDecisionTree5Day().present()`) $\implies$ `DecisionTree5Day` (subclass of `CM5DayCommon`).
- **`ClinicalModel::setHS(healthSystem)`**:
  - Loads `caseFatalityRate = AgeGroupInterpolator(healthSystem.getCFR())` and `pSequelaeInpatient = AgeGroupInterpolator(healthSystem.getPSequelaeInpatient())`.
  - Dispatches to `ESCaseManagement::setHealthSystem`, `ClinicalImmediateOutcomes::setHealthSystem`, or `DecisionTree5Day::setHealthSystem`.
- **Community Case Fatality Rate (`getCommunityCFR`)**:
  $$\text{getCommunityCFR}(c) = \frac{c \cdot \Omega}{1 - c + c \cdot \Omega}, \qquad \Omega = \exp(\text{LOG\_ODDS\_RATIO\_CF\_COMMUNITY})$$

#### Exact `doomed` Codes & State Machine (`ClinicalModel.h:158-174`, `ClinicalModel.cpp:136-181`)
`int doomed` (initialized to `NOT_DOOMED = 0`):
| Constant | Value | Meaning |
|---|---|---|
| `DOOMED_EXPIRED` | `-35` | Indirect mortality timer expired (`doomed <= -35` triggers indirect death now) |
| `DOOMED_NEXT_TS` | `-30` | Checked by `updateInfantDeaths`! (In 5-day TS, expires on next step) |
| `DOOMED_START_TIMER` | `-1` | Unused literal; indirect mortality actually sets `doomed = -sim::oneTS()` (`-5` in 5d TS, `-1` in 1d TS) |
| `NOT_DOOMED` | `0` | Healthy / not doomed (`< 0` = countdown active; `> 0` = dead) |
| `DOOMED_TOO_OLD` | `1` | Reached `sim::maxHumanAge()` |
| `DOOMED_COMPLICATED` | `4` | Direct malaria death (or ES NMF direct death) |
| `DOOMED_NEONATAL` | `6` | Died at birth from maternal malaria infection |
| `DOOMED_INDIRECT` | `7` | Died after indirect mortality timer reached `<= -35` |

- **`ClinicalModel::isDead(SimTime age0)`** (called at start of `Host::update`):
  ```text
  if age0 >= sim::maxHumanAge():
      doomed = DOOMED_TOO_OLD
  return doomed > NOT_DOOMED
  ```
  *(Note: when `doomed` becomes `4`, `6`, or `7` during `clinicalModel->update`, the human is NOT removed in that same step's `population.update()`! They remain in `population` until the NEXT step's `Host::update` sees `isDead(age0) == true` and calls `human.kill()`, after which `population.update()` erases them and runs `~Episode()`.)*

- **`ClinicalModel::update(Human& human, double ageYears, bool newBorn)`** (`ageYears = age1`, `newBorn = (age0 == 0)`):
  1. If `doomed < NOT_DOOMED`: `doomed -= sim::oneTS();`
  2. If `doomed <= DOOMED_EXPIRED` (`-35`):
     - `mon::recordEvent("nIndDeaths", human);`
     - `doomed = DOOMED_INDIRECT;`
     - `return;` *(skips morbidity & clinical update!)*
     *(Timing note: when indirect mortality starts, `doomed = -oneTS()`. In 5-day TS: `-5` at bout step $t$, then `-10, -15, -20, -25, -30, -35` — dies at $t + 30\text{ days}$. In 1-day TS: `-1` at bout step $t$, reaches `-30` at $t + 29\text{ days}$, dies at `-35` at $t + 34\text{ days}$.)*
  3. If `newBorn`:
     - Draw `human.rng.uniform_01() <= NeonatalMortality::riskFromMaternalInfection` (**[PARITY QUIRK]**: uses `<=`, not `<`!).
     - If true:
       - `mon::recordEvent("nIndDeaths", human);`
       - `doomed = DOOMED_NEONATAL;`
       - `return;`
  4. `bool isDoomed = (doomed != NOT_DOOMED);` (any nonzero `doomed` suppresses new indirect mortality inside `determineMorbidity`).
  5. `pg = human.withinHostModel->determineMorbidity(human, ageYears, isDoomed);`
  6. `latestState = static_cast<Episode::State>(pg.state);`
  7. `doClinicalUpdate(human, ageYears, pg);`

- **`NeonatalMortality::update(population)` (`Host/NeonatalMortality.cpp`)**:
  - Iterates `population` from oldest to youngest (`rbegin..rend`), `age = human.age(sim::ts0())`:
    - If `age >= fromYearsN(25.0)`: `continue`.
    - If `age < fromYearsN(20.0)`: `break`.
    - `nCounter += 1`; if `withinHostModel->diagnosticResult(human.rng, *neonatalDiagnostic)`: `pCounter += 1`.
  - `prev2025 = (nCounter == 0) ? 0.25 : double(pCounter) / nCounter`.
  - Ring buffer `prevByGestationalAge` of length `N_gest = inSteps(fromDays(150))` (`30` for 5d, `150` for 1d; initialized to `0.25`):
    - `prevByGestationalAge[moduloSteps(sim::ts0(), N_gest)] = prev2025`.
    - `maxPrev = max(prevByGestationalAge)`.
  - $\text{prevPG} = \frac{\text{maxPrev}}{0.19 + \text{maxPrev}}$, $\quad \text{riskFromMaternalInfection} = (0.3 \times 0.011) \cdot \left(1.0 - \exp\left(-\frac{\text{prevPG}}{0.25}\right)\right)$.

- **`InfantMortality` Kaplan-Meier Formula (`ClinicalModel.cpp:58-77, 165-171`)**:
  - Arrays `infantDeaths[0..stepsPerYear-1]` and `infantIntervalsAtRisk[0..stepsPerYear-1]` of type `uint32_t` (or `int`), reset to `0` in `preMainSimInit()` (end of warmup).
  - In `ClinicalModel::updateInfantDeaths(SimTime age0)`:
    - If `age0 < sim::oneYear()` (`365`):
      - `index = size_t(age0 / sim::oneTS())`
      - `isDoomed = (doomed == DOOMED_COMPLICATED) || (doomed == DOOMED_NEXT_TS) || (doomed == DOOMED_NEONATAL)`
        *(**[PARITY QUIRK]**: Checks `doomed == DOOMED_NEXT_TS` (`-30`), NOT `DOOMED_INDIRECT` (`7`)! In 5-day TS this counts an indirect infant death 1 TS before expiry; in 1-day TS it counts it 5 days before expiry, and when `doomed` later hits `7`, `isDoomed` is `false` so it is never double-counted.)*
      - `infantIntervalsAtRisk[index] += 1;`
      - `if (isDoomed) infantDeaths[index] += 1;`
  - At end of simulation (`InfantMortality::allCause()`):
    $$S_{\text{infant}} = \prod_{i=0}^{\text{stepsPerYear}-1} \frac{\text{double}(\text{infantIntervalsAtRisk}[i] - \text{infantDeaths}[i])}{\text{double}(\text{infantIntervalsAtRisk}[i])}$$
    $$\text{IMR} = (1.0 - S_{\text{infant}}) \times 1000.0 + \text{parameters}[\text{NON\_MALARIA\_INFANT\_MORTALITY}] \quad (\text{Param } 19)$$

- **`isExistingCase()` (used when `surveyOnlyNewEp` is enabled)**:
  - `sim::now() > tLastTreatment && sim::now() <= tLastTreatment + healthSystemMemory` (where `tLastTreatment` is `m_tLastTreatment` in `CM5DayCommon` or `timeLastTreatment` in `ClinicalEventScheduler`).

---

### 1.2 `Episode` Bitflags, Merging, Reporting & Flushing (`Episode.h`, `Episode.cpp`, `Pathogenesis/State.h`)

#### Exact Bitflags (`Episode::State`)
```text
NONE              = 0x0
SICK              = 0x1      // Any fever; alone = NMF (STATE_NMF = 0x1)
MALARIA           = 0x2      // STATE_MALARIA = SICK | MALARIA = 0x3
COINFECTION       = 0x4
SEVERE            = 0x8
SECOND_CASE       = 0x10     // Recurrent case within healthSystemMemory of last treatment
COMPLICATED       = 0x20     // STATE_SEVERE = 0x2B, STATE_COINFECTION = 0x27
MORBIDITY_MASK    = 0x7F

EVENT_IN_HOSPITAL = 0x400
DIRECT_DEATH      = 0x1000
SEQUELAE          = 0x2000
RECOVERY          = 0x4000   // Used for hospital recovery reporting & ES recovery
EVENT_FIRST_DAY   = 0x8000   // Used in ES first-day severe death
RUN_CM_TREE       = 0x10000  // Internal ES flag
```

#### `Episode` Fields & Lifecycle
- Initial fields: `time = sim::never()`, `surveyPeriod = mon::NOT_USED`, `ageGroup = MonitoringAgeGroup()`, `cohortSet = 0`, `state = NONE`, `infectionType = Indigenous`.
- **`Episode::update(const Human& human, Episode::State newState)`**:
  - Memory boundary check:
    - If `ModelOptions::option(HEALTH_SYSTEM_MEMORY_FIX)`: `toReport = (time + healthSystemMemory <= sim::ts0())`
    - Else (default): `toReport = (time + healthSystemMemory < sim::ts0())`
  - If `toReport`:
    1. `infectionType = human.withinHostModel->getInfectionOrigin();`
       *(**[PARITY QUIRK]**: `infectionType` is overwritten with the NEW episode's origin BEFORE `report()` is called for the PREVIOUS episode! Replicate this exact line order!)*
    2. `report();` *(if `time < 0`, e.g. `sim::never()`, `report()` is a no-op)*
    3. `time = sim::ts0();`
    4. `surveyPeriod = mon::eventSurveyNumber();`
    5. `ageGroup = human.monitoringAgeGroup;`
    6. `cohortSet = human.getCohortSet();`
    7. `state = newState;`
  - Else (`!toReport`, within HS memory window):
    - `state = Episode::State(state | newState);` *(accumulates severity & outcomes via bitwise OR; `time`, `surveyPeriod`, `ageGroup`, and `cohortSet` remain fixed at the start of the episode!)*
- **`Episode::flush()`**: calls `report(); time = sim::never();`. Triggered by:
  1. `Human::removeFirstEvent` when removing an active sub-population (`expiry >= sim::nowOrTs0()`), BEFORE clearing the `cohortSet` bit.
  2. End of simulation (`ClinicalModel::flushReports()`), before writing survey output.
- **`Episode::~Episode()`**: calls `report();`. Triggered when `Human` is erased in `Population::update()` due to death or outmigration.
- **`Episode::report()`**:
  - If `time < sim::zero()` (`0`): `return`.
  - All `mon::recordEvent` calls pass the stored `(surveyPeriod, ageGroup, cohortSet)` from episode start:
    - **If `state & MALARIA`**:
      - *Severity*:
        - If `state & COMPLICATED`:
          - `recordEvent("nSevere", ...)`
          - If `state & SEVERE`: `recordEvent("nSevereWithoutComorbidities", ...)`
        - Else:
          - `recordEvent("nUncomp", ...)`
          - Switch `infectionType`: `Indigenous` $\to$ `"nUncomp_Indigenous"`, `Introduced` $\to$ `"nUncomp_Introduced"`, `Imported` $\to$ `"nUncomp_Imported"`.
      - *Outcome* (`if / else if` chain — `DIRECT_DEATH` takes precedence over `SEQUELAE`, which takes precedence over `RECOVERY`):
        - If `state & EVENT_IN_HOSPITAL`:
          - If `state & DIRECT_DEATH`:
            - `recordEvent("nDirDeaths", ...)`, `recordEvent("nHospitalDeaths", ...)`
            - If `state & EVENT_FIRST_DAY`: `recordEvent("Clinical_FirstDayDeaths", ...)`, `recordEvent("Clinical_HospitalFirstDayDeaths", ...)`
          - Else if `state & SEQUELAE`:
            - `recordEvent("nSeq", ...)`, `recordEvent("nHospitalSeqs", ...)`
          - Else if `state & RECOVERY`:
            - `recordEvent("nHospitalRecovs", ...)`
        - Else (not in hospital):
          - If `state & DIRECT_DEATH`:
            - `recordEvent("nDirDeaths", ...)`
            - If `state & EVENT_FIRST_DAY`: `recordEvent("Clinical_FirstDayDeaths", ...)`
          - Else if `state & SEQUELAE`:
            - `recordEvent("nSeq", ...)`
          - *(Out-of-hospital `RECOVERY` is NOT reported.)*
    - **Else if `state & SICK`** (Non-malarial fever):
      - `recordEvent("nNMFever", ...)`
      - If `state & DIRECT_DEATH`: `recordEvent("nNmfDeaths", ...)`

---

### 1.3 `CM5DayCommon` (5-Day Shared Update & Severe Malaria) (`CM5DayCommon.cpp`)

- Static option: `cfr_pf_use_hospital = ModelOptions::option(CFR_PF_USE_HOSPITAL)`.
- Per-human fields: `m_tLastTreatment = sim::never()`, `m_treatmentSeekingFactor = tSF` (`1.0` default, or `0.2` / `1.8` under treatment heterogeneity options).

#### `CM5DayCommon::doClinicalUpdate(human, ageYears, pg)`
1. `pgState = static_cast<Episode::State>(pg.state)`
2. If `pgState & MALARIA`:
   - If `pgState & COMPLICATED`: `severeMalaria(human, pgState, ageYears, doomed)`
   - Else if `indirectMortBugfix || !pg.indirectMortality`: `uncomplicatedEvent(human, pgState)`
     *(**[PARITY QUIRK]**: When `INDIRECT_MORTALITY_FIX` is false, an uncomplicated malaria bout that triggers `pg.indirectMortality` skips `uncomplicatedEvent` completely — no episode report, no treatment!)*
3. Else if `pgState & SICK`: `uncomplicatedEvent(human, pgState)` (NMF).
4. If `pg.indirectMortality && doomed == NOT_DOOMED`: `doomed = -sim::oneTS();` (`-5`).
5. If `m_tLastTreatment == sim::ts0()`: `human.removeFirstEvent(SubPopRemove::ON_FIRST_TREATMENT);`
6. If `pgState & SICK`: `human.removeFirstEvent(SubPopRemove::ON_FIRST_BOUT);`

#### `CM5DayCommon::severeMalaria(human, pgState, ageYears, doomed)`
- Probabilities:
  - $p_2 = \text{accessSevere} \cdot m\_\text{treatmentSeekingFactor}$
  - $p_3 = \text{cureRateSevere}$
  - $p_4 = \text{caseFatalityRate.eval}(\text{ageYears})$
  - $p_{5a} = \text{getCommunityCFR}(p_4)$
  - $p_{5b} = \text{cfr\_pf\_use\_hospital} \ ? \ p_4 : p_{5a}$
  - $p_6 = p_7 = \text{pSequelaeInpatient.eval}(\text{ageYears})$
- Cumulative thresholds $q_0 \dots q_7$:
  - $q_0 = (1 - p_2) p_{5a}$
  - $q_1 = q_0 + (1 - p_2)(1 - p_{5a}) p_7$
  - $q_2 = q_1 + (1 - p_2)(1 - p_{5a})(1 - p_7)$ *(note: algebraically $1 - p_2$, computed via the sum above in IEEE-754)*
  - $q_3 = q_2 + p_2 (1 - p_3) p_{5b}$
  - $q_4 = q_3 + p_2 (1 - p_3)(1 - p_{5b}) p_7$
  - $q_5 = q_4 + p_2 (1 - p_3)(1 - p_{5b})(1 - p_7)$
  - $q_6 = q_5 + p_2 p_3 p_4$
  - $q_7 = q_6 + p_2 p_3 (1 - p_4) p_6$
- Expected statistics (recorded unconditionally before RNG):
  - `exHospitalDeath = p2 * (p3 * p4 + (1.0 - p3) * p5b)` $\to$ `mon::recordStat("expectedHospitalDeaths", human, exHospitalDeath)`
  - `exDeath = exHospitalDeath + (1.0 - p2) * p5a` $\to$ `mon::recordStat("expectedDirectDeaths", human, exDeath)`
  - `exSeq = (p2 * (p3 * (1.0 - p4) + (1.0 - p3) * (1.0 - p5b)) + (1.0 - p2) * (1.0 - p5a)) * p6` $\to$ `mon::recordStat("expectedSequelae", human, exSeq)`
- **RNG #1 (only direct RNG in `severeMalaria`)**: `prandom = human.rng.uniform_01()`
  - **If `prandom >= q[2]`** (Hospitalized):
    - `m_tLastTreatment = sim::ts0();`
    - `mon::recordEvent("nTreatments3", human);`
    - `stateTreated = pgState | EVENT_IN_HOSPITAL;`
    - **If `prandom >= q[5]`** (Parasites cleared):
      - `human.withinHostModel->treatment(human, treatmentSevere);` *(clears infections + triggers `TriggeredDeployments` for `treatmentSevere` with `mon::Deploy::TREAT`)*
      - If `prandom < q[6]`: `latestReport.update(human, stateTreated | DIRECT_DEATH); doomed = DOOMED_COMPLICATED;`
      - Else if `prandom < q[7]`: `latestReport.update(human, stateTreated | SEQUELAE);`
      - Else: `latestReport.update(human, stateTreated | RECOVERY);`
    - **Else (`q[2] <= prandom < q[5]`, Hospitalized, treatment failed to clear parasites)**:
      - If `prandom < q[3]`: `latestReport.update(human, stateTreated | DIRECT_DEATH); doomed = DOOMED_COMPLICATED;`
      - Else if `prandom < q[4]`: `latestReport.update(human, stateTreated | SEQUELAE);`
      - Else: `latestReport.update(human, pgState);` *(**[PARITY QUIRK]**: passes `pgState` WITHOUT `EVENT_IN_HOSPITAL | RECOVERY`!)*
  - **Else (`prandom < q[2]`, Community / Untreated)**:
    - If `prandom < q[0]`: `latestReport.update(human, pgState | DIRECT_DEATH); doomed = DOOMED_COMPLICATED;`
    - Else if `prandom < q[1]`: `latestReport.update(human, pgState | SEQUELAE);`
    - Else: `latestReport.update(human, pgState);`

---

### 1.4 `ClinicalImmediateOutcomes` (`ImmediateOutcomes.cpp`)

- **`setHealthSystem`**:
  - Allowed drugs: `"CQ"`, `"SP"`, `"AQ"`, `"SPAQ"`, `"ACT"`, `"QN"` (plus `"selfTreatment"` in ACR/compliance).
  - `accessUCOfficial1 = pSeekOfficialCareUncomplicated1`, `accessUCOfficial2 = pSeekOfficialCareUncomplicated2` (both validated in `[0, 1]`), `accessUCSelfTreat = [pSelfTreatUncomplicated, 0.0]`.
  - `accessUCAny[FirstLine] = accessUCOfficial1 + accessUCSelfTreat[FirstLine]`.
  - **[PARITY QUIRK / LEGACY BUG]**: `accessUCAny[SecondLine] = accessUCOfficial1 + accessUCSelfTreat[SecondLine]` (`ImmediateOutcomes.cpp:118` uses `accessUCOfficial1`, NOT `accessUCOfficial2`!).
  - `accessSevere = pSeekOfficialCareSevere`.
  - Cure rates:
    - `cureRateUCOfficial[FirstLine] = comp1 * acr1 + (1 - comp1) * nonCompEff1`
    - `cureRateUCOfficial[SecondLine] = comp2 * acr2 + (1 - comp2) * nonCompEff2`
    - `cureRateUCSelfTreat[FirstLine] = compSelf * acrSelf + (1 - compSelf) * nonCompEff1` *(uses `nonCompEff1`)*
    - `cureRateUCSelfTreat[SecondLine] = 0.0`
    - `cureRateSevere = initialACR[drugSevere]`
  - `useDiagnosticUC = hs.getUseDiagnosticUC()`.
  - Registers `treatmentUC[FirstLine]`, `treatmentUC[SecondLine]`, `treatmentSevere` via `WHInterface::addTreatment(...)`.
- **`uncomplicatedEvent(human, pgState)` & Exact RNG Order**:
  1. `regimen = FirstLine`; if `m_tLastTreatment + healthSystemMemory > sim::ts0()` $\implies$ `pgState |= SECOND_CASE; regimen = SecondLine;`.
  2. `latestReport.update(human, pgState);`
  3. **RNG #1**: `x = human.rng.uniform_01();`
  4. If `x < accessUCAny[regimen] * m_treatmentSeekingFactor`:
     - If `useDiagnosticUC`:
       - `mon::recordEvent("nTreatDiagnostics", human);`
       - **RNG #2 (if diagnostic stochastic)**: `if (!withinHostModel->diagnosticResult(human.rng, diagnostics::monitoringDiagnostic())) return;`
     - `m_tLastTreatment = sim::ts0();`
     - `mon::recordEvent(regimen == FirstLine ? "nTreatments1" : "nTreatments2", human);`
     - `p = (x < accessUCSelfTreat[regimen] * m_treatmentSeekingFactor) ? cureRateUCSelfTreat[regimen] : cureRateUCOfficial[regimen];` *(reuses `x`!)*
     - **RNG #3**: `if (human.rng.bernoulli(p))` (`uniform_01() < p`):
       - `withinHostModel->treatment(human, treatmentUC[regimen]);`
     - `withinHostModel->optionalPqTreatment(human);`

---

### 1.5 `DecisionTree5Day` & `CMDecisionTree` (`DecisionTree5Day.cpp`, `CMDecisionTree.cpp`)

- **`DecisionTree5Day::setHealthSystem`**:
  - Identical `accessUCAny` setup to `ImmediateOutcomes`, **including the exact same [PARITY QUIRK]**: `accessUCAny[SecondLine] = accessUCOfficial1 + accessUCSelfTreat[SecondLine]` (`DecisionTree5Day.cpp:63`)!
  - Builds `treeUCOfficial = CMDecisionTree::create(getTreeUCOfficial(), true)` and `treeUCSelfTreat = CMDecisionTree::create(getTreeUCSelfTreat(), true)`.
- **`DecisionTree5Day::uncomplicatedEvent(human, pgState)` & RNG Order**:
  1. `regimen = FirstLine`; if `m_tLastTreatment + healthSystemMemory > sim::ts0()` $\implies$ `pgState |= SECOND_CASE; regimen = SecondLine;`.
  2. `latestReport.update(human, pgState);`
  3. **RNG #1**: `x = human.rng.uniform_01();`
  4. If `x < accessUCAny[regimen] * m_treatmentSeekingFactor`:
     - `hostData = CMHostData(human, sim::inYears(human.age(sim::ts0())), pgState)` (**[PARITY QUIRK]**: uses `age(sim::ts0())`, whereas `ESCaseManagement` uses `age1` and `DecisionTreeComponent` in interventions uses `age(sim::nowOrTs1())`!).
     - `output = (x < accessUCSelfTreat[regimen] * m_treatmentSeekingFactor) ? treeUCSelfTreat->exec(hostData) : treeUCOfficial->exec(hostData);`
     - If `output.treated`: `m_tLastTreatment = sim::ts0(); mon::recordEvent(regimen == FirstLine ? "nTreatments1" : "nTreatments2", human);`
     - If `output.screened`: `mon::recordEvent("nTreatDiagnostics", human);`
     - `withinHostModel->optionalPqTreatment(human);`

#### `CMDecisionTree` Node Types, Evaluation & RNG Order (`CMDecisionTree.cpp`)
`CMDTOut { treated: bool, screened: bool }` (default `(false, false)`).
- **`CMDecisionTree::create(decisions, isUC)`**: Collects present XML child elements in **fixed category order**:
  `caseType` $\to$ `infectionOrigin` $\to$ `diagnostic` $\to$ `uncomplicated` $\to$ `severe` $\to$ `random` $\to$ `age` $\to$ `cohort` $\to$ `treatPKPD` $\to$ `treatSimple` $\to$ `report` $\to$ `deploy`.
  - If 0 children $\to$ `CMDTNoTreatment` (`CMDTOut(false, false)`).
  - If 1 child $\to$ that node directly.
  - If $>1$ children $\to$ `CMDTMultiple(list)`: executes each child in the order above, setting `result.treated = result.treated || r2.treated`. **[PARITY QUIRK]**: `CMDTMultiple::exec` NEVER propagates `r2.screened` (`result.screened` stays `false` whenever a `<diagnostic>` is a sibling of another node, e.g. `<report>`, inside the same `<decisions>` block!).
- **Branching Nodes**:
  1. `CMDTCaseType` (`isUC` required): `(hostData.pgState & SECOND_CASE) ? secondLine.exec : firstLine.exec` (0 RNG).
  2. `CMDTInfectionOrigin`: `withinHost.getInfectionOrigin()` $\to$ `Imported` / `Introduced` / `Indigenous` branch (0 RNG).
  3. `CMDTDiagnostic`: **RNG (if stochastic diagnostic)** `withinHost.diagnosticResult(human.rng, diagnostic)` $\to$ `positive.exec` or `negative.exec`; sets `result.screened = true; return result;`.
  4. `CMDTUncomplicated`: `memory` (`<= hsMemory`). Let `latest = clinicalModel.getLatestReport()`. Condition: `(((pgState & SICK) && !(pgState & COMPLICATED)) || (pgState & MALARIA)) && (latest.time + memory >= sim::nowOrTs0())` $\to$ `positive.exec` else `negative.exec` (0 RNG).
  5. `CMDTSevere`: `(hostData.pgState & COMPLICATED) ? positive.exec : negative.exec` (0 RNG).
  6. `CMDTRandom`: cumulative probabilities $C_0, C_1, \dots, C_{k-1}$ (`1.0 <= C_{k-1} <= 1.001`, stored in `std::map<double, Node*>`). **1 RNG**: `u = human.rng.uniform_01()`; selects first branch with **`C_i > u`** (`map::upper_bound(u)`).
  7. `CMDTAge`: lower bounds $0.0 = L_0 < L_1 < \dots < L_{k-1}$ stored as upper-bound keys $(L_1 \to B_0, \dots, L_{k-1} \to B_{k-2}, +\infty \to B_{k-1})$. Selects `upper_bound(hostData.ageYears)`, i.e. $L_i \le \text{ageYears} < L_{i+1}$ (0 RNG).
  8. `CMDTCohort`: `human.isInSubPop(componentId) ? positive.exec : negative.exec` (0 RNG).
- **Action / Leaf Nodes**:
  1. `CMDTNoTreatment`: returns `(false, false)`.
  2. `CMDTTreatFailure`: returns `(true, false)` (counts as treated without clearing parasites).
  3. `CMDTReport`: for each `outId`: `mon::recordEvent("nCMDTReport", human, 1, outId)`; returns `(false, false)`.
  4. `CMDTTreatPKPD`: for each `(schedule, dosage, delay_h)`: calls `withinHost.treatPkPd(schedule, dosage, hostData.ageYears, 0.0)` (**[PARITY QUIRK]**: passes `0.0` instead of `delay_h / 24.0`!); returns `(true, false)`.
  5. `CMDTTreatSimple`: for each `(durL, durB)` (`>= -oneTS()`): `bsTreatment = withinHost.treatSimple(human, durL, durB)` (**[PARITY QUIRK]**: overwrites `bsTreatment = ...` rather than `|=`, so `treated` reflects only the last element's `durB != 0`!); returns `(bsTreatment, false)`.
  6. `CMDTDeploy`: executes `HumanIntervention::deploy(human, mon::Deploy::TREAT, VaccineLimits())`; returns `(false, false)`.

---

### 1.6 `ClinicalEventScheduler` & `ESCaseManagement` (1-Day Case Management) (`EventScheduler.cpp`, `ESCaseManagement.cpp`)

- Requires `sim::oneTS() == 1` and `tSF == 1.0`.
- **Static parameters (`init` & `setHealthSystem`)**:
  - $\alpha = \exp(-\text{parameters}[\text{CFR\_NEG\_LOG\_ALPHA}])$ (Param `38`, must be in `[0, 1]`).
  - $\text{neg\_v} = -\text{parameters}[\text{CFR\_SCALE\_FACTOR}]$ (Param `33`).
  - `caseFatalityRate.scale(alpha)` (scales the `CFR` interpolator by $\alpha$ immediately after `ClinicalModel::setHS` loads it).
  - `escm_uncomplicated = CMDecisionTree::create(es.getUncomplicated(), true)`
  - `escm_complicated = CMDecisionTree::create(es.getComplicated(), false)`
  - `maxUCSeekingMemory = fromDays(co.getMaxUCSeekingMemory())` (`>= 0`)
  - `uncomplicatedCaseDuration = fromDays(co.getUncomplicatedCaseDuration())` (`>= 1`)
  - `complicatedCaseDuration = fromDays(co.getComplicatedCaseDuration())` (`>= 1`)
  - `extraDaysAtRisk = fromDays(co.getComplicatedRiskDuration()) - complicatedCaseDuration` (`<= 0`, `complicatedRiskDuration >= 1`)
  - `cumDailyPrImmUCTS`: cumulative sum of `dailyPrImmUCTS` (`0.999 <= sum <= 1.001`, last entry forced to `1.0`). *(Note: in C++, `cumDailyPrImmUCTS` lacked `.clear()` before `push_back` on `changeHS`, though `dailyPrImmUCTS` only matters if `changeHS` re-initializes ES).*
  - If `ModelOptions::option(NON_MALARIA_FEVERS)`:
    - Loads `NMF_need_antibiotic` (`prNeedTreatmentNMF`) and `MF_need_antibiotic` (`prNeedTreatmentMF`) interpolators from `clinical.NonMalariaFevers`.
    - Loads `severeNmfMortality = AgeGroupInterpolator(nmfHS.getCFR())` (plus log-odds fields that are unused due to commented-out `FIXME` block).
- **Per-human state**: `pgState = NONE`, `caseStartTime = sim::never()`, `timeOfRecovery = sim::never()`, `timeLastTreatment = sim::never()`, `previousDensity = NaN`.

#### Exact Daily Execution (`ClinicalEventScheduler::doClinicalUpdate(human, ageYears, pg)`)
1. **End-of-bout recovery/death (`if (sim::ts0() == timeOfRecovery)`)**:
   - If `pgState & DIRECT_DEATH`:
     - `doomed = DOOMED_COMPLICATED;`
     - `latestReport.update(human, pgState);` *(returns to step 2 with `pgState` intact; human is killed at start of next step)*
   - Else (survived):
     - If `pgState & COMPLICATED`:
       - `pSeq = pSequelaeInpatient.eval(ageYears);`
       - `mon::recordStat("expectedSequelae", human, pSeq);`
       - **RNG**: `if (human.rng.uniform_01() < pSeq) pgState |= SEQUELAE; else pgState |= RECOVERY;`
     - Else:
       - `pgState |= RECOVERY;`
     - `latestReport.update(human, pgState);`
     - `pgState = Episode::NONE;`
     - `human.perHostTransmission.outsideTransmission = false;`

2. **New morbidity onset**:
   - `indirectMortality = indirectMortBugfix ? pg.indirectMortality : false;`
   - `newState = pg.state;`
   - If `newState & SICK`:
     - If `newState & COMPLICATED`:
       - If `!(pgState & COMPLICATED)` *(healthy or UC escalating to severe)*:
         - `pgState = pgState | newState | RUN_CM_TREE;`
         - `indirectMortality = pg.indirectMortality;`
         - `caseStartTime = sim::ts0();`
     - Else *(newState is UC or NMF)*:
       - If `!(pgState & SICK) && caseStartTime < sim::ts0()`:
         - `pgState = pgState | newState | RUN_CM_TREE;`
         - `indirectMortality = pg.indirectMortality;`
         - **RNG**: `u = human.rng.uniform_01()`; find smallest `i` with `u < cumDailyPrImmUCTS[i]`; `caseStartTime = sim::ts0() + sim::fromDays(i);`
     - If `indirectMortality && doomed == NOT_DOOMED`: `doomed = -sim::oneTS();` (`-1`).

3. **Case management & CFR**:
   - **If `caseStartTime == sim::ts0() && (pgState & RUN_CM_TREE)`** *(Bout treatment day)*:
     - `pgState &= ~RUN_CM_TREE;`
     - If `timeLastTreatment + healthSystemMemory > sim::ts0()`: `pgState |= SECOND_CASE;`
     - `auxOut = ((pgState & COMPLICATED) ? escm_complicated : escm_uncomplicated)->exec(CMHostData(human, ageYears, pgState));` *(uses `ageYears = age1`)*
     - If `auxOut.treated`:
       - `timeLastTreatment = sim::ts0();`
       - Record `"nTreatments3"` if `COMPLICATED`, else `"nTreatments2"` if `SECOND_CASE`, else `"nTreatments1"`.
     - If `auxOut.screened`: `mon::recordEvent("nTreatDiagnostics", human);`
     - **Hospitalization**: `pgState |= EVENT_IN_HOSPITAL;` *(hardcoded `if (true)` in `EventScheduler.cpp:270` — ALL ES cases, including UC and NMF, get `EVENT_IN_HOSPITAL` and `outsideTransmission = true`!)*
     - **First-day Complicated CFR** (`if ((pgState & COMPLICATED) && !(pgState & DIRECT_DEATH))`):
       - `pDeath = caseFatalityRate.eval(ageYears);` *(already scaled by $\alpha$; `inHospital` is hardcoded `true`)*
       - `mon::recordStat("expectedDirectDeaths", human, pDeath);`
       - `mon::recordStat("expectedHospitalDeaths", human, pDeath);`
       - **RNG**: `if (human.rng.uniform_01() < pDeath) pgState |= DIRECT_DEATH | EVENT_FIRST_DAY;`
       - `previousDensity = withinHostModel->getTotalDensity();`
     - **Antibiotic / NMF Logic** (`if (opt_non_malaria_fevers && (pgState & SICK) && !(pgState & COMPLICATED))`):
       - `isMalarial = (pgState & MALARIA) != 0;`
       - `pNeedTreat = isMalarial ? MF_need_antibiotic.eval(ageYears) : NMF_need_antibiotic.eval(ageYears);`
       - **RNG #1**: `needTreat = human.rng.bernoulli(pNeedTreat);`
       - **[PARITY QUIRK — MANDATORY DEAD RNG CALL]**: In `EventScheduler.cpp:308-346`, the `AB_provider` logistic regression is commented out (`pTreatment = 0.0; treatmentEffectMult = 1.0;`), followed immediately by `if (human.rng.uniform_01() < pTreatment) { ... }`. You **MUST** consume `human.rng.uniform_01()` here unconditionally!
       - If `needTreat`:
         - `pDeath = severeNmfMortality.eval(ageYears);` (`* 1.0`)
         - **RNG #3**: `if (human.rng.uniform_01() < pDeath) pgState |= DIRECT_DEATH;` *(**[PARITY QUIRK]**: runs even when `isMalarial == true`, despite comment saying "only when not malarial"!)*
   - **Else** *(Subsequent days of bout)*:
     - If `(pgState & COMPLICATED) && !(pgState & DIRECT_DEATH) && (sim::ts0() < timeOfRecovery + extraDaysAtRisk)`:
       - `totalDensity = withinHostModel->getTotalDensity();`
       - If `totalDensity > 0.0`:
         - $r = \text{totalDensity} / \text{previousDensity}$ *(if `previousDensity == 0.0`, IEEE-754 `+inf` $\implies p_{\text{Death}} = 1.0$)*
         - $p_{\text{Death}} = 1.0 - \exp(\text{neg\_v} \cdot r)$
         - If `!(pgState & EVENT_IN_HOSPITAL)`: $p_{\text{Death}} = \text{getCommunityCFR}(p_{\text{Death}})$
         - `mon::recordStat("expectedDirectDeaths", human, pDeath);`
         - If `pgState & EVENT_IN_HOSPITAL`: `mon::recordStat("expectedHospitalDeaths", human, pDeath);`
         - **RNG**: `if (human.rng.uniform_01() < pDeath) { pgState |= DIRECT_DEATH; timeOfRecovery = timeOfRecovery + extraDaysAtRisk; }`
       - `previousDensity = totalDensity;`

4. **Bout duration scheduling (`if (caseStartTime == sim::ts0())`)**:
   - If `pgState & EVENT_IN_HOSPITAL`: `human.perHostTransmission.outsideTransmission = true;`
   - If `pgState & COMPLICATED`:
     - `timeOfRecovery = sim::ts0() + complicatedCaseDuration;`
     - If `pgState & DIRECT_DEATH`: `timeOfRecovery += extraDaysAtRisk;`
   - Else:
     - `timeOfRecovery = sim::ts0() + uncomplicatedCaseDuration;`

5. **SubPop first-event removal**:
   - If `timeLastTreatment == sim::ts0()`: `human.removeFirstEvent(SubPopRemove::ON_FIRST_TREATMENT);`
   - If `pgState & SICK`: `human.removeFirstEvent(SubPopRemove::ON_FIRST_BOUT);`

---

## 2. Specification for `crates/om-interventions`

### 2.1 Component Ordering, Conditions, Cohorts & `SubPopRemove` (`Interfaces.h`, `HumanComponents.h`, `Human.cpp`)

- **Canonical `Component::Type` Sort Order**:
  Every `HumanIntervention` sorts its `components` with **`std::stable_sort`** by `componentType()`:
  `RECRUIT_ONLY (0) < SCREEN (1) < TREAT_SIMPLE (2) < TREAT_PKPD (3) < CM_DT (4) < PEV (5) < BSV (6) < TBV (7) < IPT (8) < ITN (9) < IRS (10) < GVI (11) < CLEAR_IMMUNITY (12)`.
- **Survey-Gated `<condition>` (`HumanIntervention::deploy`)**:
  - Condition state starts at `initialState` (`bool`), updated at each reported survey in `mon::concludeSurvey()` to `(surveyMeasureTotal >= minValue && surveyMeasureTotal <= maxValue)`.
  - If any condition on the `HumanIntervention` is `false`, `HumanIntervention::deploy` returns immediately (no cohort membership added, 0 RNG).
  - Otherwise, for each `component` in sorted `components`:
    1. `human.addToCohort(component.id(), component.duration())`: if `duration > 0` (default `sim::future()`, or `fromYearsN(afterYears)` if `<subPopRemoval afterYears="..."/>`), sets `subPopExp[id] = sim::nowOrTs1() + duration` and updates `cohortSet` bitmask (`mon::updateCohortSet(cohortSet, id, true)`) **before** calling `component.deploy(...)`.
    2. `component.deploy(human, method, vaccLimits)`.
- **`SubPopRemove` Mechanics (`Human.cpp:124-174`)**:
  - `isInSubPop(id)`: `subPopExp.contains(id) && subPopExp[id] >= sim::nowOrTs0()`.
  - **Age/Duration expiry (`Human::updateCohortSet` at start of `Host::update`)**: iterates `subPopExp` in ascending `ComponentId` order; if `!(expiry >= sim::ts0())`, records `mon::recordEvent("nSubPopRemovalTooOld", *this)`, clears `cohortSet` bit, erases `id` (does **not** flush `Episode`).
  - **First-event expiry (`Human::removeFirstEvent(code)`)** (`ON_FIRST_BOUT`, `ON_FIRST_TREATMENT`, `ON_FIRST_INFECTION`): for each `id` registered under `code`, if `id` in `subPopExp`:
    - If `expiry >= sim::nowOrTs0()`: calls `clinicalModel->flushReports()` **and** `mon::recordEvent("nSubPopRemovalFirstEvent", *this)`.
    - Clears `cohortSet` bit and erases `id`.
  - **ITN Attrition (`Human::removeFromCohort(id)`)**: **[PARITY QUIRK]** only executes `subPopExp.erase(id)` — does NOT clear `cohortSet` bit or record any `nSubPopRemoval*` event!

---

### 2.2 `InterventionManager` Scheduling, Filters & Gaussian Copula (`InterventionManager.cpp`, `Deployments.h`)

#### Initialization & Execution Order
- **`continuous` list**: sorted with `std::stable_sort` by `deployAge = sim::fromYearsN(targetAgeYrs)`.
- **`timed` list**: built in XML category order: `changeHS` $\to$ `changeEIR` $\to$ `human` (within each `<timed>`, `<deploy>` steps including `repeatStep`/`repeatEnd` with `while (date < end)` are ordered via `std::multimap<SimTime, ...>` and checked for duplicate dates) $\to$ `uninfectVectors` $\to$ `vectorPop` $\to$ `addNonHumanHosts` $\to$ `nonHumanHostsModifications` $\to$ `vectorTrap`; then the entire `timed` vector is sorted with **`std::stable_sort`** by `date` (`DummyTimedDeployment` at `sim::future()` appended).
- **Per-step `InterventionManager::deploy(population, transmission)`**:
  - If `sim::intervTime() < sim::zero()`: `return`.
  1. **`importedInfections.import(population)`** (`Host/ImportedInfections.h`):
     - Piecewise-constant `rate` entries `(date - startDate, perThousandPerYear * yearsPerStep() / 1000.0)`.
     - `now = (period > 0) ? mod_nn(sim::intervTime(), period) : sim::intervTime()`, select active `rateNow`.
     - If `rateNow > 0.0`: for each `human` in `population`: **RNG** `if (human.rng.bernoulli(rateNow))` $\implies$ `withinHostModel->importInfection(human.rng)` (**consumes RNG inside WHM**), `mon::recordEvent("nNewInfections", human)`, `mon::recordEvent("nNewInfections_Imported", human)`.
  2. **Timed deployments**: `while (timed[nextTimed]->date <= sim::intervDate()) { timed[nextTimed]->deploy(population, transmission); nextTimed++; }`
  3. **Continuous deployments**: for each `human` in `population`: `while (human.nextCtsDist < continuous.size()) { if (!continuous[human.nextCtsDist].filterAndDeploy(human)) break; human.nextCtsDist++; }`

#### Availability Percentiles (`PerHostAnophParams::calcAvailabilityPercentiles`, `PerHost.h:82-112`)
- Computed once after population init (and after checkpoint load) using an isolated RNG `MasterRng seed(0, 0); seed.seed(0, 0); LocalRng rng(seed);`:
  - Draws `100,000` samples of $\sum_{s=0}^{S-1} \text{entoAvailabilityFactor}_s \cdot \text{entoAvailability}_s.\text{sample}(\text{rng})$, sorts ascending.
  - `pct[0] = 0.0`; for $p \in \{1, \dots, 99\}$, `pct[p] = samples[int(p * 100000 / 100)]` (`samples[1000 * p]`); for $p = 100$, `getEntoAvailabilityPercentile(100) = DBL_MAX`.

#### Exact Filter & RNG Order Across All 4 Human Deployment Modes

1. **`TimedHumanDeployment::deploy(population, ...)` (`Deployments.h:270-341`)**:
   - Precomputed at init: if `coverageCorr` and `coverageVar > 0` are both present, `copula = true` (requires `numSpecies() == 1`, `coverage >= 0`, `coverageVar > 0`, `|coverageCorr| <= 1`). With $\mu = \text{coverage}$, $v = \text{coverageVar}$:
     $$\alpha = \left(\frac{1 - \mu}{v} - \frac{1}{\mu}\right)\mu^2, \qquad \beta = \alpha\left(\frac{1}{\mu} - 1\right) \qquad (\text{throws if } \alpha < 0 \text{ or } \beta < 0)$$
   - Loop over `human` in `population`:
     1. **Age check**: `age = human.age(sim::now())`; require `age >= minAge && age < maxAge` (`minAge = fromYearsN(minAge)`, `maxAge = fromYearsN(maxAge)` or `sim::future()`). If false $\to$ `continue` (0 RNG).
     2. **Availability sum**: `avail = sum_{s} human.perHostTransmission.anophEntoAvailability[s]`.
     3. **Gaussian Copula** (`if (copula && coverage > 0)`) — **[PARITY QUIRK: evaluated BEFORE availability-percentile and `subPop` filters!]**:
        - $u_x = \text{entoAvailability}_0.\text{cdf}(\text{anophEntoAvailabilityRaw}[0])$:
          - `LognormalSampler`: $0$ if $x \le 0$; if $\sigma = 0$: $((\ln x \ge \mu_{\text{LN}}) ? 1 : 0)$; else `gsl_cdf_lognormal_P(x, mu_LN, sigma)`.
          - `GammaSampler`: $0$ if $x \le 0$; if $\text{isnan}(\theta)$ ($\text{CV}=0$): $((x \ge \mu_\Gamma) ? 1 : 0)$; else `gsl_cdf_gamma_P(x, k, theta)`.
        - If $u_x < 1.0$:
          - $x_x = \Phi^{-1}(u_x)$ (`gsl_cdf_ugaussian_Pinv(ux)`).
          - Look up `cid = subPop` (equals `ComponentId::wholePop()` if unrestricted!) in `human.perHostTransmission.copulaGaussianSamples`:
            - If absent: **RNG** `g = human.rng.gauss(0.0, 1.0)`, insert `copulaGaussianSamples[cid] = g`.
            - Else: `g = copulaGaussianSamples[cid]`.
          - $y_y = \rho \cdot x_x + g \sqrt{1 - \rho^2}$, $\quad u_y = \Phi(y_y)$ (`gsl_cdf_ugaussian_P(yy)`).
          - `probability = gsl_cdf_beta_Pinv(uy, alpha, beta)`.
        - Else ($u_x \ge 1.0$, e.g. homogeneous $\text{CV} = 0$):
          - **RNG**: `probability = human.rng.beta(alpha, beta)`.
        - *(If `!copula || coverage == 0`, `probability = coverage`.)*
     4. **Availability percentile check**: require `avail >= pct(minAvailability) && avail <= pct(maxAvailability)`.
     5. **Sub-pop check**: require `subPop == wholePop() || (human.isInSubPop(subPop) != complement)`.
     6. **Deployment Bernoulli RNG**: `if (human.rng.bernoulli(probability))` (`uniform_01() < probability`) $\implies$ `intervention->deploy(human, mon::Deploy::TIMED, vaccLimits)`.

2. **`TimedCumulativeHumanDeployment::deploy(population, ...)` (`Deployments.h:350-388`)**:
   - *(Note: `copula` is NOT used in `TimedCumulativeHumanDeployment`.)*
   - **Pass 1 (0 RNG)**: For each `human` in `population`: if `age >= minAge && age < maxAge`, `avail` (`sum anophEntoAvailability[s]`) in `[pct(minAvail), pct(maxAvail)]`, and `subPop` matches:
     - `total += 1`; if `!human.isInSubPop(cumCovInd)`: `unprotected.push(&human)`.
   - **Pass 2**: If `total == 0`, return. Let $p_{\text{prot}} = \frac{\text{total} - |\text{unprotected}|}{\text{total}}$.
     - If $p_{\text{prot}} < \text{coverage}$:
       $$p_{\text{add}} = \frac{\text{coverage} - p_{\text{prot}}}{1.0 - p_{\text{prot}}}$$
       For each `human` in `unprotected`: **RNG** `if (human->rng.uniform_01() < p_add)` $\implies$ `intervention->deploy(*human, mon::Deploy::TIMED, vaccLimits)`.

3. **`ContinuousHumanDeployment::filterAndDeploy(human)` (`Deployments.h:633-654`)**:
   - `age = human.age(sim::now())`.
   - If `deployAge > age`: return `false` (do not increment `nextCtsDist`).
   - If `deployAge == age`:
     - **[PARITY QUIRK]**: sums `anophEntoAvailabilityRaw[s]` (unscaled raw sample, NOT `anophEntoAvailability[s]`!) and checks `avail >= pct(minAvailability) && avail <= pct(maxAvailability)`!
     - Check `begin <= sim::intervDate() && sim::intervDate() < end`.
     - Check `subPop == wholePop() || (human.isInSubPop(subPop) != complement)`.
     - **RNG**: `if (human.rng.uniform_01() < coverage)` $\implies$ `intervention->deploy(human, mon::Deploy::CTS, vaccLimits)`. *(Note: `copula` is never used in continuous deployment.)*
   - Return `true` (increments `nextCtsDist`; if `deployAge < age`, deployment was missed and skipped).

4. **`TriggeredDeployments::SubList::deploy(human, method, vaccLimits)` (`WithinHost/Treatments.cpp`)**:
   - At init: if `coverage <= 0.0 || minAge >= maxAge`, `components` is cleared; if `components.empty()`, `SubList` is skipped entirely (0 RNG).
   - At runtime: `age = human.age(sim::nowOrTs1())`; if `age >= minAge && age < maxAge`:
     - **[PARITY QUIRK]**: `if (coverage >= 1.0 || human.rng.bernoulli(coverage))` — **consumes 0 RNG when `coverage >= 1.0`**!
     - Calls `HumanIntervention::deploy(human, method, vaccLimits)`.

---

### 2.3 Decay Functions & Heterogeneity (`DecayFunction.cpp`, `sampler.cpp`)

- Half-life / duration $L$ in days (`UnitParse::durationToDays(L, YEARS)`).
- Heterogeneity sampler `het`: `LognormalSampler::fromMeanCV(1.0, CV)`.
  - **CRITICAL RNG RULE**: When `CV == 0.0`, `hetSample(rng)` returns `DecayFuncHet(1.0)` with **0 RNG calls**! When `CV > 0`, $a = 1 + \text{CV}^2$, $\mu = \ln(1/\sqrt{a})$, $\sigma = \sqrt{\ln a}$, `hetSample(rng)` draws `rng.log_normal(mu, sigma)` (1 `gauss` RNG). When given a pre-drawn `NormalSample x` (as in `HumanITN`), `hetSample(x)` computes $\exp(\sigma x + \mu)$ (`1.0` if $\sigma = 0$).
- Base `compute(ageDays = t, hetFactor = h)` and `sampleAgeOfDecay(rng)` (where `T(d) = sim::roundToTSFromDays(d)`):
  - **`constant`**: `compute = (t * h == +inf) ? 0.0 : 1.0`; `sampleAgeOfDecay = sim::future()` (**0 RNG**).
  - **`step`**: `compute = (t * (1/L) * h < 1.0) ? 1.0 : 0.0`; `sampleAgeOfDecay = T(L)` (**0 RNG**).
  - **`linear`**: $x = t \cdot (1/L) \cdot h$, `compute = (x < 1.0) ? 1.0 - x : 0.0`; `sampleAgeOfDecay = T(rng.uniform_01() * L)` (**1 RNG**).
  - **`exponential`**: $\lambda = \frac{\ln 2}{L}$, `compute` $= \exp(-t \cdot \lambda \cdot h)$; `sampleAgeOfDecay` $= T(-\ln(\text{rng.uniform\_01}()) / \lambda)$ (**1 RNG**).
  - **`weibull`**: $c = \frac{(\ln 2)^{1/k}}{L}$, $p = -(t \cdot c \cdot h)^k$, `compute` $= (p < -700.0) ? 0.0 : \exp(p)$; `sampleAgeOfDecay` $= T\left((-\ln(\text{rng.uniform\_01}()))^{1/k} / c\right)$ (**1 RNG**).
  - **`hill`**: `compute` $= \frac{1}{1 + (t \cdot (1/L) \cdot h)^k}$; `sampleAgeOfDecay` $= T\left(\left(\frac{1}{\text{rng.uniform\_01}()} - 1\right)^{1/k} L\right)$ (**1 RNG**).
  - **`smooth-compact`**: $x = t \cdot (1/L) \cdot h$, `compute` $= (x < 1.0) ? \exp\left(k - \frac{k}{1 - x^2}\right) : 0.0$; `sampleAgeOfDecay` $= T\left(\sqrt{1 - \frac{k}{k - \ln(\text{rng.uniform\_01}())}} \cdot L\right)$ (**1 RNG**).
  - **`plus` / `minus` / `divides` / `multiplies`**: `clamp(f1.eval(t) op f2.eval(t), 0.0, 1.0)`; `sampleAgeOfDecay = T(max(f1.sampleAgeOfDecay(rng), f2.sampleAgeOfDecay(rng)))`.
  - **`emax`**: $C^s = (C_0 \cdot f.\text{eval}(t))^{\text{slope}}$, `compute` $= \text{clamp}\left(\frac{E_{\max} C^s}{C^s + \text{IC}_{50}^{\text{slope}}}, 0.0, 1.0\right)$.
- Wrapper `eval(age, het)`: let $v = \text{compute}(\text{age}, \text{het}) \cdot \text{initialEfficacy}$; returns `increasing ? (1.0 - v) : v`.

---

### 2.4 Per-Component Equations & Exact RNG Call Order

#### 2.4.1 `ITN` (`ITN.cpp`)
- **Static Setup**:
  - `initialInsecticide`: `NormalSampler(mu, sigma)`. `maxInsecticide = R::qnorm5(0.999, mu, sigma, true, false)`. *(Note: `NormalSampler::sample(rng)` consumes **0 RNG** if `sigma == 0`, else `rng.gauss(mu, sigma)`.)*
  - `holeRate` & `ripRate`: `createSampler(...)` (`Lognormal` or `Gamma`), `scaleMean(sim::yearsPerStep())`.
  - `proportionProtected = proportionUse * propActive` (`propActive` must be `0` or `1`; `proportionUnprotected = 1 - proportionProtected`).
- **Initial Deployment (`HumanITN` ctor — first time human receives this ITN `ComponentId`)**:
  - `deployTime = sim::now()` (`PerHostInterventionData` ctor), `nHoles = 0`, `holeIndex = 0.0`.
  1. **RNG #1**: `NormalSample x = NormalSample::generate(rng)` $\implies$ **ALWAYS calls `rng.gauss(0.0, 1.0)`** even if all CVs are `0`!
     - `holeRate = params.holeRate->sample(x)`
     - `ripRate = params.ripRate->sample(x)`
     - `insecticideDecayHet = params.insecticideDecay->hetSample(x)`
  2. **RNG #2**: `disposalTime = sim::now() + params.attritionOfNets->sampleAgeOfDecay(rng)`.
  3. **RNG #3**: `initialInsecticide = clamp(params.initialInsecticide.sample(rng), 0.0, maxInsecticide)`.
- **Redeployment (`HumanITN::redeploy` — human already has a `HumanITN` for this `ComponentId`)**:
  - `deployTime = sim::nowOrTs1()`, `nHoles = 0`, `holeIndex = 0.0` (`holeRate`, `ripRate`, `insecticideDecayHet` are **retained**!).
  1. **RNG #1**: `disposalTime = sim::nowOrTs1() + params.attritionOfNets->sampleAgeOfDecay(rng)`.
  2. **RNG #2**: `initialInsecticide = clamp(params.initialInsecticide.sample(rng), 0.0, maxInsecticide)`.
- **Per-Timestep Hole Formation & Attrition (`HumanITN::update`, called in `PerHost::update` during `VectorModel::calculateEIR`)**:
  - If `deployTime != sim::never()`:
    - If `sim::ts0() >= disposalTime`: `deployTime = sim::never(); human.removeFromCohort(id()); return;` (0 RNG).
    - **RNG #1**: `newHoles = (holeRate > 0.0) ? human.rng.poisson(holeRate) : 0;`
    - `nHoles += newHoles; holeIndex += newHoles;`
    - `lambda = nHoles * ripRate;`
    - **RNG #2**: `if (lambda > 0.0) holeIndex += params.ripFactor * human.rng.poisson(lambda);`
- **Insecticide Content & Mosquito Effects**:
  - If `deployTime == sim::never()`: all 4 factors return `1.0`.
  - Else $C = \text{initialInsecticide} \cdot \text{insecticideDecayHet.eval}(\text{sim::nowOrTs1}() - \text{deployTime})$, $H = \text{holeIndex}$, and every species effect $E$ is wrapped in `byProtection(E) = E * proportionProtected + proportionUnprotected`:
  - **Killing / Fecundity (`SurvivalFactor::survivalFactor(H, C)`)**:
    - *Standard*: $h = e^{-H \cdot s_H}$, $p = 1 - e^{-C \cdot s_P}$, $K = BF + HF \cdot h + PF \cdot p + IF \cdot h p$,
      $$S = \text{clamp}\left(\frac{1 - K}{1 - BF},\ 0.0,\ 1.0\right)$$
      *(If `<fecundityReduction>` is omitted, `init1()` sets $S \equiv 1.0$.)*
    - *Logit*: $h = \min(\ln(H + 1), \ln(H_{\max} + 1))$, $p = \ln(C + 1)$, $x = \exp(BF + HF \cdot h + PF \cdot p + IF \cdot h p)$, $x_0 = \exp(BF + HF \cdot \ln(H_{\max} + 1))$,
      $$S = \frac{x_0 + 1}{x + 1} \quad (\text{not clamped})$$
  - **Deterrency (`RelativeAttractiveness::relativeAttractiveness(H, C)`)**:
    - *`SINGLE_STAGE`*: $h = e^{-H \cdot s_H}$, $p = 1 - e^{-C \cdot s_P}$,
      $$A = \exp(\ln(HF) \cdot h + \ln(PF) \cdot p + \ln(IF) \cdot h p)$$
    - *`TWO_STAGE` / `TWO_STAGE_LOGIT`*: $A = f_{\text{ent}} \cdot f_{\text{att}}$, where:
      - Entering (`TWO_STAGE`): $f_{\text{ent}} = \exp\left(\ln(PF_{\text{ent}}) \cdot (1 - e^{-C \cdot s_{\text{ent}}})\right)$.
      - Entering (`TWO_STAGE_LOGIT`): $q = \exp(BF_{\text{ent}} + PF_{\text{ent}} \ln(C + 1))$, $f_{\text{ent}} = \frac{q}{q + 1} \cdot \frac{e^{BF_{\text{ent}}} + 1}{e^{BF_{\text{ent}}}}$.
      - Attacking (standard): $f_{\text{att}} = \frac{BF + HF \cdot h + PF \cdot p + IF \cdot h p}{BF}$.
      - Attacking (logit): $f_{\text{att}} = \frac{x}{x + 1} \cdot \frac{x_0 + 1}{x_0}$.

#### 2.4.2 `IRS` (`IRS.cpp`)
- `maxInsecticide = R::qnorm5(0.999, mu, sigma, true, false)`.
- **First deployment (`HumanIRS` ctor)**: `deployTime = sim::now()`.
  1. **RNG #1**: `initialInsecticide = clamp(params.initialInsecticide.sample(rng), 0.0, maxInsecticide)`.
  2. **RNG #2**: `insecticideDecayHet = params.insecticideDecay->hetSample(rng)` (0 RNG if `CV == 0`).
- **Redeployment (`redeploy`)**: `deployTime = sim::nowOrTs1()`; **RNG #1**: resamples `initialInsecticide` (retains `insecticideDecayHet`).
- **Effects** (never disposes; $C = \text{initialInsecticide} \cdot \text{insecticideDecayHet.eval}(\text{sim::nowOrTs1}() - \text{deployTime})$, $p = 1 - e^{-C \cdot s_P}$, wrapped in `byProtection`):
  - `relativeAttractiveness`: $\exp(\ln(PF) \cdot p)$
  - `preprandial` / `postprandial` / `fecundity` (if present, else $1.0$): $\frac{1 - (BF + PF \cdot p)}{1 - BF}$ (asserted in `[0, 1]`, not clamped).

#### 2.4.3 `GVI` (`GVI.cpp`)
- **First deployment (`HumanGVI` ctor)**: `deployTime = sim::now()`; **RNG #1**: `decayHet = params.decay->hetSample(rng)` (0 RNG if `CV == 0`).
- **Redeployment (`redeploy`)**: `deployTime = sim::nowOrTs1()` (0 RNG).
- **Effects**: Let $S = \text{decayHet.eval}(\text{sim::nowOrTs1}() - \text{deployTime})$. Wrapped in `byProtection`:
  - `relativeAttractiveness = byProtection(1.0 - deterrency * S)`
  - `preprandialSurvivalFactor = byProtection(1.0 - preprandialKilling * S)`
  - `postprandialSurvivalFactor = byProtection(1.0 - postprandialKilling * S)`
  - `relFecundity = byProtection(1.0 - fecundityReduction * S)`
- **Multi-Component Vector Combination (`PerHost.cpp`)**: Across all active `PerHostInterventionData` on a host, factors multiply:
  - `entoAvailabilityHetVecItv(s) = anophEntoAvailability[s] * prod_c c.relativeAttractiveness(s)`
  - `probMosqBiting(s) = anophProbMosqBiting[s] * prod_c c.preprandialSurvivalFactor(s)`
  - `probMosqResting(s) = anophProbMosqResting[s] * prod_c c.postprandialSurvivalFactor(s)`
  - `relMosqFecundity(s) = prod_c c.relFecundity(s)`

#### 2.4.4 `Vaccine` (`PEV`, `BSV`, `TBV`) (`Vaccine.cpp`)
- **[PARITY QUIRK — `reportComponent`]**: Static `VaccineComponent::reportComponent` is set to the `ComponentId` of the **first** `VaccineComponent` constructed during XML parsing (`if (reportComponent == wholePop()) reportComponent = id`). Only that single component records `nMassVaccinations` / `nEPIVaccinations`!
- **Genotype mapping**: If `ModelOptions::option(VACCINE_GENOTYPE)` is `false`, `perGenotypeInitialMeanEfficacy` has outer size `1` (`[0][dose]`); if `true`, outer size `Genotypes::N()` matched via `<phenotype>` locus/allele restrictions.
- **`possiblyVaccinate(human, componentId, vaccLimits)` & RNG Order**:
  1. Find existing `effect` for `componentId` in `human.vaccine.effects`. `numPrevDoses = effect ? effect->numDosesAdministered : 0`.
  2. Dose gate: `if (numPrevDoses < vaccLimits.minPrevDoses || numPrevDoses >= vaccLimits.maxCumDoses) return false;` (0 RNG).
  3. If `effect == nullptr` (first dose of this component):
     - Append new `PerEffectPerHumanVaccine`: **RNG #1**: `hetSample = decayFunc->hetSample(human.rng)` (0 RNG if `CV == 0`).
  4. Resize `perGenotypeInitialEfficacy` to `Genotypes::N()`. Loop `g = 0 .. perGenotypeInitialMeanEfficacy.size() - 1`:
     - `d = min(numPrevDoses, perGenotypeInitialMeanEfficacy[g].size() - 1)`
     - `ime = perGenotypeInitialMeanEfficacy[g][d]`
     - If `ime == 0.0`: `eff = 0.0` (0 RNG).
     - Else if `ime < 1.0`: **RNG**: `eff = human.rng.betaWithMean(ime, efficacyB)` where $a = \frac{\text{ime} \cdot B}{1 - \text{ime}}, b = B$, `gsl_ran_beta(rng, a, b)`.
     - Else (`ime >= 1.0`): `eff = 1.0` (0 RNG).
     - `perGenotypeInitialEfficacy[g] = eff`.
  5. `numDosesAdministered = numPrevDoses + 1`; `timeLastDeployment = sim::nowOrTs1()`; return `true`.
- **Factor Evaluation (`PerHumanVaccine::getFactor(type, genotype = 0)`)**:
  *(Note: if `!VACCINE_GENOTYPE`, callers pass `genotype = 0`; uses **`sim::ts1()`**, NOT `nowOrTs1()`!)*
  $$F(\text{type}, g) = \prod_{e:\ \text{type}(e) = \text{type}} \left(1.0 - e.\text{perGenotypeInitialEfficacy}[g] \cdot e.\text{hetSample.eval}(\text{sim::ts1}() - e.\text{timeLastDeployment})\right)$$
  - `PEV`: multiplies expected liver-stage infections (`CommonWithinHost` / `DescriptiveWithinHost` / `WHFalciparum`).
  - `BSV`: multiplies blood-stage parasite density per infection.
  - `TBV`: multiplies human-to-mosquito `probTransmission`.

#### 2.4.5 `MDA`, `MSAT` & Other Human Components (`HumanInterventionComponents.h`)
- **`RecruitmentOnlyComponent`**: records `nMassRecruitOnly` / `nCtsRecruitOnly`.
- **`ScreenComponent` (MSAT)**:
  1. `mon::recordDeploy("nMassScreenings", "nCtsScreenings", human, method)`.
  2. **RNG (if stochastic diagnostic)**: `withinHostModel->diagnosticResult(human.rng, diagnostic)` $\to$ executes `positive.deploy(human, method, vaccLimits)` or `negative.deploy(human, method, vaccLimits)`.
- **`TreatSimpleComponent` (MDA)**: `mon::recordDeploy("nMDAs", "nCtsMDA", human, method)`; `withinHostModel->treatSimple(human, timeLiver, timeBlood)`.
- **`TreatPKPDComponent` (MDA)**: `mon::recordDeploy("nMDAs", "nCtsMDA", human, method)`; `withinHostModel->treatPkPd(schedule, dosage, sim::inYears(human.age(sim::nowOrTs1())), delay_h / 24.0)`.
- **`DecisionTreeComponent` (MDA/MSAT)**: `out = tree->exec(CMHostData(human, sim::inYears(human.age(sim::nowOrTs1())), clinicalModel->getLatestState()))`; if `out.treated` $\to$ `recordDeploy("nMDAs", "nCtsMDA", ...)`; if `out.screened` $\to$ `recordDeploy("nMassScreenings", "nCtsScreenings", ...)`.
- **`ClearImmunityComponent`**: `withinHostModel->clearImmunity()` (no monitoring call).

#### 2.4.6 Population & Vector Timed Interventions (`Deployments.h`, `VectorModel.cpp`, `AnophelesModel.cpp`)
- **`changeHS` (`TimedChangeHSDeployment`)**: calls `ClinicalModel::setHS(*newHS)` (also re-applied for `date < intervDate()` on checkpoint load).
- **`changeEIR` (`TimedChangeEIRDeployment`)**: calls `transmission.changeEIRIntervention(*newEIR)`: sets `simulationMode = transientEIRknown`; averages `EIRDaily` per step into `interventionEIR[istep]` with floor `max(val, 0.01 * averageEIR)`. In `VectorModel`, requires `numSpecies == 1` and `simulationMode` in `{forcedEIR, transientEIRknown}`.
- **`uninfectVectors` (`TimedUninfectVectorsDeployment`)**: in `NonVectorModel`, zeroes `laggedKappa`; in `VectorModel`, zeroes `O_v_i`, `O_v_l`, `S_v_i`, `S_v_l`, `P_dif_i`, `P_dif_l` across all species and genotypes.
- **`vectorPop` (`TimedVectorDeployment`)**: requires `interventionMode == dynamicEIR`. Loops species `s = 0..S-1`, calling `SimpleDecayingValue::deploy(vectorModel->m_rng, sim::now())` (draws 1 `hetSample` on `vectorModel->m_rng` iff effect present and `CV > 0`) in exact order:
  1. `emergenceReduction[inst]`
  2. `probAdditionalDeathSugarFeedingIntervs[inst]`
  3. `seekingDeathRateIntervs[inst]`
  4. `probDeathOvipositingIntervs[inst]`
  - Applied in `AnophelesModel` at `sim::ts0()`:
    - $\mu_{v,\text{interv}} = \mu_v \prod_k (1 + \text{seekingDeathRateIntervs}_k(\text{ts0}))$
    - $p_{\text{sugar}} = \sum_k \text{probAdditionalDeathSugarFeedingIntervs}_k(\text{ts0})$ (`<= 1.0`). If $p_{\text{sugar}} > 0$, solves $(1 - e^{-(a_t + L_0)\theta_d}) \frac{a_t}{a_t + L_0} = p_{\text{sugar}}$ (where $L_0 = a_h + a_{\text{nhh}} + a_{\text{traps}} + \mu_{v,\text{interv}}$) via `gsl_root_fdfsolver_secant` (`a_t = 0.5` init, `max_iter = 20`, `epsrel = 1e-4`), adding $a_t$ to `leaveRate`.
    - $P_{\text{ovip}} = P_{\text{ovip},0} \prod_k (1 - \text{probDeathOvipositingIntervs}_k(\text{ts0}))$
    - Larval emergence multiplier: $\prod_k (1 - \text{emergenceReduction}_k(\text{ts0}))$.
- **`vectorTrap` (`TimedTrapDeployment`)**: requires `interventionMode == dynamicEIR`. `number = population.size() * ratioToHumans`. Loops species `s = 0..S-1`:
  - `adultAvail = entoAvailability[s].mean() * entoAvailabilityFactor[s]`
  - `initialAvail = number * adultAvail * relAvail`
  - **Vector RNG**: `availHet = availDecay->hetSample(vectorModel->m_rng)`
  - Appends `TrapInstance { initialAvail, availHet, deployTime: sim::now(), expiry: sim::now() + lifespan }`.
  - In `advancePeriod`: if `sim::ts0() > expiry` (**strict `>`**), erase; else `leaveRate += initialAvail * availHet.eval(sim::ts0() - deployTime)`.
- **`addNonHumanHosts` & `nonHumanHostsModifications`**:
  - `addNonHumanHosts`: requires `dynamicEIR`; for each species `s = 0..S-1`, `avail_i = entoAvailabilityFactor[s] * population.size() * entoAvailability[s].mean() * mosqRelativeAvailabilityHuman`, `expiry = sim::now() + lifespan` (expires when `sim::ts0() >= expiry` — note **`>=`** vs `>` for traps!). (0 RNG).
  - `nonHumanHostsModifications`: for each species `s = 0..S-1`, deploys `SimpleDecayingValue` on `vectorModel->m_rng` at `sim::now()` in order: `reduceNhhAvailability`, `reduceP_B_I`, `reduceP_C_I`, `reduceP_D_I`, `reduceFecundity`.

---

## 3. Exhaustive `mon::record*` Calls Made by Clinical & Interventions

### Helper Dispatch (`mon::recordDeploy(timedMeasure, ctsMeasure, human, method, val = 1)`)
- `method == mon::Deploy::TIMED` $\implies$ `mon::recordEvent(timedMeasure, human, val)`
- `method == mon::Deploy::CTS` $\implies$ `mon::recordEvent(ctsMeasure, human, val)`
- `method == mon::Deploy::TREAT` $\implies$ `mon::recordEvent("nTreatDeployments", human, val)`

### 3.1 Clinical `mon::record*` Calls
| Source File | Function / Condition | Exact Call & Measure Name |
|---|---|---|
| `ClinicalModel.cpp:146` | Indirect mortality timer `doomed <= -35` | `recordEvent("nIndDeaths", human)` |
| `ClinicalModel.cpp:153` | Newborn neonatal mortality event | `recordEvent("nIndDeaths", human)` |
| `Monitoring.cpp:208` | End of sim if `IMR` measure active | Survey `1`, group `1`: `InfantMortality::allCause()` (`"IMR"` / `"allCauseIMR"`) |
| `Episode.cpp:74` | `MALARIA && COMPLICATED` | `recordEvent("nSevere", surveyPeriod, ageGroup, cohortSet)` |
| `Episode.cpp:76` | `MALARIA && COMPLICATED && SEVERE` | `recordEvent("nSevereWithoutComorbidities", surveyPeriod, ageGroup, cohortSet)` |
| `Episode.cpp:79` | `MALARIA && !COMPLICATED` | `recordEvent("nUncomp", surveyPeriod, ageGroup, cohortSet)` |
| `Episode.cpp:81-86` | `MALARIA && !COMPLICATED` by `infectionType` | `recordEvent("nUncomp_Indigenous" \| "nUncomp_Introduced" \| "nUncomp_Imported", surveyPeriod, ageGroup, cohortSet)` |
| `Episode.cpp:92-97` | `MALARIA && IN_HOSPITAL && DIRECT_DEATH` | `recordEvent("nDirDeaths", ...)` + `recordEvent("nHospitalDeaths", ...)` (+ if `EVENT_FIRST_DAY`: `"Clinical_FirstDayDeaths"`, `"Clinical_HospitalFirstDayDeaths"`) |
| `Episode.cpp:99-101` | `MALARIA && IN_HOSPITAL && !DIRECT_DEATH && SEQUELAE` | `recordEvent("nSeq", ...)` + `recordEvent("nHospitalSeqs", ...)` |
| `Episode.cpp:102-104` | `MALARIA && IN_HOSPITAL && !DIRECT_DEATH && !SEQUELAE && RECOVERY` | `recordEvent("nHospitalRecovs", ...)` |
| `Episode.cpp:107-111` | `MALARIA && !IN_HOSPITAL && DIRECT_DEATH` | `recordEvent("nDirDeaths", ...)` (+ if `EVENT_FIRST_DAY`: `"Clinical_FirstDayDeaths"`) |
| `Episode.cpp:112-114` | `MALARIA && !IN_HOSPITAL && !DIRECT_DEATH && SEQUELAE` | `recordEvent("nSeq", ...)` |
| `Episode.cpp:118` | `!MALARIA && SICK` (NMF) | `recordEvent("nNMFever", surveyPeriod, ageGroup, cohortSet)` |
| `Episode.cpp:120` | `!MALARIA && SICK && DIRECT_DEATH` | `recordEvent("nNmfDeaths", surveyPeriod, ageGroup, cohortSet)` |
| `CM5DayCommon.cpp:139-145` | Every 5d `severeMalaria` bout | `recordStat("expectedHospitalDeaths", human, exHospitalDeath)`, `recordStat("expectedDirectDeaths", human, exDeath)`, `recordStat("expectedSequelae", human, exSeq)` |
| `CM5DayCommon.cpp:152` | 5d `severeMalaria` when `prandom >= q[2]` | `recordEvent("nTreatments3", human)` |
| `ImmediateOutcomes.cpp:193` | UC seeks care & `useDiagnosticUC` | `recordEvent("nTreatDiagnostics", human)` |
| `ImmediateOutcomes.cpp:200` | UC treated (`FirstLine` / `SecondLine`) | `recordEvent("nTreatments1" \| "nTreatments2", human)` |
| `DecisionTree5Day.cpp:102` | UC `output.treated` (`FirstLine` / `SecondLine`) | `recordEvent("nTreatments1" \| "nTreatments2", human)` |
| `DecisionTree5Day.cpp:105` | UC `output.screened` | `recordEvent("nTreatDiagnostics", human)` |
| `CMDecisionTree.cpp:407` | `<report>` leaf node for each `outId` | `recordEvent("nCMDTReport", human, 1, outId)` |
| `EventScheduler.cpp:177` | ES severe recovery (`ts0() == timeOfRecovery`) | `recordStat("expectedSequelae", human, pSequelae)` |
| `EventScheduler.cpp:258-262` | ES `auxOut.treated` | `recordEvent("nTreatments3" \| "nTreatments2" \| "nTreatments1", human)` |
| `EventScheduler.cpp:266` | ES `auxOut.screened` | `recordEvent("nTreatDiagnostics", human)` |
| `EventScheduler.cpp:286-288` | ES Day-1 severe CFR | `recordStat("expectedDirectDeaths", human, pDeath)` + `recordStat("expectedHospitalDeaths", human, pDeath)` |
| `EventScheduler.cpp:373-375` | ES subsequent-day severe CFR (`totalDensity > 0`) | `recordStat("expectedDirectDeaths", human, pDeath)` (+ if `EVENT_IN_HOSPITAL`: `recordStat("expectedHospitalDeaths", human, pDeath)`) |

### 3.2 Interventions, SubPop & Imported-Infection `mon::record*` Calls
| Source File | Component / Event | Exact Call & Measure Name |
|---|---|---|
| `HumanInterventionComponents.h:50` | `RecruitmentOnlyComponent` | `recordDeploy("nMassRecruitOnly", "nCtsRecruitOnly", human, method)` |
| `HumanInterventionComponents.h:83` | `ScreenComponent` | `recordDeploy("nMassScreenings", "nCtsScreenings", human, method)` |
| `HumanInterventionComponents.h:119` | `TreatSimpleComponent` | `recordDeploy("nMDAs", "nCtsMDA", human, method)` |
| `HumanInterventionComponents.h:155` | `TreatPKPDComponent` | `recordDeploy("nMDAs", "nCtsMDA", human, method)` |
| `HumanInterventionComponents.h:191-196` | `DecisionTreeComponent` | If `out.treated`: `recordDeploy("nMDAs", "nCtsMDA", human, method)`; if `out.screened`: `recordDeploy("nMassScreenings", "nCtsScreenings", human, method)` |
| `ITN.cpp:541` | `ITNComponent` | `recordDeploy("nMassITNs", "nEPI_ITNs", human, method)` *(note `"nEPI_ITNs"`!)* |
| `IRS.cpp:234` | `IRSComponent` | `recordDeploy("nMassIRS", "nCtsIRS", human, method)` |
| `GVI.cpp:63` | `GVIComponent` | `recordDeploy("nMassGVI", "nCtsGVI", human, method)` |
| `Vaccine.cpp:200-209` | `VaccineComponent` (`PEV`/`BSV`/`TBV`) | 1. If `administered && reportComponent == id()`: `recordDeploy("nMassVaccinations", "nEPIVaccinations", human, method)`<br>2. **[PARITY QUIRK]**: If `method == mon::Deploy::TREAT`: ALSO calls `recordEvent("nTreatDeployments", human)` unconditionally (so if `administered && reportComponent == id()`, `"nTreatDeployments"` is recorded **twice**!) |
| `Human.cpp:150` | `removeFirstEvent` (active sub-pop) | `recordEvent("nSubPopRemovalFirstEvent", *this)` |
| `Human.cpp:166` | `updateCohortSet` (expired sub-pop) | `recordEvent("nSubPopRemovalTooOld", *this)` |
| `ImportedInfections.h:121-122` | Imported infection Bernoulli hit | `recordEvent("nNewInfections", human)` + `recordEvent("nNewInfections_Imported", human)` |

