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

#ifndef Hmod_Output_Continuous
#define Hmod_Output_Continuous

#include "Global.h"
#include "Host/Human.h"
#include "Population.h"
#include <string>

#include <functional>

namespace scnXml {
class Monitoring;
}
namespace OM {
class Population;
namespace mon {
namespace Continuous {

/** Functions dealing with continuous output data.
 *
 * Requirements:
 *  (1) frequency of and which data is output should be controllable
 *  (2) format should be compatible with LiveGraph and (German) Excel.
 */
/** Load XML description of options. If resuming from a checkpoint,
 * append to output; if not, make sure it's not there (on boinc we
 * assume we shouldn't overwrite existing files for security reasons).
 *
 * Callbacks should be registered before init() is called. */
void init(const scnXml::Monitoring &monitoring, bool isCheckpoint);

void checkpoint(ostream &stream);
void checkpoint(istream &stream);

/** In-memory data captured by initCapture()/updateCapture(), parsed by
 * getCapturedData(). Column-major: columns[i] holds all timestep values
 * for columnTitles[i]. */
struct CapturedData {
  std::vector<std::string> columnTitles;
  std::vector<std::vector<double>> columns;
};

/** Like init(), but targets an in-memory buffer instead of opening
 * ctsout.txt
 *
 * For callers that want continuous-output rows returned as data (see
 * getCapturedData()) rather than left on disk (Simulator::run()'s
 * OutputMode::CaptureInMemory)
 *
 * Does not support resuming from a checkpoint: checkpoint/resume stays a
 * WriteFiles-only (CLI) feature, since it's inherently tied to a real file
 * to seek back into (see checkpoint(istream&) below).
 *
 * Callbacks should be registered before this is called, same as init(). */
void initCapture(const scnXml::Monitoring &monitoring);

/** In-memory sibling of update(); only valid to call after initCapture()
 * (not init()). Generates one time step's output into the in-memory
 * buffer instead of ctsOStream, so (like initCapture()) no per-timestep
 * file I/O happens either. */
void updateCapture(Population &population);

/** Parse the buffer written by initCapture()+updateCapture() into
 * column data (one array per continuous-output metric, one row per
 * timestep) for the caller to consume directly (e.g. what the Python
 * bindings hand to pandas)
 *
 * Only valid to call after initCapture() (not init()); throws
 * util::traced_exception otherwise. */
CapturedData getCapturedData();

/** Register a callback function which produces output.
 *
 * This function will be called to generate output, if enabled in XML.
 * It may output more than one statistic, if for example vector output
 * is wanted instead of a single value. It should then title these in
 * the form "name(index)".
 *
 * @param optName	Name of this output, (used for XML on/off options)
 * @param titles	Titles for the output table; each should be preceeded by
 * a \t
 * @param outputCb A callback function, which when called, outputs its
 * data to the passed stream, with each entry preceeded by '\t'.
 */
void registerCallback(string optName, string titles,
                      function<void(ostream &)> f);

void registerCallback(string optName, string titles,
                      function<void(const vector<Host::Human> &, ostream &)> f);

void registerCallback(string optName, string titles,
                      function<void(Population &, ostream &)> f);

/// Generate time-step's output. Called at beginning of time step.
/// Passed population since some callbacks use this to generate output.
void update(Population &population);

} // namespace Continuous
} // namespace mon
} // namespace OM
#endif
