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

#ifndef OM_UTIL_RANDOM
#define OM_UTIL_RANDOM

/* This module contains the random-number generator and distributions wrapper.
 *
 * We use modern RNGs:
 * 
 * -    ChaCha<8> for our master RNG; this is a low-margin cryptographic-grade
 *      generator yet still very fast
 * -    Xoshiro256+; this is very fast, decent quality and has small state
 *      http://prng.di.unimi.it/
 */

#include "Global.h"
#include "util/errors.h"
#include <set>
#include <chacha.h>
#include "util/random_variates.h"
#include "util/xoshiro.h"

#include <gsl/gsl_cdf.h>

#include <cmath>

namespace OM {
namespace util {

/// Our random number generator.
template <class T> struct RNG {
    ///@brief Construction and checkpointing
    //@{
    /// Seeding constructor: use given seed and stream
    ///
    /// Note: we don't expose a seed-only constructor to prevent accidental
    /// seeding with only a 64-bit seed. Since many instances of the LocalRng
    /// are used, 128-bit seeds are recommended to reduce chance of overlapping
    /// sections of RNG output.
    explicit RNG(uint64_t seed, uint64_t stream) : rng(seed, stream) {}

    /// Seed via another RNG
    template <class S> explicit RNG(RNG<S>& source) : rng(source.rng) {}

    // Disable copying
    RNG(const RNG&) = delete;
    RNG& operator=(const RNG&) = delete;

    RNG(RNG&&) noexcept = default;
    RNG& operator=(RNG&&) noexcept = default;
    ~RNG() = default;

    /// Seed with given 128-bit input (see notes on constructor)
    void seed(uint64_t seed, uint64_t stream) { rng.seed(seed, stream); }

    /// Checkpointing
    ///
    /// Note: this relies on the RNG having been constructed with the same seed.
    template <class S> void checkpoint(S& stream) { rng.binary_checkpoint(stream); }
    //@}

    uint64_t gen_seed() {
        uint64_t low = rng;
        uint64_t high = rng;
        return (high << 32) | low;
    }

    ///@brief Random number distributions
    //@{
    /** Generate a random number in the range [0,1). */
    inline double uniform_01() { return rng.gen_double(); }

    /** This function returns a Gaussian random variate, with mean mean and
     * standard deviation std. The sampled value x ~ N(mean, std^2) . */
    double gauss(double mean, double std) { return mean + std * variates::standard_normal(rng); }

    /** This function returns a random variate from the gamma distribution. */
    double gamma(double a, double b) { return variates::gamma(rng, a, b); }

    /** This function returns a random variate from the lognormal distribution.
     * 
     * Mean is log(mean) - (sigma^2)/2.
     * Variance is (exp(sigma^2) - 1)*exp(2mu+sigma^2).
     * 
     * @param mu mean-log
     * @param sigma sigma-log
     */
    double log_normal(double meanlog, double stdlog) {
        return std::exp(meanlog + stdlog * variates::standard_normal(rng));
    }

    /** Return the maximum over multiple log-normal samples.
     *
     * @param start Initial value for running maximum (returns max of this and
     *      all samples)
     * @param n Number of samples
     * @param meanlog mean of underlying Gaussian
     * @param stdlog standard deviation of underlying Gaussian
     */
    double max_multi_log_normal(double start, int n, double meanlog, double stdlog) {
        // Used for performance reasons. Calling GSL's log_normal 5 times is 50% slower.
        // For random variables X1, .., Xn with identical distribution X and
        // Mn = max(X1, .., Xn), the CDF:
        //      F_Mn(x) = P(Mn ≤ x)
        //              = P(X1 ≤ x) P(X2 ≤ x) .. P(Xn ≤ x)
        //              = F_X(x) ^ n
        // Thus for u = F_Mn(x) = F_X(x) ^ n, u^(1/n) = F_X(x).
        const double normp = pow(uniform_01(), 1.0 / n);
        const double zval = gsl_cdf_ugaussian_Pinv(normp);
        const double multi_sample = exp(meanlog + stdlog * zval);
        return std::max(start, multi_sample);
    }

    /** This function returns a random variate from the beta distribution. */
    double beta(double a, double b) { return variates::beta(rng, a, b); }

    /** This function wraps beta(), setting b=b and a such that m is the mean
     * of the distribution. */
    inline double beta_with_mean(double m, double b) {
        //TODO(performance): could do this calculation externally, and feed in a,b instead of mean,b
        const double a = m * b / (1.0 - m);
        return beta(a, b);
    }

    /** This function returns a random integer from the Poisson distribution with mean lambda. */
    int poisson(double lambda) {
        if (!(std::isfinite)(lambda)) {
            //This would lead to an inifinite loop
            throw TRACED_EXCEPTION("lambda is inf", Error::InfLambda);
        }
        return variates::poisson(rng, lambda);
    }

    /** This function returns true with probability prob or 0 with probability
     * 1-prob (Bernoulli distribution). */
    bool bernoulli(double prob) {
        assert((std::isfinite)(prob));
        // return true iff our variate is less than the probability
        return uniform_01() < prob;
    }

    /** This function returns an integer from 0 to 1-n, where every value has
     * equal probability of being sampled. */
    inline int uniform(int n) {
        assert((std::isfinite)(n));
        return static_cast<int>(uniform_01() * n);
    }

    /**
     * Return a variate sampled from the Weibull distribution.
     * 
     * The PDF is k · x^{k-1} exp{-(x/λ)^k} / λ^k
     * 
     * @param lambda (λ) is the scale parameter
     * @param k is the shape parameter
     */
    double weibull(double lambda, double k) { return variates::weibull(rng, lambda, k); }
    //@}

private:
    T rng;

    template <class> friend class RNG;
};

using LocalRng = RNG<Xoshiro256P>;
using MasterRng = RNG<ChaCha<8>>;

struct MasterRngs {
    explicit MasterRngs(uint64_t seed) : population(seed, 0), transmission(seed, 1) {}

    template <class S> void checkpoint(S& stream) {
        population.checkpoint(stream);
        transmission.checkpoint(stream);
    }

    MasterRng population;
    MasterRng transmission;
};

}

namespace bridge {

inline util::MasterRngs* current_master_rngs = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline util::MasterRngs& master_rngs() {
    assert(current_master_rngs != nullptr);
    return *current_master_rngs;
}

}

class MasterRngsBinding {
public:
    explicit MasterRngsBinding(util::MasterRngs& rngs) : previous(bridge::current_master_rngs) {
        bridge::current_master_rngs = &rngs;
    }
    MasterRngsBinding(const MasterRngsBinding&) = delete;
    MasterRngsBinding& operator=(const MasterRngsBinding&) = delete;
    MasterRngsBinding(MasterRngsBinding&&) = delete;
    MasterRngsBinding& operator=(MasterRngsBinding&&) = delete;
    ~MasterRngsBinding() { bridge::current_master_rngs = previous; }

private:
    util::MasterRngs* previous;
};

}
#endif
