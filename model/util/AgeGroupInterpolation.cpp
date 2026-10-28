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

#include <fstream>
#include <utility>

#include "Clock.h"
#include "util/AgeGroupInterpolation.h"
#include "util/CommandLine.h"
#include "util/errors.h"
#include "schema/healthSystem.h"

/* Required by AgeGroupSplineInterpolation
#include <gsl_interp.h>
#include <gsl_spline.h>
*/

namespace OM::util {

// ———  AgeGroupInterpolation derivatives  ———

void AgeGroupInterpolation::output_samples(const string& name, const Clock& clock,
                                           const CommandLineOptions& command_line) {
    if (!command_line.option(util::CommandLine::SampleInterpolations)) {
        return;
    }
    ofstream fstream((name + ".csv").c_str());

    const double max = Sim::in_years(clock.max_human_age());
    for (double age = 0.0; age < max; age += 0.1) {
        fstream << age << "," << this->eval(age) << endl;
    }
}

/** Dummy object, for initialization. Potentially makes code safer.
     ********************************************/
class AgeGroupDummy : public AgeGroupInterpolation {
public:
    static AgeGroupDummy singleton;

    double eval(double age_years) const override { throw TRACED_EXCEPTION_DEFAULT("AgeGroupDummy"); }
    void scale(double factor) override { throw TRACED_EXCEPTION_DEFAULT("AgeGroupDummy"); }
    double first_global_maximum() override { throw TRACED_EXCEPTION_DEFAULT("AgeGroupDummy"); }
};
AgeGroupDummy AgeGroupDummy::singleton;

/** This class gives direct access to input age-group
    * data (discontinuous).
    ********************************************/
class AgeGroupPiecewiseConstant : public AgeGroupInterpolation {
public:
    AgeGroupPiecewiseConstant(const scnXml::AgeGroupValues& age_groups, const char* elt_name, const Clock& clock,
                              const CommandLineOptions& command_line) {
        if (age_groups.getGroup().size() == 0) {
            throw util::xml_scenario_error(string(elt_name) + ": at least one age group required");
        }

        auto pos = data_groups.begin();
        assert(pos == data_groups.end());

        double greatest_lbound = -1.0;
        for (const scnXml::Group& group : age_groups.getGroup()) {
            const double lbound = group.getLowerbound();
            if (lbound >= greatest_lbound) {
                greatest_lbound = lbound;
                pos = data_groups.insert(pos, make_pair(lbound, group.getValue()));
            } else
                throw util::xml_scenario_error(string(elt_name) + ": lower bound " + to_string(lbound) +
                                               " not greater than previous " + to_string(greatest_lbound));
        }

        // first lower-bound must be 0
        if (data_groups.begin()->first != 0.0)
            throw util::xml_scenario_error(string(elt_name) + ": first lower-bound must be 0");

        output_samples(elt_name, clock, command_line);
    }

    double eval(double age_years) const override {
        assert(age_years >= 0.0);
        auto it = data_groups.upper_bound(age_years);
        --it; // previous point when ordered by age (it->first <= ageYears < (it+1)->first)
        return it->second;
    }

    void scale(double factor) override {
        for (auto& data_group : data_groups) {
            data_group.second *= factor;
        }
    }

    double first_global_maximum() override {
        double age = 0.0, max = numeric_limits<double>::min();
        for (auto& data_group : data_groups) {
            if (data_group.second > max) {
                max = data_group.second;
                age = data_group.first;
            }
        }
        return age;
    }

protected:
    // All data groups as (lower-age-bound, value) pairs.
    map<double, double> data_groups;
};

/** Filter class to convert data groups into points in middle of groups plus
     * stabilization points at ends.
     ********************************************/
class AgeGroupInterpolationPoints : public AgeGroupInterpolation {
public:
    AgeGroupInterpolationPoints(const scnXml::AgeGroupValues& age_groups, const char* elt_name, const Clock& clock) {
        // Our read iterator
        auto it = age_groups.getGroup().begin();

        if (it == age_groups.getGroup().end())
            throw util::xml_scenario_error(string(elt_name) + ": at least one age group required");

        // first lower-bound must be 0
        if (it->getLowerbound() != 0.0)
            throw util::xml_scenario_error(string(elt_name) + ": first lower-bound must be 0");

        // Our insert iterator (used for performance)
        auto pos = data_points.begin();
        assert(pos == data_points.end());

        double greatest_lbound = 0.0;       // also last lbound: age groups must be ordered lowest to highest
        double last_value = it->getValue(); // first value is repeated for const start
        for (; it != age_groups.getGroup().end(); ++it) {
            const double lbound = it->getLowerbound();
            if (lbound >= greatest_lbound) {
                const double ins_bound = 0.5 * (greatest_lbound + lbound);
                pos = data_points.insert(pos, make_pair(ins_bound, last_value));
                greatest_lbound = lbound;
                last_value = it->getValue(); // always insert previous value
            } else
                throw util::xml_scenario_error(string(elt_name) + ": lower bound " + to_string(lbound) +
                                               " less than previous " + to_string(greatest_lbound));
        }

        // add a point in middle of last age group (taking upper bound as max-age-years:
        data_points[0.5 * (greatest_lbound + Sim::in_years(clock.max_human_age()))] = last_value;
    }

