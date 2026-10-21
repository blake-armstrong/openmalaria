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
#include "Host/WithinHost/WithinHostModel.h"
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
#include "util/CommandLine.h"
#include "util/sampler.h"
#include <schema/scenario.h>
#include <algorithm>
#include <limits>
#include <cmath>

namespace OM {
namespace WithinHost {

using namespace OM::util;

HypnozoiteReleaseDistribution::HypnozoiteReleaseDistribution(const scnXml::HypnozoiteReleaseDistribution& elt)
    : sampler(createSampler(elt)), latent_relapse(elt.getLatentRelapse()) {}

SimTime HypnozoiteReleaseDistribution::sample_release_delay(LocalRng& rng) const {
    double liverStageMaximumDays = 16.0 * 30.0;         // maximum of about 16 months in liver stage
    double delay = numeric_limits<double>::quiet_NaN(); // in days
    int count = 0;
    int maxcount = 1e3;

    do {
        delay = sampler->sample(rng);
        count += 1;

        if (count >= maxcount) {
            throw util::xml_scenario_error(
                "<vivax><hypnozoiteRelease>  [random delay calculation causes probably an indefinite loop]:\n The "
                "hypnozoite release distribution seems off, sigma of secondRelease could be too high. We except "
                "the hypnozoite to reside a maximum of 16 months in the liver stage. Sigma choose well, dear "
                "padawan.");
        }
    } while (delay > liverStageMaximumDays || delay < 0.0);

    assert(delay >= 0 && delay < liverStageMaximumDays);
    return Sim::round_to_ts_from_days(delay + latent_relapse);
}

// ———  parameters  ———

#ifdef WHVivaxSamples
WHVivax* sampleHost = 0;
VivaxBrood* sampleBrood = 0;
#endif

VivaxParams::VivaxParams(const scnXml::Model& model, SimTime latent_p) : latent_p(latent_p) {
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
    const std::set<std::string> acceptedOptions = {"A1j", "A2j", "B1j", "B2j"};

    // Check if vivaxClinOption is valid
    if (acceptedOptions.find(vivax_clin_option) == acceptedOptions.end()) {
        throw util::xml_scenario_error("Invalid vivaxClinOption: " + vivax_clin_option +
                                       ". Accepted values are: A1j, A2j, B1j, B2j.");
    }

    // number of hypnozoites per brood:
    assert(base_number_hypnozoites <= 1 && base_number_hypnozoites >= 0);
    double total = 0.0;
    for (int n = 0; n <= max_number_hypnozoites; ++n)
        total += pow(base_number_hypnozoites, n);

    double cumP = 0.0;
    for (int n = 0; n <= max_number_hypnozoites; ++n) {
        cumP += pow(base_number_hypnozoites, n) / total;
        // pair n with the cumulative probability of sampling n:
        n_hypnozoites_prob_map[cumP] = n;
    }
}

void VivaxParams::set_health_system(const scnXml::LiverStageDrug* elt, const util::CommandLineOptions& command_line) {
    VivaxHealthSystemParams& hs = health_system;
    double oldPHetNoPQ = hs.p_het_no_pq;
    if (elt == 0) {
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
            if (command_line.option(util::CommandLine::DeprecationWarnings)) {
                cerr << "Deprecation warning: pUseUncomplicated is deprecated; it is "
                        "suggested to use the liver stage simple treatment option instead."
                     << endl;
            }
        }
        hs.effectiveness_pq = elt->getEffectivenessOnUse().getValue();
    }

