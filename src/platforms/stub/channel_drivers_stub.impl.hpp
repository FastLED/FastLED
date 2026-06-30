#pragma once

// IWYU pragma: private

/// @file channel_drivers_stub.impl.hpp
/// @brief Stub/WASM channel-driver fragment for `fl::enableAllDrivers()`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/stub/bus_traits.h"            // ok platform headers // IWYU pragma: keep

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDrivers<fl::Bus::BIT_BANG>();
}

}  // namespace platforms
}  // namespace fl
