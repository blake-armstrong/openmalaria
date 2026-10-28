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

#ifndef HMOD_WITHIN_HOST_VIVAX
#define HMOD_WITHIN_HOST_VIVAX

#include "Global.h"
#include "Host/WithinHost/WHInterface.h"
#include "util/sampler.h"

#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>

using namespace std;

class UnittestUtil;

namespace scnXml {
class HypnozoiteReleaseDistribution;
class LiverStageDrug;
class Model;
}
namespace OM {
namespace WithinHost {
namespace Pathogenesis {
struct PathogenesisParams;
}
class TreatmentRegistry;

class HypnozoiteReleaseDistribution {
public:
    explicit HypnozoiteReleaseDistribution(const scnXml::HypnozoiteReleaseDistribution& elt);

    /// Sample the time until next release
    SimTime sample_release_delay(LocalRng& rng) const;

private:
    std::unique_ptr<util::LognormalSampler> sampler;
    double latent_relapse; // days
};

struct VivaxHealthSystemParams {
    bool ignore_no_pq = false;
    double p_het_no_pq = std::numeric_limits<double>::signaling_NaN();
    double p_receive_pq = std::numeric_limits<double>::signaling_NaN();
    double effectiveness_pq = std::numeric_limits<double>::signaling_NaN();
};

struct VivaxParams {
    SimTime latent_p = Sim::never();
    double prob_blood_stage_infectious_to_mosq = std::numeric_limits<double>::signaling_NaN();
    std::unique_ptr<HypnozoiteReleaseDistribution> latent_relapse_1st, latent_relapse_2nd;
    double p_second_release = std::numeric_limits<double>::signaling_NaN();
    SimTime blood_stage_protection_latency = Sim::never();
    util::WeibullSampler blood_stage_length; // units: days
    double p_primary_a = std::numeric_limits<double>::signaling_NaN();
    double p_primary_b = std::numeric_limits<double>::signaling_NaN();
    double p_relapse_one_a = std::numeric_limits<double>::signaling_NaN();
    double p_relapse_one_b = std::numeric_limits<double>::signaling_NaN();
    double p_relapse_two_a = std::numeric_limits<double>::signaling_NaN();
    double p_relapse_two_b = std::numeric_limits<double>::signaling_NaN();
    double p_event_is_severe = std::numeric_limits<double>::signaling_NaN();
    std::string vivax_clin_option = " ";
    std::map<double, int> n_hypnozoites_prob_map;
    VivaxHealthSystemParams health_system;
    bool deprecation_warnings = true;
    const Clock* clock = nullptr;

    VivaxParams() = default;
    VivaxParams(const scnXml::Model& model, SimTime latent_p, bool deprecation_warnings, const Clock& clock);

    void set_health_system(const scnXml::LiverStageDrug* elt);

    int sample_n_hypnozoites(LocalRng& rng) const;
    /// Time to hypnozoite release after initial release
    SimTime sample_release_delay(LocalRng& rng) const;
};

class WHVivax;
/**
 * A brood is the set of hypnozoites resulting from an innoculation, plus an
 * associated blood stage.
 * 
 * In this model, if a hypnozoite releases while a blood stage infection
 * initiated by another hypnozoite from the same brood is active, the newly
 * released hypnozoite does nothing, however, blood stage infections from other
 * broods have no effect.
 */
class VivaxBrood {
public:
    /** Create.
     * 
     * @param host      The host creating this. Not really needed, except to
     * prevent VivaxBrood being default-constructed by its container.
     */
    VivaxBrood(LocalRng& rng, const VivaxParams& params, int origin, WHVivax* host);
    VivaxBrood(const VivaxBrood&) = default;
    VivaxBrood& operator=(const VivaxBrood&) = default;
    VivaxBrood(VivaxBrood&&) = default;
    VivaxBrood& operator=(VivaxBrood&&) = default;
    ~VivaxBrood();
    /** Save a checkpoint. */
    void checkpoint(ostream& stream);
    /** Create from checkpoint. */
    VivaxBrood(istream& stream);

    struct UpdResult {
        UpdResult() = default;
        bool new_primary_bs{false}, new_relapse_bs{false}, new_relapseb_bs{false}, new_bs{false}, is_finished{};
    };
    /**
     * Do per time step update: remove finished blood stage infections and act
     * on newly releasing hypnozoites.
     * 
     * @return pair describing whether the infection is finished (no more blood
     *  or liver stages) and whether a new primary blood-stage has started (to
     *  update cumPrimInf)
     */
    UpdResult update(LocalRng& rng, const VivaxParams& params);

