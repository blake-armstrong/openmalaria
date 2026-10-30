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

#include "Host/WithinHost/Genotypes.h"
#include "util/random.h"
#include "util/errors.h"
#include "util/vectors.h"
#include "util/CommandLine.h"
#include "util/logging.h"
#include "schema/scenario.h"

#include <algorithm>
#include <cassert>
#include <format>

namespace OM::WithinHost {

namespace {

// Represent a set of loci: all possible combinations of alleles
// This is actually just machinery to calculate the list of all genotypes
struct LocusSet {
    LocusSet(const scnXml::ParasiteLocus& elt_l, map<string, map<string, uint32_t>>& allele_codes,
             uint32_t& next_allele_code) {
        alleles.reserve(elt_l.getAllele().size());
        double cum_p = 0.0;
        for (const scnXml::ParasiteAllele& elt_a : elt_l.getAllele()) {
            const uint32_t allele_code = (next_allele_code++);
            allele_codes[elt_l.getName()][elt_a.getName()] = allele_code;
            cum_p += elt_a.getInitialFrequency();
            alleles.emplace_back(allele_code, elt_a.getInitialFrequency(), elt_a.getFitness(),
                                 elt_a.getHrp2_deletion());
        }
        if (cum_p < 0.999 || cum_p > 1.001)
            throw util::xml_scenario_error("expected sum of initial probabilities of alleles to be 1, but for the " +
                                           to_string(alleles.size()) + " alleles under locus " +
                                           string(elt_l.getName()) + " this is " + to_string(cum_p));

        alleles[0].init_freq += 1.0 - cum_p; // account for any small errors by adjusting the first frequency
    }
    void include(LocusSet& that) {
        vector<Genotype> new_alleles;
        new_alleles.reserve(alleles.size() * that.alleles.size());
        for (size_t i = 0; i < alleles.size(); i += 1) {
            for (size_t j = 0; j < that.alleles.size(); j += 1) {
                // note that we generate new codes each time; we just waste the old ones
                new_alleles.push_back(alleles[i].cross(that.alleles[j]));
            }
        }
        alleles.swap(new_alleles);
    }

