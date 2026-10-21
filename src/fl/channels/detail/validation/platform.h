// src/fl/channels/detail/validation/platform.h
//
// Platform-specific validation - verify drivers are registered

#pragma once
#include "fl/stl/noexcept.h"

namespace fl {

namespace validation {

/// @brief Validate that at least one driver is registered with ChannelManager
/// @return true if at least one driver is registered, false if empty
bool validateExpectedEngines() FL_NO_EXCEPT;

/// @brief Print validation results (logs registered drivers and status)
void printEngineValidation() FL_NO_EXCEPT;

}  // namespace validation
}  // namespace fl
