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

#ifndef HMOD_WH_GENOTYPES
#define HMOD_WH_GENOTYPES

#include "Global.h"
#include "util/random.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace scnXml {
class Scenario;
}

namespace OM {
namespace util {
class CommandLineOptions;
}
namespace WithinHost {

using util::LocalRng;

/// Represent a combination of alleles, each from a different locus
struct Genotype {
    Genotype(uint32_t allele, double freq, double fit, bool hrp2_del)
        : init_freq(freq), fitness(fit), hrp2_deficient(hrp2_del) {
        alleles.insert(allele);
    }
    Genotype cross(const Genotype& that) const;
    std::set<uint32_t> alleles; // set of codes of all alleles
    double init_freq;
    double fitness;
    bool hrp2_deficient;
};

class GenotypeRegistry {
public:
    GenotypeRegistry() = default;
    /** Initialise from XML data. */
    GenotypeRegistry(const scnXml::Scenario& scenario, const util::CommandLineOptions& command_line);

    /** Initialise with a single genotype. */
    void init_single();

    /** Switch to whichever mode has been enabled for the main simulation. */
    void pre_main_sim_init() { current_mode = interv_mode; }

    /** Map a locus name and allele name to an allele code.
     * 
     * Note that two alleles from different loci will always have different
     * codes.
     * 
     * Returns max value when no match is found. */
    uint32_t find_allele_code(const std::string& locus, const std::string& allele) const;

    const std::vector<Genotype>& genotypes() const { return genotype_list; }

    /** Sample the genotype using the configured approach.
     * 
     * @param genotype_weights When in tracking mode, this vector gives the
     *  weights of each genotype for use in sampling. Total need not be one.
     *  Also, passing a zero-length vector is a signal to use initial
     *  frequencies in sampling. */
    uint32_t sample_genotype(LocalRng& rng, std::vector<double>& genotype_weights) const;

    /** Get the number of genotypes. Functions like sample_genotype use values
     * from 0 to one less than this. */
    size_t count() const { return n_genotypes; }

    /** Get the initial frequency of some genotype. */
    double initial_freq(size_t genotype) const;

    void checkpoint(std::ostream& stream) const;
    void checkpoint(std::istream& stream);

private:
    enum class SampleMode : std::uint8_t {
        First,   // always choose first genotype (essentially the off switch)
        Initial, // sample from initial probabilities
        Tracking // sample from tracked success at genotype level (no recombination)
    };

    void print_genotypes() const;

    // keys are cumulative probabilities; last entry should equal 1; values are genotype codes
    std::map<double, uint32_t> cum_initial_freqs;
    // we give each allele of each loci a unique code
    std::map<std::string, std::map<std::string, uint32_t>> allele_codes;
    uint32_t next_allele_code = 0;
    std::vector<Genotype> genotype_list;
    size_t n_genotypes = 1;
    // Mode to use now (until switched) and from the start of the intervention period.
    SampleMode current_mode = SampleMode::First, interv_mode = SampleMode::First;
};

}

namespace bridge {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline const WithinHost::GenotypeRegistry* current_genotypes = nullptr;

inline const WithinHost::GenotypeRegistry& genotypes() {
    assert(current_genotypes != nullptr);
    return *current_genotypes;
}

}

class GenotypesBinding {
public:
    explicit GenotypesBinding(const WithinHost::GenotypeRegistry& genotypes) : previous(bridge::current_genotypes) {
        bridge::current_genotypes = &genotypes;
    }
    GenotypesBinding(const GenotypesBinding&) = delete;
    GenotypesBinding& operator=(const GenotypesBinding&) = delete;
    GenotypesBinding(GenotypesBinding&&) = delete;
    GenotypesBinding& operator=(GenotypesBinding&&) = delete;
    ~GenotypesBinding() { bridge::current_genotypes = previous; }

private:
    const WithinHost::GenotypeRegistry* previous;
};

namespace WithinHost {

/** Represents infection genotypes. */
class Genotypes {
public:
    using Genotype = WithinHost::Genotype;

    static uint32_t find_allele_code(const string& locus, const string& allele) {
        return bridge::genotypes().find_allele_code(locus, allele);
    }

    static const vector<Genotype>& get_genotypes() { return bridge::genotypes().genotypes(); }

    static uint32_t sample_genotype(LocalRng& rng, std::vector<double>& genotype_weights) {
        return bridge::genotypes().sample_genotype(rng, genotype_weights);
    }

    static size_t count() { return bridge::genotypes().count(); }

    static double initial_freq(size_t genotype) { return bridge::genotypes().initial_freq(genotype); }
};

}
}
#endif
