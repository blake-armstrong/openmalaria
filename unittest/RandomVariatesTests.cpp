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

#include "util/random.h"
#include "util/random_variates.h"
#include "util/xoshiro.h"

#include <catch2/catch_test_macros.hpp>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

using OM::util::variates::beta;
using OM::util::variates::gamma;
using OM::util::variates::poisson;
using OM::util::variates::standard_normal;
using OM::util::variates::weibull;

constexpr std::size_t SampleSize = 200000;
constexpr double KsAlpha = 1e-4;
constexpr double MomentZLimit = 5.0;
constexpr double ChiSquareAlpha = 1e-4;
constexpr double MinExpectedCount = 5.0;
constexpr uint64_t Seed = 20261021;

using Gen = Xoshiro256P;
using NewSampler = std::function<double(Gen&)>;
using GslSampler = std::function<double(const gsl_rng*)>;
using Cdf = std::function<double(double)>;
using Pmf = std::function<double(double)>;

struct Case {
    std::string name;
    NewSampler sample_new;
    GslSampler sample_gsl;
    Cdf cdf;
    double mean;
    double variance;
    Pmf pmf;
    bool check_moments = true;
};

class GslAdapter {
public:
    explicit GslAdapter(uint64_t stream) : seeder(Seed, stream), gen(seeder) {}
    GslAdapter(const GslAdapter&) = delete;
    GslAdapter& operator=(const GslAdapter&) = delete;
    GslAdapter(GslAdapter&&) = delete;
    GslAdapter& operator=(GslAdapter&&) = delete;
    ~GslAdapter() = default;

    const gsl_rng* get() { return &rng; }

private:
    static unsigned long sample_u32(void* state) { return static_cast<Gen*>(state)->gen_u32(); }
    static double sample_double(void* state) { return static_cast<Gen*>(state)->gen_double(); }

    ChaCha<8> seeder;
    Gen gen;
    gsl_rng_type type{.name = "OM_TEST_RNG",
                      .max = std::numeric_limits<uint32_t>::max(),
                      .min = std::numeric_limits<uint32_t>::min(),
                      .size = 0,
                      .set = nullptr,
                      .get = &sample_u32,
                      .get_double = &sample_double};
    gsl_rng rng{.type = &type, .state = &gen};
};

std::vector<double> draw_new(const Case& c, uint64_t stream) {
    ChaCha<8> seeder(Seed, stream);
    Gen gen(seeder);
    std::vector<double> samples(SampleSize);
    std::ranges::generate(samples, [&] { return c.sample_new(gen); });
    std::ranges::sort(samples);
    return samples;
}

std::vector<double> draw_gsl(const Case& c, uint64_t stream) {
    GslAdapter adapter(stream);
    std::vector<double> samples(SampleSize);
    std::ranges::generate(samples, [&] { return c.sample_gsl(adapter.get()); });
    std::ranges::sort(samples);
    return samples;
}

double ks_coefficient() { return std::sqrt(-0.5 * std::log(KsAlpha / 2.0)); }

double one_sample_ks(const std::vector<double>& sorted, const Cdf& cdf) {
    const auto n = static_cast<double>(sorted.size());
    double d = 0.0;
    std::size_t i = 0;
    while (i < sorted.size()) {
        const double x = sorted[i];
        const double below = static_cast<double>(i) / n;
        while (i < sorted.size() && sorted[i] == x) {
            ++i;
        }
        const double at = static_cast<double>(i) / n;
        const double f_at = cdf(x);
        const double f_below = cdf(std::nextafter(x, -std::numeric_limits<double>::infinity()));
        d = std::max({d, std::abs(at - f_at), std::abs(f_below - below)});
    }
    return d;
}

double discrete_one_sample_ks(const std::vector<double>& sorted, const Cdf& cdf) {
    const auto n = static_cast<double>(sorted.size());
    double d = 0.0;
    std::size_t i = 0;
    for (unsigned int count = 0; count <= static_cast<unsigned int>(sorted.back()); ++count) {
        const auto k = static_cast<double>(count);
        while (i < sorted.size() && sorted[i] <= k) {
            ++i;
        }
        d = std::max(d, std::abs(static_cast<double>(i) / n - cdf(k)));
    }
    return d;
}

double two_sample_ks(const std::vector<double>& a, const std::vector<double>& b) {
    const auto na = static_cast<double>(a.size());
    const auto nb = static_cast<double>(b.size());
    double d = 0.0;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size()) {
        const double x = std::min(a[i], b[j]);
        while (i < a.size() && a[i] == x) {
            ++i;
        }
        while (j < b.size() && b[j] == x) {
            ++j;
        }
        d = std::max(d, std::abs(static_cast<double>(i) / na - static_cast<double>(j) / nb));
    }
    return d;
}

struct Moments {
    double mean;
    double variance;
    double mean_se;
    double variance_se;
};

