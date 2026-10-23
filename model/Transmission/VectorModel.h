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

#ifndef HMOD_VECTOR_MODEL
#define HMOD_VECTOR_MODEL

#include "Global.h"
#include "Transmission/TransmissionModel.h"
#include "Transmission/Anopheles/AnophelesModel.h"
#include "Transmission/Anopheles/EmergenceRateEstimator.h"
#include "Host/Human.h"

namespace scnXml {
class Vector;
}

namespace OM {
class Population;
namespace Transmission {

/** Transmission models, Chitnis et al.
 *
 * This class contains code for species-independent components. Per-species
 * code is in the Anopheles directory and namespace. */
class VectorModel : public TransmissionModel {
public:
    /// Get the map of species names to indicies.
    const map<string, size_t>& get_species_index_map() { return species_index; }

    VectorModel(vector<double> init_eir, int intervention_mode,
                vector<std::unique_ptr<Anopheles::AnophelesModel>> species_list,
                vector<std::unique_ptr<Anopheles::EmergenceRateEstimator>> emergence_rate_estimators_list,
                map<string, size_t> species_index_list, int population_size, util::MasterRng& transmission_rng,
                size_t n_genotypes, SimTime adult_age, bool vaccine_genotype, mon::Continuous& continuous);

    ~VectorModel() override;

    /** Extra initialisation when not loading from a checkpoint, requiring
     * information from the human population structure. */
    void init2(const vector<Host::Human>& population) override;

    virtual void init_vector_interv(const scnXml::Description::AnophelesSequence& list, size_t instance,
                                    const string& name);
    virtual void init_vector_trap(const scnXml::VectorTrap::DescriptionSequence& list, size_t instance,
                                  const scnXml::VectorTrap::NameOptional& name_opt);

    void scale_eir(double factor) override;

    SimTime init_iterate() override;

    void vector_update(const vector<Host::Human>& population) override;

    double update_kappa(const vector<Host::Human>& population) override;

    void calculate_eir(Host::Human& human, double age_years, vector<double>& eir_i,
                       vector<double>& eir_l) const override;

    virtual void deploy_vector_pop_interv(size_t instance);
    virtual void deploy_vector_trap(size_t instance, double pop_size, SimTime lifespan);

    void change_eir_intervention(const scnXml::NonVector& non_vector_data) override;

    void uninfect_vectors() override;

    void summarize() override;

protected:
    void checkpoint(istream& stream) override;
    void checkpoint(ostream& stream) override;

private:
    void cts_cb_n_v0(ostream& stream);
    void cts_cb_p_a(ostream& stream);
    void cts_cb_p_amu(ostream& stream);
    void cts_cb_p_a1(ostream& stream);
    void cts_cb_p_ah(ostream& stream);
    void cts_cb_p_df(ostream& stream);
    void cts_cb_p_dif(ostream& stream);
    void cts_cb_n_v(ostream& stream);
    void cts_cb_o_v(ostream& stream);
    void cts_cb_s_v(ostream& stream);
    void cts_cb_alpha(const vector<Host::Human>& population, ostream& stream);
    void cts_cb_p_b(const vector<Host::Human>& population, ostream& stream);
    void cts_cb_p_cd(const vector<Host::Human>& population, ostream& stream);
    void cts_net_insecticide_content(const vector<Host::Human>& population, ostream& stream);
    void cts_irs_insecticide_content(const vector<Host::Human>& population, ostream& stream);
    void cts_irs_effects(const vector<Host::Human>& population, ostream& stream);
    void cts_cb_res_availability(ostream& stream);
    void cts_cb_res_requirements(ostream& stream);

    vector<double> mean_annual_kappa() const;

public:
    /// RNG used by the transmission model
    LocalRng rng;

    /// Number of iterations performed during initialization.
    ///
    /// Special cases: 0 during initial one-human-lifespan warmup, <0 after
    /// initialisation.
    int init_iterations{0};

    double mean_avail = 1.0;

    vector<double> quinquennial_kappa;

    /** Per anopheles species data.
     *
     * Array will be recreated by constructor, but some members of AnophelesModel
     * need to be checkpointed. */
    vector<std::unique_ptr<Anopheles::AnophelesModel>> species;
    vector<std::unique_ptr<Anopheles::EmergenceRateEstimator>> emergence_rate_estimators;

    /** A map of anopheles species/variant name to an index in species.
     *
     * When the main ento data is read from XML, each anopheles section is read
     * into one index of the species array. The name and this index are added
     * here.
     *
     * Other data read from XML should look up the name here and use the index
     * found. Doesn't need checkpointing. */
    map<string, size_t> species_index;

    bool opt_vaccine_genotype = false;

    friend class PerHost;
    friend class AnophelesModelSuite;
};

} // namespace Transmission
} // namespace OM
#endif
