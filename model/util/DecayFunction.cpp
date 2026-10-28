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

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "Clock.h"
#include "Global.h"
#include "util/sampler.h"
#include "util/UnitParse.h"
#include "util/DecayFunction.h"
#include <numbers>

namespace OM::util {

inline double read_l_to_days(const scnXml::DecayFunction& elt, const Clock& clock,
                             const CommandLineOptions& command_line) {
    if (!elt.getL().present()) {
        throw xml_scenario_error("decay function: attribute L required");
    }
    return unit_parse::duration_to_days(elt.getL().get(), unit_parse::YEARS, clock, command_line);
}

class ConstantDecayFunction : public DecayFunction {
public:
    ConstantDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()) {}

    double compute(double effective_age) const override {
        // Note: we now require all decay functions to return 0 when time > 0
        // and the DecayFunction is default-constructed. So const *after deployment*.
        if (effective_age * het_factor == numeric_limits<double>::infinity())
            return 0.0;
        return 1.0;
    }
    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return Sim::
            future(); // decay occurs "in the future" (don't use sim::never() because that is interpreted as being in the past)
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<ConstantDecayFunction> copy = std::make_unique<ConstantDecayFunction>(*this);
        return std::move(copy);
    }
};

class StepDecayFunction : public DecayFunction {
public:
    StepDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          inv_l(1.0 / read_l_to_days(elt, clock, command_line)) {}

    double compute(double effective_age) const override {
        if (effective_age * inv_l * het_factor < 1.0) {
            return 1.0;
        } else {
            return 0.0;
        }
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override { return round_to_ts_from_days(1.0 / inv_l); }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<StepDecayFunction> copy = make_unique<StepDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double inv_l;
};

class LinearDecayFunction : public DecayFunction {
public:
    LinearDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          inv_l(1.0 / read_l_to_days(elt, clock, command_line)) {}

    double compute(double effective_age) const override {
        if (effective_age * inv_l * het_factor < 1.0)
            return 1.0 - effective_age * inv_l * het_factor;
        else
            return 0.0;
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        // Note: rounds to nearest. Object may decay instantly or at time L.
        return round_to_ts_from_days(rng.uniform_01() / inv_l);
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<LinearDecayFunction> copy = make_unique<LinearDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double inv_l;
};

class ExponentialDecayFunction : public DecayFunction {
public:
    ExponentialDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock,
                             const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          inv_l(std::numbers::ln2 / read_l_to_days(elt, clock, command_line)) {}

    double compute(double effective_age) const override { return exp(-effective_age * inv_l * het_factor); }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(-log(rng.uniform_01()) / inv_l);
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<ExponentialDecayFunction> copy = make_unique<ExponentialDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double inv_l;
};

class WeibullDecayFunction : public DecayFunction {
public:
    WeibullDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          const_over_lambda(pow(std::numbers::ln2, 1.0 / elt.getK()) / read_l_to_days(elt, clock, command_line)),
          k(elt.getK()) {}

    double compute(double effective_age) const override {
        const double p = -pow(effective_age * const_over_lambda * het_factor, k);
        if (p < -700.0)
            return 0.0;
        else
            return exp(p);
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(pow(-log(rng.uniform_01()), 1.0 / k) / const_over_lambda);
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<WeibullDecayFunction> copy = make_unique<WeibullDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double const_over_lambda, k;
};

class HillDecayFunction : public DecayFunction {
public:
    HillDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          inv_l(1.0 / read_l_to_days(elt, clock, command_line)), k(elt.getK()) {}

    double compute(double effective_age) const override {
        return 1.0 / (1.0 + pow(effective_age * inv_l * het_factor, k));
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(pow(1.0 / rng.uniform_01() - 1.0, 1.0 / k) / inv_l);
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<HillDecayFunction> copy = make_unique<HillDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double inv_l, k;
};

class SmoothCompactDecayFunction : public DecayFunction {
public:
    SmoothCompactDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock,
                               const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()),
          inv_l(1.0 / read_l_to_days(elt, clock, command_line)), k(elt.getK()) {}

