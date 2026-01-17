/// @file channel_bus_manager_esp32.cpp
/// @brief ESP32-specific channel engine initialization
///
/// This file provides lazy initialization of ESP32-specific channel engines
/// (LCD_RGB, PARLIO, SPI, RMT, UART, I2S) in priority order. Engines are registered
/// on first access to ChannelBusManager::instance().
///
/// Priority Order:
/// - PARLIO (4): Highest priority, best timing (ESP32-P4, C6, H2, C5)
/// - LCD_RGB (3): High performance, parallel LED output via LCD peripheral (ESP32-P4 only)
/// - SPI (2): Good performance, reliable (ESP32-S3, others)
/// - RMT (1): Fallback, lower performance (all ESP32 variants)
/// - UART (0): Efficient wave8 encoding, automatic start/stop bits
///             (all ESP32 variants, lowest priority because it's unused)
/// - I2S (-1): Experimental, LCD_CAM via I80 bus (ESP32-S3 only)

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
#if FASTLED_ESP32_HAS_UART
#include "uart/channel_engine_uart.h"
#include "uart/uart_peripheral_esp.h"
#endif
#if FASTLED_ESP32_HAS_RMT
// Include the appropriate RMT driver (RMT4 or RMT5) based on platform
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
#include "rmt/rmt_5/channel_engine_rmt.h"
#else
#include "rmt/rmt_4/channel_engine_rmt4.h"
#endif
#endif  // FASTLED_ESP32_HAS_RMT
#if FASTLED_ESP32_HAS_LCD_RGB
#include "lcd_cam/channel_engine_lcd_rgb.h"
#endif
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
#include "i2s/channel_engine_i2s.h"
#endif

namespace fl {

namespace detail {

/// @brief Engine priority constants for ESP32
/// @note Higher values = higher precedence (PARLIO 4 > LCD_RGB 3 > SPI 2 > RMT 1 > UART 0)
constexpr int PRIORITY_PARLIO = 4;   ///< Highest priority (PARLIO engine - ESP32-P4/C6/H2/C5)
constexpr int PRIORITY_LCD_RGB = 3;  ///< High priority (LCD RGB engine - ESP32-P4 only, parallel LED output via LCD peripheral)
constexpr int PRIORITY_SPI = 2;      ///< Medium priority (SPI engine)
constexpr int PRIORITY_RMT = 1;      ///< Low priority (Fallback RMT engine - all ESP32 variants)
constexpr int PRIORITY_UART = 0;     ///< Lowest priority (Beta - UART engine with wave8 encoding)
constexpr int PRIORITY_I2S = -1;     ///< Experimental priority (I2S LCD_CAM engine - ESP32-S3 only)

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

/// @brief Add LCD RGB engine if supported by platform
static void addLcdRgbIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_LCD_RGB
    auto engine = createLcdRgbEngine();
    if (engine) {
        manager.addEngine(PRIORITY_LCD_RGB, engine, "LCD_RGB");
        FL_DBG("ESP32: Added LCD_RGB engine (priority " << PRIORITY_LCD_RGB << ")");
    } else {
        FL_DBG("ESP32-P4: LCD_RGB engine creation failed");
    }
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

/// @brief Add UART engine if supported by platform
static void addUartIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_UART
    // UART engine uses wave8 encoding adapted for UART framing
    // Available on all ESP32 variants (C3, S3, C6, H2, P4) with ESP-IDF 4.0+
    // Uses automatic start/stop bit insertion for efficient waveform generation
    auto peripheral = fl::make_shared<UartPeripheralEsp>();
    auto engine = fl::make_shared<ChannelEngineUART>(peripheral);
    manager.addEngine(PRIORITY_UART, engine, "UART");
    FL_DBG("ESP32: Added UART engine (priority " << PRIORITY_UART << ")");
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

/// @brief Add I2S LCD_CAM engine if supported by platform
static void addI2sIfPossible(ChannelBusManager& manager) {
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
    // I2S LCD_CAM engine uses LCD_CAM peripheral via I80 bus (ESP32-S3 only)
    // Experimental driver - uses transpose encoding for parallel LED output
    auto engine = createI2sEngine();
    if (engine) {
        manager.addEngine(PRIORITY_I2S, engine, "I2S");
        FL_DBG("ESP32-S3: Added I2S LCD_CAM engine (priority " << PRIORITY_I2S << ")");
    } else {
        FL_DBG("ESP32-S3: I2S LCD_CAM engine creation failed");
    }
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

} // namespace detail

namespace platform {

/// @brief Initialize channel engines for ESP32
///
/// Called lazily on first access to ChannelBusManager::instance().
/// Registers platform-specific engines (PARLIO, SPI, UART, RMT) with the bus manager.
void initChannelEngines() {
    FL_DBG("ESP32: Lazy initialization of channel engines");

    auto& manager = channelBusManager();

    // Add engines in priority order (each function handles platform-specific ifdefs)
    detail::addParlioIfPossible(manager);
    detail::addLcdRgbIfPossible(manager);
    detail::addSpiIfPossible(manager);
    detail::addUartIfPossible(manager);
    detail::addRmtIfPossible(manager);
    detail::addI2sIfPossible(manager);

    FL_DBG("ESP32: Channel engines initialized");
}

} // namespace platform

} // namespace fl

#endif // ESP32
