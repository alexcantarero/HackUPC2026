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

} // namespace io