    double compute(double effective_age) const override {
        if (effective_age * inv_l * het_factor < 1.0)
            return exp(k - k / (1.0 - pow(effective_age * inv_l * het_factor, 2.0)));
        else
            return 0.0;
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(sqrt(1.0 - k / (k - log(rng.uniform_01()))) / inv_l);
    }

    unique_ptr<DecayFunction> make_het_sample(double) const override {
        unique_ptr<SmoothCompactDecayFunction> copy = make_unique<SmoothCompactDecayFunction>(*this);
        return std::move(copy);
    }

private:
    double inv_l, k;
};

template <class T> class OperatorDecayFunction : public DecayFunction {
public:
    OperatorDecayFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()) {
        const scnXml::DecayFunction::DecaySequence& decay_sequence = elt.getDecay();
        if (decay_sequence.size() != 2)
            throw xml_scenario_error("Operator decay function expects two decay functions, " +
                                     to_string(decay_sequence.size()) + "  were given.");

        f1 = DecayFunction::make_object(decay_sequence[0], "Operator::f1", clock, command_line);
        f2 = DecayFunction::make_object(decay_sequence[1], "Operator::f2", clock, command_line);
    }

    OperatorDecayFunction(const OperatorDecayFunction& copy, unique_ptr<DecayFunction> f1, unique_ptr<DecayFunction> f2)
        : DecayFunction(copy), f1(std::move(f1)), f2(std::move(f2)) {}

    double compute(double effective_age) const override {
        return max(min(op(f1->eval(effective_age), f2->eval(effective_age)), 1.0), 0.0);
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(max(f1->sample_age_of_decay(rng), f2->sample_age_of_decay(rng)));
    }

    unique_ptr<DecayFunction> make_het_sample(double het_factor) const override {
        unique_ptr<DecayFunction> f1_het_sample = f1->het_sample(het_factor);
        unique_ptr<DecayFunction> f2_het_sample = f2->het_sample(het_factor);
        unique_ptr<OperatorDecayFunction> copy =
            make_unique<OperatorDecayFunction>(*this, std::move(f1_het_sample), std::move(f2_het_sample));
        return std::move(copy);
    }

private:
    unique_ptr<DecayFunction> f1, f2;
    T op;
};

class EmaxFunction : public DecayFunction {
public:
    EmaxFunction(const scnXml::DecayFunction& elt, const Clock& clock, const CommandLineOptions& command_line)
        : DecayFunction(clock, elt.getIncreasing(), elt.getInitialEfficacy(), elt.getCV()) {
        const scnXml::DecayFunction::DecaySequence& decay_sequence = elt.getDecay();
        if (decay_sequence.size() != 1)
            throw util::xml_scenario_error("The Emax function expects exactly one user function, but " +
                                           to_string(decay_sequence.size()) + "  were given.");
        if (!elt.getEmax().present())
            throw util::xml_scenario_error("Emax function: the Emax parameter is not declared");
        if (!elt.getIC50().present())
            throw util::xml_scenario_error("Emax function: the IC50 parameter is not declared");
        if (!elt.getSlope().present())
            throw util::xml_scenario_error("Emax function: the Slope parameter is not declared");
        if (!elt.getInitialConcentration().present())
            throw util::xml_scenario_error("Emax function: the InitialConcentration parameter is not declared");

        f = make_object(decay_sequence[0], "Emax::f", clock, command_line);

        emax = elt.getEmax().get();
        const double raw_ic50 = elt.getIC50().get();
        slope = elt.getSlope().get();
        initial_concentration = elt.getInitialConcentration().get();

        if (emax < 0.0 || emax > 1.0) {
            throw util::xml_scenario_error("Emax function: Emax must be in [0, 1], got " + std::to_string(emax));
        }

        if (raw_ic50 <= 0.0) {
            throw util::xml_scenario_error("Emax function: IC50 must be > 0, got " + std::to_string(raw_ic50));
        }

        if (slope <= 0.0) {
            throw util::xml_scenario_error("Emax function: Slope must be > 0, got " + std::to_string(slope));
        }

        if (initial_concentration < 0.0) {
            throw util::xml_scenario_error("Emax function: InitialConcentration must be >= 0, got " +
                                           std::to_string(initial_concentration));
        }

        // Store IC50^slope once since compute() evaluates this repeatedly.
        ic50 = std::pow(raw_ic50, slope);
    }

