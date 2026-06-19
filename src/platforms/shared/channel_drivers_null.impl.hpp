#pragma once

// IWYU pragma: private

/// @file channel_drivers_null.impl.hpp
/// @brief Null channel-driver fragment for `fl::enableAllDrivers()`.

#include "fl/stl/compiler_control.h"

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {}

}  // namespace platforms
}  // namespace fl
