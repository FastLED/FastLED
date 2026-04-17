// IWYU pragma: private

/// @file channel_manager_esp32.cpp
/// @brief ESP32-specific channel driver initialization
///
/// This file provides lazy initialization of ESP32-specific channel drivers
/// (LCD_RGB, PARLIO, RMT, I2S, SPI, UART) in priority order. Engines are registered
/// on first access to ChannelManager::instance().
///
/// Priority Order (default, can be overridden with FASTLED_ESP32_FORCE_* macros):
/// - I2S_SPI (10): Native I2S parallel SPI (ESP32dev, true SPI chipsets)
/// - LCD_SPI (10): Native LCD_CAM SPI (ESP32-S3, true SPI chipsets)
/// - PARLIO (4): Highest clockless priority (ESP32-P4, C6, H2, C5)
/// - LCD_RGB (3): High performance, parallel LED output via LCD peripheral (ESP32-P4 only)
/// - RMT (2): Good performance, reliable (all ESP32 variants, recommended default)
/// - I2S (1): Experimental, LCD_CAM via I80 bus (ESP32-S3 only)
/// - SPI (0): Deprioritized due to reliability issues (ESP32-S3, others)
/// - UART (-1): Lowest priority, broken (all ESP32 variants, not recommended)
///
/// Force macros (define before including FastLED.h):
/// - FASTLED_ESP32_FORCE_I2S_LCD_CAM: Boost I2S LCD_CAM to priority 100
/// - FASTLED_ESP32_FORCE_I2S_SPI: Boost I2S parallel SPI to priority 100
/// - FASTLED_ESP32_FORCE_LCD_SPI: Boost LCD_CAM SPI to priority 100
/// Legacy macros FASTLED_USES_ESP32S3_I2S and FASTLED_ESP32_LCD_DRIVER
/// are automatically mapped to FASTLED_ESP32_FORCE_I2S_LCD_CAM.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/channels/manager.h"
#include "fl/system/log.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_1.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/stl/noexcept.h"

// Platform detection for I2S/LCD_SPI drivers
#include "platforms/esp/is_esp.h"

// Include concrete driver implementations
#if FASTLED_ESP32_HAS_PARLIO
// IWYU pragma: begin_keep
#include "parlio/channel_driver_parlio.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
// IWYU pragma: begin_keep
#include "spi/channel_driver_spi.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_UART
// IWYU pragma: begin_keep
#include "uart/channel_driver_uart.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "uart/uart_peripheral_esp.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_RMT
// Include the appropriate RMT driver (RMT4 or RMT5) based on platform
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
// IWYU pragma: begin_keep
#include "rmt/rmt_5/channel_driver_rmt.h"
// IWYU pragma: end_keep
#else
// IWYU pragma: begin_keep
#include "rmt/rmt_4/channel_driver_rmt4.h"
// IWYU pragma: end_keep
#endif
#endif  // FASTLED_ESP32_HAS_RMT
#if FASTLED_ESP32_HAS_LCD_RGB
// IWYU pragma: begin_keep
#include "lcd_cam/channel_driver_lcd_rgb.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
// IWYU pragma: begin_keep
#include "i2s/channel_driver_i2s.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_I2S
// IWYU pragma: begin_keep
#include "i2s_spi/channel_driver_i2s_spi.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_LCD_SPI
// IWYU pragma: begin_keep
#include "lcd_spi/channel_driver_lcd_spi.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "lcd_spi/channel_driver_lcd_clockless.h"
// IWYU pragma: end_keep
#endif

