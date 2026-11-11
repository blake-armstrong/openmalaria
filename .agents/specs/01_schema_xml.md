# Complete Technical Reference for `crates/om-schema` (OpenMalaria Schema v50)

All 9 XSD files (`schema/*.xsd`), `model/util/XMLChecker.h` (plus `DocumentLoader.cpp` & `ModelNameProvider.h`), and the 8 target `test/scenario*.xml` files (plus a scan across all 53 `test/scenario*.xml` files) have been inspected in full.

---

## 1. Root Tag, Namespaces, and Global Structural Conventions

### Root Element & Namespace Behavior
- **Target namespace**: `http://openmalaria.org/schema/scenario_50` (`xmlns:om="http://openmalaria.org/schema/scenario_50"` in `scenario.xsd`).
- **`elementFormDefault` is NOT set** in any of the 9 XSDs, so XSD defaults to `elementFormDefault="unqualified"`:
  - **Only the global root element** belongs to `http://openmalaria.org/schema/scenario_50`.
  - **Every child/descendant element** across the entire document is **unqualified** (empty namespace `""`).
  - Across all 53 `test/scenario*.xml` files, the root tag is written as:
    ```xml
    <om:scenario xmlns:om="http://openmalaria.org/schema/scenario_50"
                 xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
                 name="..." schemaVersion="50"
                 xsi:schemaLocation="http://openmalaria.org/schema/scenario_50 scenario_current.xsd">
    ```
    with **all** inner tags unprefixed (`<demography>`, `<monitoring>`, etc.).
  - **Rust parser rule**: Match root by local name `"scenario"` (accepting both `<om:scenario>` and `<scenario>`), match all child elements by local name, and ignore `xmlns`, `xmlns:*`, and `xsi:*` attributes.

### Value Types & Primitives
- **`xs:boolean`**: XSD permits `"true"`, `"false"`, `"1"`, `"0"`. (Standard `serde` `bool` rejects `"1"`/`"0"`, so parse `bool` via `"true" | "1" => true`, `"false" | "0" => false`.)
- **Unit/Time strings (`xs:string`)**: Many time/duration attributes and `<surveyTime>` text use unit-suffixed strings (`"5d"`, `"14t"`, `"1y"`, `"-1"`, `"-1t"`, `"0"`, `"0.008219178y"`) or dates (`"2000-01-1"`, `"1351-1-1"`, `"1235-01-05"` — **note single-digit months/days without zero-padding!**). Keep as raw `String` in `om-schema` AST (or parse with a dedicated unit/date helper).
- **Ordering (`xs:all` vs `xs:sequence`)**: Many complexTypes use `xs:all` (children may appear in **any order**, and real `test/scenario*.xml` files DO permute them!). Note `roxmltree` makes `xs:all` and complex `xs:choice` trivial compared to `quick-xml`+`serde` (where `quick-xml::de` struggles with interleaved/unordered sibling sequences or `$value`+attributes in some edge cases).

---

## 2. Complete Schema Specification Across All 9 XSDs

Notation:
- `[ALL]` = `xs:all` (unordered children), `[SEQ]` = `xs:sequence` (ordered children), `[CHOICE]` = `xs:choice`.
- `?` = `0..1` (`Option<T>`), `*` = `0..unbounded` (`Vec<T>`), `+` = `1..unbounded` (`Vec<T>`, non-empty), `1` = `1..1` (required).
- `@attr: Type` = required attribute; `@attr?: Type` = optional attribute; `@attr: Type = val` = optional attribute with XSD default `val`.
- `TEXT(Type)` = text content inside element (`<el>123</el>`), as opposed to `@value` attribute (`<el value="123"/>`).

### 2.1 `scenario.xsd` (Root, Model, Human, ParasiteGenetics, Diagnostics)
- **`scenario` / `om:scenario`** `[ALL]`:
  - Attrs: `@schemaVersion: i32`, `@name: String`, `@analysisNo?: i32`, `@wuID?: i32`
  - Children (`[ALL]` — seen in different orders across tests, e.g. `parasiteGenetics`/`diagnostics`/`pharmacology` placed either before `model` or at different positions):
    - `demography: Demography` (`1`)
    - `monitoring: Monitoring` (`1`)
    - `interventions: Interventions` (`1`)
    - `healthSystem: HealthSystem` (`1`)
    - `entomology: Entomology` (`1`)
    - `parasiteGenetics?: ParasiteGenetics` (`?`)
    - `pharmacology?: Pharmacology` (`?`)
    - `diagnostics?: Diagnostics` (`?`)
    - `model: Model` (`1`)
- **`Model`** (anonymous complexType on `<model>`) `[ALL]`:
  - `ModelName?: ModelName` (`?`) — `@name: String`
  - `ModelOptions?: OptionSet` (`?`) — `[SEQ]` of `option*: OptionElement`
  - `clinical: Clinical` (`1`) — defined in `healthSystem.xsd`
  - `human: Human` (`1`)
  - `vivax?: Vivax` (`?`) — defined in `vivax.xsd`
  - `computationParameters: ComputationParameters` (`1`) — `@iseed: i32`
  - `parameters?: Parameters` (`?`):
    - Attrs: `@interval: i32`, `@latentp: String`
    - Children `[SEQ]`: `parameter*: Parameter` (`@name?: String`, `@number: i32`, `@value: f64`)
