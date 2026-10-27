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

#ifndef HMOD_GENOTYPES_SUITE
#define HMOD_GENOTYPES_SUITE

#include <cxxtest/TestSuite.h>
#include "UnittestUtil.h"
#include "Host/WithinHost/Genotypes.h"
#include "PkPd/Drug/LSTMDrugType.h"
#include "util/CommandLine.h"

#include <array>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using OM::WithinHost::GenotypeRegistry;

class GenotypesSuite : public CxxTest::TestSuite {
public:
    void test_print_one_allele_locus_first() {
        TS_ASSERT_EQUALS(printed({first_locus(), mdr_locus()}), "\n"
                                                                "|Genotype|first|      mdr|init freq|fitness|\n"
                                                                "|--------|-----|---------|---------|-------|\n"
                                                                "|       0| only|sensitive|    0.500|  1.000|\n"
                                                                "| 1000000| only|resistant|    0.500|  1.000|\n");
    }

    void test_print_one_allele_locus_last() {
        TS_ASSERT_EQUALS(printed({mdr_locus(), first_locus()}), "\n"
                                                                "|Genotype|      mdr|first|init freq|fitness|\n"
                                                                "|--------|---------|-----|---------|-------|\n"
                                                                "|       0|sensitive| only|    0.500|  1.000|\n"
                                                                "| 1000000|resistant| only|    0.500|  1.000|\n");
    }

    void test_print_phenotype_mapping() {
        const GenotypeRegistry genotypes(scenario_with({mdr_locus()}), options(false));
        scnXml::Drugs drugs;
        drugs.getDrug().push_back(drug_with_phenotypes("CQ"));
        std::ostringstream out;
        std::streambuf* const cout_buffer = std::cout.rdbuf(out.rdbuf());
        const pk_pd::DrugRegistry registry(drugs, genotypes, options(true));
        std::cout.rdbuf(cout_buffer);
        TS_ASSERT_EQUALS(out.str(), "\n"
                                    "Phenotype mapping for drug CQ:\n"
                                    "------------------------------\n"
                                    "|genotype|              phenotype|\n"
                                    "|--------|-----------------------|\n"
                                    "|       0|              sensitive|\n"
                                    "| 1000000|resistant to everything|\n");
    }

    void test_one_allele_locus_genotypes() {
        const GenotypeRegistry genotypes(scenario_with({first_locus(), mdr_locus()}), options(false));
        const uint32_t only = genotypes.find_allele_code("first", "only");
        TS_ASSERT_EQUALS(genotypes.count(), 2U);
        TS_ASSERT(genotypes.genotypes()[0].alleles ==
                  std::set<uint32_t>({only, genotypes.find_allele_code("mdr", "sensitive")}));
        TS_ASSERT(genotypes.genotypes()[1].alleles ==
                  std::set<uint32_t>({only, genotypes.find_allele_code("mdr", "resistant")}));
    }

private:
    static scnXml::ParasiteLocus first_locus() {
        scnXml::ParasiteLocus locus("first");
        locus.getAllele().push_back(scnXml::ParasiteAllele("only", 1.0, 1.0));
        return locus;
    }

    static scnXml::ParasiteLocus mdr_locus() {
        scnXml::ParasiteLocus locus("mdr");
        locus.getAllele().push_back(scnXml::ParasiteAllele("sensitive", 0.5, 1.0));
        locus.getAllele().push_back(scnXml::ParasiteAllele("resistant", 0.5, 1.0));
        return locus;
    }

    static scnXml::PKPDDrug drug_with_phenotypes(const char* abbrev) {
        scnXml::PKPDDrug drug =
            xml_helpers::drug(abbrev, xml_helpers::PK1C(17.4, 1e-17, 3.96, 0.0), xml_helpers::PD(27.6, 0.0023, 4.0));
        scnXml::PD::PhenotypeSequence& phenotypes = drug.getPD().getPhenotype();
        phenotypes.push_back(phenotypes[0]);
        phenotypes[0].setName("sensitive");
        phenotypes[0].getRestriction().push_back(scnXml::Restriction("mdr", "sensitive"));
        phenotypes[1].setName("resistant to everything");
        phenotypes[1].getRestriction().push_back(scnXml::Restriction("mdr", "resistant"));
        return drug;
    }

    static scnXml::Scenario scenario_with(const std::vector<scnXml::ParasiteLocus>& loci) {
        scnXml::ParasiteGenetics genetics("initial");
        for (const scnXml::ParasiteLocus& locus : loci)
            genetics.getLocus().push_back(locus);
        scnXml::Scenario scenario(dummyXML::scenario);
        scenario.setParasiteGenetics(genetics);
        return scenario;
    }

    static util::CommandLineOptions options(bool print_genotypes) {
        std::string program = "openMalaria";
        std::string flag = print_genotypes ? "--print-genotypes" : "--verbose";
        const std::array<char*, 2> args{program.data(), flag.data()};
        return util::CommandLine::parse(args);
    }

    static std::string printed(const std::vector<scnXml::ParasiteLocus>& loci) {
        const scnXml::Scenario scenario = scenario_with(loci);
        const util::CommandLineOptions command_line = options(true);
        std::ostringstream out;
        std::streambuf* const cout_buffer = std::cout.rdbuf(out.rdbuf());
        const std::ios_base::fmtflags cout_flags = std::cout.flags();
        const std::streamsize cout_precision = std::cout.precision();
        const GenotypeRegistry genotypes(scenario, command_line);
        std::cout.precision(cout_precision);
        std::cout.flags(cout_flags);
        std::cout.rdbuf(cout_buffer);
        return out.str();
    }
};

#endif
