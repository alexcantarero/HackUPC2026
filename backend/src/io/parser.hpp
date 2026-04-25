#pragma once

#include "../core/types.hpp"
#include <string>

namespace io {

/**
 * @brief Parses the 4 input CSVs from a case directory into a StaticState.
 * @param directoryPath Path to the CaseX folder (e.g. "../../data/input/Case0").
 * @param out           Output struct to populate.
 * @return true on success, false if any file could not be opened.
 */
bool parseStaticState(const std::string& directoryPath, StaticState& out);

/**
 * @brief Writes the solution out to a CSV file.
 * @param filePath Output path for the CSV.
 * @param solution The solution to write.
 * @return true on success, false if the file could not be written.
 */
bool writeSolution(const std::string& filePath, const Solution& solution);

} // namespace io