    EmaxFunction(const EmaxFunction& copy, std::unique_ptr<DecayFunction> f)
        : DecayFunction(copy), f(std::move(f)), emax(copy.emax), ic50(copy.ic50), slope(copy.slope),
          initial_concentration(copy.initial_concentration) {}

    double compute(double effective_age) const override {
        // Emax model: E(C)=Emax*C^slope/(C^slope + IC50^slope), C=C0*f(t).
        const double concentration_pow = std::pow(initial_concentration * f->eval(effective_age), slope);
        const double efficacy = emax * concentration_pow / (concentration_pow + ic50);
        return std::max(std::min(efficacy, 1.0), 0.0);
    }

    SimTime sample_age_of_decay(LocalRng& rng) const override {
        return round_to_ts_from_days(f->sample_age_of_decay(rng));
    }

    std::unique_ptr<DecayFunction> make_het_sample(double het_factor) const override {
        std::unique_ptr<DecayFunction> f_het_sample = f->het_sample(het_factor);
        std::unique_ptr<EmaxFunction> copy = std::make_unique<EmaxFunction>(*this, std::move(f_het_sample));
        return std::move(copy);
    }

private:
    std::unique_ptr<DecayFunction> f;
    double emax, ic50, slope, initial_concentration;
};

// -----  interface / static functions  -----
void DecayFunction::checkpointSample(const unique_ptr<DecayFunction>& sample, ostream& stream) const {
    const bool present = static_cast<bool>(sample);
    present & stream;
    if (present)
        sample->het_factor & stream;
}

void DecayFunction::checkpointSample(unique_ptr<DecayFunction>& sample, istream& stream) const {
    bool present = false;
    present & stream;
    if (present) {
        double factor = NAN;
        factor & stream;
        sample = het_sample(factor);
    } else {
        sample.reset();
    }
}

SimTime DecayFunction::round_to_ts_from_days(double days) const { return clock->round_to_ts_from_days(days); }

unique_ptr<DecayFunction> DecayFunction::make_object(const scnXml::DecayFunction& elt, const char* elt_name,
                                                     const Clock& clock, const CommandLineOptions& command_line) {
    // Type mostly equivalent to a std::string:
    const scnXml::Function& func = elt.getFunction();
    if (func == "constant")
        return make_unique<ConstantDecayFunction>(elt, clock);
    else if (func == "step")
        return make_unique<StepDecayFunction>(elt, clock, command_line);
    else if (func == "linear")
        return make_unique<LinearDecayFunction>(elt, clock, command_line);
    else if (func == "exponential")
        return make_unique<ExponentialDecayFunction>(elt, clock, command_line);
    else if (func == "weibull")
        return make_unique<WeibullDecayFunction>(elt, clock, command_line);
    else if (func == "hill")
        return make_unique<HillDecayFunction>(elt, clock, command_line);
    else if (func == "smooth-compact")
        return make_unique<SmoothCompactDecayFunction>(elt, clock, command_line);
    else if (func == "plus")
        return make_unique<OperatorDecayFunction<std::plus<>>>(elt, clock, command_line);
    else if (func == "minus")
        return make_unique<OperatorDecayFunction<std::minus<>>>(elt, clock, command_line);
    else if (func == "divides")
        return make_unique<OperatorDecayFunction<std::divides<>>>(elt, clock, command_line);
    else if (func == "multiplies")
        return make_unique<OperatorDecayFunction<std::multiplies<>>>(elt, clock, command_line);
    else if (func == "emax")
        return make_unique<EmaxFunction>(elt, clock, command_line);
    else
        throw xml_scenario_error("decay function " + string(elt_name) + " of type " + string(func) + " unrecognized");
}

}
