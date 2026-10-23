/* This file is part of OpenMalaria.
 * 
 * Copyright (C) 2005-2026 Swiss Tropical and Public Health Institute
 * Copyright (C) 2005-2015 Liverpool School Of Tropical Medicine
 * Copyright (C) 2020-2026 University of Basel
 * Copyright (C) 2025-2026 The Kids Research Institute Australia
 *
 * OpenMalaria is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef HMOD_ANOPHELES_MODEL
#define HMOD_ANOPHELES_MODEL

#include "Global.h"
#include "Host/WithinHost/Genotypes.h"
#include "Transmission/PerHost.h"
#include "util/SimpleDecayingValue.h"
#include "util/vectors.h"
#include "util/CommandLine.h"
#include "util/errors.h"

#include <vector>
#include <limits>

namespace OM {

namespace Transmission::Anopheles {
using std::numeric_limits;
using namespace OM::util;

enum VecStat { PA, PAmu, PA1, PAh, PDF, PDIF, NV, OV, SV };

/** Per-species part for vector transmission model.
 *
 * Data in this class is specific to a species of anopheles mosquito, where
 * species is used in a relaxed way to mean any variation of anopheles
 * mosquito, not just those types formally recognised as distinct species.
 *
 * A list of this class type is used by the VectorModel class to hold
 * (potentially) species-specific per-population data.
 *
 * Instead of storing static variables in this class, store them in the
 * VectorModel class.
 *
 * Variable names largely come from Nakul Chitnis's paper:
 * "A mathematical model for the dynamics of malaria in mosquitoes feeding on
 * a heterogeneous host population" (3rd Oct. 2007).
 * 
 * Some terminology extensions (not in paper):
 * 
 * $\nu_A$ / ν_A / leaveRate = sum_i=1^n α_i N_i + μ_vA
 * $\alpha_d$ / α_d / availDivisor = P_Ai / (α_i N_i) = (1 - P_A) / ν_A
 * $\sigma_{df}$ / σ_f / sigma_f = sum_i α_i N_i P_Bi
 * $\sigma_{df}$ / σ_df / sigma_df = sum_i α_i N_i P_Bi P_Ci P_Di
 * $\sigma_{dif}$ / σ_dif / sigma_dif = sum_i α_i N_i P_Bi P_Ci P_Di K_vi
 * 
 * Thus:
 * 
 * P_df = σ_df α_d P_E
 * P_dif = σ_dif α_d P_E
 */

struct MosquitoParams {
    // Proportion of host-seeking parous mosquitoes (those who have laid eggs) which laid eggs that day:
    double laid_eggs_same_day_proportion{};
    // Probability that the mosquito survives the feeding cycle.
    // Note: Pf = M, the parous rate (prop mosqs which have laid eggs):
    double survival_feeding_cycle_probability{};
    double human_blood_index{}; // χ (chi)
    // Cycle probabilities, when biting a human:
    // B: Host encountered
    double prob_biting{};
    // C: Fed
    double prob_find_rest_site{};
    // D: Resting
    double prob_resting{};
    // E: Laying eggs (ovipositing)
    double prob_ovipositing{};

    /** Duration of host-seeking per day; the maximum fraction of a day that a
     * mosquito would spend seeking (θ_d). */
    double seeking_duration{};

    /** Probability of a mosquito successfully laying eggs given that it has
     * rested (P_E).
     *
     * Currently assumed constant, although NC's non-autonomous model provides
     * an alternative. */
    double prob_mosq_survival_ovipositing{};

    /** If less than this many mosquitoes remain infected, transmission is interrupted. */
    double min_infected_threshold{};

    /** @brief Probabilities and rates associated with life-cycle model
     * 
     * These are calculated during initialisation and thereafter constant.
     * 
     * Probabilities have no units; others have units specified.
     *
     * All parameters are calculated during initialisation and in theory don't
     * need checkpointing. 
     * Death rate of mosquitoes while host-seeking (μ_vA).
     *
     * Unit: animals/day. */
    double seeking_death_rate{};

