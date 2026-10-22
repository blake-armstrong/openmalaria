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

#include "Clinical/EventScheduler.h"
#include "util/random.h"
#include "Host/WithinHost/WHInterface.h"
#include "mon/Monitoring.h"
#include "util/ModelOptions.h"
#include "util/errors.h"
#include "util/StreamValidator.h"
#include <schema/scenario.h>

#include <limits>

namespace OM::Clinical {
using namespace OM::util;

// -----  simulation-wide parameters  -----

EventSchedulerParams::EventSchedulerParams(const OM::Parameters& parameters, const util::ModelOptionFlags& options,
                                           const scnXml::Clinical& clinical) {
    if (Sim::one_ts() != Sim::one_day())
        throw util::xml_scenario_error("ClinicalEventScheduler is only designed for a 1-day time step.");

    non_malaria_fevers = options.option(util::NonMalariaFevers);

    alpha = exp(-parameters[Parameter::CfrNegLogAlpha]);
    if (!(0.0 <= alpha && alpha <= 1.0)) {
        throw util::xml_scenario_error("Clinical outcomes: propDeathsFirstDay should be within range [0,1]");
    }
    neg_v = -parameters[Parameter::CfrScaleFactor];

    if (options.option(util::NonMalariaFevers)) {
        if (!clinical.getNonMalariaFevers().present()) {
            throw util::xml_scenario_error("NonMalariaFevers element of model->clinical required");
        }
        const scnXml::Clinical::NonMalariaFeversType& nmf_desc2 = clinical.getNonMalariaFevers().get();
        if (!nmf_desc2.getPrNeedTreatmentMF().present() || !nmf_desc2.getPrNeedTreatmentNMF().present()) {
            throw util::xml_scenario_error("prNeedTreatmentMF and "
                                           "prNeedTreatmentNMF elements required in model->clinical->"
                                           "NonMalariaFevers");
        }
        nmf_need_antibiotic.set(nmf_desc2.getPrNeedTreatmentNMF().get(), "prNeedTreatmentNMF");
        mf_need_antibiotic.set(nmf_desc2.getPrNeedTreatmentMF().get(), "prNeedTreatmentMF");
    }
}

void EventSchedulerParams::set_parameters(const scnXml::HSEventScheduler& es_data,
                                          util::AgeGroupInterpolator& case_fatality_rate) {
    const scnXml::ClinicalOutcomes& co_data = es_data.getClinicalOutcomes();

    max_uc_seeking_memory = Sim::from_days(co_data.getMaxUCSeekingMemory());
    uncomplicated_case_duration = Sim::from_days(co_data.getUncomplicatedCaseDuration());
    complicated_case_duration = Sim::from_days(co_data.getComplicatedCaseDuration());
    extra_days_at_risk = Sim::from_days(co_data.getComplicatedRiskDuration()) - complicated_case_duration;
    if (uncomplicated_case_duration < Sim::from_days(1) || complicated_case_duration < Sim::from_days(1) ||
        max_uc_seeking_memory < Sim::zero() ||
        extra_days_at_risk + complicated_case_duration < Sim::from_days(1) // at risk at least 1 day
        || extra_days_at_risk > Sim::zero()                                // at risk longer than case duration
    ) {
        throw util::xml_scenario_error(
            "Clinical outcomes: constraints on case/risk/memory duration not met (see documentation)");
    }

    cum_daily_pr_imm_ucts.clear();
    cum_daily_pr_imm_ucts.reserve(co_data.getDailyPrImmUCTS().size());
    double cum_p = 0.0;
    for (const double daily_pr : co_data.getDailyPrImmUCTS()) {
        cum_p += daily_pr;
        cum_daily_pr_imm_ucts.push_back(cum_p);
    }
    if (cum_p < 0.999 || cum_p > 1.001) {
        throw util::xml_scenario_error("Event scheduler: dailyPrImmUCTS seq must add up to 1");
    }
    cum_daily_pr_imm_ucts.back() = 1.0;

    case_fatality_rate.scale(alpha);

    if (non_malaria_fevers) {
        if (!es_data.getNonMalariaFevers().present()) {
            throw util::xml_scenario_error("NonMalariaFevers element of healthSystem->EventScheduler required");
        }
        const scnXml::HSESNMF& nmf_desc = es_data.getNonMalariaFevers().get();

        const double p_t = nmf_desc.getPrTreatment();
        log_odds_ab_base = log(p_t / (1.0 - p_t));
        log_odds_ab_neg_test = log(nmf_desc.getEffectNegativeTest());
        log_odds_ab_pos_test = log(nmf_desc.getEffectPositiveTest());
        log_odds_ab_need = log(nmf_desc.getEffectNeed());
        log_odds_ab_informal = log(nmf_desc.getEffectInformal());
        one_minus_efficacy_ab = 1.0 - nmf_desc.getTreatmentEfficacy();
        severe_nmf_mortality.set(nmf_desc.getCFR(), "CFR");
    }
}

// -----  construction, destruction and checkpointing  -----

ClinicalEventScheduler::ClinicalEventScheduler(const ClinicalParams& params, double tsf)
    : ClinicalModel(params), case_start_time(Sim::never()), time_of_recovery(Sim::never()),
      time_last_treatment(Sim::never()), previous_density(numeric_limits<double>::quiet_NaN()) {
    if (tsf != 1.0) {
        // p(treatment seeking) is part of tree & handled generically, so we
        // don't have a way of modifying it.
        throw xml_scenario_error("treatment seeking heterogeneity not supported");
    }
}

// -----  other methods  -----

bool ClinicalEventScheduler::is_existing_case() {
    // If treated in the recent past:
    return Sim::now() > time_last_treatment && Sim::now() <= time_last_treatment + params().health_system_memory;
}

void ClinicalEventScheduler::do_clinical_update(Human& human, double age_years,
                                                WithinHost::Pathogenesis::StatePair& pg) {
    const EventSchedulerParams& es = params().event_scheduler;
    WHInterface& within_host_model = *human.withinHostModel;
    const auto new_state = static_cast<Episode::State>(pg.state);
    util::streamValidate((new_state << 16) & pg_state);

    if (Sim::ts0() == time_of_recovery) {
        if (pg_state & Episode::DirectDeath) {
            // Human dies this time step (last day of risk of death)
            doomed = DoomedComplicated;

            latest_report.update(human, pg_state);
        } else {
            if (pg_state & Episode::Complicated) {
                const double p_sequelae = params().p_sequelae_inpatient.eval(age_years);
                mon::recordStat(mon::measure("expectedSequelae"), human, p_sequelae);
                if (human.clinical_rng.uniform_01() < p_sequelae) {
                    pg_state = Episode::State(pg_state | Episode::Sequelae);
                } else {
                    pg_state = Episode::State(pg_state | Episode::Recovery);
                }
            } else {
                pg_state = Episode::State(pg_state | Episode::Recovery);
            }
            // report bout, at conclusion of episode:
            latest_report.update(human, pg_state);

            // Individual recovers (and is immediately susceptible to new cases)
            pg_state = Episode::None; // recovery (reset to healthy state)

            // And returns to transmission (if was removed)
            human.perHostTransmission.outsideTransmission = false;
        }
    }

    // The bug fixed here is that indirectMortality was not set if no event occurred
    bool indirect_mortality = false;
    if (params().indirect_mort_bugfix)
        indirect_mortality = pg.indirect_mortality;

    if (new_state & Episode::Sick) {
        // we have some new case: is it severe/complicated?
        if (new_state & Episode::Complicated) {
            if (pg_state & Episode::Complicated) {
                // previously severe: no events happen for course of medication
            } else {
                // previously healthy or UC: progress to severe
                pg_state = Episode::State(pg_state | new_state | Episode::RunCmTree);
                indirect_mortality = pg.indirect_mortality;
                case_start_time = Sim::ts0();
            }
        } else {
            // uncomplicated case (UC/UC2/NMF): is it new?
            if (pg_state & Episode::Sick) {
                // previously UC; nothing to do
            } else {
                if (case_start_time < Sim::ts0()) {
                    // new UC case
                    pg_state = Episode::State(pg_state | new_state | Episode::RunCmTree);
                    indirect_mortality = pg.indirect_mortality;

                    const double u_variate = human.clinical_rng.uniform_01();
                    size_t i = 0; // units: days
                    for (; i < es.cum_daily_pr_imm_ucts.size(); ++i) {
                        if (u_variate < es.cum_daily_pr_imm_ucts[i])
                            break;
                    }
                    assert(i < es.cum_daily_pr_imm_ucts.size()); // should have uVariate < 1 = cumDailyPrImmUCTS[len-1]
                    // set start time: current time plus length of delay (days)
                    case_start_time = Sim::ts0() + Sim::from_days(static_cast<int>(i));
                }
            }
        }

        if (indirect_mortality && doomed == NotDoomed)
            doomed = -Sim::one_ts(); // start indirect mortality countdown
    }

    if (case_start_time == Sim::ts0() && (pg_state & Episode::RunCmTree)) {
        // OK, we're about to run the CM tree
        pg_state = Episode::State(pg_state & ~Episode::RunCmTree);

        // If last treatment prescribed was in recent memory, consider second line.
        if (time_last_treatment + params().health_system_memory > Sim::ts0()) {
            pg_state = Episode::State(pg_state | Episode::SecondCase);
        }

        const CMDTOut aux_out =
            ESCaseManagement::execute(es, CMHostData(human, human.clinical_rng, age_years, pg_state));

        if (aux_out.treated) { // I.E. some treatment was given
            time_last_treatment = Sim::ts0();
            if (pg_state & Episode::Complicated) {
                mon::recordEvent(mon::measure("nTreatments3"), human);
            } else {
                if (pg_state & Episode::SecondCase) {
                    mon::recordEvent(mon::measure("nTreatments2"), human);
                } else {
                    mon::recordEvent(mon::measure("nTreatments1"), human);
                }
            }
        }
        if (aux_out.screened) {
            mon::recordEvent(mon::measure("nTreatDiagnostics"), human);
        }

        if (true /*FIXME auxOut.hospitalisation != CMAuxOutput::NONE*/) { // in hospital
            pg_state = Episode::State(pg_state | Episode::EventInHospital);

            /*FIXME if( auxOut.hospitalisation == CMAuxOutput::DELAYED )
		++caseStartTime;*/
        }

        // Case fatality rate (first day of illness)
        // P(death) is some fixed input scaled by age-specific CFR.
        if ((pg_state & Episode::Complicated) && !(pg_state & Episode::DirectDeath)) {
            const bool in_hospital = true /*FIXME auxOut.hospitalisation == CMAuxOutput::IMMEDIATE */;
            double p_death = params().case_fatality_rate.eval(age_years);
            // community fatality rate when not in hospital or delayed hospital entry
            if (!in_hospital)
                p_death = get_community_cfr(p_death);
            mon::recordStat(mon::measure("expectedDirectDeaths"), human, p_death);
            if (in_hospital)
                mon::recordStat(mon::measure("expectedHospitalDeaths"), human, p_death);
            if (human.clinical_rng.uniform_01() < p_death) {
                pg_state = Episode::State(pg_state | Episode::DirectDeath | Episode::EventFirstDay);
                // Human is killed at end of time at risk
                //timeOfRecovery += extraDaysAtRisk;	(no point updating; will be set later: ATORWD)
            }
            previous_density = within_host_model.get_total_density();
        }

        if (es.non_malaria_fevers) {
            if ((pg_state & Episode::Sick) && !(pg_state & Episode::Complicated)) {
                // Have a NMF or UC malaria case

                /** Given a non-malaria fever, return the probability of it requiring
                 * treatment. */
                const bool is_malarial = pg_state & Episode::Malaria;
                const double p_need_treat =
                    is_malarial ? es.mf_need_antibiotic.eval(age_years) : es.nmf_need_antibiotic.eval(age_years);
                const bool need_treat = human.clinical_rng.bernoulli(p_need_treat);

                // Calculate chance of antibiotic administration:
                const double p_treatment = 0.0;
                /*FIXME
                if( auxOut.AB_provider != CMAuxOutput::NO_AB ){
                    // Treatment seeking; may be at a health facility or from
                    // the informal sector.
                    double logOddsAb = logOddsAbBase - logOddsAbNeed * pNeedTreat;
                    if( auxOut.AB_provider == CMAuxOutput::FACILITY ){
                        if( auxOut.diagnostic == CMAuxOutput::NEGATIVE ){
                            logOddsAb += logOddsAbNegTest;
                        }else if( auxOut.diagnostic == CMAuxOutput::POSITIVE ){
                            logOddsAb += logOddsAbPosTest;
                        }
                        if( needTreat ){
                            logOddsAb += logOddsAbNeed;
                        }
                    }else{
                        assert( auxOut.AB_provider == CMAuxOutput::INFORMAL );
                        logOddsAb += logOddsAbInformal;
                    }
                    
                    double oddsTreatment = exp( logOddsAb );
                    pTreatment = oddsTreatment / (1.0 + oddsTreatment);
                }*/

                const double treatment_effect_mult = 1.0;

                if (human.clinical_rng.uniform_01() < p_treatment) {
                    /*FIXME: impossible due to above; NMF output removed
                    Survey::current().addInt( Report::MI_NMF_TREATMENTS, human, 1 );
                    treatmentEffectMult = oneMinusEfficacyAb;
                    */
                }

                // In a severe NMF case (only when not malarial), there is a
                // chance of death:
                if (need_treat) {
                    const double p_death = es.severe_nmf_mortality.eval(age_years) * treatment_effect_mult;
                    if (human.clinical_rng.uniform_01() < p_death) {
                        pg_state = Episode::State(pg_state | Episode::DirectDeath);
                    }
                }
            }
        }
    } else {
        // No new event (haven't changed state this time step).

        // Case fatality rate (subsequent days)
        // Complicated case & at risk of death (note: extraDaysAtRisk <= 0)
        if ((pg_state & Episode::Complicated) && !(pg_state & Episode::DirectDeath) &&
            (Sim::ts0() < time_of_recovery + es.extra_days_at_risk)) {
            // In complicated episodes, S(t), the probability of survival on
            // subsequent days t, is described by log(S(t)) = -v(Y(t)/Y(t-1)),
            // for parasite density Y(t). v_neg below is -v.
            // TODO: this model should be revisited, and if possible placed
            // within the WHFalciparum class (or a subclass).
            if (within_host_model.get_total_density() > 0.0) { // avoid division by zero
                const double parasite_reduction_effect = within_host_model.get_total_density() / previous_density;
                double p_death = 1.0 - exp(es.neg_v * parasite_reduction_effect);
                // community fatality rate when not in hospital
                if (!(pg_state & Episode::EventInHospital))
                    p_death = get_community_cfr(p_death);
                mon::recordStat(mon::measure("expectedDirectDeaths"), human, p_death);
                if (pg_state & Episode::EventInHospital)
                    mon::recordStat(mon::measure("expectedHospitalDeaths"), human, p_death);
                if (human.clinical_rng.uniform_01() < p_death) {
                    pg_state = Episode::State(pg_state | Episode::DirectDeath);
                    // Human is killed at end of time at risk
                    time_of_recovery = time_of_recovery + es.extra_days_at_risk; // may be re-set later (see ATORWD)
                }
            }
            previous_density = within_host_model.get_total_density();
        }
    }

    // Start of case. Not necessarily start of sickness due to treatment-seeking
    // delays and travel time.
    if (case_start_time == Sim::ts0()) {
        // Patients in hospital are removed from the transmission cycle.
        // This should have an effect from the start of the next time step.
        // NOTE: This is not very accurate, but considered of little importance.
        if (pg_state & Episode::EventInHospital)
            human.perHostTransmission.outsideTransmission = true;

        if (pg_state & Episode::Complicated) {
            // complicatedCaseDuration should to some respects be associated
            // with medication duration, however ongoing medications after
            // exiting hospital are OK and medications terminating before the
            // end of hospitalisation shouldn't matter too much if the person
            // can't recieve new infections due to zero transmission in hospital.
            time_of_recovery = Sim::ts0() + es.complicated_case_duration;
            // Time should be adjusted to end of at-risk period when patient dies:
            if (pg_state & Episode::DirectDeath)                             // death may already have been determined
                time_of_recovery = time_of_recovery + es.extra_days_at_risk; // ATORWD (search keyword)
        } else {
            time_of_recovery = Sim::ts0() + es.uncomplicated_case_duration;
        }
    }

    // Remove on first models...
    if (time_last_treatment == Sim::ts0())
        human.removeFirstEvent(interventions::SubPopRemove::ON_FIRST_TREATMENT);

    if (pg_state & Episode::Sick)
        human.removeFirstEvent(interventions::SubPopRemove::ON_FIRST_BOUT);
}

void ClinicalEventScheduler::checkpoint(istream& stream) {
    ClinicalModel::checkpoint(stream);
    int s = 0;
    s & stream;
    pg_state = Episode::State(s);
    case_start_time & stream;
    time_of_recovery & stream;
    time_last_treatment & stream;
    previous_density & stream;
}
void ClinicalEventScheduler::checkpoint(ostream& stream) {
    ClinicalModel::checkpoint(stream);
    pg_state & stream;
    case_start_time & stream;
    time_of_recovery & stream;
    time_last_treatment & stream;
    previous_density & stream;
}

}
