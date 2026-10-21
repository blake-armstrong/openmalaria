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

#include <iostream>
#include <format>
#include <fstream>
#include <gzstream/gzstream.h>

#include "Global.h"
#include "mon/Continuous.h"
#include "mon/Monitoring.h"
#include "interventions/InterventionManager.h"
#include "Host/NeonatalMortality.h"
#include "Clinical/ClinicalModel.h"

#include "checkpoint.h"

namespace OM {

/** @brief checkpointing functions
*
* readCheckpoint/writeCheckpoint prepare to read/write the file,
* and read/write read and write the actual data. */
//@{
int read_checkpoint_num(const string& checkpoint_file_name) {
    ifstream checkpoint_file;
    checkpoint_file.open(checkpoint_file_name, fstream::in);
    int checkpoint_num = 0;
    checkpoint_file >> checkpoint_num;
    checkpoint_file.close();
    if (!checkpoint_file)
        throw util::checkpoint_error("error reading from file \"checkpoint\"");
    return checkpoint_num;
}

void checkpoint(istream& stream, const util::CommandLineOptions& command_line, Clock& clock,
                util::MasterRngs& master_rngs, WithinHost::GenotypeRegistry& genotypes, SimTime& end_time,
                SimTime& est_end_time, Population& population, Transmission::TransmissionModel& transmission) {
    try {
        util::checkpoint::header(stream);
        command_line.checkpoint(stream);
        Host::NeonatalMortality::staticCheckpoint(stream);
        Host::InfectionIncidenceModel::staticCheckpoint(stream);
        Clinical::InfantMortality::staticCheckpoint(stream);
        genotypes.checkpoint(stream);
        mon::Continuous::checkpoint(stream);
        mon::checkpoint(stream);
#ifdef OM_STREAM_VALIDATOR
        util::StreamValidator& stream;
#endif

        clock.checkpoint_interv_time(stream);
        end_time & stream;
        est_end_time & stream;
        transmission & stream;
        population.checkpoint(stream);
        interventions::InterventionManager::checkpoint(stream);
        interventions::InterventionManager::loadFromCheckpoint(population.humans, transmission);

        // read last, because other loads may use random numbers or expect time
        // to be negative
        clock.checkpoint_step_times(stream);
        master_rngs.checkpoint(stream);
    } catch (const util::checkpoint_error& e) { // append " (pos X of Y bytes)"
        const streamoff pos = stream.tellg();
        stream.ignore(numeric_limits<streamsize>::max() - 1); // skip to end of file
        const streamoff size = stream.tellg();
        throw util::checkpoint_error(std::format("{} (pos {} of {} bytes)", e.what(), pos, size));
    }

    stream.ignore(numeric_limits<streamsize>::max() - 1); // skip to end of file
    if (stream.gcount() != 0) {
        throw util::checkpoint_error(std::format("Checkpointing file has {} bytes remaining.\n", stream.gcount()));
    } else if (stream.fail())
        throw util::checkpoint_error("stream read error");
}

void checkpoint(ostream& stream, const util::CommandLineOptions& command_line, Clock& clock,
                util::MasterRngs& master_rngs, WithinHost::GenotypeRegistry& genotypes, SimTime& end_time,
                SimTime& est_end_time, Population& population, Transmission::TransmissionModel& transmission) {
    util::checkpoint::header(stream);
    if (!stream.good())
        throw util::checkpoint_error("Unable to write to file");

    command_line.checkpoint(stream);
    Host::NeonatalMortality::staticCheckpoint(stream);
    Host::InfectionIncidenceModel::staticCheckpoint(stream);
    Clinical::InfantMortality::staticCheckpoint(stream);
    genotypes.checkpoint(stream);
    mon::Continuous::checkpoint(stream);
    mon::checkpoint(stream);
#ifdef OM_STREAM_VALIDATOR
    util::StreamValidator& stream;
#endif

    clock.checkpoint_interv_time(stream);
    end_time & stream;
    est_end_time & stream;
    transmission & stream;
    population.checkpoint(stream);
    interventions::InterventionManager::checkpoint(stream);

    clock.checkpoint_step_times(stream);
    master_rngs.checkpoint(stream);

    if (stream.fail())
        throw util::checkpoint_error("stream write error");
}

void write_checkpoint(const bool started_from_checkpoint, const string& checkpoint_file_name,
                      const util::CommandLineOptions& command_line, Clock& clock, util::MasterRngs& master_rngs,
                      WithinHost::GenotypeRegistry& genotypes, SimTime& end_time, SimTime& est_end_time,
                      Population& population, Transmission::TransmissionModel& transmission) {
    // We alternate between two checkpoints, in case program is closed while writing.
    const int num_checkpoints = 2;

    int old_checkpoint_num = 0, checkpoint_num = 0;
    if (started_from_checkpoint) {
        old_checkpoint_num = read_checkpoint_num(checkpoint_file_name);
        checkpoint_num = mod_nn(old_checkpoint_num + 1, num_checkpoints); // Get next checkpoint number:
    }

    { // Open the next checkpoint file for writing:
        const string name = std::format("{}{}.gz", checkpoint_file_name, checkpoint_num);
        ogzstream out(name.c_str(), ios::out | ios::binary);
        checkpoint(out, command_line, clock, master_rngs, genotypes, end_time, est_end_time, population, transmission);
        out.close();
    }

    { // Indicate which is the latest checkpoint file.
        ofstream checkpoint_file;
        checkpoint_file.open(checkpoint_file_name, ios::out);
        checkpoint_file << checkpoint_num;
        checkpoint_file.close();
        if (!checkpoint_file)
            throw util::checkpoint_error("error writing to file \"checkpoint\"");
    }

    // Truncate the old checkpoint to save disk space, when it existed
    if (old_checkpoint_num != checkpoint_num) {
        const string name = std::format("{}{}.gz", checkpoint_file_name, old_checkpoint_num);
        ofstream out(name, ios::out | ios::binary);
        out.close();
    }
}

void read_checkpoint(const string& checkpoint_file_name, const util::CommandLineOptions& command_line, Clock& clock,
                     util::MasterRngs& master_rngs, WithinHost::GenotypeRegistry& genotypes, SimTime& end_time,
                     SimTime& est_end_time, Population& population, Transmission::TransmissionModel& transmission) {
    const int checkpoint_num = read_checkpoint_num(checkpoint_file_name);

    // Open the latest file
    const string name = std::format("{}{}.gz", checkpoint_file_name, checkpoint_num);
    igzstream in(name.c_str(), ios::in | ios::binary);
    //Note: gzstreams are considered "good" when file not open!
    if (!(in.good() && in.rdbuf()->is_open()))
        throw util::checkpoint_error("Unable to read file");
    checkpoint(in, command_line, clock, master_rngs, genotypes, end_time, est_end_time, population, transmission);
    in.close();

    cerr << clock.in_steps(clock.now()) << "t loaded checkpoint\n";
}

}
