// src/fl/channels/detail/validation/platform.h
//
// Platform-specific validation - verify expected drivers are available

#pragma once

#include "fl/stl/vector.h"

// Include DriverInfo definition

namespace fl {

namespace validation {

/// @brief Get list of expected drivers for the current platform
/// @return Vector of expected driver names (e.g., ["PARLIO", "RMT"])
vector<string> getExpectedEngines();

/// @brief Validate that all expected drivers are available
/// @param available_drivers List of available drivers from FastLED
/// @return true if all expected drivers are present, false otherwise
bool validateExpectedEngines(const fl::vector<fl::DriverInfo>& available_drivers);

/// @brief Print validation results (logs warnings/errors)
/// @param available_drivers List of available drivers from FastLED
void printEngineValidation(const fl::vector<fl::DriverInfo>& available_drivers);

}  // namespace validation
}  // namespace fl
