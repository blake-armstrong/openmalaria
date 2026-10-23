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

#include "interventions/ITN.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/SpeciesIndexChecker.h"
#include "Host/Human.h"
#include "R_nmath/qnorm.h"
#include "util/CommandLine.h"
#include <cmath>

namespace OM::interventions {

// —————  utility classes (internal use only)  —————

void factors::SurvivalFactor::init(const scnXml::ITNKillingEffect& elt, double max_insecticide, const char* elt_name,
                                   bool is_deterrent) {
    use_logit_eqns = false;
    a.bf = elt.getBaseFactor();
    a.hf = elt.getHoleFactor();
    a.pf = elt.getInsecticideFactor();
    a.IF = elt.getInteractionFactor();
    a.hole_scaling = elt.getHoleScalingFactor();
    a.insecticide_scaling = elt.getInsecticideScalingFactor();
    a.inv_base_survival = 1.0 / (1.0 - a.bf);
    if (!(a.bf >= 0.0 && a.bf < 1.0)) {
        ostringstream msg;
        msg << elt_name << ": expected baseFactor to be in range [0,1]";
        throw util::xml_scenario_error(msg.str());
    }
    if (!(a.hole_scaling >= 0.0 && a.insecticide_scaling >= 0.0)) {
        ostringstream msg;
        msg << elt_name << ": expected scaling factors to be non-negative";
        throw util::xml_scenario_error(msg.str());
    }
    // see below
    double pmax = 1.0 - exp(-max_insecticide * a.insecticide_scaling);

    if (is_deterrent) {
        // Note: the following argument is a modification of the one below
        // (when !isDeterrent). The original may make more sense.
        /* We want K ≥ 0 where K is the killing factor:
    K=BF+HF×h+PF×p+IF×h×p, with h and p defined as:
    h=exp(-holeIndex×holeScalingFactor),
    p=1−exp(-insecticideContent×insecticideScalingFactor). 
    
    By their nature, holeIndex ≥ 0 and insecticideContent ≥ 0. We restrict:
        holeScalingFactor ≥ 0
        insecticideScalingFactor ≥ 0
    Which implies both h and p lie in the range [0,1]. We also know 0 ≤ BF ≤ 1.
    
    We need K ≥ 0 or:
        BF + HF×h + PF×p + IF×h×p ≥ 0   (1)
    
    Lets derive some limits on HF, PF and IF such that the above inequality (1)
    is satisfied.
    
    A net can theoretically be unholed (holeIndex=0 ⇒ h=1) and have no
    insecticide (thus have p=0). Substituting these values in (1) yields:
        BF + HF ≥ 0     (2)
    
    The maximum value for p depends on the maximum insecticide content; denote
    pmax = max(p). Note that holeIndex has no finite maximum; thus, although
    for any finite value of holeIndex, h > 0, there is no h₀ > 0 s.t. for all
    values of holeIndex h ≥ h₀. For the limiting case of a tattered but
    insecticide-saturated net our parameters are therefore p=pmax, h=0:
        BF + PF×pmax ≥ 0        (3)
    
    Consider a net saturated with insecticide (p=pmax) and without holes (h=1):
        BF + HF + (PF+IF)×pmax ≥ 0      (4)
    
    The opposite extreme (the limiting case of a decayed net with no remaining
    insecticide and a large number of holes) yields only BF ≥ 0.
    
    Some of the above examples of nets may be unlikely, but there is only one
    restriction in our model making any of these cases impossible: some
    insecticide must have been lost by the time any holes occur. We ignore this
    since its effect is likely small, and thus all of the above are required to
    keep the factor in the range [0,1]. Further, the inequalities (2) - (3) are
    sufficient to keep the factor within [0,1] since h and p are non-negative
    and act linearly in (1).
    
    From the definition of p, we always have pmax ≤ 1, so substituting pmax=1
    in (5) - (8) gives us bounds which imply our requirement, however if pmax
    is finite they are stricter than necessary. Since insecticideScalingFactor
    is constant, max(p) coincides with max(insecticideContent) which, since
    insecticide content only decays over time, coincides with the maximum
    initial insecticide content, Pmax. Since the initial insecticide content is
    sampled from a normal distribution in our model it should have no finite
    maximum, thus implying we cannot achieve more relaxed bounds than (5) - (8)
    when pmax=1 (unless the standard deviation of our normal distribution is 1).
    We would however like to impose less strict bounds than these, thus we
    impose a maximum value on the initial insecticide content, Pmax, such that
    the probability of sampling a value from our parameterise normal
    distribution greater than Pmax is 0.001. */
        if (!(a.bf + a.hf >= 0.0 && a.bf + a.pf * pmax >= 0.0 && a.bf + a.hf + (a.pf + a.IF) * pmax >= 0.0)) {
            ostringstream msg;
            msg << elt_name << ": bounds not met:";
            if (!(a.bf + a.hf >= 0.0))
                msg << " baseFactor+holeFactor≥0";
            if (!(a.bf + a.pf * pmax >= 0.0))
                msg << " baseFactor+" << pmax << "×insecticideFactor≥0";
            if (!(a.pf + a.hf + (a.pf + a.IF) * pmax >= 0.0))
                msg << " baseFactor+holeFactor+" << pmax << "×(insecticideFactor+interactionFactor)≥0";
            throw util::xml_scenario_error(msg.str());
        }
    } else {
        /* We want the calculated survival factor (1−K)/(1−BF) to be in the range
    [0,1] where K is the killing factor: K=BF+HF×h+PF×p+IF×h×p, with h and p
    defined as: h=exp(-holeIndex×holeScalingFactor),
    p=1−exp(-insecticideContent×insecticideScalingFactor). 
    
    By their nature, holeIndex ≥ 0 and insecticideContent ≥ 0. We restrict:
        holeScalingFactor ≥ 0
        insecticideScalingFactor ≥ 0
    Which implies both h and p lie in the range [0,1]. We also know the base
    survival factor, 1−BF, is in the range [0,1].
    
    To make sure the survival factor is not negative we need (1−K)/(1−BF) ≥ 0.
    Since 1−BF > 0 we need 1−K ≥ 0, which, substituting K, gives us
        BF + HF×h + PF×p + IF×h×p ≤ 1	(1)
    We also want to make sure the survival factor is not greater than one (since
    nets shouldn't increase mosquito survival), (1−K)/(1−BF) ≤ 1 or equivalently
    1-K ≤ 1-BF or K ≥ BF, which, substituting K, yields
        HF×h + PF×p + IF×h×p ≥ 0		(2)
    
    Lets derive some limits on HF, PF and IF such that the above inequalities
    (1) and (2) are satisfied.
    
    A net can theoretically be unholed (holeIndex=0 ⇒ h=1) and have no
    insecticide (thus have p=0). Substituting these values in (1) and (2) yields:
        BF + HF ≤ 1	(3)
        HF ≥ 0		(4)
    
    The maximum value for p depends on the maximum insecticide content; denote
    pmax = max(p). Note that holeIndex has no finite maximum; thus, although
    for any finite value of holeIndex, h > 0, there is no h₀ > 0 s.t. for all
    values of holeIndex h ≥ h₀. For the limiting case of a tattered but
    insecticide-saturated net our parameters are therefore p=pmax, h=0:
        BF + PF×pmax ≤ 1	(5)
        PF×pmax ≥ 0			(6)
    (Assuming pmax > 0, (6) is equivalent to PF ≥ 0.)
    
    Consider a net saturated with insecticide (p=pmax) and without holes (h=1):
        BF + HF + (PF+IF)×pmax ≤ 1	(7)
        HF + (PF+IF)×pmax ≥ 0		(8)
    
    The opposite extreme (the limiting case of a decayed net with no remaining
    insecticide and a large number of holes) yields only BF ≤ 1 which we already
    know.
    
    Some of the above examples of nets may be unlikely, but there is only one
    restriction in our model making any of these cases impossible: some
    insecticide must have been lost by the time any holes occur. We ignore this
    since its effect is likely small, and thus all of the above are required to
    keep the survival factor in the range [0,1]. Further, these six inequalities
    (3) - (8) are sufficient to keep the survival factor within [0,1] since h
    and p are non-negative and act linearly in (1) and (2).
    
    From the definition of p, we always have pmax ≤ 1, so substituting pmax=1
    in (5) - (8) gives us bounds which imply our requirement, however if pmax
    is finite they are stricter than necessary. Since insecticideScalingFactor
    is constant, max(p) coincides with max(insecticideContent) which, since
    insecticide content only decays over time, coincides with the maximum
    initial insecticide content, Pmax. Since the initial insecticide content is
    sampled from a normal distribution in our model it should have no finite
    maximum, thus implying we cannot achieve more relaxed bounds than (5) - (8)
    when pmax=1 (unless the standard deviation of our normal distribution is 1).
    We would however like to impose less strict bounds than these, thus we
    impose a maximum value on the initial insecticide content, Pmax, such that
    the probability of sampling a value from our parameterise normal
    distribution greater than Pmax is 0.001. */
        if (!(a.bf + a.hf <= 1.0 && a.hf >= 0.0 && a.bf + a.pf * pmax <= 1.0 && a.pf >= 0.0 &&
              a.bf + a.hf + (a.pf + a.IF) * pmax <= 1.0 && a.hf + (a.pf + a.IF) * pmax >= 0.0)) {
            ostringstream msg;
            msg << elt_name << ": bounds not met:";
            if (!(a.bf + a.hf <= 1.0))
                msg << " baseFactor+holeFactor≤1";
            if (!(a.hf >= 0.0))
                msg << " holeFactor≥0";
            if (!(a.bf + a.pf * pmax <= 1.0))
                msg << " baseFactor+" << pmax << "×insecticideFactor≤1";
            if (!(a.pf >= 0.0))
                msg << " insecticideFactor≥0"; // if this fails, we know pmax>0 (since it is in any case non-negative) — well, or an NaN
            if (!(a.pf + a.hf + (a.pf + a.IF) * pmax <= 1.0))
                msg << " baseFactor+holeFactor+" << pmax << "×(insecticideFactor+interactionFactor)≤1";
            if (!(a.hf + (a.pf + a.IF) * pmax >= 0.0))
                msg << " holeFactor+" << pmax << "×(insecticideFactor+interactionFactor)≥0";
            throw util::xml_scenario_error(msg.str());
        }
    }
}

void factors::SurvivalFactor::init_logit(const scnXml::ITNEffectLogit& elt, double hole_index_max, bool is_deterrent) {
    use_logit_eqns = true;
    b.bf = elt.getBaseFactor();
    b.hf = elt.getHoleFactor();
    b.pf = elt.getInsecticideFactor();
    b.IF = elt.getInteractionFactor();
    b.h_max = log(hole_index_max + 1.0);
    const double x = exp(b.bf + b.hf * b.h_max);
    if (is_deterrent) {
        // pAtt0 =x/(x+1); this is 1/pAtt0:
        b.inv_base_effect = (x + 1.0) / x;
    } else {
        // K0 = x/(x+1), so 1/(1-K0) = x+1:
        b.inv_base_effect = x + 1.0;

        // We expect K >= K0 and deduce these "advisory limits":
        if (b.pf < 0.0 || b.pf + b.IF * b.h_max < 0.0) {
            cerr << "ITN.description.anophelesParams.*KillingEffectLogit: expected"
                 << "\n(insecticide) P >= 0, found P = " << b.pf
                 << "\n(interaction) P+I*log(holeIndexMax+1) >= 0, found " << b.pf + b.IF * b.h_max << endl;
        }
    }
}

void factors::SurvivalFactor::init1() {
    use_logit_eqns = false;
    a.inv_base_survival = 1.0;
    a.bf = 0.0;
    a.hf = 0.0;
    a.pf = 0.0;
    a.IF = 0.0;
    a.insecticide_scaling = 0.0;
    a.hole_scaling = 0.0;
}

double factors::SurvivalFactor::rel_p_att(double hole_index, double insecticide_content) const {
    if (!use_logit_eqns) {
        const double hole_component = exp(-hole_index * a.hole_scaling);
        const double insecticide_component = 1.0 - exp(-insecticide_content * a.insecticide_scaling);
        const double p_att =
            a.bf + a.hf * hole_component + a.pf * insecticide_component + a.IF * hole_component * insecticide_component;
        assert(p_att >= 0.0);
        return p_att / a.bf;
    } else {
        const double hole_component = min(log(hole_index + 1.0), b.h_max);
        const double insecticide_component = log(insecticide_content + 1.0);
        const double x = exp(b.bf + b.hf * hole_component + b.pf * insecticide_component +
                             b.IF * hole_component * insecticide_component);
        const double p_att = x / (x + 1.0);

        // By construction, this is non-negative:
        return p_att * b.inv_base_effect;
    }
}

double factors::SurvivalFactor::survival_factor(double hole_index, double insecticide_content) const {
    if (!use_logit_eqns) {
        double hole_component = exp(-hole_index * a.hole_scaling);
        double insecticide_component = 1.0 - exp(-insecticide_content * a.insecticide_scaling);
        const double killing_effect =
            a.bf + a.hf * hole_component + a.pf * insecticide_component + a.IF * hole_component * insecticide_component;

        double survival_factor = (1.0 - killing_effect) * a.inv_base_survival;
        assert(killing_effect <= 1.0);
        // survivalFactor might be out of bounds due to precision error, see #49
        if (survival_factor < 0.0)
            return 0.0;
        else if (survival_factor > 1.0)
            return 1.0;
        return survival_factor;
    } else {
        const double hole_component = min(log(hole_index + 1.0), b.h_max);
        const double insecticide_component = log(insecticide_content + 1.0);
        const double x = exp(b.bf + b.hf * hole_component + b.pf * insecticide_component +
                             b.IF * hole_component * insecticide_component);
        // K = x/(x+1), so (1-K) = 1/(x+1):
        const double surv = 1.0 / (x + 1.0);

        return surv * b.inv_base_effect;
    }
}

void factors::RelativeAttractiveness::init_single_stage(const scnXml::ITNDeterrency& elt, double max_insecticide) {
    model = DeterrencyModel::SingleStage;
    const double hf = elt.getHoleFactor();
    const double pf = elt.getInsecticideFactor();
    double IF = elt.getInteractionFactor();
    a.hole_scaling = elt.getHoleScalingFactor();
    a.insecticide_scaling = elt.getInsecticideScalingFactor();
    if (!(a.hole_scaling >= 0.0 && a.insecticide_scaling >= 0.0)) {
        throw util::xml_scenario_error(
            "ITN.description.anophelesParams.deterrency: expected scaling factors to be non-negative");
    }

    /* We need to ensure the relative availability is non-negative. However,
     * since it's an exponentiated value, it always will be.
     * 
     * If don't want nets to be able to increase transmission, the following
     * limits could also be applied. In general, however, there is no reason
     * nets couldn't make individuals more attractive to mosquitoes.
     * 
     * To ensure relative availability is at most one: relative availability is
     *  exp( log(HF)*h + log(PF)*p + log(IF)*h*p )
     * where HF, PF and IF are the hole, insecticide and interaction factors
     * respectively, with h and p defined as:
     *  h=exp(-holeIndex*holeScalingFactor),
     *  p=1−exp(-insecticideContent*insecticideScalingFactor).
     * We therefore need to ensure that:
     *  log(HF)*h + log(PF)*p + log(IF)*h*p ≤ 0
     * 
     * As with the argument below concerning limits of the killing effect
     * parameters, h and p will always be in the range [0,1] and p ≤ pmax.
     * We can then derive some bounds for HF and PF:
     *  log(HF) ≤ 0
     *  log(PF)×pmax ≤ 0
     *  log(HF) + (log(PF)+log(IF))×pmax = log(HF×(PF×IF)^pmax) ≤ 0
     * or equivalently (assuming pmax>0):
     *  HF ∈ (0,1]
     *  PF ∈ (0,1]
     *  HF×(PF×IF)^pmax ∈ (0,1]
     *
     * Weaker limits would not be sufficient, as with the argument for the
     * limits of killing effect arguments below. */
    // Print out a warning if nets may increase transmission.
    double pmax = 1.0 - exp(-max_insecticide * a.insecticide_scaling);
    if (!(hf > 0.0 && pf > 0.0 && IF > 0.0 && hf <= 1.0 && pf <= 1.0 && hf * pow(pf * IF, pmax) <= 1.0)) {
        cerr << "Note: since the following bounds are not met, the ITN could make humans more\n";
        cerr << "attractive to mosquitoes than they would be without a net.\n";
        cerr << "ITN.description.anophelesParams.deterrency: bounds not met:\n";
        if (!(hf > 0.0))
            cerr << "  holeFactor>0\n";
        if (!(pf > 0.0))
            cerr << "  insecticideFactor>0\n";
        if (!(IF > 0.0))
            cerr << "  interactionFactor>0\n";
        if (!(hf <= 1.0))
            cerr << "  holeFactor≤1\n";
        if (!(pf <= 1.0))
            cerr << "  insecticideFactor≤1\n";
        if (!(hf * pow(pf * IF, pmax) <= 1.0))
            cerr << "  holeFactor×(insecticideFactor×interactionFactor)^" << pmax << "≤1\n";
        cerr.flush();
    }
    a.l_hf = log(hf);
    a.l_pf = log(pf);
    a.l_if = log(IF);
}

void factors::RelativeAttractiveness::init_two_stage(const scnXml::TwoStageDeterrency& elt, double max_insecticide,
                                                     double hole_index_max, bool hole_index_max_present) {
    b.l_pf_entering = numeric_limits<double>::quiet_NaN();
    if (elt.getEntering().present()) {
        model = DeterrencyModel::TwoStage;
        const double pf = elt.getEntering().get().getInsecticideFactor();
        b.insecticide_scaling_entering = elt.getEntering().get().getInsecticideScalingFactor();
        if (!(pf > 0.0)) {
            // we take the log of PF, so it must be positive
            throw util::xml_scenario_error(
                "ITN.description.anophelesParams.twoStageDeterrency.entering: insecticideFactor must be positive "
                "since we take its logarithm.");
        }

        /* We need to ensure the relative availability is non-negative. However,
        * since it's an exponentiated value, it always will be.
        * 
        * If we don't want ITNs to be able to increase transmission, the following
        * limits could also be applied. In general, however, there is no reason
        * ITNs couldn't make individuals more attractive to mosquitoes.
        * 
        * To ensure relative availability is at most one: relative availability is
        *  exp( log(PF)*p ) = PF^p
        * where PF is the insecticide factor, with p∈[0,1] defined as:
        *  p=1−exp(-insecticideContent*insecticideScalingFactor).
        * We therefore just need PF ≤ 1. */
        // Print out a warning if ITNs may increase transmission.
        if (!(pf <= 1.0)) {
            cerr << "Note: since the following bounds are not met, the IRS could make humans more\n";
            cerr << "attractive to mosquitoes than they would be without IRS.\n";
            cerr << "IRS.description.anophelesParams.deterrency: bounds not met:\n";
            cerr << "  0<insecticideFactor≤1\n";
            cerr.flush();
        }
        b.l_pf_entering = log(pf);
    } else {
        assert(elt.getEnteringLogit().present());
        model = DeterrencyModel::TwoStageLogit;
        c.ent_base_factor = elt.getEnteringLogit().get().getBaseFactor();
        c.ent_insecticide_factor = elt.getEnteringLogit().get().getInsecticideFactor();
        if (c.ent_insecticide_factor > 0.0) {
            cerr << "ITN.description.anophelesParams.twoStageDeterrency.enteringLogit: \n"
                 << "insecticideFactor should be negative to for nets to reduce chance of entering." << endl;
        }
        // pre-calculate for efficieny:
        c.p_ent0_inv = (exp(c.ent_base_factor) + 1.0) / exp(c.ent_base_factor);
    }

    // Note: b.pAttacking and c.pAttacking overlap, so we can use either:
    if (elt.getAttacking().present()) {
        b.p_attacking.init(elt.getAttacking().get(), max_insecticide,
                           "ITN.description.anophelesParams.twoStageDeterrency.attacking", true);
    } else {
        assert(elt.getAttackingLogit().present());
        if (hole_index_max == 0.0) {
            throw util::xml_scenario_error(
                "ITN.description.anophelesParams: holeIndexMax required when using logit attacking deterrency");
        }
        b.p_attacking.init_logit(elt.getAttackingLogit().get(), hole_index_max, true);
    }
}

double factors::RelativeAttractiveness::relative_attractiveness(double hole_index, double insecticide_content) const {
    if (model == DeterrencyModel::SingleStage) {
        double hole_component = exp(-hole_index * a.hole_scaling);
        double insecticide_component = 1.0 - exp(-insecticide_content * a.insecticide_scaling);
        double rel_avail = exp(a.l_hf * hole_component + a.l_pf * insecticide_component +
                               a.l_if * hole_component * insecticide_component);
        assert(rel_avail >= 0.0);
        return rel_avail;
    }

    double factor = numeric_limits<double>::quiet_NaN();
    if (model == DeterrencyModel::TwoStage) {
        // This is essentially a combination of the relative attractiveness as used
        // by IRS and a killing factor.

        // Note that an alternative, simpler, model could have been used, but was
        // not for consistency with other models. Alternative (here we don't take
        // the logarithm of PF):
        // pEnt = 1 - PFEntering × insecticideComponent

        const double insecticide_component = 1.0 - exp(-insecticide_content * b.insecticide_scaling_entering);
        const double p_ent = exp(b.l_pf_entering * insecticide_component);
        // In this model, effect with 0 insectice, pEnt0 = exp(0) = 1, hence we don't need to
        // divide by a denominator like in the logit model:
        factor = p_ent;
    } else {
        assert(model == DeterrencyModel::TwoStageLogit);
        const double p = log(insecticide_content + 1.0);
        // We directly take the exponential (this is exp(logit.pEnt0):
        const double q = exp(c.ent_base_factor + c.ent_insecticide_factor * p);
        const double p_ent = q / (q + 1.0);
        factor = p_ent * c.p_ent0_inv;
    }
    assert(factor >= 0.0);

    // Note: b.pAttacking and c.pAttacking overlap, so we can use either:
    const double rel_p_att = b.p_attacking.rel_p_att(hole_index, insecticide_content);
    return factor * rel_p_att;
}

// —————  main, public classes  —————

ITNComponent::ITNComponent(ComponentId id, const scnXml::ITNDescription& elt,
                           const map<string, size_t>& species_name_map)
    : Transmission::HumanVectorInterventionComponent(id), rip_factor(elt.getRipFactor().getValue()) {
    initial_insecticide.setParams(elt.getInitialInsecticide());
    const double max_prop = 0.999; //NOTE: this could be exposed in XML, but probably doesn't need to be
    max_insecticide = R::qnorm5(max_prop, initial_insecticide.getMu(), initial_insecticide.getSigma(), true, false);
    hole_rate = util::createSampler(elt.getHoleRate()); // per year
    hole_rate->scaleMean(Sim::years_per_step());        // convert to per step
    rip_rate = util::createSampler(elt.getRipRate());
    rip_rate->scaleMean(Sim::years_per_step());

    insecticide_decay = DecayFunction::makeObject(elt.getInsecticideDecay(), "ITNDescription.insecticideDecay");
    attrition_of_nets = DecayFunction::makeObject(elt.getAttritionOfNets(), "ITNDescription.attritionOfNets");
    // assume usage modifier is 100% if none is specified
    double prop_use = NAN;
    if (elt.getUsage().present()) {
        prop_use = elt.getUsage().get().getValue();
    } else {
        prop_use = 1.0;
    }
    if (!(prop_use >= 0.0 && prop_use <= 1.0)) {
        throw util::xml_scenario_error("ITN.description.proportionUse: must be within range [0,1]");
    }

    using AP = scnXml::ITNDescription::AnophelesParamsSequence;
    const AP& ap = elt.getAnophelesParams();
    species.resize(species_name_map.size());
    util::SpeciesIndexChecker checker("ITN", species_name_map);
    for (const auto& it : ap) {
        species[checker.getIndex(it.getMosquito())].init(it, prop_use, max_insecticide);
    }
    checker.checkNoneMissed();
}

void ITNComponent::deploy(Host::Human& human, mon::Deploy::Method method, VaccineLimits) const {
    human.perHostTransmission.deploy_component(human.intervention_rng, *this);
    mon::recordDeploy(mon::measure("nMassITNs"), mon::measure("nEPI_ITNs"), human, method);
}

component_types::Type ITNComponent::component_type() const { return component_types::ITN; }

void ITNComponent::print_details(std::ostream& out) const { out << id().id << "\tITN"; }

unique_ptr<PerHostInterventionData> ITNComponent::make_human_part(LocalRng& rng) const {
    return std::make_unique<HumanITN>(rng, *this);
}
unique_ptr<PerHostInterventionData> ITNComponent::make_human_part(istream& stream, ComponentId id) const {
    return std::make_unique<HumanITN>(stream, id, *this);
}

void ITNComponent::ITNAnopheles::init(const scnXml::ITNDescription::AnophelesParamsType& elt, double proportion_use,
                                      double max_insecticide) {
    bool hole_index_max_present = false;

    double hole_index_max = 0.0;
    if (elt.getHoleIndexMax().present()) {
        hole_index_max = elt.getHoleIndexMax().get().getValue();
        hole_index_max_present = true;
    }

    if (elt.getDeterrency().present()) {
        rel_attractiveness.init_single_stage(elt.getDeterrency().get(), max_insecticide);
    } else {
        assert(elt.getTwoStageDeterrency().present());
        rel_attractiveness.init_two_stage(elt.getTwoStageDeterrency().get(), max_insecticide, hole_index_max,
                                          hole_index_max_present);
    }
    if (elt.getPreprandialKillingEffect().present()) {
        preprandial_killing_effect.init(elt.getPreprandialKillingEffect().get(), max_insecticide,
                                        "ITN.description.anophelesParams.preprandialKillingFactor", false);
    } else {
        assert(elt.getPreprandialKillingEffectLogit().present());
        if (hole_index_max == 0.0) {
            throw util::xml_scenario_error(
                "ITN.description.anophelesParams: holeIndexMax required when using logit killing effect");
        }
        preprandial_killing_effect.init_logit(elt.getPreprandialKillingEffectLogit().get(), hole_index_max, false);
    }
    if (elt.getPostprandialKillingEffect().present()) {
        postprandial_killing_effect.init(elt.getPostprandialKillingEffect().get(), max_insecticide,
                                         "ITN.description.anophelesParams.postprandialKillingFactor", false);
    } else {
        assert(elt.getPostprandialKillingEffectLogit().present());
        if (hole_index_max == 0.0) {
            throw util::xml_scenario_error(
                "ITN.description.anophelesParams: holeIndexMax required when using logit killing effect");
        }
        postprandial_killing_effect.init_logit(elt.getPostprandialKillingEffectLogit().get(), hole_index_max, false);
    }
    if (elt.getFecundityReduction().present()) {
        rel_fecundity_effect.init(elt.getFecundityReduction().get(), max_insecticide,
                                  "ITN.description.anophelesParams.fecundityReduction", false);
    } else if (elt.getFecundityReductionLogit().present()) {
        if (hole_index_max == 0.0) {
            throw util::xml_scenario_error(
                "ITN.description.anophelesParams: holeIndexMax required when using logit fecundity effect");
        }
        rel_fecundity_effect.init_logit(elt.getFecundityReductionLogit().get(), hole_index_max, false);
    } else {
        rel_fecundity_effect.init1();
    }
    // Nets only affect people while they're using the net. NOTE: we may want
    // to revise this at some point (heterogeneity, seasonal usage patterns).
    const double prop_active = elt.getPropActive();
    if (prop_active != 1.0 && prop_active != 0.0)
        throw util::xml_scenario_error("GVI.description.propActive: must be either 0 or 1");
    assert(proportion_use >= 0.0 && proportion_use <= 1.0);
    assert(prop_active >= 0.0 && prop_active <= 1.0);
    proportion_protected = proportion_use * prop_active;
    proportion_unprotected = 1.0 - proportion_protected;
}

// Net rips and insecticide loss are assumed to co-vary dependent on
// handling of net. They are sampled once per human: human handling is
// presumed to be the largest cause of variance.
HumanITN::HumanITN(LocalRng& rng, const ITNComponent& params)
    : HumanITN(rng, params, util::NormalSample::generate(rng)) {}

HumanITN::HumanITN(LocalRng& rng, const ITNComponent& params, util::NormalSample handling)
    : PerHostInterventionData(params.id()), disposal_time(Sim::now() + params.attrition_of_nets->sampleAgeOfDecay(rng)),

