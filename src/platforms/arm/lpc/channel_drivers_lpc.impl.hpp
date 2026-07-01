#pragma once

// IWYU pragma: private

/// @file channel_drivers_lpc.impl.hpp
/// @brief LPC platform fragment for `fl::enableAllDrivers()`.
///
/// Registers every channels-API driver shipped for LPC845 today. Tree-shaken
/// when no sketch references `FastLED.enableAllDrivers()` — see
/// `src/platforms/channel_drivers.impl.cpp.hpp` for the dispatch contract
/// and `src/fl/channels/all_drivers.h` for the public API.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845)
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"
#endif

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
#if defined(FL_IS_ARM_LPC_845)
    // SCT+DMA clockless engine (#3459). Registers under Bus::BIT_BANG,
    // matching the LPC `DefaultBus<ClocklessChipset>` resolution from
    // PR #3451.
    BusTraits<Bus::BIT_BANG, 0>::registerWithManager();
#endif
}

}  // namespace platforms
}  // namespace fl