    if (!std::isnan(oldPHetNoPQ) && oldPHetNoPQ != hs.p_het_no_pq) {
        throw util::xml_scenario_error("changeHS cannot change pHumanCannotReceive value");
    }
}

int VivaxParams::sample_n_hypnozoites(LocalRng& rng) const {
    double x = rng.uniform_01();
    // upper_bound finds the first key (cumulative probability) greater than x:
    auto it = n_hypnozoites_prob_map.upper_bound(x);
    assert(it != n_hypnozoites_prob_map.end()); // i.e. that we did find a real key
    return it->second;                          // corresponding n
}

SimTime VivaxParams::sample_release_delay(LocalRng& rng) const {
    bool isFirstRelease = true;
    if (p_second_release == 1.0 || (p_second_release > 0.0 && rng.bernoulli(p_second_release))) {
        isFirstRelease = false;
    }

    if (isFirstRelease) {
        return latent_relapse_1st->sample_release_delay(rng);
    } else {
        return latent_relapse_2nd->sample_release_delay(rng);
    }
}

// ———  per-brood code  ———

VivaxBrood::VivaxBrood(LocalRng& rng, const VivaxParams& params, int origin, WHVivax* host)
    : primaryHasStarted(false), relapseHasStarted(false), relapsebHasStarted(false), hadEvent(false), hadRelapse(false),
      origin(origin) {
    set<SimTime> releases; // used to initialise releaseDates; a set is better to use now but a vector later

    // primary blood stage plus hypnozoites (relapses)
    releases.insert(Sim::now_or_ts0() + params.latent_p);
    int numberHypnozoites = params.sample_n_hypnozoites(rng);

    for (int i = 0; i < numberHypnozoites;) {
        SimTime randomReleaseDelay = params.sample_release_delay(rng);
        SimTime timeToRelease = Sim::now_or_ts0() + params.latent_p + randomReleaseDelay;
        bool inserted = releases.insert(timeToRelease).second;
        if (inserted)
            ++i; // successful
        // else: sample clash with an existing release date, so resample
    }

    // Copy times to the vector, backwards (smallest last):
    releaseDates.insert(releaseDates.end(), releases.rbegin(), releases.rend());

#ifdef WHVivaxSamples
    if (sampleHost == host && sampleBrood == 0) {
        sampleBrood = this;
        cout << "New sample brood";
        for (auto it = releaseDates.begin(); it != releaseDates.end(); ++it)
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
    releaseDates & stream;
    bloodStageClearDate & stream;
    primaryHasStarted & stream;
    relapseHasStarted & stream;
    relapsebHasStarted & stream;
    hadEvent & stream;
    hadRelapse & stream;
    origin & stream;
}
VivaxBrood::VivaxBrood(istream& stream) {
    releaseDates & stream;
    bloodStageClearDate & stream;
    primaryHasStarted & stream;
    relapseHasStarted & stream;
    relapsebHasStarted & stream;
    hadEvent & stream;
    hadRelapse & stream;
    origin & stream;
}

VivaxBrood::UpdResult VivaxBrood::update(LocalRng& rng, const VivaxParams& params) {
    if (bloodStageClearDate == Sim::ts0()) {
        //NOTE: this effectively means that both asexual and sexual stage
        // parasites self-terminate. It also means the immune system can
        // protect against new blood-stage infections for a short time.
    }

    UpdResult result;
    while (releaseDates.size() > 0 && releaseDates.back() == Sim::ts0()) {
        releaseDates.pop_back();

#ifdef WHVivaxSamples
        if (sampleBrood == this) {
            cout << "Time\t" << Sim::ts0();
            for (auto it = releaseDates.begin(); it != releaseDates.end(); ++it)
                cout << '\t' << *it;
            cout << endl;
        }
#endif

        // an existing or recently terminated blood stage from the same brood
        // protects against a newly released hypnozoite for relapse classification B
        if ((params.vivax_clin_option == "B1j" || params.vivax_clin_option == "B2j") &&
            (bloodStageClearDate + params.blood_stage_protection_latency >= Sim::ts0()))
            continue;

        if (!relapsebHasStarted && relapseHasStarted) {
            relapsebHasStarted = true;
            result.newRelapsebBS = true;
        }
        if (!relapseHasStarted && primaryHasStarted) {
            relapseHasStarted = true;
            result.newRelapseBS = true;
        }
        if (!primaryHasStarted) {
            primaryHasStarted = true;
            result.newPrimaryBS = true;
        }
        result.newBS = true;

        double lengthDays = params.blood_stage_length.sample(rng);
        bloodStageClearDate = Sim::ts0() + Sim::round_to_ts_from_days(lengthDays);
        // Assume gametocytes emerge at the same time (they mature quickly and
        // we have little data, thus assume coincedence of start)
    }

    result.isFinished = (!isPatent()) && (releaseDates.size() == 0);
    return result;
}

void VivaxBrood::treatmentBS() {
    // Blood stage treatment: clear both asexual and sexual parasites from the
    // blood. NOTE: we assume infections removed via treatment do not leave
    // protective immunity since the patient was unable to self-clear.
    bloodStageClearDate = Sim::never();
}

void VivaxBrood::treatmentLS() {
    releaseDates.clear(); // 100% clearance

    /* partial clearance code, in case of need:
    vector<SimTime> survivingZoites;
    survivingZoites.reserve( releaseDates.size() );   // maximum size we need
    for( auto it = releaseDates.begin(); it != releaseDates.end(); ++it ){
        if( !rng.bernoulli( pClearEachHypnozoite ) ){
            survivingZoites.push_back( *it );    // copy            
        }
    }
    swap( releaseDates, survivingZoites );
    */
}

// ———  per-host code  ———

WHVivax::WHVivax(const VivaxParams& params, LocalRng& rng, double comorbidityFactor)
    : params(&params), cumPrimInf(0), treatExpiryLiver(0), treatExpiryBlood(0),
      pEvent(numeric_limits<double>::quiet_NaN()), pFirstRelapseEvent(numeric_limits<double>::quiet_NaN()),
      pSevere(0.0) {
    if (comorbidityFactor != 1.0) {
        throw TRACED_EXCEPTION_DEFAULT("This vivax model cannot be used with comorbidity heterogeneity");
    }
#ifdef WHVivaxSamples
    if (sampleHost == 0) {
        sampleHost = this;
        cout << "New host" << endl;
    }
#endif
    const double p_het_no_pq = params.health_system.p_het_no_pq;
    noPQ = (p_het_no_pq > 0.0 && rng.bernoulli(p_het_no_pq));
}

WHVivax::~WHVivax() {
#ifdef WHVivaxSamples
    if (this == sampleHost) {
        sampleHost = 0;
        cout << "Host terminates" << endl;
    }
#endif
}

double WHVivax::prob_transmission_to_mosquito(vector<double>& probTransGenotype_i,
                                              vector<double>& probTransGenotype_l) const {
    assert(WithinHost::Genotypes::N() == 1);
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        if (inf->isPatent()) {
            // we have gametocytes from at least one brood
            return params->prob_blood_stage_infectious_to_mosq;
        }
    }
    return 0; // no gametocytes
}

bool WHVivax::summarize(Host::Human& human) const {
    if (infections.size() == 0)
        return false; // no infections: not patent, nothing to report
    mon::recordStat(mon::measure("nInfect"), human);
    bool patentHost = false;
    // (patent) infections are reported by genotype, even though we don't have
    // genotype in this model
    mon::recordStat(mon::measure("totalInfs"), human, static_cast<int>(infections.size()));
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        if (inf->isPatent()) {
            mon::recordStat(mon::measure("totalPatentInf"), human);
            patentHost = true;
        }
    }
    if (patentHost)
        mon::recordStat(mon::measure("nPatent"), human);
    return patentHost;
}