Moments sample_moments(const std::vector<double>& samples) {
    const auto n = static_cast<double>(samples.size());
    double sum = 0.0;
    for (const double x : samples) {
        sum += x;
    }
    const double mean = sum / n;
    double m2 = 0.0;
    double m4 = 0.0;
    for (const double x : samples) {
        const double d2 = (x - mean) * (x - mean);
        m2 += d2;
        m4 += d2 * d2;
    }
    m2 /= n;
    m4 /= n;
    return {.mean = mean,
            .variance = m2 * n / (n - 1.0),
            .mean_se = std::sqrt(m2 / n),
            .variance_se = std::sqrt(std::max(m4 - m2 * m2, 0.0) / n)};
}

double chi_square_p(const std::vector<double>& sorted, const Pmf& pmf) {
    const auto n = static_cast<double>(sorted.size());
    std::vector<double> observed;
    std::vector<double> expected;
    double bin_observed = 0.0;
    double bin_expected = 0.0;
    double cumulative = 0.0;
    std::size_t i = 0;
    for (unsigned int count = 0;; ++count) {
        const auto k = static_cast<double>(count);
        const std::size_t start = i;
        while (i < sorted.size() && sorted[i] <= k) {
            ++i;
        }
        const double p = pmf(k);
        cumulative += p;
        bin_observed += static_cast<double>(i - start);
        bin_expected += n * p;
        const double tail_expected = n * std::max(1.0 - cumulative, 0.0);
        if (tail_expected < MinExpectedCount) {
            bin_observed += static_cast<double>(sorted.size() - i);
            bin_expected += tail_expected;
            if (bin_expected < MinExpectedCount && !observed.empty()) {
                observed.back() += bin_observed;
                expected.back() += bin_expected;
            } else {
                observed.push_back(bin_observed);
                expected.push_back(bin_expected);
            }
            break;
        }
        if (bin_expected >= MinExpectedCount) {
            observed.push_back(bin_observed);
            expected.push_back(bin_expected);
            bin_observed = 0.0;
            bin_expected = 0.0;
        }
    }
    double statistic = 0.0;
    for (std::size_t b = 0; b < observed.size(); ++b) {
        const double diff = observed[b] - expected[b];
        statistic += diff * diff / expected[b];
    }
    return gsl_cdf_chisq_Q(statistic, static_cast<double>(observed.size() - 1));
}

void check_case(const Case& c, uint64_t stream) {
    CAPTURE(c.name);
    const std::vector<double> fresh = draw_new(c, stream);
    const std::vector<double> old = draw_gsl(c, stream + 1);
    const double n = SampleSize;

    const bool discrete = static_cast<bool>(c.pmf);
    const double d_cdf = discrete ? discrete_one_sample_ks(fresh, c.cdf) : one_sample_ks(fresh, c.cdf);
    const double d_cdf_limit = ks_coefficient() / std::sqrt(n);
    CAPTURE(d_cdf, d_cdf_limit);
    CHECK(d_cdf < d_cdf_limit);

    if (discrete) {
        const double p_chi_square = chi_square_p(fresh, c.pmf);
        CAPTURE(p_chi_square);
        CHECK(p_chi_square > ChiSquareAlpha);
    }

    const double d_gsl = two_sample_ks(fresh, old);
    const double d_gsl_limit = ks_coefficient() * std::sqrt(2.0 / n);
    CAPTURE(d_gsl, d_gsl_limit);
    CHECK(d_gsl < d_gsl_limit);

    if (c.check_moments) {
        const Moments m = sample_moments(fresh);
        const double mean_z = (m.mean - c.mean) / m.mean_se;
        const double variance_z = (m.variance - c.variance) / m.variance_se;
        CAPTURE(m.mean, c.mean, mean_z, m.variance, c.variance, variance_z);
        CHECK(std::abs(mean_z) < MomentZLimit);
        CHECK(std::abs(variance_z) < MomentZLimit);
    }
}

void check_cases(const std::vector<Case>& cases, uint64_t first_stream) {
    uint64_t stream = first_stream;
    for (const Case& c : cases) {
        check_case(c, stream);
        stream += 2;
    }
}

Case normal_case(double mean, double sd) {
    return {.name = "normal(" + std::to_string(mean) + ", " + std::to_string(sd) + ")",
            .sample_new = [=](Gen& g) { return mean + sd * standard_normal(g); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_gaussian(r, sd) + mean; },
            .cdf = [=](double x) { return gsl_cdf_gaussian_P(x - mean, sd); },
            .mean = mean,
            .variance = sd * sd};
}

Case gamma_case(double shape, double scale) {
    return {.name = "gamma(" + std::to_string(shape) + ", " + std::to_string(scale) + ")",
            .sample_new = [=](Gen& g) { return gamma(g, shape, scale); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_gamma(r, shape, scale); },
            .cdf = [=](double x) { return gsl_cdf_gamma_P(x, shape, scale); },
            .mean = shape * scale,
            .variance = shape * scale * scale};
}

