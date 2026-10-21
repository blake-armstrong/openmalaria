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

#ifndef HMOD_UTIL_MODEL_NAME_PROVIDER
#define HMOD_UTIL_MODEL_NAME_PROVIDER

#include <schema/scenario.h>
#include "util/errors.h"

namespace OM::util {
/*
        * It is intended that this holds the entire collection of named models.
        */
enum class ModelNames {
    None, // Represents the case where input XML contains no model name.
    Base
};

/*
        * Stores model name (if any) specified in XML at initialization time
        * then exposes it as an enum class.  This is exists so that clients
        * don't have to hardcode strings referring (or possibly, erroneously,
        * not referring) to model names.
        *
        * If new named models are added, support for them should be added to
        * this class.
        */
class ModelNameProvider {
public:
    ModelNameProvider(const scnXml::Model& model) {
        const bool use_named_model = model.getModelName().present();
        if (use_named_model) {
            const std::string name_from_xml = model.getModelName().get().getName();
            if (name_from_xml == "base") {
                model_in_use = ModelNames::Base;
            } else {
                throw util::xml_scenario_error("Unrecognized model name: " + name_from_xml);
            }
        }
    }

    ~ModelNameProvider() = default;

    ModelNames get_model_name() const { return model_in_use; }

private:
    ModelNames model_in_use = ModelNames::None;
};
}

#endif