void WHVivax::import_infection(LocalRng& rng) {
    // this means one new liver stage infection, which can result in multiple blood stages
    infections.push_back(VivaxBrood(rng, *params, WithinHost::InfectionOrigin::Imported, this));
}

void WHVivax::update(Host::Human& human, LocalRng& rng, int& nNewInfs_i, int& nNewInfs_l,
                     vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double ageInYears) {
    pSevere = 0.0;

    // create new infections, letting the constructor do the initialisation work:
    for (int i = 0; i < nNewInfs_i; ++i)
        infections.push_back(VivaxBrood(rng, *params, InfectionOrigin::Introduced, this));

    for (int i = 0; i < nNewInfs_l; ++i)
        infections.push_back(VivaxBrood(rng, *params, InfectionOrigin::Indigenous, this));

    // update infections
    // NOTE: currently no BSV model
    morbidity = Pathogenesis::NONE;
    uint32_t oldCumInf = cumPrimInf;
    bool treatmentLiver = treatExpiryLiver > Sim::ts0();
    bool treatmentBlood = treatExpiryBlood > Sim::ts0();
    double matImmClin = 1 - (0.90 * exp(-2.53 * ageInYears));
    //double matImmClin = 1 - (1 * exp(-(0.639*8*ageInYears)/2.53));
    auto inf = infections.begin();
    while (inf != infections.end()) {
        if (treatmentLiver)
            inf->treatmentLS();
        if (treatmentBlood)
            inf->treatmentBS(); // clearance due to treatment; no protection against reemergence
        VivaxBrood::UpdResult result = inf->update(rng, *params);
        if (result.newPrimaryBS)
            cumPrimInf += 1;

        if (result.newBS) {
            // Sample for each new blood stage infection: the chance of some clinical event.
            // model variant: no illness from relapses possible unless there was illness from the primary infection

            bool clinicalEvent = false;
            if (result.newPrimaryBS) {
                // Blood stage is primary. oldCumInf wasn't updated yet.
                //double pPrimaryInfEvent = matImmClin * pPrimaryA * pPrimaryB / (pPrimaryB+oldCumInf);
                double pPrimaryInfEvent = matImmClin * params->p_primary_a * exp(-params->p_primary_b * oldCumInf);
                clinicalEvent = rng.bernoulli(pPrimaryInfEvent);
                inf->setHadEvent(clinicalEvent);

            } else if (result.newRelapseBS) {
                // Blood stage is a relapse. oldCumInf wasn't updated yet. Subtract 1 from oldCumInf not
                // to count the current brood in the number of cumulative primary infections

                double pFirstRelapseEvent =
                    matImmClin * params->p_relapse_one_a * exp(-params->p_relapse_one_b * (oldCumInf - 1));
                clinicalEvent = rng.bernoulli(pFirstRelapseEvent);
                inf->setHadRelapse(clinicalEvent);

            } else if (result.newRelapsebBS) {

                if (params->vivax_clin_option == "A1j" || params->vivax_clin_option == "A2j") {
                    double pFirstRelapseEvent =
                        matImmClin * params->p_relapse_one_a * exp(-params->p_relapse_one_b * (oldCumInf - 1));
                    clinicalEvent = rng.bernoulli(pFirstRelapseEvent);
                    inf->setHadRelapse(clinicalEvent);
                }
                if (params->vivax_clin_option == "B1j" || params->vivax_clin_option == "B2j") {
                    double pSecondRelapseEvent =
                        matImmClin * params->p_relapse_two_a * exp(-params->p_relapse_two_b * (oldCumInf - 1));
                    clinicalEvent = rng.bernoulli(pSecondRelapseEvent);
                    inf->setHadRelapse(clinicalEvent);
                }

            } else {
                double pSecondRelapseEvent =
                    matImmClin * params->p_relapse_two_a * exp(-params->p_relapse_two_b * (oldCumInf - 1));
                clinicalEvent = rng.bernoulli(pSecondRelapseEvent);
            }

            if (clinicalEvent) {
                pSevere = pSevere + (1.0 - pSevere) * params->p_event_is_severe;
                if (rng.bernoulli(params->p_event_is_severe))
                    morbidity = static_cast<Pathogenesis::State>(morbidity | Pathogenesis::STATE_SEVERE);
                else
                    morbidity = static_cast<Pathogenesis::State>(morbidity | Pathogenesis::STATE_MALARIA);
            }
        }

        if (result.isFinished)
            inf = infections.erase(inf);
        else
            ++inf;
    }

    //NOTE: currently we don't model co-infection or indirect deaths
    if (morbidity == Pathogenesis::NONE) {
        morbidity = Pathogenesis::PathogenesisModel::sampleNMF(rng, ageInYears);
    }
}