    void scale(double factor) override {
        for (auto& data_point : data_points) {
            data_point.second *= factor;
        }
    }

protected:
    // Points to interpolate between in the middle of input age groups. Extra
    // points at zero and infinity are added with equal value to first and last
    // points respectively.
    map<double, double> data_points;
};

/** This class gives piecewise linear inpolation on top of input age-group
     * data (continuous but with discontinuous derivative).
     ********************************************/
class AgeGroupPiecewiseLinear : public AgeGroupInterpolationPoints {
public:
    AgeGroupPiecewiseLinear(const scnXml::AgeGroupValues& age_groups, const char* elt_name, const Clock& clock,
                            const CommandLineOptions& command_line)
        : AgeGroupInterpolationPoints(age_groups, elt_name, clock) {
        // Add first point at zero
        data_points[0.0] = data_points.begin()->second;
        // add a point at infinity to catch remaining values
        data_points[numeric_limits<double>::infinity()] = data_points.rbegin()->second;

        output_samples(elt_name, clock, command_line);
    }

    double eval(double age_years) const override {
        assert(age_years >= 0.0);
        //TODO: optimise by putting (f1-f0)/(a1-a0) into the array and checking usage
        // Get the first point with age greater than ageYears:
        auto it = data_points.upper_bound(age_years);
        assert(it != data_points.end());
        const double a1 = it->first; // a1 > ageYears
        const double f1 = it->second;
        --it;                        // previous point when ordered by age
        const double a0 = it->first; // a0 ≤ ageYears
        const double f0 = it->second;
        return (age_years - a0) / (a1 - a0) * (f1 - f0) + f0;
    }

    double first_global_maximum() override {
        double age = 0.0, max = numeric_limits<double>::min();
        for (auto& data_point : data_points) {
            if (data_point.second > max) {
                max = data_point.second;
                age = data_point.first;
            }
        }
        return age;
    }
};

/* * Spline interpolation using GSL.
     * 
     * GSL has a few interpolation algorithms: linear, polynomial,
     * cspline, akima. They fit a single curve to the entire interval which
     * looks nothing like the data set in some cases (e.g. CFR).
     ******************************************** /
    class AgeGroupSplineInterpolation : public AgeGroupInterpolationPoints
    {
    public:
        AgeGroupSplineInterpolation (
            const scnXml::AgeGroupValues& ageGroups, const char* eltName
        ) : AgeGroupInterpolationPoints(ageGroups, eltName)
        {
            vector<double> x, y;
            x.reserve( dataPoints.size() );
            y.reserve( dataPoints.size() );
            for( auto it=dataPoints.begin(); it!=dataPoints.end(); ++it ){
                x.push_back( it->first );
                y.push_back( it->second );
            }
            
            acc = gsl_interp_accel_alloc ();
            spline = gsl_spline_alloc (gsl_interp_cspline, 10);
            gsl_spline_init (spline, x.data(), y.data(), 10);
            
            outputSamples( eltName );
        }
        virtual ~AgeGroupSplineInterpolation() {
            gsl_spline_free (spline);
            gsl_interp_accel_free (acc);
        }
        
        virtual double eval( double ageYears ) const{
            return gsl_spline_eval( spline, ageYears, acc );
        }
        
    protected:
        gsl_interp_accel *acc;
        gsl_spline *spline;
    };
    */

// -----  AgeGroupInterpolator  -----

AgeGroupInterpolator::AgeGroupInterpolator() : obj(&AgeGroupDummy::singleton) {}

AgeGroupInterpolator::AgeGroupInterpolator(AgeGroupInterpolator&& other) noexcept
    : obj(std::exchange(other.obj, &AgeGroupDummy::singleton)) {}

AgeGroupInterpolator& AgeGroupInterpolator::operator=(AgeGroupInterpolator&& other) noexcept {
    if (this != &other) {
        reset();
        obj = std::exchange(other.obj, &AgeGroupDummy::singleton);
    }
    return *this;
}

void AgeGroupInterpolator::set(const scnXml::AgeGroupValues& age_groups, const char* elt_name, const Clock& clock,
                               const CommandLineOptions& command_line) {
    reset();
    // Type mostly equivalent to a std::string:
    const scnXml::AgeGroupValues::InterpolationOptional& interp = age_groups.getInterpolation();
    if (!interp.present() || interp.get() == "linear") {
        obj = new AgeGroupPiecewiseLinear(age_groups, elt_name, clock, command_line);
    } else if (interp.get() == "none") {
        obj = new AgeGroupPiecewiseConstant(age_groups, elt_name, clock, command_line);
    } else
        throw util::xml_scenario_error(string("age group interpolation ") + interp.get() + " not implemented");
}
void AgeGroupInterpolator::reset() {
    assert(obj != nullptr); // should not do that
    if (obj != &AgeGroupDummy::singleton) {
        delete obj;
        obj = &AgeGroupDummy::singleton;
    }
}
bool AgeGroupInterpolator::isSet() const { return obj != &AgeGroupDummy::singleton; }
}
