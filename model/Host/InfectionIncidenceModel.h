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

#ifndef HMOD_INFECTION_INCIDENCE
#define HMOD_INFECTION_INCIDENCE

#include "Global.h"
#include "Transmission/PerHost.h"
#include "util/random.h"

#include <memory>

namespace OM {
class Parameters;
namespace util {
class ModelOptionFlags;
}
namespace Host {
class Human;
class InfectionIncidenceModel;

using util::LocalRng;

class InfectionIncidenceParams {
public:
    enum class Kind { Default, HeterogeneityWorkaround, NegativeBinomial, LogNormal };

    InfectionIncidenceParams() = default;
    InfectionIncidenceParams(const Parameters& parameters, const util::ModelOptionFlags& model_options);

    std::unique_ptr<InfectionIncidenceModel> create() const;

    Kind kind = Kind::Default;
    double baseline_avail_shape_param = 0.0;
    double baseline_avail_offset = 0.0;
    double gamma_p = 0.0;
    double s_inf = 0.0;
    double s_imm = 0.0;
    double e_star_inv = 0.0;
    double x_star_p_inv = 0.0;
    double inf_rate_shape_param = 0.0;
    double inf_rate_offset = 0.0;
    bool no_pre_erythrocytic = false;
    bool vaccine_genotype = false;
};

class NewInfections {
public:
    void add(int count) { new_infections += count; }
    void pre_main_sim_init() { new_infections = 0; }
    void report(ostream& stream) {
        stream << '\t' << new_infections;
        new_infections = 0;
    }
    template <class S> void checkpoint(S& stream) { new_infections & stream; }

private:
    int new_infections = 0;
};

/** Models how a per-host EIR translates into new infections
 * (roughly when bites from infected mosquitos infect the host).
 *
 * There are four versions of this model, with different availability models:
 * - InfectionIncidenceModel (the default): Smith et al, AJTMH 2006 75 Suppl 2
 * - HeterogeneityWorkaroundII: emulates old, presumably unintended, behaviour
 * - NegBinomMAII: NEGATIVE_BINOMIAL_MASS_ACTION
 * - LogNormalMAII: LOGNORMAL_MASS_ACTION
 * 
 * There are also two susceptibility models which should be compatible with all
 * of these (see susceptibility()). */
class InfectionIncidenceModel {
public:
    explicit InfectionIncidenceModel(const InfectionIncidenceParams& params);

    /// Checkpointing
    template <class S> void operator&(S& stream) {
        p_infected & stream;
        cumulative_eir_a & stream;
    }

    virtual ~InfectionIncidenceModel() {}

    /** Return an availability multiplier, dependant on the model (NegBinomMAII
   * and LogNormalMAII models use this). Ideally availability adjustments
   * should have nothing to do with the InfectionIncidenceModel though.
   * 
   * @param baseAvailability This was BaselineAvailabilityMean from Constant.h,
   *	and had the value 1.0. Whether it should be anything else I don't know.
   */
    virtual double get_availability_factor(LocalRng& rng, double base_availability = 1.0);

    /// Output _pinfected to the summary
    void summarize(const Host::Human& human);

    /** Calculate the expected number of new infections to introduce.
   * 
   * Firstly converts the EIR into an expected number of infections (what
   * getExpectedNumberOfInfections() used to do):
   * 1. Calculates h from the EIR measured on adults where h is the expected
   * number of epidemiological inoculations.
   * 2. Calculates the updated values of the pre-erythrocytic exposure.
   * 
   * Secondly calculates the number of new infections to introduce via a
   * stochastic process.
   * 
   * @param human   Reference to human
   * @param effectiveEIR    EIR adjusted for availability (age, het,
   *        outside transmission due to hospitalisation)
   */
    double expected_num_new_infections(OM::Host::Human& human, double effective_eir);

    /** Calculate the number of new infections to introduce.
   * 
   * @param human   Reference to human
   * @param expectedNumNewInfections   Expected number of infections
   * to be introduced
   */
    int num_new_infections(OM::Host::Human& human, double expected_num_new_infections);

    /** Report the number of new infections
   * 
   * This number might be lower than the numNewInfections from this model
   * due to vaccine factors being applied in the WithinHost model
   * 
   * @param int newNumInfections_i  Number of introduced infections to be reported
   * @param int newNumInfections_l  Number of local (indigenous) infections to be reported
   */
    void report_num_new_infections(Human& human, int new_num_infections_i, int new_num_infections_l,
                                   NewInfections& new_infections);

protected:
    /// Calculates the expected number of infections, excluding vaccine effects
    virtual double get_model_expected_infections(LocalRng& rng, double effective_eir,
                                                 const Transmission::PerHost& ph_trans);

    double susceptibility();

    const InfectionIncidenceParams* params;

    /** Probability of infection (cumulative or reset to zero in massTreatment).
   *
   * Appears to be used only for calculating expected inoculations for the
   * analysis of pre-erythrocytic immunity. */
    double p_infected;

    //!Number of infective bites since birth
    double cumulative_eir_a; //TODO(memory opt): not needed by NegBinomMAII and LogNormalMAII
};

//TODO(optimisation): none of these add data members, so should we be using
// inheritance and dynamic type? Could instead use static function pointers.
/** A workaround to produce the same results as with heterogeneity work-units.
 *
 * The EIR passed into the function was not in one place adjusted by the
 * availability factor used in transmission heterogeneity, where it possibly
 * should have been. In any case, this should allow reproducing those results.
 */
class HeterogeneityWorkaroundII : public InfectionIncidenceModel {
public:
    explicit HeterogeneityWorkaroundII(const InfectionIncidenceParams& params) : InfectionIncidenceModel(params) {}
    virtual ~HeterogeneityWorkaroundII() {}

protected:
    double get_model_expected_infections(LocalRng& rng, double effective_eir, const Transmission::PerHost& ph_trans);
};
class NegBinomMAII : public InfectionIncidenceModel {
public:
    explicit NegBinomMAII(const InfectionIncidenceParams& params) : InfectionIncidenceModel(params) {}
    virtual ~NegBinomMAII() {}
    virtual double get_availability_factor(LocalRng& rng, double base_availability = 1.0);

protected:
    double get_model_expected_infections(LocalRng& rng, double effective_eir, const Transmission::PerHost&);
};
class LogNormalMAII : public InfectionIncidenceModel {
public:
    explicit LogNormalMAII(const InfectionIncidenceParams& params) : InfectionIncidenceModel(params) {}
    virtual ~LogNormalMAII() {}
    virtual double get_availability_factor(LocalRng& rng, double base_availability = 1.0);

protected:
    double get_model_expected_infections(LocalRng& rng, double effective_eir, const Transmission::PerHost&);
};

}
}
#endif