bool WHVivax::diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const {
    //TODO(monitoring): this shouldn't ignore the diagnostic (especially since
    // it should always return true if diagnostic.density=0)
    for (auto inf = infections.begin(); inf != infections.end(); ++inf) {
        if (inf->isPatent())
            return true; // at least one patent infection
    }
    return false;
}

Pathogenesis::StatePair WHVivax::determine_morbidity(Host::Human& human, double ageYears, bool) {
    mon::recordStat(mon::measure("expectedSevere"), human, pSevere);
    Pathogenesis::StatePair result; // no indirect mortality in the vivax model
    result.state = morbidity;
    return result;
}

void WHVivax::clear_immunity() { throw TRACED_EXCEPTION_DEFAULT("vivax model does not include immune suppression"); }

void WHVivax::treatment(Host::Human& human, TreatmentId treatId) {
    const Treatments& treat = Treatments::select(treatId);
    treat_simple(human, treat.liverEffect(), treat.bloodEffect());

    // triggered intervention deployments:
    treat.deploy(human, mon::Deploy::TREAT, interventions::VaccineLimits(/*default initialise: no limits*/));
}
void WHVivax::optional_pq_treatment(Host::Human& human) {
    // PQ clears liver stages. We don't worry about the effect of PQ on
    // gametocytes, because these are always cleared by blood-stage drugs with
    // Vivax, and PQ is not given without BS drugs. NOTE: this ignores drug failure.
    const VivaxHealthSystemParams& hs = params->health_system;
    if (hs.p_receive_pq > 0.0 && (hs.ignore_no_pq || !noPQ) && human.clinical_rng.bernoulli(hs.p_receive_pq)) {
        if (human.host_rng.bernoulli(hs.effectiveness_pq)) {
            for (auto it = infections.begin(); it != infections.end(); ++it) {
                it->treatmentLS();
            }
        }
        mon::recordEvent(mon::measure("nLiverStageTreatments"), human);
    }
}
bool WHVivax::treat_simple(Host::Human& human, SimTime timeLiver, SimTime timeBlood) {
    //TODO: this should be implemented properly (allowing effects on next
    // update instead of now)

    // liver-stage treatment is only via "LiverStageDrug" option, if at all
    const VivaxHealthSystemParams& hs = params->health_system;
    if (timeLiver != Sim::zero()) {
        if (hs.p_receive_pq > 0.0) {
            // This is only really disallowed to prevent simultaneous treatment through both methods
            throw util::xml_scenario_error(
                "simple treatment for vivax liver "
                "stages is incompatible with case-management pUseUncomplicated "
                "(liverStageDrug) option; it is suggested to use the former over the latter");
        }
        if ((hs.ignore_no_pq || !noPQ) &&
            (hs.effectiveness_pq == 1.0 || human.host_rng.bernoulli(hs.effectiveness_pq))) {
            if (timeLiver >= Sim::zero()) {
                treatExpiryLiver = max(int(treatExpiryLiver), Sim::now_or_ts1() + timeLiver);
            } else {
                for (auto it = infections.begin(); it != infections.end(); ++it) {
                    it->treatmentLS();
                }
            }
        }
        mon::recordEvent(mon::measure("nLiverStageTreatments"), human);
    }

    // there probably will be blood-stage treatment
    if (timeBlood != Sim::zero()) {
        if (timeBlood < Sim::zero()) {
            // legacy mode: retroactive clearance
            for (auto it = infections.begin(); it != infections.end(); ++it) {
                it->treatmentBS();
            }
        } else {
            treatExpiryBlood = max(int(treatExpiryBlood), Sim::now_or_ts1() + timeBlood);
        }
        return true; // blood stage treatment
    }
    return false; // no blood stage treatment
}