    vector<Genotype> alleles;
};

}

Genotype Genotype::cross(const Genotype& that) const {
    Genotype result(*that.alleles.begin(), init_freq * that.init_freq, fitness * that.fitness,
                    hrp2_deficient || that.hrp2_deficient);
    result.alleles.insert(alleles.begin(), alleles.end());
    result.alleles.insert(that.alleles.begin(), that.alleles.end());
    return result;
}

void GenotypeRegistry::init_single() {
    // no specification implies there is a single genotype
    genotype_list.assign(1, Genotype(0 /*allele code*/, 1.0 /*frequency*/, 1.0 /*fitness*/, false /*hrp2 deficiency*/));
    n_genotypes = 1;
}

GenotypeRegistry::GenotypeRegistry(const scnXml::Scenario& scenario, const util::CommandLineOptions& command_line) {
    if (scenario.getParasiteGenetics().present()) {
        const scnXml::ParasiteGenetics& genetics = scenario.getParasiteGenetics().get();

        current_mode = SampleMode::Initial; // turn on sampling
        if (genetics.getSamplingMode() == "initial") {
            interv_mode = SampleMode::Initial;
        } else if (genetics.getSamplingMode() == "tracking") {
            interv_mode = SampleMode::Tracking;
            if (!scenario.getEntomology().getVector().present()) {
                throw util::xml_scenario_error("incompatibility; either use "
                                               "entomology/vector (not nonVector) or set "
                                               "parasiteGenetics/samplingMode to \"initial\" (not \"tracking\")");
            }
            if (scenario.getEntomology().getMode() != "dynamic") {
                throw util::xml_scenario_error("incompatibility; either set "
                                               "entomology/mode to \"dynamic\" (not \"forced\") or set "
                                               "parasiteGenetics/samplingMode to \"initial\" (not \"tracking\")");
            }
        } else {
            throw util::xml_scenario_error(R"(parasiteGenetics/samplingMode: expected "initial" or "tracking")");
        }

        // Build the list of all allele combinations by iterating over loci (plural of locus):
        LocusSet loci(genetics.getLocus()[0], allele_codes, next_allele_code);
        for (size_t i = 1; i < genetics.getLocus().size(); i += 1) {
            LocusSet new_locus(genetics.getLocus()[i], allele_codes, next_allele_code);
            loci.include(new_locus);
        }
        genotype_list.swap(loci.alleles);
        n_genotypes = genotype_list.size();

        double cum_p = 0.0;
        for (size_t i = 0; i < genotype_list.size(); ++i) {
            cum_p += genotype_list[i].init_freq;
            const auto genotype_id = static_cast<uint32_t>(i);
            assert(genotype_id == i);
            cum_initial_freqs.insert(make_pair(cum_p, genotype_id));
        }

        // Test cum_p is approx. 1.0 in case the input tree is wrong.
        if (cum_p < 0.999 || cum_p > 1.001)
            throw util::xml_scenario_error(
                "decision tree (random node): expected probability sum to be 1.0 but found " + to_string(cum_p));

        // last cum_p might be slightless less than 1 due to arithmetic errors; add a failsafe:
        cum_initial_freqs[1.0] = genotype_list.size() - 1;
    } else {
        init_single();
        cum_initial_freqs[1.0] = genotype_list.size() - 1;
    }

    if (command_line.option(util::CommandLine::PrintGenotypes))
        print_genotypes();
}

void GenotypeRegistry::print_genotypes() const {
    // reorganise allele_codes so that we can look up codes, not names
    uint32_t n_allele_codes = 0;
    for (const Genotype& genotype : genotype_list)
        n_allele_codes = std::max(n_allele_codes, *genotype.alleles.rbegin() + 1);
    vector<pair<string, string>> allele_names(n_allele_codes);
    for (const auto& [locus, codes] : allele_codes) {
        for (const auto& [allele, code] : codes) {
            assert(code < allele_names.size());
            allele_names[code] = make_pair(locus, allele);
        }
    }

    // determine our columns
    map<string, uint32_t> longest; // longest name in column; key is locus
    for (const Genotype& genotype : genotype_list) {
        if (longest.size() == 0) {
            for (const uint32_t allele : genotype.alleles) {
                const string& locus = allele_names[allele].first;

                longest[locus] = locus.length(); // locus name is included in column
            }
        } else
            assert(longest.size() == genotype.alleles.size());

        for (const uint32_t allele : genotype.alleles) {
            auto it = longest.find(allele_names[allele].first);
            assert(it != longest.end());
            const uint32_t len_allele = allele_names[allele].second.length();
            if (len_allele > it->second)
                it->second = len_allele;
        }
    }

    // find original loci order
    vector<string> loci;
    loci.reserve(longest.size());
    for (size_t i = 0; loci.size() < longest.size(); ++i) {
        assert(i < allele_names.size());
        const string& locus = allele_names[i].first;
        if (std::ranges::find(loci, locus) == loci.end()) {
            loci.push_back(locus);
        }
    }

    const int id_width = genotype_id_width(genotype_list.size());
    logging::info("");

    string header = std::format("|{:>{}}", "Genotype", id_width);
    for (const auto& locus : loci)
        header += std::format("|{:>{}}", locus, longest[locus]);
    logging::info("{}|{:>9}|{:>7}|", header, "init freq", "fitness");

    string rule = "|" + string(id_width, '-');
    for (const auto& locus : loci)
        rule += "|" + string(longest[locus], '-');
    logging::info("{}|{}|{}|", rule, string(9, '-'), string(7, '-'));

    for (size_t i = 0; i < genotype_list.size(); ++i) {
        const Genotype& genotype = genotype_list[i];
        map<string, string> locus_allele; // to find allele for each locus
        for (const uint32_t allele : genotype.alleles) {
            assert(allele < allele_names.size());
            locus_allele[allele_names[allele].first] = allele_names[allele].second;
        }

        string row = std::format("|{:>{}}", i * GenotypeOutputIdStep, id_width);
        for (const auto& locus : loci) {
            auto la = locus_allele.find(locus);
            assert(la != locus_allele.end());
            row += std::format("|{:>{}}", la->second, longest[locus]);
        }
        logging::info("{}|{:>9.3f}|{:>7.3f}|", row, genotype.init_freq, genotype.fitness);
    }
}

uint32_t GenotypeRegistry::find_allele_code(const string& locus, const string& allele) const {
    auto it = allele_codes.find(locus);
    if (it == allele_codes.end())
        return numeric_limits<uint32_t>::max();
    auto it2 = it->second.find(allele);
    if (it2 == it->second.end())
        return numeric_limits<uint32_t>::max();
    return it2->second;
}

uint32_t GenotypeRegistry::sample_genotype(LocalRng& rng, vector<double>& genotype_weights) const {
    if (current_mode == SampleMode::First) {
        return 0; // always the first genotype code
    } else if (current_mode == SampleMode::Initial || genotype_weights.size() == 0) {
        const double sample = rng.uniform_01();
        auto it = cum_initial_freqs.upper_bound(sample);
        assert(it != cum_initial_freqs.end());
        return it->second;
    } else {
        assert(current_mode == SampleMode::Tracking);
        assert(genotype_weights.size() == n_genotypes);
        const double weight_sum = util::vectors::sum(genotype_weights);
        assert(weight_sum >= 0.0 && weight_sum < 1e5); // possible loss of precision or other error
        const double sample = rng.uniform_01() * weight_sum;
        double cum = 0.0;
        for (size_t g = 0; g < n_genotypes; ++g) {
            cum += genotype_weights[g];
            if (sample < cum)
                return g;
        }
        return 0; // just to be safe (could happen if weight_sum == 0.0)
    }
}

double GenotypeRegistry::initial_freq(size_t genotype) const {
    if (genotype_list.size() == 0) {
        assert(genotype == 0);
        assert(n_genotypes == 1);
        return 1.0; // only 1 genotype, thus initial frequency is 100%
    } else {
        return genotype_list[genotype].init_freq;
    }
}

// ———  checkpointing  ———

void GenotypeRegistry::checkpoint(ostream& stream) const {
    const int t = static_cast<int>(current_mode);
    t & stream;
}
void GenotypeRegistry::checkpoint(istream& stream) {
    int t = 0;
    t & stream;
    current_mode = static_cast<SampleMode>(t);
    assert(current_mode == SampleMode::First || current_mode == SampleMode::Initial ||
           current_mode == SampleMode::Tracking);
}

}
