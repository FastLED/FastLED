#pragma once

/// @file bus_priorities.h
/// @brief Per-Bus priority constants for ChannelManager registration.
///
/// `ChannelManager` selects drivers in priority order (higher = preferred).
/// Each `BusTraits<B>::registerWithManager()` reads its priority from this
/// table to keep the values centralized rather than scattered across
/// per-driver headers and the legacy `initChannelDrivers()` body.
///
/// The values mirror the priorities historically defined in
/// `src/platforms/esp/32/drivers/channel_manager_esp32.cpp.hpp` so behavior
/// is unchanged for legacy auto-registered builds.

#include "fl/channels/bus.h"
#include "fl/stl/stdint.h"

// Platform CMSIS/Arduino headers may define SPI / UART / I2S as register
// pointer macros (e.g. Sam3X8E: `#define UART ((Uart*)0x400E0800U)`).
// Those macros expand inside qualified names like `Bus::SPI` below and
// produce a syntax error. bus.h itself uses push_macro/pop_macro around
// its enum definition; once bus.h pops the macros are restored. So we
// must apply the same guard locally here. See fl/channels/bus.h.
#pragma push_macro("UART")
#pragma push_macro("SPI")
#pragma push_macro("I2S")
#undef UART
#undef SPI
#undef I2S

namespace fl {

/// @brief Default priority assigned to a `Bus` when registered with the manager.
///
/// Higher = preferred. The function is `constexpr` so callers can use it in
/// constant expressions if needed. Buses without a registered priority fall
/// back to 0.
constexpr int default_bus_priority(Bus b, fl::u8 = 0) FL_NO_EXCEPT {
    // FORCE-style overrides (FASTLED_ESP32_FORCE_*) are intentionally not
    // applied here -- the legacy initChannelDrivers() retains its own
    // priority-bumping logic for backward compatibility. This table is the
    // baseline used by the new enableDrivers<>() opt-in API.
    return
        b == Bus::FLEX_IO   ? 4  :
        b == Bus::RMT       ? 2  :
        b == Bus::SPI       ? 1  :
        b == Bus::DUAL_SPI  ? 1  :
        b == Bus::QUAD_SPI  ? 1  :
        b == Bus::OCTAL_SPI ? 1  :
        b == Bus::BIT_BANG  ? 0  :
        b == Bus::UART      ? -1 :
        0;
}

}  // namespace fl

#pragma pop_macro("I2S")
#pragma pop_macro("SPI")
#pragma pop_macro("UART")
