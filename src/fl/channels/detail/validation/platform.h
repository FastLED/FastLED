// src/fl/channels/detail/validation/platform.h
//
// Platform-specific validation - verify expected engines are available

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"

// Include DriverInfo definition
#include "fl/channels/bus_manager.h"

namespace fl {

namespace validation {

/// @brief Get list of expected engines for the current platform
/// @return Vector of expected engine names (e.g., ["PARLIO", "RMT"])
vector<string> getExpectedEngines();

/// @brief Validate that all expected engines are available
/// @param available_drivers List of available drivers from FastLED
/// @return true if all expected engines are present, false otherwise
bool validateExpectedEngines(const fl::vector<fl::DriverInfo>& available_drivers);

/// @brief Print validation results (logs warnings/errors)
/// @param available_drivers List of available drivers from FastLED
void printEngineValidation(const fl::vector<fl::DriverInfo>& available_drivers);

}  // namespace validation
}  // namespace fl
