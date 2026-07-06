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
#if defined(FASTLED_LPC_UART_DMA)
#include "platforms/arm/lpc/drivers/uart_dma/bus_traits.h"
#endif
#if defined(FASTLED_LPC_PWM_DMA)
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"
#else
#include "platforms/shared/bitbang/bus_traits.h"
#endif
#endif

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
#if defined(FL_IS_ARM_LPC_845)
#if defined(FASTLED_LPC_UART_DMA)
    BusTraits<Bus::UART, 0>::registerWithManager();
#endif
    // Register the LPC clockless fallback bus as SCT+DMA when
    // FASTLED_LPC_PWM_DMA is enabled, otherwise as shared bit-bang.
    BusTraits<Bus::BIT_BANG, 0>::registerWithManager();
#endif
}

}  // namespace platforms
}  // namespace fl
