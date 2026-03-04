// src/fl/channels/detail/validation/platform.h
//
// Platform-specific validation - verify drivers are registered

#pragma once

namespace fl {

namespace validation {

/// @brief Validate that at least one driver is registered with ChannelManager
/// @return true if at least one driver is registered, false if empty
bool validateExpectedEngines();

/// @brief Print validation results (logs registered drivers and status)
void printEngineValidation();

}  // namespace validation
}  // namespace fl