    /** Duration parameters for mosquito/parasite life-cycle
     * 
     * Currently these are all constant. In theory they could be made to vary
     * seasonally, based on a fixed periodic cycle, though some code and
     * possibly model changes would be needed to accomodate this.
     * 
     * All have units of days.
     *
     * Set in initialise function from XML data; no need to checkpoint.
     * Duration of feeding cycle (equals duration of resting period) for mosquito (τ).
     * Units: days. */
    SimTime rest_duration = Sim::never();

    /** Duration of the extrinsic incubation period (sporozoite development time)
    * (θ_s).
    * Units: Days.
    *
    * Doesn't need checkpointing. */
    SimTime eip_duration = Sim::never();

    string name;
};

struct NhhParams {
    double mosq_relative_ento_availability{};
    double mosq_prob_biting{};
    double mosq_prob_find_rest_site{};
    double mosq_prob_resting{};
    double host_fecundity_factor{};
    string name;
};

struct Nhh {
    double avail_i{};
    double p_b_i{};
    double p_c_i{};
    double p_d_i{};
    double rel_fecundity{};
    SimTime expiry = Sim::never();
};

struct TrapParams {
    double rel_avail = numeric_limits<double>::signaling_NaN(); // Initial availability of a trap relative to an adult
    unique_ptr<util::DecayFunction> avail_decay;                // Decay of availability
};

struct TrapData {
    size_t instance{};                   // index in trapParams
    double initial_avail{};              // initial availability (avail per trap * num traps)
    unique_ptr<DecayFunction> avail_het; // parameter for decay of availability
    SimTime deploy_time = Sim::never();  // deploy time (for decay function)
    SimTime expiry = Sim::never();       // date at which this intervention should be deleted
};

class AnophelesModel {
public:
    ///@brief Initialisation and destruction
    //@{
    AnophelesModel()
        : nhh_avail(numeric_limits<double>::signaling_NaN()), nhh_sigma_df(numeric_limits<double>::signaling_NaN()),
          nhh_sigma_dff(numeric_limits<double>::signaling_NaN()),
          // EmergenceModel
          eir_rotate_angle(numeric_limits<double>::quiet_NaN()), init_nv_from_sv(numeric_limits<double>::quiet_NaN()),
          init_ov_from_sv(numeric_limits<double>::quiet_NaN()), init_nv0_from_sv(numeric_limits<double>::quiet_NaN()) {
        forced_s_v.resize(Sim::one_year());
        quinquennial_s_v.resize(Sim::from_years_i(5), 0.0);
        mosq_emerge_rate.resize(Sim::one_year(), 0.0);
    }

    AnophelesModel(const AnophelesModel&) = delete;
    AnophelesModel& operator=(const AnophelesModel&) = delete;
    AnophelesModel(AnophelesModel&&) = delete;
    AnophelesModel& operator=(AnophelesModel&&) = delete;
    virtual ~AnophelesModel() = default;

    /** Called to initialise variables instead of a constructor. At this point,
     * the size of the human population is known but that population has not
     * yet been constructed. Called whether data is loaded from a check-point
     * or not.
     *
     * @param anoph Data structure from XML to use
     * @param initialisationEIR In/out parameter: TransmissionModel::initialisationEIR
     * @param nonHumanHostPopulations Size of each non-human population
     * @param populationSize Size of human population (assumed constant)
     */
    void initialise(size_t species, MosquitoParams mosq_params, const PerHostParams& per_host,
                    const WithinHost::GenotypeRegistry& genotype_registry);

    ///@brief Initialisation helper functions
    //@{
    /** Calculate availability rate of hosts (α_i) and death rate while seeking
     * (µ_vA)
     *
     * Documentation: "Parameter Values for Transmission model"
     * (Chitnis, Smith and Schapira, 4.3.2010)
     * 
     * @param species Species index
     * @param anoph Data from XML
     * @param nonHumanHostPopulations Size of each non-human population
     * @param populationSize Size of the human population (assumed constant)
     */
    void init_availability(size_t species, const vector<NhhParams>& nhhs, int population_size, PerHostParams& per_host);

    void init_eir(const vector<double>& init_eir365, vector<double> fs_coeffic_init, double eir_rotate_angle_init,
                  double prop_infectious, double prop_infected);

