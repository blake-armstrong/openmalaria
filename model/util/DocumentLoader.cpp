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

#include "util/DocumentLoader.h"
#include "util/errors.h"
#include <fstream>
#include <sstream>

namespace OM {
namespace util {

namespace {

// Shared by loadScenarioFromFile() and loadScenarioFromXml(): checks the
// parsed scenario's schema version against SCHEMA_VERSION, warning (not
// aborting) on an old version and throwing on a newer, unsupported one.
// `sourceDescription` is only used to identify the scenario in the warning
// message (the scenario file path, or "in-memory scenario").
void checkSchemaVersion(const scnXml::Scenario &scenario,
                        const std::string &sourceDescription) {
  const int scenarioVersion = scenario.getSchemaVersion();

  if (scenarioVersion < SCHEMA_VERSION) {
    // Don't bother aborting. Mostly if something really is incompatible
    // loading will not succeed anyway.
    std::cerr << "Warning: " << sourceDescription
              << " uses an old schema version (latest is " << SCHEMA_VERSION
              << ")." << endl;
  } else if (scenarioVersion > SCHEMA_VERSION)
    throw util::xml_scenario_error("Error: new schema version unsupported");
}

} // namespace

std::unique_ptr<scnXml::Scenario> loadScenarioFromFile(std::string lXmlFile) {
  // Opening by filename causes a schema lookup in the scenario file's dir,
  // which does always work. Opening with a stream uses the working directory.

  // Note that the schema location can be set manually by passing properties,
  // but we won't necessarily have the right schema version associated with
  // the XML file in that case.
  std::ifstream fileStream(lXmlFile.c_str(), ios::binary);
  if (!fileStream.good()) {
    std::string msg = "Error: unable to open " + lXmlFile;
    throw util::xml_scenario_error(msg);
  }
  std::unique_ptr<scnXml::Scenario> scenario =
      scnXml::parseScenario(fileStream);
  fileStream.close();

  checkSchemaVersion(*scenario, lXmlFile);

  return scenario;
}

std::unique_ptr<scnXml::Scenario>
loadScenarioFromXml(const std::string &xmlContent) {
  std::istringstream stream(xmlContent);
  std::unique_ptr<scnXml::Scenario> scenario = scnXml::parseScenario(stream);

  checkSchemaVersion(*scenario, "in-memory scenario");

  return scenario;
}
} // namespace util
} // namespace OM
