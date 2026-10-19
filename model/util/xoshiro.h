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

/*
 * RNG is from: http://prng.di.unimi.it/
 */

#ifndef OM_UTIL_XOSHIRO
#define OM_UTIL_XOSHIRO

#include <array>
#include <cstdint>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>

/// Implementation of Xoshiro256+
class Xoshiro256P {
public:
    using result_type = uint64_t;

    Xoshiro256P(uint64_t a, uint64_t b = 0x220b5e8d, uint64_t c = 0x712a58a2, uint64_t d = 0x712a58a2) {
        this->seed(a, b, c, d);
    }

    template <typename R> explicit Xoshiro256P(R& source) { seed(source); }

    // Disable copying
    Xoshiro256P(const Xoshiro256P&) = delete;
    Xoshiro256P& operator=(const Xoshiro256P&) = delete;

    Xoshiro256P(Xoshiro256P&&) noexcept = default;
    Xoshiro256P& operator=(Xoshiro256P&&) noexcept = default;
    ~Xoshiro256P() = default;

    void seed(uint64_t a, uint64_t b = 0x220b5e8d, uint64_t c = 0x712a58a2, uint64_t d = 0x712a58a2) {
        s[0] = a;
        s[1] = b;
        s[2] = c;
        s[3] = d;
    }

    template <typename R> void seed(R& source) {
        s[0] = source.gen_u64();
        s[1] = source.gen_u64();
        s[2] = source.gen_u64();
        s[3] = source.gen_u64();
    }

    uint64_t operator()();
    uint32_t gen_u32();
    double gen_double();

    friend bool operator==(const Xoshiro256P& lhs, const Xoshiro256P& rhs);
    friend bool operator!=(const Xoshiro256P& lhs, const Xoshiro256P& rhs);

    void binary_checkpoint(std::ostream& stream) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        stream.write(reinterpret_cast<const char*>(s.data()), sizeof(s));
    }
    void binary_checkpoint(std::istream& stream) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        stream.read(reinterpret_cast<char*>(s.data()), sizeof(s));
        if (!stream || stream.gcount() != sizeof(s))
            throw std::runtime_error("Xoshiro256P::binary_checkpoint: stream read error");
    }

    static constexpr uint64_t min() { return std::numeric_limits<uint64_t>::min(); }
    static constexpr uint64_t max() { return std::numeric_limits<uint64_t>::max(); }

private:
    std::array<uint64_t, 4> s{};
};

static inline uint64_t rotl(const uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

inline uint64_t Xoshiro256P::operator()() {
    const uint64_t result = s[0] + s[3];

    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;

    s[3] = rotl(s[3], 45);

    return result;
}

inline uint32_t Xoshiro256P::gen_u32() {
    // Since it is recommended not to use the low bits, we take the high part.
    const uint64_t x = this->operator()();
    return static_cast<uint32_t>(x >> 32);
}

inline double Xoshiro256P::gen_double() {
    // Doubles support 53-bits of precision (1 bit implied). Use our high bits.
    const uint64_t x = this->operator()();
    const double v = 1.1102230246251565e-16; // = 0x1.0p-53
    return static_cast<double>(x >> 11) * v;
}

// Implement <random> interface.
inline bool operator==(const Xoshiro256P& lhs, const Xoshiro256P& rhs) { return lhs.s == rhs.s; }

inline bool operator!=(const Xoshiro256P& lhs, const Xoshiro256P& rhs) { return !(lhs == rhs); }

#endif