    /** Scale the internal EIR representation by factor; used as part of
     * initialisation. */
    inline void scale_eir(double factor) {
        fs_coeffic[0] += log(factor);
        vectors::scale(partial_init_eir, factor);
        vectors::scale(species_eir, factor);
    }

    virtual void scale(double factor) {
        vectors::scale(n_v, factor);
        vectors::scale(o_v_i, factor);
        vectors::scale(o_v_l, factor);
        vectors::scale(s_v_i, factor);
        vectors::scale(s_v_l, factor);
    }

    /** Initialisation which must wait until a human population is available.
     * This is only called when a checkpoint is not loaded.
     *
     * @param nHumans Human population size
     * @param meanPopAvail The mean availability of age-based relative
     * availability of humans to mosquitoes across populations.
     * @param sum_avail sum_i α_i * N_i for human hosts i
     * @param sigma_f sum_i α_i * N_i * P_Bi for human hosts i
     * @param sigma_df sum_i α_i * N_i * P_Bi * P_Ci * P_Di for human hosts i
     * @param sigma_dff sum_i α_i * N_i * P_Bi * P_Ci * P_Di * rel_mosq_fecundity for human hosts i
     *
     * Can only usefully run its calculations when not checkpointing, due to
     * population not being the same when loaded from a checkpoint. */
    virtual void init2(int n_humans, double mean_pop_avail, double sum_avail, double sigma_f, double sigma_df,
                       double sigma_dff);

    /** Set up the non-host-specific interventions. */
    void init_vector_interv(const scnXml::VectorSpeciesIntervention& elt, size_t instance);

    /** Set up trap parameters. */
    void init_vector_trap(const scnXml::Description1& desc, size_t instance);

    /** Work out whether another interation is needed for initialisation and if
     * so, make necessary changes.
     *
     * @returns false if another iteration is needed. */
    virtual bool init_iterate();

    ///@brief Functions called as part of usual per-time-step operations
    //@{
    /** Called per time-step. Does most of calculation of EIR.
     *
     * @param sum_avail sum_i α_i * N_i for human hosts i
     * @param sigma_df sum_i α_i * N_i * P_Bi * P_Ci * P_Di for human hosts i
     * @param sigma_dif sum_i α_i * N_i * P_Bi * P_Ci * P_Di * Kvi for human hosts i; this vector is modified!
     * @param sigma_dff sum_i α_i * N_i * P_Bi * P_Ci * P_Di * rel_mosq_fecundity for human hosts i
     * @param isDynamic True to use full model; false to drive model from current contents of S_v.
     */
    void advance_period(double sum_avail, double sigma_df, vector<double>& sigma_dif_i, vector<double>& sigma_dif_l,
                        double sigma_dff, bool is_dynamic);

    /// Intermediatary from vector model equations used to calculate EIR
    inline double get_init_partial_eir() const {
        return partial_init_eir[Sim::modulo_year_steps(Sim::ts0())] / init_avail;
    }
    //@}

    /// Intermediatary from vector model equations used to calculate EIR
    inline const vector<double>& get_partial_eir_imported() const { return partial_eir_i; }
    inline const vector<double>& get_partial_eir_local() const { return partial_eir_l; }
    //@}

    ///@brief Functions called to deploy interventions
    //@{
    void deploy_vector_pop_interv(LocalRng& rng, size_t instance);
    /// Deploy some traps
    ///
    /// @param instance Index of this type of trap
    /// @param number The number of traps to deploy
    /// @param lifespan Time until these traps are removed/replaced/useless
    void deploy_vector_trap(LocalRng& rng, size_t species, size_t instance, double pop_size, SimTime lifespan);

    /** (Re) allocate and initialise some state variables. Must be called
     * before model is run. */
    void init_state(double ts_p_a, double ts_p_amu, double ts_p_a1, double ts_p_ah, double ts_p_df, double ts_p_dff,
                    double init_nv_from_sv, double init_ov_from_sv, const std::vector<double>& forced_s_v);

    /// Helper function for initialisation.
    void init_iterate_scale(double factor);

