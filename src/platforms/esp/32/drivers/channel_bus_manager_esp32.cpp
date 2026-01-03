/// @file channel_bus_manager_esp32.cpp
/// @brief ESP32-specific channel engine initialization
///
/// This file provides lazy initialization of ESP32-specific channel engines
/// (PARLIO, SPI, RMT) in priority order. Engines are registered on first
/// access to ChannelBusManager::instance().
///
/// Priority Order:
/// - PARLIO (100): Highest performance, best timing (ESP32-P4, C6, H2, C5)
/// - SPI (50): Good performance, reliable (ESP32-S3, others)
/// - RMT (10): Fallback, lower performance (all ESP32 variants)

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/bus_manager.h"
#include "fl/dbg.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/shared_ptr.h"

// Include concrete engine implementations
#if FASTLED_ESP32_HAS_PARLIO
#include "parlio/channel_engine_parlio.h"
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
#include "spi/channel_engine_spi.h"
#endif
#if FASTLED_ESP32_HAS_RMT
// Include the appropriate RMT driver (RMT4 or RMT5) based on platform
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#else
#include "rmt/rmt_4/channel_engine_rmt4.h"
#endif
#endif  // FASTLED_ESP32_HAS_RMT

namespace fl {

namespace detail {

/// @brief Engine priority constants for ESP32
/// @note Higher values = higher precedence (PARLIO 100 > SPI 50 > RMT 10)
constexpr int PRIORITY_PARLIO = 100;  ///< Highest priority value (PARLIO engine - ESP32-P4/C6/H2/C5)
constexpr int PRIORITY_SPI = 50;      ///< Medium priority value (SPI engine)
constexpr int PRIORITY_RMT = 10;      ///< Lowest priority value (Fallback RMT engine - all ESP32 variants)

/// @brief Add PARLIO engine if supported by platform
static void addParlioIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_PARLIO
    // ESP32-C6: Fixed by explicitly setting clk_in_gpio_num = -1 (Iteration 2)
    // This tells the driver to use internal clock source instead of external GPIO 0
    manager.addEngine(PRIORITY_PARLIO, fl::make_shared<ChannelEnginePARLIO>(), "PARLIO");
    FL_DBG("ESP32: Added PARLIO engine (priority " << PRIORITY_PARLIO << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add SPI engine if supported by platform
static void addSpiIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    #if defined(CONFIG_IDF_TARGET_ESP32C6)
    // ESP32-C6 has SPI2 available, but only 1 host vs 2-3 on other ESP32 chips.
    // RMT5 provides better performance and scalability for LED control on this platform.
    // Note: SPI0/SPI1 are reserved for flash operations and cannot be used.
    FL_DBG("ESP32-C6: SPI engine not enabled (only 1 SPI host available, RMT5 preferred)");
    (void)manager;  // Suppress unused parameter warning
    #else
    manager.addEngine(PRIORITY_SPI, fl::make_shared<ChannelEngineSpi>(), "SPI");
    FL_DBG("ESP32: Added SPI engine (priority " << PRIORITY_SPI << ")");
    #endif
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add RMT engine if supported by platform
static void addRmtIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_RMT
    // Create RMT engine using the appropriate driver (RMT4 or RMT5)
    #if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
    auto engine = fl::ChannelEngineRMT::create();
    const char* version = "RMT5";
    #else
    auto engine = fl::ChannelEngineRMT4::create();
    const char* version = "RMT4";
    #endif

    manager.addEngine(PRIORITY_RMT, engine, "RMT");
    FL_DBG("ESP32: Added " << version << " engine (priority " << PRIORITY_RMT << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

} // namespace detail

namespace platform {

/// @brief Initialize channel engines for ESP32
///
/// Called lazily on first access to ChannelBusManager::instance().
/// Registers platform-specific engines (PARLIO, SPI, RMT) with the bus manager.
void initChannelEngines() {
    FL_DBG("ESP32: Lazy initialization of channel engines");

    auto& manager = channelBusManager();

    // Add engines in priority order (each function handles platform-specific ifdefs)
    detail::addParlioIfPossible(manager);
    detail::addSpiIfPossible(manager);
    detail::addRmtIfPossible(manager);

    FL_DBG("ESP32: Channel engines initialized");
}

} // namespace platform

} // namespace fl

#endif // ESP32