    inline void set_had_event(bool had_event) { this->had_event = had_event; }
    inline bool has_had_event() const { return had_event; }
    inline void set_had_relapse(bool had_relapse) { this->had_relapse = had_relapse; }
    inline bool has_had_relapse() const { return had_relapse; }

    /** Equivalent to a blood stage existing. We do not model incidence of
     * gametocytes independently, thus this also tests existance of
     * gametocytes. */
    inline bool is_patent() const { return blood_stage_clear_date > Sim::latest_ts0(); }

    /** Fully clear blood stage parasites. */
    void treatment_bs();

    /** Fully clear liver stage parasites. */
    void treatment_ls();

private:
    // list of times at which the merozoite and hypnozoites release, ordered by
    // time of release, soonest last (i.e. last element is next one to release)
    vector<SimTime> release_dates;

    // Either sim::never() (no blood stage) or the start of the time step on
    // which the blood stage will clear.
    SimTime blood_stage_clear_date = Sim::never();

    // Whether the primary blood stage infection has started
    bool primary_has_started{};

    // Whether the relapse blood stage infection has started
    bool relapse_has_started{};

    //Whether the later category of relapses has started
    bool relapseb_has_started{};

    // Whether any clinical event has been triggered by this brood
    bool had_event{};

    // Whether a relapse event has been triggered by this brood
    bool had_relapse{};

    // Infection origin
    int origin{};
};

/**
 * Implementation of a basic vivax model
 */
class WHVivax : public WHInterface {
public:
    /// @brief Constructors, destructors and checkpointing functions
    //@{
    WHVivax(const VivaxParams& params, const Pathogenesis::PathogenesisParams& pathogenesis,
            const TreatmentRegistry& treatments, LocalRng& rng, double comorbidity_factor);
    WHVivax(const WHVivax&) = delete;
    WHVivax& operator=(const WHVivax&) = delete;
    WHVivax(WHVivax&&) = delete;
    WHVivax& operator=(WHVivax&&) = delete;
    ~WHVivax() override;
    //@}

    double prob_transmission_to_mosquito(vector<double>& prob_trans_genotype_i,
                                         vector<double>& prob_trans_genotype_l) const override;

    bool summarize(Host::Human& human) const override;

    void import_infection(LocalRng& rng) override;

    void update(Host::Human& human, LocalRng& rng, int& n_new_infs_i, int& n_new_infs_l,
                vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double age_in_years) override;

    bool diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const override;

    Pathogenesis::StatePair determine_morbidity(Host::Human& human, double age_years, bool) override;

    void clear_immunity() override;

    InfectionOrigin infection_origin() const override { return InfectionOrigin::Indigenous; }

protected:
    void treatment(Host::Human& human, TreatmentId treat_id) override;
    bool treat_simple(Host::Human& human, SimTime time_liver, SimTime time_blood) override;
    void optional_pq_treatment(Host::Human& human) override;

    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

    // None of these do anything in this model:
    void treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d) override;
    double get_total_density() const override;
    double get_cumulative_h() const override;
    double get_cumulative_y() const override;

private:
    const VivaxParams* params;
    const Pathogenesis::PathogenesisParams* pathogenesis;
    const TreatmentRegistry* treatments;

    list<VivaxBrood> infections;

    /* Is flagged as never getting PQ: this is a heterogeneity factor. Example:
     * Set to zero if everyone can get PQ, 0.5 if females can't get PQ and
     * males aren't tested (i.e. all can get it) or (1+p)/2 where p is the
     * chance a male being tested and found to be G6PD deficient. */
    bool no_pq;

    Pathogenesis::State morbidity = Pathogenesis::None;

    // The number of primary blood stage infections to have started since this
    // human was born.
    uint32_t cum_prim_inf{0};

    // Either never or expiry time of treatment
    SimTime treat_expiry_liver{0}, treat_expiry_blood{0};

    // The probability of a clinical event from a primary infection
    double p_event;
    // The probability of a clinical event from an early relapse
    double p_first_relapse_event;

    // Used for reporting; updated by update()
    double p_severe{0.0};

    friend class ::UnittestUtil;
};

}
}
#endif
