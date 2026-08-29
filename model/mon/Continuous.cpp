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

#include "mon/Continuous.h"
#include "schema/monitoring.h"
#include "util/CommandLine.h"
#include "util/UnitParse.h"
#include "util/errors.h"

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace OM {
namespace mon {
namespace Continuous {
using util::xml_scenario_error;

/// This is used to output some statistics in a tab-deliminated-value file.
/// (It used to be csv, but German Excel can't open csv directly.)
fstream ctsOStream;

// Serializable output position used when resuming from a checkpoint.
streamoff streamOffset;

map<string, tuple<string, std::function<void(Population &, ostream &)>>>
    registered;

// List that we report.
vector<tuple<string, std::function<void(Population &, ostream &)>>> toReport;

SimTime ctsPeriod = sim::zero();

bool duringInit = false;

// In-memory sibling of ctsOStream, used only by initCapture()/
// updateCapture()/getCapturedData()
unique_ptr<ostringstream> captureStream;
bool captureMode = false;

namespace {

// Shared by init() and initCapture(): reads ctsPeriod/duringInit/toReport
// from the scenario XML. Returns false if continuous output is disabled
// by the scenario XML (ctsPeriod is left at sim::zero() in that case;
// callers should return without opening a stream).
bool configureFromXml(const scnXml::Monitoring &monitoring) {
  const scnXml::Monitoring::ContinuousOptional &ctsOpt =
      monitoring.getContinuous();
  if (!ctsOpt.present()) {
    ctsPeriod = sim::zero();
    return false;
  }
  const auto &options = ctsOpt.get();
  try {
    // NOTE: if changing XSD, this should not have a default unit:
    ctsPeriod =
        UnitParse::readShortDuration(options.getPeriod(), UnitParse::STEPS);
    if (ctsPeriod < sim::oneTS())
      throw util::format_error("must be >= 1 time step");
  } catch (const util::format_error &e) {
    throw xml_scenario_error(
        string("monitoring/continuous/period: ").append(e.message()));
  }

  if (options.getDuringInit().present())
    duringInit = options.getDuringInit().get();

  for (const auto &option : options.getOption()) {
    auto registeredOutput = registered.find(option.getName());
    if (registeredOutput == registered.end()) {
      throw xml_scenario_error("monitoring.continuous: no output " +
                               string(option.getName()));
    }
    if (option.getValue())
      toReport.push_back(registeredOutput->second);
  }
  return true;
}

// Shared by update() and updateCapture(): decides whether this timestep
// produces a continuous-output row and, if so, writes it (without a
// trailing newline) to `target`. Returns false (leaving `target` untouched)
// if output is disabled or not yet due this timestep. Callers handle their
// own target-specific bookkeeping afterwards (flush()/streamOffset for the
// file path; nothing extra for the in-memory path).
bool writeRow(ostream &target, Population &population) {
  if (ctsPeriod == sim::zero())
    return false; // output disabled
  if (!duringInit) {
    if (sim::intervTime() < sim::zero() ||
        mod_nn(sim::intervTime(), ctsPeriod) != sim::zero())
      return false;
  } else {
    if (mod_nn(sim::now(), ctsPeriod) != sim::zero())
      return false;
    target << sim::inSteps(sim::now()) << '\t';
  }

  if (duringInit && sim::intervTime() < sim::zero()) {
    target << "nan";
  } else {
    // NOTE: we could switch this to output dates, but (1) it would be
    // breaking change and (2) it may be harder to use.
    target << sim::inSteps(sim::intervTime());
  }
  for (const auto &report : toReport)
    std::get<1>(report)(population, target);
  return true;
}

} // namespace

/* Initialise: enable outputs registered and requested in XML.
 * Search for Continuous::registerCallback to see outputs available. */
void init(const scnXml::Monitoring &monitoring, bool isCheckpoint) {
  if (!configureFromXml(monitoring))
    return;

  const string filename = util::CommandLine::getCtsoutName();

  if (isCheckpoint) {
    // When loading a check-point, we resume reporting to this file.
    ctsOStream.open(filename, ios::binary | ios::ate | ios::in | ios::out);
    if (ctsOStream.fail())
      throw util::checkpoint_error("Continuous: resume error (no file)");
    return; // checkpoint() sets the saved output position later
  }

  ctsOStream.open(filename, ios::binary | ios::out);
  ctsOStream << "##\t##\n"; // live-graph needs a delimiter specifier when it is
                            // not a comma
  if (duringInit)
    ctsOStream << "simulation time\t";
  ctsOStream << "timestep"; // TODO: change to days or remove or leave?
  for (const auto &report : toReport)
    ctsOStream << std::get<0>(report);
  ctsOStream << '\n' << flush;
  streamOffset = ctsOStream.tellp();
}

void checkpoint(ostream &stream) {
  if (ctsPeriod == sim::zero())
    return; // output disabled

  streamOffset & stream;
}
void checkpoint(istream &stream) {
  if (ctsPeriod == sim::zero())
    return; // output disabled

  /* We attempt to resume output correctly after a reload by recording
   * the last position, and relocating there.
   *
   * (Keeping results in memory until end of sim would be another,
   * slightly safer, option, but loses real-time output.) */
  streamOffset & stream;
  // We skip back to the last write-point, so anything written after the
  // last checkpoint will be repeated:
  ctsOStream.seekp(streamOffset, ios_base::beg);

  if (ctsOStream.fail())
    throw util::checkpoint_error("Continuous: resume error (bad pos/file)");
}

void registerCallback(string optName, string titles,
                      function<void(ostream &)> f) {
  assert(registered.count(optName) == 0); // name clash/registered twice?
  function<void(const Population &, ostream &)> _f =
      [f](const Population &, ostream &ostream) { f(ostream); };
  registered[optName] = {titles, _f};
}

void registerCallback(
    string optName, string titles,
    function<void(const vector<Host::Human> &, ostream &)> f) {
  assert(registered.count(optName) == 0); // name clash/registered twice?
  function<void(const Population &, ostream &)> _f =
      [f](const Population &p, ostream &ostream) { f(p.humans, ostream); };
  registered[optName] = {titles, _f};
}

void registerCallback(string optName, string titles,
                      function<void(Population &, ostream &)> f) {
  assert(registered.count(optName) == 0); // name clash/registered twice?
  registered[optName] = {titles, f};
}

void update(Population &population) {
  if (!writeRow(ctsOStream, population))
    return;

  // We must flush often to avoid temporarily outputting partial lines
  // (resulting in incorrect real-time graphs).
  ctsOStream << '\n' << flush;

  streamOffset = ctsOStream.tellp();
}

// ----- in-memory capture path -----

void initCapture(const scnXml::Monitoring &monitoring) {
  captureMode = true;
  if (!configureFromXml(monitoring))
    return;

  captureStream = make_unique<ostringstream>();
  if (duringInit)
    *captureStream << "simulation time\t";
  *captureStream << "timestep"; // TODO: change to days or remove or leave?
  for (const auto &report : toReport)
    *captureStream << std::get<0>(report);
  *captureStream << '\n';
}

void updateCapture(Population &population) {
  // captureStream is only null when continuous output is disabled.
  // Check before dereferencing, since writeRow()'s own
  // ctsPeriod==zero() guard runs too late to protect the `*captureStream`
  // argument expression below.
  if (!captureStream)
    return;
  if (!writeRow(*captureStream, population))
    return;
  *captureStream << '\n';
}

CapturedData getCapturedData() {
  if (!captureMode)
    throw TRACED_EXCEPTION_DEFAULT(
        "Continuous::getCapturedData() called without a prior initCapture()");

  CapturedData result;
  if (!captureStream)
    return result; // continuous output disabled in this scenario's XML

  istringstream lines(captureStream->str());
  string headerLine;
  if (!std::getline(lines, headerLine))
    return result;

  istringstream headerStream(headerLine);
  string title;
  while (std::getline(headerStream, title, '\t'))
    result.columnTitles.push_back(title);
  result.columns.resize(result.columnTitles.size());

  string bodyLine;
  while (std::getline(lines, bodyLine)) {
    if (bodyLine.empty())
      continue;
    istringstream fieldStream(bodyLine);
    string field;
    size_t col = 0;
    while (std::getline(fieldStream, field, '\t')) {
      if (col < result.columns.size())
        result.columns[col].push_back(std::stod(field));
      ++col;
    }
  }
  return result;
}
} // namespace Continuous
} // namespace mon
} // namespace OM