- **`Human`** `[SEQ]`:
  - `availabilityToMosquitoes: AgeGroupValues` (`1`)
  - `weight?: HumanWeight` (`?`) — extends `AgeGroupValues` + `@multStdDev: f64`
- **`ParasiteGenetics`** `[SEQ]`:
  - Attr: `@samplingMode: String` (`"initial" | "tracking"`)
  - Children `[SEQ]`: `locus+: ParasiteLocus`:
    - Attr: `@name: String`
    - Children `[SEQ]`: `allele+: ParasiteAllele` (`@name: String`, `@initialFrequency: f64`, `@fitness: f64`, `@hrp2_deletion: bool = false`)
  - *(Note: `ParasiteGenotype` complexType exists in `scenario.xsd` but is unreferenced by any element.)*
- **`Diagnostics`** `[SEQ]`:
  - `diagnostic*: Diagnostic`:
    - Attrs: `@name: String`, `@units?: String` (`"Other" | "Garki" | "Malariatherapy"`), `@mechanism: String = "Other"` (`"Other" | "HRP2"`)
    - Body `[CHOICE (1)]`:
      - `deterministic`: `@minDensity: f64`
      - `stochastic`: `@dens_50: f64`, `@specificity: f64`

### 2.2 `util.xsd` (Shared Helper Types)
- **`DoubleValue`**: `@value: f64`
- **`IntValue`**: `@value: i32`
- **`BooleanValue`**: `@value: bool`
- **`Component`**: `@id: String`
- **`ModelName`**: `@name: String`
- **`OptionSet`** `[SEQ]`: `option*: OptionElement` (`@name: String`, `@value: bool = true` — **default `true`!**)
- **`DoubleList`** `[SEQ]`: `item+: TEXT(f64)`
- **`TriggeredDeployments`** `[SEQ]`:
  - `deploy*`: `@maxAge?: f64`, `@minAge: f64 = 0.0`, `@p: f64 = 1.0`; children `[SEQ]`: `component+: Component`
- **`AgeGroupValues`** `[SEQ]`:
  - Attr: `@interpolation?: String` (`"none" | "linear"`; XSD omits default due to CodeSynthesis quirk, C++ defaults to `"linear"`)
  - Children `[SEQ]`: `group+` (`@lowerbound: f64`, `@value: f64`)
- **`SampledValueN`**: `@mean: f64`, `@SD?: f64`, `@distr: String = "const"` (`"const" | "normal"`)
- **`BetaMeanSample`**: `@mean: f64`, `@variance: f64`
- **`SampledValueCV`**: `@CV?: f64`, `@distr: String = "const"` (`"const" | "lognormal" | "gamma"`), `@variance?: f64`, `@truncate?: f64` *(all attrs optional — `<availability/>` is empty!)*
- **`SampledValueLN`** (extends `SampledValueCV`): `@mean: f64`, `@CV?: f64`, `@distr: String = "const"`, `@variance?: f64`, `@truncate?: f64`
- **`WeibullSample`**: `@scale: f64`, `@shape: f64`, `@distr: String` (fixed enum `"weibull"`)
- **`DecayFunction`** (**recursive!**) `[SEQ]`:
  - Attrs:
    - `@function: String` (`"constant" | "step" | "linear" | "exponential" | "weibull" | "hill" | "smooth-compact" | "plus" | "minus" | "divides" | "multiplies" | "emax"`)
    - `@L?: String` (optional; e.g. omitted on `"constant"` or composite functions; unit string defaulting to years)
    - `@k: f64 = 1.0`
    - `@CV: f64 = 0.0`
    - `@increasing: bool = false`
    - `@initialEfficacy: f64 = 1.0`
    - `@initialConcentration?: f64`, `@Emax?: f64`, `@slope?: f64`, `@IC50?: f64` (used by `"emax"`)
  - Children `[SEQ]`: `decay*: DecayFunction` (nested sub-functions for `plus`/`minus`/`divides`/`multiplies`/`emax`)

### 2.3 `demography.xsd`
- **`Demography`** `[SEQ]`:
  - Attrs: `@name: String`, `@popSize: i32`, `@maximumAgeYrs: f64`
  - Child: `ageGroup: DemogAgeGroup` (`1`):
    - Attr: `@lowerbound: f64`
    - Children `[SEQ]`: `group+: DemogGroupBounds` (`@poppercent: f64`, `@upperbound: f64`)

### 2.4 `monitoring.xsd`
- **`Monitoring`** `[SEQ]`:
  - Attrs: `@name: String`, `@startDate?: String`
  - Children `[SEQ]`:
    1. `continuous?: MonitoringContinuous` (`?`) — extends `OptionSet` (`option*`) + `@period: String`, `@duringInit?: bool`
    2. `SurveyOptions: MonitoringOptions` (`1`):
       - Attr: `@onlyNewEpisode: bool = false`
       - Children `[SEQ]`: `option*: MonitoringOption` — extends `OptionElement` (`@name: String`, `@value: bool = true`) + `@outputNumber?: i32`, `@byAge?: bool`, `@byCohort?: bool`, `@bySpecies?: bool`, `@byGenotype?: bool`, `@byDrugType?: bool`
    3. `surveys` (`1`):
       - Attrs: `@detectionLimit?: f64`, `@diagnostic?: String`
       - Children `[SEQ]`: `surveyTime+` — **`TEXT(String)`** (MUST `.trim()`!) + `@repeatStep?: String`, `@repeatEnd?: String`, `@reported: bool = true`
    4. `ageGroup: MonAgeGroup` (`1`):
       - Attr: `@lowerbound: f64`
       - Children `[SEQ]`: `group+` (`@upperbound: f64`)
    5. `cohorts?: Cohorts` (`?`):
       - Children `[SEQ]`: `subPop+` (`@id: String`, `@number: i64` / `u64` power-of-2 integer)

