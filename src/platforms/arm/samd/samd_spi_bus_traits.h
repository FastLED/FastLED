#pragma once

// IWYU pragma: private

/// @file samd_spi_bus_traits.h
/// @brief `BusTraits<Bus::SPI, 0>` for SAMD21/SAMD51 (#4593).
///
/// The SERCOM hardware SPI engine ("SPI_UNIFIED", a SpiChannelEngineAdapter
/// over SpiHw2) is registered by the platform's `initChannelDrivers()`
/// (init_channel_driver_samd21/51.cpp.hpp). Legacy `addLeds<APA102, ...>()`
/// routes through SlimSpiBridgeController, which calls
/// `BusTraits<kBus>::registerWithManager()`; here that simply forces the
/// lazy platform init so SPI_UNIFIED is present when the frame is dispatched.

#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD)

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"

namespace fl {

template<> struct BusTraits<Bus::SPI, 0> {
    static void registerWithManager() FL_NO_EXCEPT {
        (void)ChannelManager::instance();
    }
};

template<> struct BusSupports<Bus::SPI, SpiChipsetConfig, 0> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_SAMD
