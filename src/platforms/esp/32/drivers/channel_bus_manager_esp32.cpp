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
// RMT5-only platforms must use new driver architecture
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM
#include "rmt/rmt_5/channel_engine_rmt.h"
#elif FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#else
#include "rmt/rmt_4/channel_engine_rmt4.h"
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "spi/channel_engine_spi.h"
#endif
#if FASTLED_ESP32_HAS_PARLIO
#include "parlio/channel_engine_parlio.h"
#endif

namespace fl {

/// @brief Initialize ESP32 channel bus manager with platform-specific engines
///
/// This function is called automatically during C++ static initialization (before main())
/// to configure the ChannelBusManager singleton with ESP32-specific engines.
namespace detail {
void initializeChannelBusManager() {

    /// @brief Engine priority constants for ESP32
    /// @note Higher values = higher precedence (PARLIO 100 > SPI 50 > RMT 10)
    /// @note These are defined here (not in header) to avoid exposing platform-specific details
    constexpr int PRIORITY_PARLIO = 100;  ///< Highest priority value (PARLIO engine - ESP32-P4/C6/H2/C5)
    constexpr int PRIORITY_SPI = 50;      ///< Medium priority value (SPI engine)
    constexpr int PRIORITY_RMT = 10;      ///< Lowest priority value (Fallback RMT engine - all ESP32 variants)

    FL_DBG("ESP32: Initializing ChannelBusManager with platform engines");

    auto& manager = ChannelBusManagerSingleton::instance();

    // Add engines with string names (automatically sorted by priority on each insertion)
    #if FASTLED_ESP32_HAS_PARLIO
    // ESP32-C6: Fixed by explicitly setting clk_in_gpio_num = -1 (Iteration 2)
    // This tells the driver to use internal clock source instead of external GPIO 0
    manager.addEngine(PRIORITY_PARLIO, fl::make_shared<ChannelEnginePARLIO>(), "PARLIO");
    FL_DBG("ESP32: Added PARLIO engine (priority " << PRIORITY_PARLIO << ")");
    #endif

    #if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    #if defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6 has SPI2 available, but only 1 host vs 2-3 on other ESP32 chips.
    // RMT5 provides better performance and scalability for LED control on this platform.
    // Note: SPI0/SPI1 are reserved for flash operations and cannot be used.
    FL_DBG("ESP32-C6: SPI engine not enabled (only 1 SPI host available, RMT5 preferred)");
    #else
    manager.addEngine(PRIORITY_SPI, fl::make_shared<ChannelEngineSpi>(), "SPI");
    FL_DBG("ESP32: Added SPI engine (priority " << PRIORITY_SPI << ")");
    #endif
    #endif

    // RMT5-only platforms must use new driver architecture
    #if FASTLED_ESP32_RMT5_ONLY_PLATFORM
    manager.addEngine(PRIORITY_RMT, fl::ChannelEngineRMT::create(), "RMT");
    FL_DBG("ESP32: Added RMT5 engine (priority " << PRIORITY_RMT << ") [forced for RMT5-only chip]");
    #elif FASTLED_RMT5
    manager.addEngine(PRIORITY_RMT, fl::ChannelEngineRMT::create(), "RMT");
    FL_DBG("ESP32: Added RMT5 engine (priority " << PRIORITY_RMT << ")");
    #else
    manager.addEngine(PRIORITY_RMT, fl::ChannelEngineRMT4::create(), "RMT");
    FL_DBG("ESP32: Added RMT4 engine (priority " << PRIORITY_RMT << ")");
    #endif

    FL_DBG("ESP32: ChannelBusManager initialization complete");
}

// Register initialization function to run before main()
FL_INIT(initializeChannelBusManager);

} // namespace detail

} // namespace fl

#endif // ESP32