### 2.5 `vivax.xsd`
- **`Vivax`** `[ALL]`:
  - `probBloodStageInfectiousToMosq: DoubleValue` (`1`)
  - `hypnozoiteRelease: HypnozoiteRelease` (`1`) `[ALL]`:
    - Attr: `@pSecondRelease: f64 = 0.0`
    - `numberHypnozoites` (`1`): `@max: i32`, `@base: f64`
    - `firstReleaseDays: HypnozoiteReleaseDistribution` (`1`) — extends `SampledValueLN` + `@latentRelapse: f64`
    - `secondReleaseDays?: HypnozoiteReleaseDistribution` (`?`) — extends `SampledValueLN` + `@latentRelapse: f64`
  - `bloodStageProtectionLatency: DoubleValue` (`1`)
  - `bloodStageLengthDays: WeibullSample` (`1`)
  - `clinicalEvents: ClinicalEvents` (`1`) `[SEQ]`:
    - `pPrimaryInfection: ProbVivaxEvent` (`1`) (`@a: f64`, `@b: f64`)
    - `pRelapseOne: ProbVivaxEvent` (`1`) (`@a: f64`, `@b: f64`)
    - `pRelapseTwoPlus: ProbVivaxEvent` (`1`) (`@a: f64`, `@b: f64`)
    - `pEventIsSevere: DoubleValue` (`1`) (`@value: f64`)
    - `vivaxClinOption: TEXT(String)` (`1`) (e.g. `<vivaxClinOption>A2j</vivaxClinOption>`)

### 2.6 `healthSystem.xsd`
- **`Clinical`** (inside `<model><clinical>`) `[ALL]`:
  - Attr: `@healthSystemMemory: String`
  - `NeonatalMortality?` (`?`): `@diagnostic: String`
  - `NonMalariaFevers?: ClinicalNMF` (`?`) `[SEQ]`:
    - `incidence: AgeGroupValues` (`1`)
    - `prNeedTreatmentNMF?: AgeGroupValues` (`?`)
    - `prNeedTreatmentMF?: AgeGroupValues` (`?`)
- **`HealthSystem`** `[SEQ]`:
  1. `[CHOICE (1)]`:
     - `EventScheduler: HSEventScheduler`
     - `ImmediateOutcomes: HSImmediateOutcomes`
     - `DecisionTree5Day: HSDT5Day`
  2. `CFR: AgeGroupValues` (`1`)
  3. `pSequelaeInpatient: AgeGroupValues` (`1`)
- **`LiverStageDrug`** `[ALL]`:
  - `pHumanCannotReceive: DoubleValue` (`1`)
  - `ignoreCannotReceive?: BooleanValue` (`?`)
  - `pUseUncomplicated?: DoubleValue` (`?`)
  - `effectivenessOnUse: DoubleValue` (`1`)
- **`HSImmediateOutcomes`** `[ALL]`:
  - Attrs: `@name?: String`, `@useDiagnosticUC: bool = false`
  - `drugRegimen` (`1`): `@firstLine: String`, `@secondLine: String`, `@inpatient: String`
  - `initialACR: TreatmentDetails` (`1`)
  - `compliance: TreatmentDetails` (`1`)
  - `nonCompliersEffective: TreatmentDetails` (`1`)
  - `treatmentActions` (`1`) `[ALL]`: `CQ?`, `SP?`, `AQ?`, `SPAQ?`, `ACT?`, `QN?` (each `TreatmentOption`)
  - `pSeekOfficialCareUncomplicated1: DoubleValue` (`1`)
  - `pSelfTreatUncomplicated: DoubleValue` (`1`)
  - `pSeekOfficialCareUncomplicated2: DoubleValue` (`1`)
  - `pSeekOfficialCareSevere: DoubleValue` (`1`)
  - `liverStageDrug?: LiverStageDrug` (`?`)
- **`TreatmentDetails`** `[SEQ]`:
  - `CQ?: DoubleValue`, `SP?: DoubleValue`, `AQ?: DoubleValue`, `SPAQ?: DoubleValue`, `ACT?: DoubleValue`, `QN?: DoubleValue`, `selfTreatment: DoubleValue` (`1`)
- **`TreatmentOption`** (extends `TriggeredDeployments`) `[SEQ]`:
  - Attr: `@name?: String`
  - `deploy*` (from `TriggeredDeployments`: `@maxAge?: f64`, `@minAge: f64 = 0.0`, `@p: f64 = 1.0`, `component+`)
  - `clearInfections*` (`@timesteps: String`, `@stage: String` in `"liver" | "blood" | "both"`)
  - *(Note: both `deploy*` and `clearInfections*` have `minOccurs="0"`, so `<treatmentSevere></treatmentSevere>` can be completely empty!)*
