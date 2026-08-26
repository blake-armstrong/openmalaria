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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

// parse the xml scenario file
//

#ifndef Hmod_util_DocumentLoader
#define Hmod_util_DocumentLoader

#include "Global.h"
#include <memory>
#include <schema/scenario.h>
#include <string>

namespace OM {
namespace util {
static const int SCHEMA_VERSION = 49;

/** Load a scenario from a file on disk at path `lXmlFile`. */
std::unique_ptr<scnXml::Scenario> loadScenarioFromFile(std::string lXmlFile);

/** Load a scenario from XML already held in memory (`xmlContent`), without
 * ever routing through disk
 *
 * For callers that already have scenario XML as a string: built
 * programmatically (e.g., templating/editing a base scenario per iteration
 * of a parameter sweep), fetched from a database, etc. Avoids the
 * round-trip cost of writing that string to disk purely so
 * loadScenarioFromFile() has a path to open.
 *
 * NOTE: schema lookup still resolves relative to the working directory
 * (as loadScenarioFromFile() also does; see its .cpp comment), not
 * relative to anything about where this XML content originated. */
std::unique_ptr<scnXml::Scenario>
loadScenarioFromXml(const std::string &xmlContent);
} // namespace util
} // namespace OM

#endif
