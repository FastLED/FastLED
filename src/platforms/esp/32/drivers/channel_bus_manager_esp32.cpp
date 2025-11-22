/// @file channel_bus_manager_esp32.cpp
/// @brief ESP32-specific ChannelBusManager singleton and initialization
///
/// This file provides the ESP32 singleton accessor and configures it with
/// ESP32-specific engines (PARLIO, SPI, RMT) in priority order.
///
/// Priority Order:
/// - PARLIO (100): Highest performance, best timing (ESP32-P4, C6, H2, C5)
/// - SPI (50): Good performance, reliable (ESP32-S3, others)
/// - RMT (10): Fallback, lower performance (all ESP32 variants)

#include "fl/compiler_control.h"
#ifdef ESP32

#include "channel_bus_manager.h"
#include "fl/dbg.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "ftl/shared_ptr.h"

// Include concrete engine implementations
#if FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "spi/channel_engine_spi.h"
#endif
#if FASTLED_ESP32_HAS_PARLIO
#include "parlio/channel_engine_parlio.h"
#endif

namespace fl {

/// @brief Engine priority constants for ESP32
/// @note These are defined here (not in header) to avoid exposing platform-specific details
namespace {
    constexpr int PRIORITY_PARLIO = 100;  ///< Highest (PARLIO engine - ESP32-P4/C6/H2/C5)
    constexpr int PRIORITY_SPI = 50;      ///< Medium (SPI engine)
    constexpr int PRIORITY_RMT = 200;     ///< TESTING: Temporarily highest priority for RMT testing
}

/// @brief Initialize ESP32 channel bus manager with platform-specific engines
///
/// This function is called once on first access to configure the ChannelBusManager
/// singleton with ESP32-specific engines.
static void initializeChannelBusManager() {
    FL_DBG("ESP32: Initializing ChannelBusManager with platform engines");

    auto& manager = ChannelBusManagerSingleton::instance();

    // Add engines (automatically sorted by priority on each insertion)
    #if FASTLED_ESP32_HAS_PARLIO
    manager.addEngine(PRIORITY_PARLIO, fl::make_shared<ChannelEnginePARLIO>());
    FL_DBG("ESP32: Added PARLIO engine (priority " << PRIORITY_PARLIO << ")");
    #endif

    #if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    manager.addEngine(PRIORITY_SPI, fl::make_shared<ChannelEngineSpi>());
    FL_DBG("ESP32: Added SPI engine (priority " << PRIORITY_SPI << ")");
    #endif

    #if FASTLED_RMT5
    manager.addEngine(PRIORITY_RMT, fl::make_shared<ChannelEngineRMT>());
    FL_DBG("ESP32: Added RMT engine (priority " << PRIORITY_RMT << ")");
    #endif

    FL_DBG("ESP32: ChannelBusManager initialization complete");
}

/// @brief Get the initialized ChannelBusManager singleton for ESP32
/// @return Reference to the ChannelBusManager singleton
/// @note First call initializes the manager with ESP32 engines
ChannelBusManager& getChannelBusManager() {
    // Use a static bool to ensure initialization happens exactly once
    static bool initialized = false;
    if (!initialized) {
        initializeChannelBusManager();
        initialized = true;
    }
    return ChannelBusManagerSingleton::instance();
}

} // namespace fl

#endif // ESP32