- **`HSDT5Day`** `[ALL]`:
  - Attr: `@name?: String`
  - `pSeekOfficialCareUncomplicated1: DoubleValue` (`1`)
  - `pSelfTreatUncomplicated: DoubleValue` (`1`)
  - `pSeekOfficialCareUncomplicated2: DoubleValue` (`1`)
  - `pSeekOfficialCareSevere: DoubleValue` (`1`)
  - `liverStageDrug?: LiverStageDrug` (`?`)
  - `treeUCOfficial: DecisionTree` (`1`)
  - `treeUCSelfTreat: DecisionTree` (`1`)
  - `cureRateSevere: DoubleValue` (`1`)
  - `treatmentSevere: TreatmentOption` (`1`)
- **`HSEventScheduler`** `[SEQ]`:
  - `uncomplicated: DecisionTree` (`1`)
  - `complicated: DecisionTree` (`1`)
  - `ClinicalOutcomes: ClinicalOutcomes` (`1`) `[SEQ]` (**ALL TEXT CONTENT!**):
    - `maxUCSeekingMemory: TEXT(i32)` (`1`)
    - `uncomplicatedCaseDuration: TEXT(i32)` (`1`)
    - `complicatedCaseDuration: TEXT(i32)` (`1`)
    - `complicatedRiskDuration: TEXT(i32)` (`1`)
    - `dailyPrImmUCTS+: TEXT(f64)` (`+`)
  - `NonMalariaFevers?: HSESNMF` (`?`) `[SEQ]` (**mixed TEXT CONTENT + `AgeGroupValues`!**):
    - `prTreatment: TEXT(f64)` (`1`)
    - `effectNegativeTest: TEXT(f64)` (`1`)
    - `effectPositiveTest: TEXT(f64)` (`1`)
    - `effectNeed: TEXT(f64)` (`1`)
    - `effectInformal: TEXT(f64)` (`1`)
    - `CFR: AgeGroupValues` (`1`)
    - `TreatmentEfficacy: TEXT(f64)` (`1`)
- **`DecisionTree`** (recursive polymorphic `[CHOICE]`, `@name?: String`):
  - `multiple: DTMultiple` (`@name?: String`; `[CHOICE 0..unbounded]` of any combination/order of `caseType | infectionOrigin | diagnostic | uncomplicated | severe | random | age | treatPKPD | treatSimple | deploy | report | cohort`)
  - `caseType` (`@name?: String`; `[ALL]`: `firstLine: DecisionTree`, `secondLine: DecisionTree`)
  - `infectionOrigin` (`@name?: String`; `[ALL]`: `imported: DecisionTree`, `introduced: DecisionTree`, `indigenous: DecisionTree`)
  - `diagnostic` (`@diagnostic: String`, `@name?: String`; `[ALL]`: `positive: DecisionTree`, `negative: DecisionTree`)
  - `uncomplicated` (`@name?: String`, `@memory: String = "1"`; `[ALL]`: `positive: DecisionTree`, `negative: DecisionTree`)
  - `severe` (`@name?: String`, `@memory?: String`; `[ALL]`: `positive: DecisionTree`, `negative: DecisionTree`)
  - `cohort` (`@name?: String`, `@component: String`; `[ALL]`: `positive: DecisionTree`, `negative: DecisionTree`)
  - `random` (`@name?: String`; `[SEQ]`: `outcome+` where each `outcome` **extends `DecisionTree` directly** + `@p: f64`)
  - `age` (`@name?: String`; `[SEQ]`: `age+` where each inner `age` **extends `DecisionTree` directly** + `@lb: f64`)
  - `noTreatment` (`@name?: String`)
  - `treatFailure` (`@name?: String`)
  - `report+` (`1..unbounded` in `DecisionTree`! `@name?: String`, `@outputNumber: i32 = 0`)
  - `treatPKPD+` (`1..unbounded` in `DecisionTree`! `@schedule: String`, `@dosage: String`, `@delay_h: f64 = 0.0`)
  - `treatSimple+` (`1..unbounded` in `DecisionTree`! `@durationLiver: String`, `@durationBlood: String`)
  - `deploy*` (`0..unbounded` in `DecisionTree`! `@component: String` — note `0..unbounded` means an empty `<DecisionTree>` / `<outcome p="1"/>` is valid!)

### 2.7 `entomology.xsd`
- **`Entomology`**:
  - Attrs: `@name: String`, `@mode: String` (`"forced" | "dynamic"`), `@scaledAnnualEIR?: f64`
  - Body `[CHOICE (1)]`:
    - `nonVector: NonVector` (`@eipDuration: i32`; `[SEQ]`: `EIRDaily+` with **`TEXT(f64)`** + `@origin?: String`)
    - `vector` (`[SEQ]`: `anopheles+: AnophelesParams`, `nonHumanHosts*` (`@name: String`, `@number: f64`))