    virtual double get_emergence_rate(const SimTime& d0, const std::vector<double>& mosq_emerge_rate,
                                      double n_ovipositing) {
        // Get emergence at start of step:
        const SimTime d_year1 = mod_nn(d0, Sim::one_year());
        // Simple model: fixed emergence scaled by larviciding
        return mosq_emerge_rate[d_year1];
    }

    /** Update by one day (may be called multiple times for 1 time-step update).
     * 
     * @param d0 Time of the start of the day-long update period
     * @param tsP_A P_A for this time-step
     * @param tsP_df P_df for this time-step
     * @param tsP_dif P_dif for this time-step, per parasite genotype
     * @param tsP_dff P_dff for this time step
     * @param partialEIR Vector, per genotype; after calculation, the latest
     *  S_v values are multiplied by EIR_factor and added to this.
     * @param EIR_factor see parameter partialEIR
     */
    void update(SimTime d0, double ts_p_a, double ts_p_amu, double ts_p_a1, double ts_p_ah, double ts_p_df,
                const vector<double>& ts_p_dif_i, const vector<double>& ts_p_dif_l, double ts_p_dff, bool is_dynamic,
                vector<double>& partial_eir_i, vector<double>& partial_eir_l, double eir_factor);

    /// Intermediatary from vector model equations used to calculate EIR in intervention mode
    inline double get_intervention_eir() const {
        return intervention_eir[Sim::in_steps(Sim::interv_time())] / init_avail;
    }
    //@}

    void change_eir_intervention(const scnXml::NonVector& non_vector_data);

    ///@brief Interventions and reporting
    //@{
    void uninfect_vectors();
    //@}

    inline SimTime get_eip_duration() const { return mosq.eip_duration; }

    ///@brief Functions used in reporting
    //@{
    /// Reset per-time-step statistics before running time-step updates
    inline void reset_ts_stats() { time_step_n_v0 = 0.0; }

    /// Get mean emergence per day during last time-step
    inline double get_last_n_v0() const { return time_step_n_v0 / Sim::one_ts(); }

    /// Get mean P_A/P_df/P_dif/N_v/O_v/S_v during last time-step
    /// @param vs PA, PDF, PDIF, NV, OV or SV
    double get_last_vec_stat(VecStat vs) const;

    virtual double get_res_availability() const { return numeric_limits<double>::quiet_NaN(); }
    virtual double get_res_requirements() const { return numeric_limits<double>::quiet_NaN(); }

    /// Write some per-species summary information.
    void summarize(size_t species) const;
    //@}

    virtual void checkpoint(istream& stream) { (*this) & stream; }
    virtual void checkpoint(ostream& stream) { (*this) & stream; }

    /// Checkpointing
    //Note: below comments about what does and doesn't need checkpointing are ignored here.
    template <class S> void operator&(S& stream) {
        mosq.seeking_death_rate & stream;
        mosq.seeking_duration & stream;
        mosq.prob_mosq_survival_ovipositing & stream;
        // transmission & stream;
        seeking_death_rate_intervs & stream;
        prob_death_ovipositing_intervs & stream;
        partial_eir_i & stream;
        partial_eir_l & stream;
        eir_rotate_angle & stream;
        fs_coeffic & stream;
        forced_s_v & stream;
        init_nv_from_sv & stream;
        init_ov_from_sv & stream;
        emergence_reduction & stream;
        // interventionSurvival & stream;
        // EmergenceModel
        mosq_emerge_rate & stream;
        quinquennial_s_v & stream;
        init_nv0_from_sv & stream;
        init_sv_from_eir & stream;
        // (*emergence) & stream;
        // MosqTransmission
        mosq.rest_duration & stream;
        mosq.eip_duration & stream;
        n_v_length & stream;
        p_a & stream;
        p_df & stream;
        p_dif_i & stream;
        p_dif_l & stream;
        p_dff & stream;
        n_v & stream;
        o_v_i & stream;
        o_v_l & stream;
        s_v_i & stream;
        s_v_l & stream;
        p_amu & stream;
        p_a1 & stream;
        p_ah & stream;
        //TODO: do we actually need to checkpoint these next three?
        f_array & stream;
        ftau_array & stream;
        uninfected_v & stream;
        time_step_n_v0 & stream;
    }