Case beta_case(double a, double b) {
    const double sum = a + b;
    return {.name = "beta(" + std::to_string(a) + ", " + std::to_string(b) + ")",
            .sample_new = [=](Gen& g) { return beta(g, a, b); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_beta(r, a, b); },
            .cdf = [=](double x) { return gsl_cdf_beta_P(x, a, b); },
            .mean = a / sum,
            .variance = a * b / (sum * sum * (sum + 1.0))};
}

Case log_normal_case(double meanlog, double sdlog, bool check_moments) {
    const double s2 = sdlog * sdlog;
    return {.name = "log_normal(" + std::to_string(meanlog) + ", " + std::to_string(sdlog) + ")",
            .sample_new = [=](Gen& g) { return std::exp(meanlog + sdlog * standard_normal(g)); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_lognormal(r, meanlog, sdlog); },
            .cdf = [=](double x) { return gsl_cdf_lognormal_P(x, meanlog, sdlog); },
            .mean = std::exp(meanlog + s2 / 2.0),
            .variance = (std::exp(s2) - 1.0) * std::exp(2.0 * meanlog + s2),
            .check_moments = check_moments};
}

Case poisson_case(double mean) {
    return {.name = "poisson(" + std::to_string(mean) + ")",
            .sample_new = [=](Gen& g) { return poisson(g, mean); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_poisson(r, mean); },
            .cdf = [=](double k) { return gsl_cdf_poisson_P(static_cast<unsigned int>(k), mean); },
            .mean = mean,
            .variance = mean,
            .pmf = [=](double k) { return gsl_ran_poisson_pdf(static_cast<unsigned int>(k), mean); }};
}

Case weibull_case(double scale, double shape) {
    const double g1 = std::tgamma(1.0 + 1.0 / shape);
    const double g2 = std::tgamma(1.0 + 2.0 / shape);
    return {.name = "weibull(" + std::to_string(scale) + ", " + std::to_string(shape) + ")",
            .sample_new = [=](Gen& g) { return weibull(g, scale, shape); },
            .sample_gsl = [=](const gsl_rng* r) { return gsl_ran_weibull(r, scale, shape); },
            .cdf = [=](double x) { return gsl_cdf_weibull_P(x, scale, shape); },
            .mean = scale * g1,
            .variance = scale * scale * (g2 - g1 * g1)};
}

TEST_CASE("normal variates match GSL and the normal CDF", "[util][random][g-diff]") {
    check_cases({normal_case(0.0, 1.0), normal_case(5.0, 0.1), normal_case(-3.0, 20.0)}, 100);
}

TEST_CASE("gamma variates match GSL and the gamma CDF", "[util][random][g-diff]") {
    check_cases({gamma_case(0.05, 1.0), gamma_case(0.3, 2.5), gamma_case(0.9, 1.0), gamma_case(1.0, 1.0),
                 gamma_case(1.5, 0.2), gamma_case(4.0, 1.0), gamma_case(30.0, 2.5), gamma_case(500.0, 0.01)},
                200);
}

TEST_CASE("beta variates match GSL and the beta CDF", "[util][random][g-diff]") {
    check_cases({beta_case(0.1, 0.1), beta_case(0.5, 0.9), beta_case(1.0, 1.0), beta_case(0.3, 5.0),
                 beta_case(2.0, 3.0), beta_case(20.0, 0.8), beta_case(50.0, 60.0)},
                300);
}

TEST_CASE("log-normal variates match GSL and the log-normal CDF", "[util][random][g-diff]") {
    check_cases({log_normal_case(0.0, 0.25, true), log_normal_case(2.0, 1.0, true), log_normal_case(-1.0, 1.5, true),
                 log_normal_case(1.0, 3.0, false)},
                400);
}

TEST_CASE("Poisson variates match GSL and the Poisson CDF", "[util][random][g-diff]") {
    check_cases({poisson_case(0.01), poisson_case(0.5), poisson_case(3.0), poisson_case(9.99), poisson_case(10.0),
                 poisson_case(25.0), poisson_case(300.0), poisson_case(1e4)},
                500);
}

TEST_CASE("Poisson of a non-positive mean is zero", "[util][random]") {
    ChaCha<8> seeder(Seed, 0);
    Gen gen(seeder);
    CHECK(poisson(gen, 0.0) == 0);
    CHECK(poisson(gen, -1.0) == 0);
}

TEST_CASE("Weibull variates match GSL and the Weibull CDF", "[util][random][g-diff]") {
    check_cases({weibull_case(1.0, 0.5), weibull_case(2.5, 1.0), weibull_case(0.3, 3.0)}, 600);
}

} // namespace
