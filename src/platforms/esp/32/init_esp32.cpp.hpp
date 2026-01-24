/// @file init_esp32.cpp
/// @brief ESP32-specific platform initialization implementation
///
/// This file provides the implementation of platform initialization for ESP32.
/// It initializes the channel bus manager and SPI system on first call.

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/init_esp32.h"
#include "platforms/esp/32/init_channel_engine.h"
#include "platforms/shared/spi_bus_manager.h"
#include "fl/dbg.h"

namespace fl {
namespace platforms {

/// @brief Initialize ESP32 platform
///
/// Performs one-time initialization of ESP32-specific subsystems.
/// This function uses a static flag to ensure initialization happens only once.
void init() {
    static bool initialized = false;

    if (initialized) {
        return;  // Already initialized
    }

    FL_DBG("ESP32: Platform initialization starting");

    // Initialize channel bus manager (PARLIO, SPI, RMT, UART engines)
    // Note: This is actually called lazily on first access to ChannelBusManager::instance()
    // but we ensure it's initialized here for predictable behavior
    fl::platform::initChannelEngines();

    // Initialize SPI bus manager
    // Note: The SPIBusManager is lazily initialized on first use, but we can
    // ensure the singleton exists here
    (void)fl::getSPIBusManager();

    initialized = true;
    FL_DBG("ESP32: Platform initialization complete");
}

} // namespace platforms
} // namespace fl

#endif // ESP32
