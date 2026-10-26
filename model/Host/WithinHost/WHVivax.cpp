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

#include "Host/WithinHost/WHVivax.h"
#include "Host/WithinHost/Pathogenesis/PathogenesisModel.h"
#include "Host/WithinHost/Treatments.h"
#include "Host/WithinHost/Genotypes.h"
#include "Host/WithinHost/Infection/Infection.h"
#include "Host/Human.h"
#include "mon/Monitoring.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/checkpoint_containers.h"
#include "util/UnitParse.h"
#include "util/sampler.h"
#include <schema/scenario.h>
#include <algorithm>
#include <limits>
#include <cmath>

namespace OM::WithinHost {

using namespace OM::util;

HypnozoiteReleaseDistribution::HypnozoiteReleaseDistribution(const scnXml::HypnozoiteReleaseDistribution& elt)
    : sampler(createSampler(elt)), latent_relapse(elt.getLatentRelapse()) {}

SimTime HypnozoiteReleaseDistribution::sample_release_delay(LocalRng& rng) const {
    const double liver_stage_maximum_days = 16.0 * 30.0; // maximum of about 16 months in liver stage
    double delay = numeric_limits<double>::quiet_NaN();  // in days
    int count = 0;
    const int maxcount = 1000;

    do { // NOLINT(cppcoreguidelines-avoid-do-while)
        delay = sampler->sample(rng);
        count += 1;

        if (count >= maxcount) {
            throw util::xml_scenario_error(
                "<vivax><hypnozoiteRelease>  [random delay calculation causes probably an indefinite loop]:\n The "
                "hypnozoite release distribution seems off, sigma of secondRelease could be too high. We except "
                "the hypnozoite to reside a maximum of 16 months in the liver stage. Sigma choose well, dear "
                "padawan.");
        }
    } while (delay > liver_stage_maximum_days || delay < 0.0);

    assert(delay >= 0 && delay < liver_stage_maximum_days);
    return Sim::round_to_ts_from_days(delay + latent_relapse);
}

// ———  parameters  ———

#ifdef WHVivaxSamples
WHVivax* sampleHost = 0;
VivaxBrood* sampleBrood = 0;
#endif

VivaxParams::VivaxParams(const scnXml::Model& model, SimTime latent_p, bool deprecation_warnings)
    : latent_p(latent_p), deprecation_warnings(deprecation_warnings) {
    if (!model.getVivax().present())
        throw util::xml_scenario_error("no vivax model description in scenario XML");
    const scnXml::Vivax& elt = model.getVivax().get();
    prob_blood_stage_infectious_to_mosq = elt.getProbBloodStageInfectiousToMosq().getValue();
    const int max_number_hypnozoites = elt.getHypnozoiteRelease().getNumberHypnozoites().getMax();
    const double base_number_hypnozoites = elt.getHypnozoiteRelease().getNumberHypnozoites().getBase();
    const scnXml::HypnozoiteRelease& hr = elt.getHypnozoiteRelease();
    latent_relapse_1st = std::make_unique<HypnozoiteReleaseDistribution>(hr.getFirstReleaseDays());
    if (hr.getSecondReleaseDays().present()) {
        latent_relapse_2nd = std::make_unique<HypnozoiteReleaseDistribution>(hr.getSecondReleaseDays().get());
        p_second_release = hr.getPSecondRelease();
        assert(p_second_release >= 0 && p_second_release <= 1);
    } // else p_second_release is NaN and other values don't get used
    blood_stage_protection_latency = Sim::round_to_ts_from_days(elt.getBloodStageProtectionLatency().getValue());
    blood_stage_length.setParams(elt.getBloodStageLengthDays());

    const scnXml::ClinicalEvents& ce = elt.getClinicalEvents();
    p_primary_a = ce.getPPrimaryInfection().getA();
    p_primary_b = ce.getPPrimaryInfection().getB();
    p_relapse_one_a = ce.getPRelapseOne().getA();
    p_relapse_one_b = ce.getPRelapseOne().getB();
    p_relapse_two_a = ce.getPRelapseTwoPlus().getA();
    p_relapse_two_b = ce.getPRelapseTwoPlus().getB();
    p_event_is_severe = ce.getPEventIsSevere().getValue();
    vivax_clin_option = ce.getVivaxClinOption();

    // Define accepted values for vivaxClinOption
    const std::set<std::string> accepted_options = {"A1j", "A2j", "B1j", "B2j"};

    // Check if vivaxClinOption is valid
    if (accepted_options.find(vivax_clin_option) == accepted_options.end()) {
        throw util::xml_scenario_error("Invalid vivaxClinOption: " + vivax_clin_option +
                                       ". Accepted values are: A1j, A2j, B1j, B2j.");
    }

    // number of hypnozoites per brood:
    assert(base_number_hypnozoites <= 1 && base_number_hypnozoites >= 0);
    double total = 0.0;
    for (int n = 0; n <= max_number_hypnozoites; ++n)
        total += pow(base_number_hypnozoites, n);

    double cum_p = 0.0;
    for (int n = 0; n <= max_number_hypnozoites; ++n) {
        cum_p += pow(base_number_hypnozoites, n) / total;
        // pair n with the cumulative probability of sampling n:
        n_hypnozoites_prob_map[cum_p] = n;
    }
}

void VivaxParams::set_health_system(const scnXml::LiverStageDrug* elt) {
    VivaxHealthSystemParams& hs = health_system;
    const double old_p_het_no_pq = hs.p_het_no_pq;
    if (elt == nullptr) {
        hs.ignore_no_pq = false;
        hs.p_het_no_pq = 0.0;
        hs.p_receive_pq = 0.0;
        hs.effectiveness_pq = 1.0; // sensible default: does not affect simple liver stage treatment option
    } else {
        hs.ignore_no_pq =
            elt->getIgnoreCannotReceive().present() ? elt->getIgnoreCannotReceive().get().getValue() : false;
        hs.p_het_no_pq = elt->getPHumanCannotReceive().getValue();
        hs.p_receive_pq = elt->getPUseUncomplicated().present() ? elt->getPUseUncomplicated().get().getValue() : 0.0;
        if (hs.p_receive_pq > 0.0) {
            if (deprecation_warnings) {
                cerr << "Deprecation warning: pUseUncomplicated is deprecated; it is "
                        "suggested to use the liver stage simple treatment option instead."
                     << endl; // NOLINT(performance-avoid-endl)
            }
        }
        hs.effectiveness_pq = elt->getEffectivenessOnUse().getValue();
    }

    if (!std::isnan(old_p_het_no_pq) && old_p_het_no_pq != hs.p_het_no_pq) {
        throw util::xml_scenario_error("changeHS cannot change pHumanCannotReceive value");
    }
}

int VivaxParams::sample_n_hypnozoites(LocalRng& rng) const {
    const double x = rng.uniform_01();
    // upper_bound finds the first key (cumulative probability) greater than x:
    auto it = n_hypnozoites_prob_map.upper_bound(x);
    assert(it != n_hypnozoites_prob_map.end()); // i.e. that we did find a real key
    return it->second;                          // corresponding n
}

SimTime VivaxParams::sample_release_delay(LocalRng& rng) const {
    bool is_first_release = true;
    if (p_second_release == 1.0 || (p_second_release > 0.0 && rng.bernoulli(p_second_release))) {
        is_first_release = false;
    }

    if (is_first_release) {
        return latent_relapse_1st->sample_release_delay(rng);
    } else {
        return latent_relapse_2nd->sample_release_delay(rng);
    }
}

// ———  per-brood code  ———

VivaxBrood::VivaxBrood(LocalRng& rng, const VivaxParams& params, int origin, WHVivax* host) : origin(origin) {
    set<SimTime> releases; // used to initialise release_dates; a set is better to use now but a vector later

    // primary blood stage plus hypnozoites (relapses)
    releases.insert(Sim::now_or_ts0() + params.latent_p);
    const int number_hypnozoites = params.sample_n_hypnozoites(rng);

    for (int i = 0; i < number_hypnozoites;) {
        const SimTime random_release_delay = params.sample_release_delay(rng);
        const SimTime time_to_release = Sim::now_or_ts0() + params.latent_p + random_release_delay;
        const bool inserted = releases.insert(time_to_release).second;
        if (inserted)
            ++i; // successful
        // else: sample clash with an existing release date, so resample
    }

    // Copy times to the vector, backwards (smallest last):
    release_dates.insert(release_dates.end(), releases.rbegin(), releases.rend());

#ifdef WHVivaxSamples
    if (sampleHost == host && sampleBrood == 0) {
        sampleBrood = this;
        cout << "New sample brood";
        for (auto it = release_dates.begin(); it != release_dates.end(); ++it)
            cout << '\t' << *it;
        cout << endl;
    }
#endif
}
VivaxBrood::~VivaxBrood() {
#ifdef WHVivaxSamples
    if (sampleBrood == this) {
        sampleBrood = 0;
        cout << "Brood terminated" << endl;
    }
#endif
}

void VivaxBrood::checkpoint(ostream& stream) {
    release_dates & stream;
    blood_stage_clear_date & stream;
    primary_has_started & stream;
    relapse_has_started & stream;
    relapseb_has_started & stream;
    had_event & stream;
    had_relapse & stream;
    origin & stream;
}
VivaxBrood::VivaxBrood(istream& stream) {
    release_dates & stream;
    blood_stage_clear_date & stream;
    primary_has_started & stream;
    relapse_has_started & stream;
    relapseb_has_started & stream;
    had_event & stream;
    had_relapse & stream;
    origin & stream;
}

VivaxBrood::UpdResult VivaxBrood::update(LocalRng& rng, const VivaxParams& params) {
    if (blood_stage_clear_date == Sim::ts0()) {
        //NOTE: this effectively means that both asexual and sexual stage
        // parasites self-terminate. It also means the immune system can
        // protect against new blood-stage infections for a short time.
    }

    UpdResult result;
    while (release_dates.size() > 0 && release_dates.back() == Sim::ts0()) {
        release_dates.pop_back();

#ifdef WHVivaxSamples
        if (sampleBrood == this) {
            cout << "Time\t" << Sim::ts0();
            for (auto it = release_dates.begin(); it != release_dates.end(); ++it)
                cout << '\t' << *it;
            cout << endl;
        }
#endif

        // an existing or recently terminated blood stage from the same brood
        // protects against a newly released hypnozoite for relapse classification B
        if ((params.vivax_clin_option == "B1j" || params.vivax_clin_option == "B2j") &&
            (blood_stage_clear_date + params.blood_stage_protection_latency >= Sim::ts0()))
            continue;

        if (!relapseb_has_started && relapse_has_started) {
            relapseb_has_started = true;
            result.new_relapseb_bs = true;
        }
        if (!relapse_has_started && primary_has_started) {
            relapse_has_started = true;
            result.new_relapse_bs = true;
        }
        if (!primary_has_started) {
            primary_has_started = true;
            result.new_primary_bs = true;
        }
        result.new_bs = true;

        const double length_days = params.blood_stage_length.sample(rng);
        blood_stage_clear_date = Sim::ts0() + Sim::round_to_ts_from_days(length_days);
        // Assume gametocytes emerge at the same time (they mature quickly and
        // we have little data, thus assume coincedence of start)
    }

    result.is_finished = (!is_patent()) && (release_dates.size() == 0);
    return result;
}

void VivaxBrood::treatment_bs() {
    // Blood stage treatment: clear both asexual and sexual parasites from the
    // blood. NOTE: we assume infections removed via treatment do not leave
    // protective immunity since the patient was unable to self-clear.
    blood_stage_clear_date = Sim::never();
}

void VivaxBrood::treatment_ls() {
    release_dates.clear(); // 100% clearance

    /* partial clearance code, in case of need:
    vector<SimTime> survivingZoites;
    survivingZoites.reserve( release_dates.size() );   // maximum size we need
    for( auto it = release_dates.begin(); it != release_dates.end(); ++it ){
        if( !rng.bernoulli( pClearEachHypnozoite ) ){
            survivingZoites.push_back( *it );    // copy            
        }
    }
    swap( release_dates, survivingZoites );
    */
}

// ———  per-host code  ———

WHVivax::WHVivax(const VivaxParams& params, const Pathogenesis::PathogenesisParams& pathogenesis,
                 const TreatmentRegistry& treatments, LocalRng& rng, double comorbidity_factor)
    : params(&params), pathogenesis(&pathogenesis), treatments(&treatments),
      p_event(numeric_limits<double>::quiet_NaN()), p_first_relapse_event(numeric_limits<double>::quiet_NaN()) {
    if (comorbidity_factor != 1.0) {
        throw TRACED_EXCEPTION_DEFAULT("This vivax model cannot be used with comorbidity heterogeneity");
    }
#ifdef WHVivaxSamples
    if (sampleHost == 0) {
        sampleHost = this;
        cout << "New host" << endl;
    }
#endif
    const double p_het_no_pq = params.health_system.p_het_no_pq;
    no_pq = (p_het_no_pq > 0.0 && rng.bernoulli(p_het_no_pq));
}

WHVivax::~WHVivax() {
#ifdef WHVivaxSamples
    if (this == sampleHost) {
        sampleHost = 0;
        cout << "Host terminates" << endl;
    }
#endif
}

double WHVivax::prob_transmission_to_mosquito(vector<double>& prob_trans_genotype_i,
                                              vector<double>& prob_trans_genotype_l) const {
    assert(WithinHost::Genotypes::count() == 1);
    for (const auto& infection : infections) {
        if (infection.is_patent()) {
            // we have gametocytes from at least one brood
            return params->prob_blood_stage_infectious_to_mosq;
        }
    }
    return 0; // no gametocytes
}

bool WHVivax::summarize(Host::Human& human) const {
    if (infections.size() == 0)
        return false; // no infections: not patent, nothing to report
    mon::record_stat(mon::measure("nInfect"), human);
    bool patent_host = false;
    // (patent) infections are reported by genotype, even though we don't have
    // genotype in this model
    mon::record_stat(mon::measure("totalInfs"), human, static_cast<int>(infections.size()));
    for (const auto& infection : infections) {
        if (infection.is_patent()) {
            mon::record_stat(mon::measure("totalPatentInf"), human);
            patent_host = true;
        }
    }
    if (patent_host)
        mon::record_stat(mon::measure("nPatent"), human);
    return patent_host;
}

void WHVivax::import_infection(LocalRng& rng) {
    // this means one new liver stage infection, which can result in multiple blood stages
    infections.emplace_back(rng, *params, WithinHost::InfectionOrigin::Imported, this);
}

void WHVivax::update(Host::Human& human, LocalRng& rng, int& n_new_infs_i, int& n_new_infs_l,
                     vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double age_in_years) {
    p_severe = 0.0;

    // create new infections, letting the constructor do the initialisation work:
    for (int i = 0; i < n_new_infs_i; ++i)
        infections.emplace_back(rng, *params, InfectionOrigin::Introduced, this);

    for (int i = 0; i < n_new_infs_l; ++i)
        infections.emplace_back(rng, *params, InfectionOrigin::Indigenous, this);

    // update infections
    // NOTE: currently no BSV model
    morbidity = Pathogenesis::None;
    const uint32_t old_cum_inf = cum_prim_inf;
    const bool treatment_liver = treat_expiry_liver > Sim::ts0();
    const bool treatment_blood = treat_expiry_blood > Sim::ts0();
    const double mat_imm_clin = 1 - (0.90 * exp(-2.53 * age_in_years));
    //double matImmClin = 1 - (1 * exp(-(0.639*8*ageInYears)/2.53));
    auto inf = infections.begin();
    while (inf != infections.end()) {
        if (treatment_liver)
            inf->treatment_ls();
        if (treatment_blood)
            inf->treatment_bs(); // clearance due to treatment; no protection against reemergence
        const VivaxBrood::UpdResult result = inf->update(rng, *params);
        if (result.new_primary_bs)
            cum_prim_inf += 1;

        if (result.new_bs) {
            // Sample for each new blood stage infection: the chance of some clinical event.
            // model variant: no illness from relapses possible unless there was illness from the primary infection

            bool clinical_event = false;
            if (result.new_primary_bs) {
                // Blood stage is primary. oldCumInf wasn't updated yet.
                //double pPrimaryInfEvent = matImmClin * pPrimaryA * pPrimaryB / (pPrimaryB+oldCumInf);
                const double p_primary_inf_event =
                    mat_imm_clin * params->p_primary_a * exp(-params->p_primary_b * old_cum_inf);
                clinical_event = rng.bernoulli(p_primary_inf_event);
                inf->set_had_event(clinical_event);

            } else if (result.new_relapse_bs) {
                // Blood stage is a relapse. oldCumInf wasn't updated yet. Subtract 1 from oldCumInf not
                // to count the current brood in the number of cumulative primary infections

                const double p_first_relapse_event =
                    mat_imm_clin * params->p_relapse_one_a * exp(-params->p_relapse_one_b * (old_cum_inf - 1));
                clinical_event = rng.bernoulli(p_first_relapse_event);
                inf->set_had_relapse(clinical_event);

            } else if (result.new_relapseb_bs) {

                if (params->vivax_clin_option == "A1j" || params->vivax_clin_option == "A2j") {
                    const double p_first_relapse_event =
                        mat_imm_clin * params->p_relapse_one_a * exp(-params->p_relapse_one_b * (old_cum_inf - 1));
                    clinical_event = rng.bernoulli(p_first_relapse_event);
                    inf->set_had_relapse(clinical_event);
                }
                if (params->vivax_clin_option == "B1j" || params->vivax_clin_option == "B2j") {
                    const double p_second_relapse_event =
                        mat_imm_clin * params->p_relapse_two_a * exp(-params->p_relapse_two_b * (old_cum_inf - 1));
                    clinical_event = rng.bernoulli(p_second_relapse_event);
                    inf->set_had_relapse(clinical_event);
                }

            } else {
                const double p_second_relapse_event =
                    mat_imm_clin * params->p_relapse_two_a * exp(-params->p_relapse_two_b * (old_cum_inf - 1));
                clinical_event = rng.bernoulli(p_second_relapse_event);
            }

            if (clinical_event) {
                p_severe = p_severe + (1.0 - p_severe) * params->p_event_is_severe;
                if (rng.bernoulli(params->p_event_is_severe))
                    morbidity = static_cast<Pathogenesis::State>(morbidity | Pathogenesis::StateSevere);
                else
                    morbidity = static_cast<Pathogenesis::State>(morbidity | Pathogenesis::StateMalaria);
            }
        }

        if (result.is_finished)
            inf = infections.erase(inf);
        else
            ++inf;
    }

    //NOTE: currently we don't model co-infection or indirect deaths
    if (morbidity == Pathogenesis::None) {
        morbidity = pathogenesis->sample_nmf(rng, age_in_years);
    }
}

bool WHVivax::diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const {
    //TODO(monitoring): this shouldn't ignore the diagnostic (especially since
    // it should always return true if diagnostic.density=0)
    for (const auto& infection : infections) {
        if (infection.is_patent())
            return true; // at least one patent infection
    }
    return false;
}

Pathogenesis::StatePair WHVivax::determine_morbidity(Host::Human& human, double age_years, bool) {
    mon::record_stat(mon::measure("expectedSevere"), human, p_severe);
    Pathogenesis::StatePair result; // no indirect mortality in the vivax model
    result.state = morbidity;
    return result;
}

void WHVivax::clear_immunity() { throw TRACED_EXCEPTION_DEFAULT("vivax model does not include immune suppression"); }

void WHVivax::treatment(Host::Human& human, TreatmentId treat_id) {
    const Treatments& treat = treatments->select(treat_id);
    treat_simple(human, treat.liver_effect(), treat.blood_effect());

    // triggered intervention deployments:
    treat.deploy(human, mon::DeployMethod::Treat, interventions::VaccineLimits(/*default initialise: no limits*/));
}
void WHVivax::optional_pq_treatment(Host::Human& human) {
    // PQ clears liver stages. We don't worry about the effect of PQ on
    // gametocytes, because these are always cleared by blood-stage drugs with
    // Vivax, and PQ is not given without BS drugs. NOTE: this ignores drug failure.
    const VivaxHealthSystemParams& hs = params->health_system;
    if (hs.p_receive_pq > 0.0 && (hs.ignore_no_pq || !no_pq) && human.clinical_rng.bernoulli(hs.p_receive_pq)) {
        if (human.host_rng.bernoulli(hs.effectiveness_pq)) {
            for (auto& infection : infections) {
                infection.treatment_ls();
            }
        }
        mon::record_event(mon::measure("nLiverStageTreatments"), human);
    }
}
bool WHVivax::treat_simple(Host::Human& human, SimTime time_liver, SimTime time_blood) {
    //TODO: this should be implemented properly (allowing effects on next
    // update instead of now)

    // liver-stage treatment is only via "LiverStageDrug" option, if at all
    const VivaxHealthSystemParams& hs = params->health_system;
    if (time_liver != Sim::zero()) {
        if (hs.p_receive_pq > 0.0) {
            // This is only really disallowed to prevent simultaneous treatment through both methods
            throw util::xml_scenario_error(
                "simple treatment for vivax liver "
                "stages is incompatible with case-management pUseUncomplicated "
                "(liverStageDrug) option; it is suggested to use the former over the latter");
        }
        if ((hs.ignore_no_pq || !no_pq) &&
            (hs.effectiveness_pq == 1.0 || human.host_rng.bernoulli(hs.effectiveness_pq))) {
            if (time_liver >= Sim::zero()) {
                treat_expiry_liver = max(treat_expiry_liver, Sim::now_or_ts1() + time_liver);
            } else {
                for (auto& infection : infections) {
                    infection.treatment_ls();
                }
            }
        }
        mon::record_event(mon::measure("nLiverStageTreatments"), human);
    }

    // there probably will be blood-stage treatment
    if (time_blood != Sim::zero()) {
        if (time_blood < Sim::zero()) {
            // legacy mode: retroactive clearance
            for (auto& infection : infections) {
                infection.treatment_bs();
            }
        } else {
            treat_expiry_blood = max(treat_expiry_blood, Sim::now_or_ts1() + time_blood);
        }
        return true; // blood stage treatment
    }
    return false; // no blood stage treatment
}

// ———  boring stuff: checkpointing and set-up  ———

void WHVivax::checkpoint(istream& stream) {
    WHInterface::checkpoint(stream);
    size_t len = 0;
    len & stream;
    for (size_t i = 0; i < len; ++i) {
        infections.emplace_back(stream);
    }
    no_pq & stream;
    int morbidity_i = 0;
    morbidity_i & stream;
    morbidity = static_cast<Pathogenesis::State>(morbidity_i);
    cum_prim_inf & stream;
    treat_expiry_liver & stream;
    treat_expiry_blood & stream;
    p_event & stream;
    p_first_relapse_event & stream;
}
void WHVivax::checkpoint(ostream& stream) {
    WHInterface::checkpoint(stream);
    infections.size() & stream;
    for (auto& infection : infections) {
        infection.checkpoint(stream);
    }
    no_pq & stream;
    static_cast<int>(morbidity) & stream;
    cum_prim_inf & stream;
    treat_expiry_liver & stream;
    treat_expiry_blood & stream;
    p_event & stream;
    p_first_relapse_event & stream;
}

char const* const NotImpl = "feature not available in Vivax model";
void WHVivax::treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d) {
    throw TRACED_EXCEPTION(NotImpl, util::Error::WHFeatures);
}
double WHVivax::get_total_density() const { throw TRACED_EXCEPTION(NotImpl, util::Error::WHFeatures); }
double WHVivax::get_cumulative_h() const { throw TRACED_EXCEPTION(NotImpl, util::Error::WHFeatures); }
double WHVivax::get_cumulative_y() const { throw TRACED_EXCEPTION(NotImpl, util::Error::WHFeatures); }

}