- **`AnophelesParams`** (`@mosquito: String`) `[SEQ]`:
  1. `seasonality` (`@input: String` (`"EIR"`), `@annualEIR?: f64`) `[CHOICE (1)]`:
     - `fourierSeries` (`@EIRRotateAngle?: f64`; `[SEQ]`: `coeffic*` with `@a: f64`, `@b: f64`)
     - `monthlyValues` (`@smoothing: String` (`"none" | "fourier"`), `@EIRRotateAngle?: f64`; `[SEQ]`: `value` `12..12` **`TEXT(f64)`**)
     - `dailyValues` (`[SEQ]`: `value+` **`TEXT(f64)`**)
  2. `mosq` (`@minInfectedThreshold: f64 = 0.0`) `[ALL]` (**unordered in real XMLs!**):
     - `mosqRestDuration: IntValue`
     - `extrinsicIncubationPeriod: IntValue`
     - `mosqLaidEggsSameDayProportion: DoubleValue`
     - `mosqSeekingDuration: DoubleValue`
     - `mosqSurvivalFeedingCycleProbability: DoubleValue`
     - `availability: SampledValueCV`
     - `mosqProbBiting: BetaMeanSample`
     - `mosqProbFindRestSite: BetaMeanSample`
     - `mosqProbResting: BetaMeanSample`
     - `mosqProbOvipositing: DoubleValue`
     - `mosqHumanBloodIndex: DoubleValue`
  3. `lifeCycle?: LifeCycle` (`@estimatedLarvalResources: f64 = 1e8`) `[ALL]`:
     - `eggStage: MosqStage` (`@duration: i32`, `@survival: f64`)
     - `larvalStage` (`@duration: i32`, `@survival: f64`; `[SEQ]`: `daily+` with `@resourceUsage: f64`, `@effectCompetition: f64`)
     - `pupalStage: MosqStage` (`@duration: i32`, `@survival: f64`)
     - `femaleEggsLaidByOviposit: DoubleValue`
  4. `simpleMPD?: SimpleMPD` `[ALL]`:
     - `developmentDuration: IntValue`, `developmentSurvival: DoubleValue`, `femaleEggsLaidByOviposit: DoubleValue`
  5. `nonHumanHosts*` (`@name: String`) `[ALL]`:
     - `mosqRelativeEntoAvailability: DoubleValue`, `mosqProbBiting: DoubleValue`, `mosqProbFindRestSite: DoubleValue`, `mosqProbResting: DoubleValue`, `hostFecundityFactor?: DoubleValue`

### 2.8 `pharmacology.xsd`
- **`Pharmacology`** `[SEQ]`:
  1. `treatments` (`1`) `[SEQ]`:
     - `schedule+: PKPDSchedule` (`@name: String`; `[SEQ]`: `medicate*` with `@drug: String`, `@mg: f64`, `@hour: f64`)
     - `dosages+: PKPDDosages` (`@name: String`; `[CHOICE (1)]`):
       - `age+` (`@lowerbound: f64`, `@dose_mult: f64`)
       - `bodymass+` (`@lowerbound: f64`, `@dose_mult: f64`)
       - `multiply` (`@by: String` = `"kg"`)
  2. `drugs` (`1`) `[SEQ]`:
     - `drug+: PKPDDrug` (`@abbrev: String`) `[SEQ]`:
       1. `PD` (`@locus?: String`) `[SEQ]`:
          - `phenotype+` (`@name?: String`) `[SEQ]` (**mixed TEXT and attributes!**):
            - `restriction*` (`@onLocus: String`, `@toAllele: String`)
            - `max_killing_rate: TEXT(f64)` (`1`)
            - `IC50: SampledValueLN` (`1`, attributes!)
            - `slope: TEXT(f64)` (`1`)
       2. `PK` `[SEQ]` (**mixed TEXT and attributes!**):
          - `negligible_concentration: TEXT(f64)` (`1`)
          - `[CHOICE (1)]`:
            - `half_life: TEXT(f64)` (`1`)
            - `[SEQ]` of BOTH: `k: SampledValueLN` (`1`, attrs) followed by `m_exponent: TEXT(f64)` (`1`)
          - `k_a?: SampledValueLN` (`?`, attrs)
          - `conversion?: PKConversion` (`?`) `[ALL]`:
            - `metabolite: TEXT(String)` (`1`), `rate: SampledValueLN` (`1`, attrs), `molRatio: TEXT(f64)` (`1`), `IC50_log_correlation: TEXT(f64)` (`1`)
          - `vol_dist: SampledValueLN` (`1`, attrs)
          - `compartment2?` (`?`) `[ALL]`: `k12: SampledValueLN`, `k21: SampledValueLN`
          - `compartment3?` (`?`) `[ALL]`: `k13: SampledValueLN`, `k31: SampledValueLN`