      // this is sampled independently: initial insecticide content doesn't depend on handling
      initial_insecticide(params.initial_insecticide.sample(rng)), hole_rate(params.hole_rate->sample(handling)),
      rip_rate(params.rip_rate->sample(handling)), insecticide_decay_het(params.insecticide_decay->hetSample(handling)),
      component(&params) {
    if (initial_insecticide < 0.0)
        initial_insecticide = 0.0; // avoid negative samples
    if (initial_insecticide > params.max_insecticide)
        initial_insecticide = params.max_insecticide;
}

void HumanITN::redeploy(LocalRng& rng, const OM::Transmission::HumanVectorInterventionComponent& params0) {
    const ITNComponent& params = *dynamic_cast<const ITNComponent*>(&params0);

    deploy_time = Sim::now_or_ts1();
    disposal_time = Sim::now_or_ts1() + params.attrition_of_nets->sampleAgeOfDecay(rng);
    n_holes = 0;
    hole_index = 0.0;
    // this is sampled independently: initial insecticide content doesn't depend on handling
    initial_insecticide = params.initial_insecticide.sample(rng);
    if (initial_insecticide < 0.0)
        initial_insecticide = 0.0; // avoid negative samples
    if (initial_insecticide > params.max_insecticide)
        initial_insecticide = params.max_insecticide;
}

void HumanITN::update(Host::Human& human) {
    const ITNComponent& params = *component;
    if (deploy_time != Sim::never()) {
        // First use is at age 0 relative to ts0()
        if (Sim::ts0() >= disposal_time) {
            deploy_time = Sim::never();
            human.removeFromCohort(id());
            return;
        }

        int new_holes = 0;
        if (hole_rate > 0)
            new_holes = human.intervention_rng.poisson(hole_rate);

        n_holes += new_holes;
        hole_index += new_holes;

        const double lambda = n_holes * rip_rate;
        if (lambda > 0)
            hole_index += params.rip_factor * human.intervention_rng.poisson(lambda);
    }
}

double HumanITN::relative_attractiveness(size_t species_index) const {
    if (deploy_time == Sim::never())
        return 1.0;
    const ITNComponent& params = *component;
    const ITNComponent::ITNAnopheles& anoph = params.species[species_index];
    return anoph.relative_attractiveness(hole_index, get_insecticide_content(params));
}

double HumanITN::preprandial_survival_factor(size_t species_index) const {
    if (deploy_time == Sim::never())
        return 1.0;
    const ITNComponent& params = *component;
    const ITNComponent::ITNAnopheles& anoph = params.species[species_index];
    return anoph.preprandial_survival_factor(hole_index, get_insecticide_content(params));
}

double HumanITN::postprandial_survival_factor(size_t species_index) const {
    if (deploy_time == Sim::never())
        return 1.0;
    const ITNComponent& params = *component;
    const ITNComponent::ITNAnopheles& anoph = params.species[species_index];
    return anoph.postprandial_survival_factor(hole_index, get_insecticide_content(params));
}
double HumanITN::rel_fecundity(size_t species_index) const {
    if (deploy_time == Sim::never())
        return 1.0;
    const ITNComponent& params = *component;
    const ITNComponent::ITNAnopheles& anoph = params.species[species_index];
    return anoph.rel_fecundity(hole_index, get_insecticide_content(params));
}

void HumanITN::checkpoint(ostream& stream) {
    deploy_time & stream;
    disposal_time & stream;
    n_holes & stream;
    hole_index & stream;
    initial_insecticide & stream;
    hole_rate & stream;
    rip_rate & stream;
    component->insecticide_decay->checkpointSample(insecticide_decay_het, stream);
}
HumanITN::HumanITN(istream& stream, ComponentId id, const ITNComponent& params)
    : PerHostInterventionData(id), component(&params) {
    deploy_time & stream;
    disposal_time & stream;
    n_holes & stream;
    hole_index & stream;
    initial_insecticide & stream;
    hole_rate & stream;
    rip_rate & stream;
    component->insecticide_decay->checkpointSample(insecticide_decay_het, stream);
}

}