    MosquitoParams mosq;

    // sum_i N_i * α_i for i in NHH types: total availability of non-human hosts
    double nhh_avail;
    // sum_i N_i * α_i * P_B_i * P_C_i * P_D_i for i in NHH types: chance feeding
    // and surviving a complete cycle across all non-human hosts
    double nhh_sigma_df;
    // as above, but modified by fertility factors
    double nhh_sigma_dff;
    //@}

    /** The initialisationEIR for this species */
    std::vector<double> partial_init_eir;

    /** The initialisation availDivisor: (1 - P_A[t]) / (sum_{h in hosts} α_h[t] + μ_vA */
    double init_avail{};

    /** init EIR.
    *
    * Doesn't need to be checkpointed (is calculated during initialization). */
    std::vector<double> species_eir;

    /** Per time-step init EIR.
    *
    * Doesn't need to be checkpointed (is calculated during initialization). */
    std::vector<double> partial_eir_i, partial_eir_l;

    /** Per time-step intervention EIR
    *
    * Doesn't need to be checkpointed (is calculated during initialization). */
    std::vector<double> intervention_eir;

    // Emergence Model
    // -----  parameters (constant after initialisation)  -----

    ///@brief Descriptions of transmission, used primarily during warmup
    //@{
    /// Angle (in radians) to rotate series generated by FSCoeffic by, for EIR.
    double eir_rotate_angle;

    /** Fourier coefficients for EIR / forcedS_v series, input from XML file.
     *
     * Initially used to calculate initialisation EIR, then scaled to calc. S_v.
     *
     * When calcFourierEIR is used to produce an EIR from this over 365
     * (365) elements, the resulting EIR has units of
     * infectious bites per adult per day.
     *
     * fcEir must have odd length and is ordered: [a0, a1, b1, ..., an, bn].
     * FSCoeffic[0] needs checkpointing, the rest doesn't. */
    std::vector<double> fs_coeffic;

    /** S_v used to force an EIR during vector init.
     * 
     * Has annual periodicity: length is 365. First value (index 0) corresponds
     * to first day of year (1st Jan or something else if rebased). In 5-day
     * time-step model values at indecies 0 through 4 are used to calculate the
     * state at time-step 1.
     *
     * Should be checkpointed. */
    std::vector<double> forced_s_v;

    /** Conversion factor from forcedS_v to (initial values of) N_v (1 / ρ_S).
     * Should be checkpointed. */
    double init_nv_from_sv;

    /** Used to estimate S_v from EIR during initialization 
     * Should be checkpointed. */
    double init_sv_from_eir{};

    /** Conversion factor from forcedS_v to (initial values of) O_v (ρ_O / ρ_S).
     * Should be checkpointed. */
    double init_ov_from_sv;
    //@}

    ///@brief Descriptions of transmission, used primarily during warmup
    //@{
    /** Summary of S_v over the last five years, used by vectorInitIterate to
     * calculate scaling factor.
     *
     * Length is 365 * 5. Checkpoint.
     *
     * Units: inoculations. */
    std::vector<double> quinquennial_s_v;

    /** Conversion factor from forcedS_v to mosqEmergeRate.
     *
     * Should be checkpointed. */
    double init_nv0_from_sv;
    //@}

    /** Emergence rate of new mosquitoes, for every day of the year (N_v0).
     * 
     * Has annual periodicity: length is 365. First value (index 0) corresponds
     * to first day of year (1st Jan or something else if rebased). In 5-day
     * time-step model values at indecies 0 through 4 are used to calculate the
     * state at time-step 1.
     * 
     * Units: Animals per day.
     *
     * Should be checkpointed. */
    std::vector<double> mosq_emerge_rate;

    /** N_v_length-1 is the number of previous days for which some parameters are
     * stored: P_A, P_df, P_dif, N_v, O_v and S_v. This is longer than some of
     * the arrays need to be, but simplifies code with no real impact.
     *
     * Should equal EIPDuration + mosqRestDuration to allow values up to
     * θ_s + τ - 1 days back, plus current day.
     *
     * Set by initialise; no need to checkpoint. */
    int n_v_length{0};