namespace fl {

namespace detail {

/// @brief Engine priority constants for ESP32
/// @note Higher values = higher precedence (PARLIO 4 > LCD_RGB 3 > RMT 2 > I2S 1 > SPI 0 > UART -1)
/// Force macros (FASTLED_ESP32_FORCE_*) boost a driver to priority 100.
constexpr int PRIORITY_FORCE = 100;  ///< Priority used by FASTLED_ESP32_FORCE_* macros
constexpr int PRIORITY_PARLIO = 4;   ///< Highest priority (PARLIO driver - ESP32-P4/C6/H2/C5)
constexpr int PRIORITY_LCD_RGB = 3;  ///< High priority (LCD RGB driver - ESP32-P4 only, parallel LED output via LCD peripheral)
constexpr int PRIORITY_RMT = 2;      ///< Medium priority (RMT driver - all ESP32 variants, recommended default)
#if defined(FASTLED_ESP32_FORCE_I2S_LCD_CAM)
constexpr int PRIORITY_I2S = PRIORITY_FORCE; ///< FORCED highest by FASTLED_ESP32_FORCE_I2S_LCD_CAM
#else
constexpr int PRIORITY_I2S = 1;      ///< Low priority (I2S LCD_CAM driver - ESP32-S3 only, experimental)
#endif
constexpr int PRIORITY_SPI = 0;      ///< Lower priority (SPI driver - deprioritized due to reliability issues)
constexpr int PRIORITY_UART = -1;    ///< Lowest priority (UART driver - broken, not recommended)
// Note: I2S_SPI and LCD_SPI are mutually exclusive -- I2S_SPI targets
// ESP32dev only (original ESP32), LCD_SPI targets ESP32-S3 only.
// Equal priorities are safe since they never coexist on the same platform.
#if defined(FASTLED_ESP32_FORCE_I2S_SPI)
constexpr int PRIORITY_I2S_SPI = PRIORITY_FORCE; ///< FORCED highest by FASTLED_ESP32_FORCE_I2S_SPI
#else
constexpr int PRIORITY_I2S_SPI = 10; ///< Native I2S parallel SPI driver (ESP32dev, true SPI chipsets)
#endif
#if defined(FASTLED_ESP32_FORCE_LCD_SPI)
constexpr int PRIORITY_LCD_SPI = PRIORITY_FORCE; ///< FORCED highest by FASTLED_ESP32_FORCE_LCD_SPI
#else
constexpr int PRIORITY_LCD_SPI = 10; ///< Native LCD_CAM SPI driver (ESP32-S3, true SPI chipsets)
#endif
#if defined(FASTLED_ESP32_FORCE_LCD_CLOCKLESS)
constexpr int PRIORITY_LCD_CLOCKLESS = PRIORITY_FORCE; ///< FORCED highest by FASTLED_ESP32_FORCE_LCD_CLOCKLESS
#else
constexpr int PRIORITY_LCD_CLOCKLESS = 2; ///< LCD_CAM clockless driver (ESP32-S3, outranks legacy I2S)
#endif

/// @brief Add HW SPI drivers if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelManager& manager) FL_NOEXCEPT {
    FL_DBG("ESP32: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw1 controllers (priority: 5)
    // Note: SpiHw16 (I2S parallel) replaced by I2S_SPI/LCD_SPI channel drivers
    // ========================================================================
    const auto& hw1Controllers = SpiHw1::getAll();
    FL_DBG("ESP32: Found " << hw1Controllers.size() << " SpiHw1 controllers");

    for (const auto& ctrl : hw1Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(5);
            names.push_back(ctrl->getName());  // "SPI2" or "SPI3"
        }
    }