### 2.9 `interventions.xsd`
- **`Interventions`** (`@name: String`) `[ALL]` (all 9 children optional `0..1`; empty `<interventions name="..."/>` is common):
  - `changeHS?` (`@name?: String`; `[SEQ]`: `timedDeployment*` extending `HealthSystem` + `@time: String`)
  - `changeEIR?` (`@name?: String`; `[SEQ]`: `timedDeployment*` extending `NonVector` + `@time: String`)
  - `importedInfections?` (`@name?: String`; `[SEQ]`: `timed` (`1`, `@period: String = "0"`, `[SEQ]`: `rate+` with `@time: String`, `@value: f64`))
  - `insertR_0Case?` (`@name?: String`; `[SEQ]`: `timedDeployment*` with `@time: String`)
  - `uninfectVectors?` (`@name?: String`; `[SEQ]`: `timedDeployment*` with `@time: String`)
  - `vectorPop?` (`[SEQ]`: `intervention+` (`@name: String`) `[SEQ]`):
    1. `description` (`[SEQ]`: `anopheles+` (`@mosquito: String`) `[ALL]` of optional `seekingDeathRateIncrease?`, `probDeathOvipositing?`, `emergenceReduction?`, `probAdditionalDeathSugarFeeding?` — each has `@initial: f64` + child `decay: DecayFunction`)
    2. `timed?` (`[SEQ]`: `deploy+` with `@time: String`)
  - `vectorTrap?` (`[SEQ]`: `intervention+` (`@name?: String`) `[SEQ]`):
    1. `description+` (`@mosquito: String`; `[SEQ]`: `relativeAvailability: DoubleValue`, `decayOfAvailability: DecayFunction`)
    2. `timed?` (`[SEQ]`: `deploy*` with `@time: String`, `@ratioToHumans: f64`, `@lifespan: String`)
  - `nonHumanHostsModifications?` (`[SEQ]`: `intervention+` (`@name: String`, `@nonHumanHostsName: String`) `[SEQ]`):
    1. `decay: DecayFunction` (`1`)
    2. `description` (`[SEQ]`: `anopheles+` (`@mosquito: String`) `[ALL]` of optional `availabilityReduction?`, `preprandialKillingEffect?`, `postprandialKillingEffect?`, `restingKillingEffect?`, `fecundityReduction?` — each has ONLY `@initial: f64`, NO child `decay`)
    3. `timed?` (`[SEQ]`: `deploy+` with `@time: String`)
  - `addNonHumanHosts?` (`[SEQ]`: `nonHumanHosts+` (`@name: String`) `[SEQ]`):
    1. `description` (`[SEQ]`: `anopheles+` (`@mosquito: String`) `[ALL]` of 5 required `DoubleValue`s: `mosqRelativeAvailabilityHuman`, `mosqProbBiting`, `mosqProbFindRestSite`, `mosqProbResting`, `hostFecundityFactor`)
    2. `timed?` (`[SEQ]`: `deploy*` with `@time: String`, `@lifespan: String`)
  - `human?: HumanInterventions` `[SEQ]`:
    1. `component+: HumanInterventionComponent` (`@id: String`, `@name?: String`) `[SEQ]`:
       - `[CHOICE]` *(optional because `recruitmentOnly` has `minOccurs="0"`, though in practice 1 variant is written)*:
         - `screen: Screen` (`@diagnostic: String`; `[SEQ]`: `positive*: Component`, `negative*: Component`)
         - `treatSimple: DTTreatSimple` (`@durationLiver: String`, `@durationBlood: String`)
         - `treatPKPD: DTTreatPKPD` (`@schedule: String`, `@dosage: String`, `@delay_h: f64 = 0.0`)
         - `decisionTree: DecisionTree`
         - `PEV: VaccineDescription` | `BSV: VaccineDescription` | `TBV: VaccineDescription`:
           - `[SEQ]`: `decay: DecayFunction` (`1`), `efficacyB: DoubleValue` (`1`), `initialEfficacy*: DoubleValue`, `phenotype*` (`@name?: String`; `[SEQ]`: `restriction*` (`@onLocus`, `@toAllele`), `initialEfficacy+: DoubleValue`)
         - `ITN: ITNDescription` `[SEQ]`:
           - `usage?: DoubleValue`, `holeRate: SampledValueLN`, `ripRate: SampledValueLN`, `ripFactor: DoubleValue`, `initialInsecticide: SampledValueN`, `insecticideDecay: DecayFunction`, `attritionOfNets: DecayFunction`
           - `anophelesParams+` (`@mosquito: String`, `@propActive: f64 = 1.0`) `[SEQ]`:
             1. `holeIndexMax?: DoubleValue`
             2. `[CHOICE (1)]`:
                - `deterrency: ITNDeterrency` (`@insecticideFactor: f64`, `@insecticideScalingFactor: f64`, `@holeFactor: f64`, `@interactionFactor: f64`, `@holeScalingFactor: f64`)
                - `twoStageDeterrency` `[SEQ]`:
                  - `[CHOICE (1)]`: `entering: IRSDeterrency` (`@insecticideFactor`, `@insecticideScalingFactor`) | `enteringLogit: ITNEnteringDeterrencyLogit` (`@baseFactor`, `@insecticideFactor`)
                  - `[CHOICE (1)]`: `attacking: ITNKillingEffect` | `attackingLogit: ITNEffectLogit`
             3. `[CHOICE (1)]`: `preprandialKillingEffect: ITNKillingEffect` (`@baseFactor`, `@insecticideFactor`, `@insecticideScalingFactor`, `@holeFactor`, `@interactionFactor`, `@holeScalingFactor`) | `preprandialKillingEffectLogit: ITNEffectLogit` (`@baseFactor`, `@insecticideFactor`, `@holeFactor`, `@interactionFactor`)
             4. `[CHOICE (1)]`: `postprandialKillingEffect: ITNKillingEffect` | `postprandialKillingEffectLogit: ITNEffectLogit`
             5. `[CHOICE (0..1)]`: `fecundityReduction: ITNKillingEffect` | `fecundityReductionLogit: ITNEffectLogit` | None
         - `IRS: IRSDescription` `[SEQ]`:
           - `usage?: DoubleValue`, `initialInsecticide: SampledValueN`, `insecticideDecay: DecayFunction`
           - `anophelesParams+` (`@mosquito: String`, `@propActive: f64 = 1.0`) `[SEQ]`: `deterrency: IRSDeterrency` (`1`), `preprandialKillingEffect: IRSKillingEffect` (`1`: `@baseFactor`, `@insecticideFactor`, `@insecticideScalingFactor`), `postprandialKillingEffect: IRSKillingEffect` (`1`), `fecundityReduction?: IRSKillingEffect` (`?`)
         - `GVI: GVIDescription` `[SEQ]`:
           - `usage?: DoubleValue`, `decay: DecayFunction`
           - `anophelesParams+` (`@mosquito: String`, `@propActive: f64 = 1.0`) `[SEQ]`: `deterrency?: DoubleValue`, `preprandialKillingEffect?: DoubleValue`, `postprandialKillingEffect?: DoubleValue`, `fecundityReduction?: DoubleValue`
         - `recruitmentOnly` (empty tag; `minOccurs="0"`)
         - `clearImmunity` (empty tag)
       - `subPopRemoval?: SubPopRemoval` (`?`: `@onFirstBout: bool = false`, `@onFirstTreatment: bool = false`, `@onFirstInfection: bool = false`, `@afterYears?: f64`)
    2. `deployment*` (`@name?: String`) `[SEQ]`:
       - `component+: Component`
       - `condition*` (`@measure: String`, `@minValue?: f64`, `@maxValue?: f64`, `@initialState: bool`)
       - `continuous*` (`[SEQ]`: `restrictToSubPop?` (`@id: String`, `@complement: bool = false`), `deploy+: ContinuousDeployment`)
       - `timed*` (`[SEQ]`: `restrictToSubPop?`, `cumulativeCoverage?` (`@component: String`), `deploy+: MassDeployment`)
