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

#ifndef OM_UTIL_RANDOM_VARIATES
#define OM_UTIL_RANDOM_VARIATES

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace OM::util::variates { // NOLINT(readability-identifier-naming)

template <class Gen> double uniform_open_closed(Gen& gen) { return 1.0 - gen.gen_double(); }

template <class Gen> double standard_normal(Gen& gen) {
    while (true) {
        const double x = 2.0 * gen.gen_double() - 1.0;
        const double y = 2.0 * gen.gen_double() - 1.0;
        const double r2 = x * x + y * y;
        if (r2 <= 1.0 && r2 != 0.0) {
            return y * std::sqrt(-2.0 * std::log(r2) / r2);
        }
    }
}

template <class Gen> double standard_gamma_marsaglia_tsang(Gen& gen, double shape) {
    const double d = shape - 1.0 / 3.0;
    const double c = 1.0 / std::sqrt(9.0 * d);
    while (true) {
        const double x = standard_normal(gen);
        const double v_root = 1.0 + c * x;
        if (v_root <= 0.0) {
            continue;
        }
        const double v = v_root * v_root * v_root;
        const double u = uniform_open_closed(gen);
        const double x2 = x * x;
        if (u < 1.0 - 0.0331 * x2 * x2 || std::log(u) < 0.5 * x2 + d * (1.0 - v + std::log(v))) {
            return d * v;
        }
    }
}

template <class Gen> double standard_gamma(Gen& gen, double shape) {
    if (shape < 1.0) {
        const double u = uniform_open_closed(gen);
        return standard_gamma_marsaglia_tsang(gen, shape + 1.0) * std::pow(u, 1.0 / shape);
    }
    return standard_gamma_marsaglia_tsang(gen, shape);
}

template <class Gen> double gamma(Gen& gen, double shape, double scale) { return scale * standard_gamma(gen, shape); }

template <class Gen> double beta_johnk(Gen& gen, double a, double b) {
    while (true) {
        const double u = gen.gen_double();
        const double v = gen.gen_double();
        const double x = std::pow(u, 1.0 / a);
        const double y = std::pow(v, 1.0 / b);
        const double sum = x + y;
        if (sum <= 1.0 && u + v > 0.0) {
            if (sum > 0.0) {
                return x / sum;
            }
            double log_x = std::log(u) / a;
            double log_y = std::log(v) / b;
            const double log_max = std::max(log_x, log_y);
            log_x -= log_max;
            log_y -= log_max;
            return std::exp(log_x - std::log(std::exp(log_x) + std::exp(log_y)));
        }
    }
}

template <class Gen> double beta(Gen& gen, double a, double b) {
    if (a <= 1.0 && b <= 1.0) {
        return beta_johnk(gen, a, b);
    }
    const double x = standard_gamma(gen, a);
    const double y = standard_gamma(gen, b);
    return x / (x + y);
}

inline double log_factorial(double k) {
    constexpr std::array<double, 10> SmallLogFactorials{0.0,
                                                        0.0,
                                                        std::numbers::ln2,
                                                        1.791759469228055,
                                                        3.1780538303479458,
                                                        4.787491742782046,
                                                        6.579251212010101,
                                                        8.525161361065415,
                                                        10.60460290274525,
                                                        12.801827480081469};
    if (k < static_cast<double>(SmallLogFactorials.size())) {
        return SmallLogFactorials[static_cast<std::size_t>(k)];
    }
    const double x = k + 1.0;
    const double inv_x = 1.0 / x;
    const double inv_x2 = inv_x * inv_x;
    const double series =
        inv_x * (1.0 / 12.0 - inv_x2 * (1.0 / 360.0 - inv_x2 * (1.0 / 1260.0 - inv_x2 * (1.0 / 1680.0))));
    return (x - 0.5) * std::log(x) - x + 0.5 * std::log(2.0 * std::numbers::pi) + series;
}

template <class Gen> int poisson_multiplication(Gen& gen, double mean) {
    const double limit = std::exp(-mean);
    int count = 0;
    double product = gen.gen_double();
    while (product > limit) {
        ++count;
        product *= gen.gen_double();
    }
    return count;
}

template <class Gen> int poisson_ptrs(Gen& gen, double mean) {
    const double sqrt_mean = std::sqrt(mean);
    const double log_mean = std::log(mean);
    const double b = 0.931 + 2.53 * sqrt_mean;
    const double a = -0.059 + 0.02483 * b;
    const double log_inv_alpha = std::log(1.1239 + 1.1328 / (b - 3.4));
    const double v_r = 0.9277 - 3.6224 / (b - 2.0);
    while (true) {
        const double u = gen.gen_double() - 0.5;
        const double v = gen.gen_double();
        const double u_s = 0.5 - std::abs(u);
        const double k = std::floor((2.0 * a / u_s + b) * u + mean + 0.43);
        if (u_s >= 0.07 && v <= v_r) {
            return static_cast<int>(k);
        }
        if (k < 0.0 || (u_s < 0.013 && v > u_s)) {
            continue;
        }
        if (std::log(v) + log_inv_alpha - std::log(a / (u_s * u_s) + b) <= -mean + k * log_mean - log_factorial(k)) {
            return static_cast<int>(k);
        }
    }
}

template <class Gen> int poisson(Gen& gen, double mean) {
    if (mean <= 0.0) {
        return 0;
    }
    if (mean < 10.0) {
        return poisson_multiplication(gen, mean);
    }
    return poisson_ptrs(gen, mean);
}

template <class Gen> double weibull(Gen& gen, double scale, double shape) {
    return scale * std::pow(-std::log(uniform_open_closed(gen)), 1.0 / shape);
}

} // namespace OM::util::variates

#endif