    // -----  variable model state  -----

    /** @brief Variable arrays N_v_length long.
     *
     * P_A, P_df, P_dif, N_v, O_v and S_v are set in advancePeriod() and have
     * values stored per day.
     * 
     * P_dif, O_v and S_v have a second index: the parasite genotype.
     *
     * Values at index ((d-1) mod N_v_length) are used to derive the state of
     * the population on day d. The state during days (t×I+1) through to ((t+1)×I)
     * where t is sim::ts0() and I is sim::oneTS() is what
     * drives the transmission at time-step t.
     * 
     * These arrays should be checkpointed. */
    //@{
    /** Probability of a mosquito not finding a host one night. */
    std::vector<double> p_a;

    /** P_df is the probability of a mosquito finding a host and completing a
     * feeding cycle without being killed. */
    std::vector<double> p_df;

    /** P_dif is the probability of a mosquito finding a host, getting
     * infected, and successfully completing a feeding cycle. 
     * 
     * We keep separate the probability of a mosquito being infected by an
     * imported infection _i and a local infection _l */
    std::vector<double> p_dif_i, p_dif_l;

    /** Like P_df but including fertility factors */
    std::vector<double> p_dff;

    /** Numbers of host-seeking mosquitos each day */
    std::vector<double> n_v;

    /** Numbers of infected host-seeking mosquitoes */
    std::vector<double> o_v_i, o_v_l;

    /** Nnumbers of infective (to humans) host-seeking mosquitoes */
    std::vector<double> s_v_i, s_v_l;

    /** Probability of a mosquito dying */
    std::vector<double> p_amu;

    /** Probability of a mosquito finding a host */
    std::vector<double> p_a1;

    /** Probability of a mosquito finding a non-human host type*/
    std::vector<double> p_ah;
    //@}

    ///@brief Working memory
    /** Used for calculations within advancePeriod. Only saved for optimisation.
     *
     * Used to calculate recursive functions f and f_τ in NDEMD eq 1.6, 1.7.
     * Values are recalculated each step; only fArray[0] and
     * ftauArray[0..mosqRestDuration] are stored across steps for optimisation
     * (reallocating each time they are needed would be slow).
     * 
     * uninfected_v[0] is not used.
     *
     * Length (fArray): EIPDuration - mosqRestDuration + 1 (θ_s - τ + 1)
     * Length (ftauArray): EIPDuration (θ_s)
     * Length (uninfected_v): N_v_length
     *
     * Don't need to be checkpointed, but some values need to be initialised. */
    //@{
    std::vector<double> f_array;
    std::vector<double> ftau_array;
    std::vector<double> uninfected_v;
    //@}

    /** Variables tracking data to be reported. */
    double time_step_n_v0{0.0};

    /** Active Non-Human hosts instances in the simulation. */
    map<string, Nhh> nhh_instances;

    /** Parameters for trap interventions. Doesn't need checkpointing. */
    vector<TrapParams> trap_params;

    /** Interventions affecting death rate while seeking (parameters + state) based on a given probability */
    vector<util::SimpleDecayingValue> prob_additional_death_sugar_feeding_intervs;

    /** Interventions affecting death rate while seeking (parameters + state) */
    vector<util::SimpleDecayingValue> seeking_death_rate_intervs;

    /** Interventions affecting probability of dying while ovipositing (pre laying of eggs) (parameters + state) */
    vector<util::SimpleDecayingValue> prob_death_ovipositing_intervs;

    /** Baited trap interventions.
     * 
     * This is the "full" availability: availability per trap times the number
     * of traps. Each deployment has its own availiability along with expiry
     * date; total availability is the sum. */
    list<TrapData> baited_traps;

    map<string, vector<util::SimpleDecayingValue>> reduce_nhh_availability, reduce_p_b_i, reduce_p_c_i, reduce_p_d_i,
        reduce_fecundity;

    /** Description of intervention killing effects on emerging pupae */
    vector<util::SimpleDecayingValue> emergence_reduction;

    const PerHostParams* per_host_params = nullptr;
    const WithinHost::GenotypeRegistry* genotypes = nullptr;
};

}

}
#endif
