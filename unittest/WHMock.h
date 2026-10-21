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

#ifndef Hmod_WithinHost_Mock
#define Hmod_WithinHost_Mock

#include "Global.h"
#include "Host/WithinHost/WHInterface.h"
#include "PkPd/LSTMModel.h"

using namespace std;

namespace OM {
namespace WithinHost {
namespace Pathogenesis {
class PathogenesisModel;
}
}

namespace UnitTest {
using namespace WithinHost;

/**
 * A mock implementation of WHInterface for unit testing.
 */
class WHMock : public WHInterface {
public:
    WHMock();
    virtual ~WHMock();

    virtual double prob_transmission_to_mosquito(vector<double>& probTransGenotype_i,
                                                 vector<double>& probTransGenotype_l) const;
    virtual bool summarize(Host::Human& human) const;
    virtual void import_infection(LocalRng& rng);
    virtual void treatment(Host::Human& human, TreatmentId treatId);
    virtual void optional_pq_treatment(Host::Human& human);
    virtual bool treat_simple(Host::Human& human, SimTime timeLiver, SimTime timeBlood);
    virtual void treat_pk_pd(size_t schedule, size_t dosages, double age, double delay_d);
    virtual void update(Host::Human& human, LocalRng& rng, int& nNewInfs_i, int& nNewInfs_l,
                        vector<double>& genotype_weights_i, vector<double>& genotype_weights_l, double ageInYears);
    virtual double get_total_density() const;
    virtual bool diagnostic_result(LocalRng& rng, const Diagnostic& diagnostic) const;
    virtual Pathogenesis::StatePair determine_morbidity(Host::Human& human, double ageYears, bool isDoomed);
    virtual void clear_immunity();
    virtual double get_cumulative_h() const;
    virtual double get_cumulative_y() const;
    virtual InfectionOrigin infection_origin() const;

    // This mock class does not have actual infections. Just set this as you please.
    double totalDensity;

    // This mock class counts the number of times treatment() was called. Read/write this as you like.
    int nTreatments;

    // The last treatment time-spans used by the simple treatment model. sim::never() if not used.
    SimTime lastTimeLiver, lastTimeBlood;

    // Lists medications and drugs in the body
    pk_pd::LSTMModel pkpd;

protected:
    virtual void checkpoint(istream& stream);
    virtual void checkpoint(ostream& stream);
};

}
}
#endif