- **`DeploymentBase` / `ContinuousDeployment` / `MassDeployment` attributes**:
  - Common (`DeploymentBase`): `@coverage: f64`, `@vaccMinPrevDoses?: i32`, `@vaccMaxCumDoses?: i32`, `@minAvailability?: i32`, `@maxAvailability?: i32`, `@coverageCorr?: f64`, `@coverageVar?: f64`
  - `ContinuousDeployment` adds: `@targetAgeYrs: f64`, `@begin?: String`, `@end?: String`
  - `MassDeployment` adds: `@time: String`, `@maxAge?: f64`, `@minAge: f64 = 0.0`, `@repeatStep?: String`, `@repeatEnd?: String`

---

## 3. Post-Validation Rules in `XMLChecker.h` (and Adjacent Loader Checks)

### `model/util/XMLChecker.h`
`XMLChecker::PerformPostValidationChecks(const scnXml::Scenario& scenario)` executes **one** check (`CheckModelOptionsAndParams`):
- Let:
  - `modelName = scenario.model.ModelName.is_some()`
  - `parameters = scenario.model.parameters.is_some()`
  - `modelOptions = scenario.model.ModelOptions.is_some()`
- **Rule**: If `!modelName && (!parameters || !modelOptions)`, fail with `xml_scenario_error`:
  > `"If a model name is not specified then both <ModelOptions> and <parameters> must be specified"`
- **Important edge case** (verified in `test/scenarioDecisionTree5DayDielmo.xml` & `test/scenarioModelNameNoOverrides.xml`):
  - If `<ModelName name="base"/>` IS present, BOTH `<ModelOptions>` and `<parameters>` may be omitted (or partially provided as overrides).
  - If `<ModelName>` is NOT present, an **empty** `<ModelOptions></ModelOptions>` (0 `<option>` children) counts as `modelOptions == true` and passes validation!

### Adjacent Loader Rules (`DocumentLoader.cpp` & `ModelNameProvider.h`)
- **`schemaVersion` check** (`DocumentLoader.cpp`, `SCHEMA_VERSION = 50`):
  - `schemaVersion < 50`: logs warning to `stderr` (`Warning: <file> uses an old schema version (latest is 50).`), **continues execution**.
  - `schemaVersion > 50`: throws `xml_scenario_error("Error: new schema version unsupported")`.
- **`ModelName` value check** (`ModelNameProvider.h`):
  - Only `name == "base"` is valid; any other string throws `xml_scenario_error("Unrecognized model name: " + name)`.

---

## 4. Subtle XML Quirks in `test/scenario*.xml` & Rust Parser Pitfalls

1. **Inconsistent `@value` attribute vs element text content (`$value`)**:
   - Most numbers use `<el value="0.5"/>` (`DoubleValue`), **EXCEPT**:
     - `<surveyTime repeatStep="14t" repeatEnd="1y"> 0 </surveyTime>` (`monitoring.xsd` — text **with leading/trailing spaces** + attributes!)
     - `<EIRDaily origin="imported">0.00412</EIRDaily>` (`entomology.xsd` — text + optional `@origin` attribute!)
     - `<monthlyValues><value>0.1</value>...</monthlyValues>` and `<dailyValues><value>...</value></dailyValues>` (`entomology.xsd`)
     - `<vivaxClinOption>A2j</vivaxClinOption>` (`vivax.xsd`)
     - `<ClinicalOutcomes>` (`maxUCSeekingMemory`, `uncomplicatedCaseDuration`, `complicatedCaseDuration`, `complicatedRiskDuration`, `dailyPrImmUCTS`) and `<NonMalariaFevers>` inside `<EventScheduler>` (`prTreatment`, `effectNegativeTest`, `effectPositiveTest`, `effectNeed`, `effectInformal`, `TreatmentEfficacy` are text, while sibling `<CFR>` uses `<group lowerbound="..." value="..."/>`!)
     - `<pharmacology>` mixes text elements (`<max_killing_rate>3.45</max_killing_rate>`, `<slope>2.5</slope>`, `<negligible_concentration>0.005</negligible_concentration>`, `<half_life>0.18</half_life>`, `<m_exponent>0.25</m_exponent>`, `<metabolite>`, `<molRatio>`, `<IC50_log_correlation>`) right alongside attribute-based `SampledValueLN` siblings (`<IC50 mean="0.009" .../>`, `<vol_dist mean="1.4" .../>`, `<k mean="..."/>`, `<k_a mean="..."/>`, `<k12 mean="..."/>`)!
