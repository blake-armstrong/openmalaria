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

#ifndef Hmod_Simulator
#define Hmod_Simulator

#include "Global.h"
#include "mon/Continuous.h"
#include "mon/Monitoring.h"

#include <optional>
#include <string>
#include <vector>

namespace OM {
namespace Simulator {

// assign a run's monitoring output
enum class OutputMode {
  WriteFiles,      // write to disk (output.txt/ctsout.txt)
  CaptureInMemory, // collect rows/columns into RunResult instead
};

// assign where the scenario XML comes from
struct ScenarioSource {
  bool isPath = true;
  std::string path; // used when isPath == true: util::loadScenarioFromFile()
  std::string xml;  // used when isPath == false: util::loadScenarioFromXml()
};

struct RunConfig {
  ScenarioSource scenario;
  OutputMode outputMode = OutputMode::WriteFiles;
  // Overrides the scenario XML's model/parameters/computationParameters/@iseed
  // when set.
  std::optional<int> seedOverride;
  // TODO: checkpointing
};

// populated only when outputMode == CaptureInMemory
struct RunResult {
  std::vector<mon::SurveyRow> surveyRows;
  mon::Continuous::CapturedData continuous;
};

// loads scenario XML and runs simulation
// kept separate from CLI logic
RunResult run(const RunConfig &config);

} // namespace Simulator
} // namespace OM
#endif