// ———  boring stuff: checkpointing and set-up  ———

void WHVivax::checkpoint(istream& stream) {
    WHInterface::checkpoint(stream);
    size_t len;
    len & stream;
    for (size_t i = 0; i < len; ++i) {
        infections.push_back(VivaxBrood(stream));
    }
    noPQ & stream;
    int morbidity_i;
    morbidity_i & stream;
    morbidity = static_cast<Pathogenesis::State>(morbidity_i);
    cumPrimInf & stream;
    treatExpiryLiver & stream;
    treatExpiryBlood & stream;
    pEvent & stream;
    pFirstRelapseEvent & stream;
}
void WHVivax::checkpoint(ostream& stream) {
    WHInterface::checkpoint(stream);
    infections.size() & stream;
    for (auto it = infections.begin(); it != infections.end(); ++it) {
        it->checkpoint(stream);
    }
    noPQ & stream;
    static_cast<int>(morbidity) & stream;
    cumPrimInf & stream;
    treatExpiryLiver & stream;
    treatExpiryBlood & stream;
    pEvent & stream;
    pFirstRelapseEvent & stream;
}

char const* const not_impl = "feature not available in Vivax model";
void WHVivax::treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d) {
    throw TRACED_EXCEPTION(not_impl, util::Error::WHFeatures);
}
double WHVivax::get_total_density() const { throw TRACED_EXCEPTION(not_impl, util::Error::WHFeatures); }
double WHVivax::get_cumulative_h() const { throw TRACED_EXCEPTION(not_impl, util::Error::WHFeatures); }
double WHVivax::get_cumulative_y() const { throw TRACED_EXCEPTION(not_impl, util::Error::WHFeatures); }

void WHVivax::setHSParameters(const scnXml::LiverStageDrug* elt) {
    bridge::mutable_within_host_model().vivax.set_health_system(elt, bridge::command_line());
}

}
}