2. **`<option name="..."/>` omits `@value` to mean `true`**:
   - Seen in `scenarioVecFullTest.xml`, `scenarioVivax.xml`, `scenarioMolineaux.xml`, `scenarioESTS.xml`, `scenarioModelNameNoOverrides.xml`, etc. Both `Option` (`ModelOptions`, `continuous`) and `MonitoringOption` (`SurveyOptions`) default `@value` to **`true`** when omitted.
3. **Duplicate `<option name="..."/>` names in `<SurveyOptions>`**:
   - In `scenarioESTS.xml`, `<SurveyOptions>` contains multiple `<option name="nCMDTReport" outputNumber="1" value="true"/>`, `<option name="nCMDTReport" outputNumber="2" value="true"/>`, etc. Do **not** deserialize `SurveyOptions` into a `HashMap<String, ...>` keyed solely by `name` — keep `Vec<MonitoringOption>`.
4. **Empty tags that carry semantic weight or exercise `0..unbounded` choices**:
   - `<interventions name="No interventions"/>` (all `xs:all` children omitted).
   - `<ModelOptions></ModelOptions>` (satisfies `XMLChecker` presence check while setting zero options).
   - `<availability/>` inside `<mosq>` (`SampledValueCV` with all optional attributes omitted -> `distr = "const"`).
   - `<treatmentSevere></treatmentSevere>` in `scenarioDecisionTree5DayDielmo.xml` (`TreatmentOption` with 0 `deploy` and 0 `clearInfections`).
   - `<component id="..."/>` vs `<component id="..."><recruitmentOnly/></component>`:Note `<component id="..."/>` is ALSO used as a reference tag (`util.xsd` `Component`) inside `<deployment>`, whereas inside `<human><component id="...">` it is `HumanInterventionComponent`!
5. **Unordered `xs:all` blocks are actively permuted across test files**:
   - `<om:scenario>` children: `scenarioEffectiveDrug.xml` puts `<pharmacology>` before `<model>`, while `scenarioESTS.xml` puts `<diagnostics>` and `<pharmacology>` at different positions.
   - `<mosq>` children: `scenario2ITNs.xml` puts `<availability/>` *after* `<mosqProbResting>`, whereas `scenarioVecFullTest.xml` puts `<availability/>` *before* `<mosqProbBiting>`.
   - `<DecisionTree5Day>` and `<ImmediateOutcomes>` permute children across scenarios.
   - `<caseType>`, `<diagnostic>`, `<uncomplicated>`, `<severe>`, `<cohort>`, `<infectionOrigin>` in `DecisionTree` use `xs:all` (e.g., `<negative>` can appear before `<positive>`).
6. **Recursive `DecisionTree` inline inheritance & `<multiple>`**:
   - `<random><outcome p="0.8"> ... </outcome></random>` and `<age><age lb="5"> ... </age></age>` do **not** wrap a `<DecisionTree>` child tag; `<outcome>` and inner `<age>` *extend* `DecisionTree`, so `<treatSimple .../>`, `<multiple>`, `<diagnostic>`, etc. appear directly inside `<outcome>` / `<age>`.
   - Inside `<multiple>` (`scenarioDecisionTree5DayDielmo.xml`, `scenarioESTS.xml`), heterogeneous decision/action nodes (`<treatPKPD .../>`, `<deploy .../>`, `<report .../>`, `<diagnostic ...>`, `<random>`) appear as direct siblings in sequence and can repeat (`<treatPKPD>` repeated for multi-drug regimens in `scenarioESTS.xml`).
7. **Repeated `lowerbound` values in `AgeGroupValues`**:
   - `scenario1.xml`, `scenario2ITNs.xml`, `scenarioESTS.xml`, etc. have two consecutive `<group lowerbound="20.0" value="1.0"/>` (or two `lowerbound="0"` in `CFR`) to clamp linear interpolation. Preserve exact `Vec<Group>` order without deduplicating `lowerbound`.
8. **Recommendation for `crates/om-schema` parser**:
   - Using **`roxmltree`** to parse into strongly typed `serde::{Serialize, Deserialize}` structs is dramatically cleaner and safer than `quick-xml::de` here: `roxmltree` handles arbitrary `xs:all` child order, namespace-prefixed `<om:scenario>` with unqualified children, `$value` + attributes (`surveyTime`, `EIRDaily`), mixed text/attribute siblings (`Pharmacology`), and recursive polymorphic `DecisionTree` (`<multiple>`, `<outcome>`, `<age>`) with zero `serde` XML workarounds, in ~1–2 ms per scenario file.