    // ========================================================================
    // Create unified adapter with all controllers
    // ========================================================================
    if (!controllers.empty()) {
        auto adapter = SpiChannelEngineAdapter::create(
            controllers,
            priorities,
            names,
            "SPI_UNIFIED"
        );

        if (adapter) {
            // Register with highest priority found
            int maxPriority = priorities[0];
            for (size_t i = 1; i < priorities.size(); i++) {
                if (priorities[i] > maxPriority) {
                    maxPriority = priorities[i];
                }
            }

            manager.addDriver(maxPriority, adapter);

            FL_DBG("ESP32: Registered unified SPI driver with "
                   << controllers.size() << " controllers (priority "
                   << maxPriority << ")");
        } else {
            FL_WARN("ESP32: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG("ESP32: No SPI hardware controllers available");
    }
}

/// @brief Add PARLIO driver if supported by platform
static void addParlioIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_PARLIO
    // ESP32-C6: Fixed by explicitly setting clk_in_gpio_num = -1 (Iteration 2)
    // This tells the driver to use internal clock source instead of external GPIO 0
    manager.addDriver(PRIORITY_PARLIO, fl::make_shared<ChannelDriverPARLIO>());
    FL_DBG("ESP32: Added PARLIO driver (priority " << PRIORITY_PARLIO << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add LCD RGB driver if supported by platform
static void addLcdRgbIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_LCD_RGB
    auto driver = createLcdRgbEngine();
    if (driver) {
        manager.addDriver(PRIORITY_LCD_RGB, driver);
        FL_DBG("ESP32: Added LCD_RGB driver (priority " << PRIORITY_LCD_RGB << ")");
    } else {
        FL_DBG("ESP32-P4: LCD_RGB driver creation failed");
    }
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add SPI driver if supported by platform
static void addSpiIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    manager.addDriver(PRIORITY_SPI, fl::make_shared<ChannelEngineSpi>());
    FL_DBG("ESP32: Added SPI driver (priority " << PRIORITY_SPI << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add UART driver if supported by platform
static void addUartIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_UART
    // UART driver uses wave8 encoding adapted for UART framing
    // Available on all ESP32 variants (C3, S3, C6, H2, P4) with ESP-IDF 4.0+
    // Uses automatic start/stop bit insertion for efficient waveform generation
    auto peripheral = fl::make_shared<UartPeripheralEsp>();
    auto driver = fl::make_shared<ChannelEngineUART>(peripheral);
    manager.addDriver(PRIORITY_UART, driver);
    FL_DBG("ESP32: Added UART driver (priority " << PRIORITY_UART << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add RMT driver if supported by platform
static void addRmtIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_RMT
    // Create RMT driver using the appropriate driver (RMT4 or RMT5)
    #if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
    auto driver = fl::ChannelEngineRMT::create();
    const char* version = "RMT5";
    #else
    auto driver = fl::ChannelEngineRMT4::create();
    const char* version = "RMT4";
    #endif

    manager.addDriver(PRIORITY_RMT, driver);
    FL_DBG("ESP32: Added " << version << " driver (priority " << PRIORITY_RMT << ")");
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add I2S_SPI driver if supported by platform (ESP32dev true SPI)
static void addI2sSpiIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_I2S
    auto driver = createI2sSpiEngine();
    if (driver) {
        manager.addDriver(PRIORITY_I2S_SPI, driver);
        FL_DBG("ESP32: Added I2S_SPI driver (priority " << PRIORITY_I2S_SPI << ")");
    } else {
        FL_DBG("ESP32: I2S_SPI driver creation deferred (no ESP peripheral yet)");
    }
#else
    (void)manager;
#endif
}

/// @brief Add LCD_SPI driver if supported by platform (ESP32-S3 true SPI)
static void addLcdSpiIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_LCD_SPI
    auto driver = createLcdSpiEngine();
    if (driver) {
        manager.addDriver(PRIORITY_LCD_SPI, driver);
        FL_DBG("ESP32-S3: Added LCD_SPI driver (priority " << PRIORITY_LCD_SPI << ")");
    } else {
        FL_DBG("ESP32-S3: LCD_SPI driver creation deferred (no ESP peripheral yet)");
    }
#else
    (void)manager;
#endif
}

/// @brief Add LCD_CAM clockless driver if supported (ESP32-S3, replaces misnamed I2S)
static void addLcdClocklessIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_LCD_SPI
    auto driver = createLcdClocklessEngine();
    if (driver) {
        manager.addDriver(PRIORITY_LCD_CLOCKLESS, driver);
        FL_DBG("ESP32-S3: Added LCD_CLOCKLESS driver (priority " << PRIORITY_LCD_CLOCKLESS << ")");
    } else {
        FL_DBG("ESP32-S3: LCD_CLOCKLESS driver creation deferred");
    }
#else
    (void)manager;
#endif
}

/// @brief Add I2S LCD_CAM driver if supported by platform
static void addI2sIfPossible(ChannelManager& manager) FL_NOEXCEPT {
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
    // I2S LCD_CAM driver uses LCD_CAM peripheral via I80 bus (ESP32-S3 only)
    // Experimental driver - uses transpose encoding for parallel LED output
    auto driver = createI2sEngine();
    if (driver) {
        manager.addDriver(PRIORITY_I2S, driver);
        FL_DBG("ESP32-S3: Added I2S LCD_CAM driver (priority " << PRIORITY_I2S << ")");
    } else {
        FL_DBG("ESP32-S3: I2S LCD_CAM driver creation failed");
    }
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for ESP32
///
/// Called lazily on first access to ChannelManager::instance().
/// Registers platform-specific drivers (PARLIO, SPI, UART, RMT) with the bus manager.
void initChannelDrivers() FL_NOEXCEPT {
    FL_DBG("ESP32: Lazy initialization of channel drivers");

    auto& manager = channelManager();

    // Add drivers in priority order (each function handles platform-specific ifdefs)
    // CRITICAL: Native SPI drivers (priority 10) registered first, then HW SPI fallback
    detail::addI2sSpiIfPossible(manager);       // Priority 10 (native I2S parallel SPI, ESP32dev)
    detail::addLcdSpiIfPossible(manager);       // Priority 10 (native LCD_CAM SPI, ESP32-S3)
    detail::addSpiHardwareIfPossible(manager);  // Priority 5-9 (unified, true SPI fallback)
    detail::addParlioIfPossible(manager);       // Priority 4 (clockless)
    detail::addLcdRgbIfPossible(manager);       // Priority 3 (clockless)
    detail::addSpiIfPossible(manager);          // Priority 0 (clockless-over-SPI)
    detail::addUartIfPossible(manager);         // Priority -1 (clockless)
    detail::addRmtIfPossible(manager);          // Priority 2 (clockless)
    detail::addLcdClocklessIfPossible(manager); // Priority 1 (clockless, ESP32-S3, replaces misnamed I2S)
    detail::addI2sIfPossible(manager);          // Priority 1 (clockless, legacy I2S name)

    FL_DBG("ESP32: Channel drivers initialized");
}

} // namespace platforms

} // namespace fl

#endif // FL_IS_ESP32
