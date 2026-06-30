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

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/log/log.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_1.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/stl/noexcept.h"

// Platform detection for I2S/LCD_SPI drivers
#include "platforms/esp/is_esp.h"

// Include per-driver bus_traits.h (each is internally guarded by its
// FASTLED_ESP32_HAS_* macro and degrades to an empty header when the
// corresponding driver is not enabled on this platform). The bus_traits
// headers transitively include the concrete driver headers we need.
#if FASTLED_ESP32_HAS_PARLIO
// IWYU pragma: begin_keep
#include "parlio/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
// IWYU pragma: begin_keep
#include "spi/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_UART
// IWYU pragma: begin_keep
#include "uart/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_RMT
// Include the appropriate RMT driver (RMT4 or RMT5) based on platform
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
// IWYU pragma: begin_keep
#include "rmt/rmt_5/bus_traits.h"
// IWYU pragma: end_keep
#else
// IWYU pragma: begin_keep
#include "rmt/rmt_4/bus_traits.h"
// IWYU pragma: end_keep
#endif
#endif  // FASTLED_ESP32_HAS_RMT
#if FASTLED_ESP32_HAS_LCD_RGB
// IWYU pragma: begin_keep
#include "lcd_cam/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
// IWYU pragma: begin_keep
#include "i2s/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_I2S
// IWYU pragma: begin_keep
#include "i2s_spi/bus_traits.h"
// IWYU pragma: end_keep
#endif
#if FASTLED_ESP32_HAS_LCD_SPI
// IWYU pragma: begin_keep
#include "lcd_spi/bus_traits.h"
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
static void addSpiHardwareIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
    FL_DBG_F("ESP32: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw1 controllers (priority: 5)
    // Note: SpiHw16 (I2S parallel) replaced by I2S_SPI/LCD_SPI channel drivers
    // ========================================================================
    const auto& hw1Controllers = SpiHw1::getAll();
    FL_DBG_F("ESP32: Found %s SpiHw1 controllers", hw1Controllers.size());

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

            FL_DBG_F("ESP32: Registered unified SPI driver with %s controllers (priority %s)", controllers.size(), maxPriority);
        } else {
            FL_WARN_F("ESP32: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG_F("ESP32: No SPI hardware controllers available");
    }
}

// Each addXxxIfPossible delegates to BusTraits<Bus::X>::instancePtr() so the
// legacy auto-init path and the new fl::enableDrivers<>() opt-in API share the
// SAME singleton driver instance. Without this unification, calling
// enableDriver<Bus::FLEX_IO, 0>() after the legacy auto-init created a *second*
// PARLIO driver instance, which then tried to claim the PARLIO peripheral
// twice. Phase 4 documented this caveat; Phase 5a (this commit) fixes it.

/// @brief Add PARLIO driver if supported by platform
static void addParlioIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_PARLIO
    manager.addDriver(PRIORITY_PARLIO, BusTraits<Bus::FLEX_IO, 0>::instancePtr());
    FL_DBG_F("ESP32: Added PARLIO driver (priority %s)", PRIORITY_PARLIO);
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add LCD RGB driver if supported by platform
static void addLcdRgbIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_LCD_RGB
    auto driver = BusTraits<Bus::FLEX_IO, 1>::instancePtr();
    if (driver) {
        manager.addDriver(PRIORITY_LCD_RGB, driver);
        FL_DBG_F("ESP32: Added LCD_RGB driver (priority %s)", PRIORITY_LCD_RGB);
    } else {
        FL_DBG_F("ESP32-P4: LCD_RGB driver creation failed");
    }
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add SPI driver if supported by platform
static void addSpiIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
    manager.addDriver(PRIORITY_SPI, BusTraits<Bus::SPI>::instancePtr());
    FL_DBG_F("ESP32: Added SPI driver (priority %s)", PRIORITY_SPI);
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add UART driver if supported by platform
static void addUartIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_UART
    // UART driver uses wave8 encoding adapted for UART framing.
    // Available on all ESP32 variants (C3, S3, C6, H2, P4) with ESP-IDF 4.0+.
    // BusTraits<Bus::UART>::instancePtr() also constructs the UartPeripheralEsp
    // dependency inside its UartBusHolder.
    manager.addDriver(PRIORITY_UART, BusTraits<Bus::UART>::instancePtr());
    FL_DBG_F("ESP32: Added UART driver (priority %s)", PRIORITY_UART);
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add RMT driver if supported by platform
static void addRmtIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_RMT
    // BusTraits<Bus::RMT> resolves to ChannelEngineRMT (RMT5) or
    // ChannelEngineRMT4 depending on the build path; the right header is
    // pulled in transitively via the bus_traits.h include above.
    auto driver = BusTraits<Bus::RMT>::instancePtr();
    #if FASTLED_ESP32_RMT5_ONLY_PLATFORM || FASTLED_RMT5
    const char* version = "RMT5";
    #else
    const char* version = "RMT4";
    #endif

    manager.addDriver(PRIORITY_RMT, driver);
    FL_DBG_F("ESP32: Added %s driver (priority %s)", version, PRIORITY_RMT);
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

/// @brief Add I2S_SPI driver if supported by platform (ESP32dev true SPI)
static void addI2sSpiIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_I2S
    auto driver = BusTraits<Bus::FLEX_IO, 0>::instancePtr();
    if (driver) {
        manager.addDriver(PRIORITY_I2S_SPI, driver);
        FL_DBG_F("ESP32: Added I2S_SPI driver (priority %s)", PRIORITY_I2S_SPI);
    } else {
        FL_DBG_F("ESP32: I2S_SPI driver creation deferred (no ESP peripheral yet)");
    }
#else
    (void)manager;
#endif
}

/// @brief Add LCD_SPI driver if supported by platform (ESP32-S3 true SPI)
static void addLcdSpiIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_LCD_SPI
    BusTraits<Bus::FLEX_IO, 0>::registerWithManager();
    auto driver = manager.findDriverByName(fl::string::from_literal("LCD_SPI"));
    if (driver) {
        manager.addDriver(PRIORITY_LCD_SPI, driver);
        FL_DBG_F("ESP32-S3: Added LCD_SPI driver (priority %s)", PRIORITY_LCD_SPI);
    } else {
        FL_DBG_F("ESP32-S3: LCD_SPI driver creation deferred (no ESP peripheral yet)");
    }
#else
    (void)manager;
#endif
}

/// @brief Add LCD_CAM clockless driver if supported (ESP32-S3, replaces misnamed I2S)
static void addLcdClocklessIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_LCD_SPI
    BusTraits<Bus::FLEX_IO, 0>::registerWithManager();
    auto driver = manager.findDriverByName(fl::string::from_literal("LCD_CLOCKLESS"));
    if (driver) {
        manager.addDriver(PRIORITY_LCD_CLOCKLESS, driver);
        FL_DBG_F("ESP32-S3: Added LCD_CLOCKLESS driver (priority %s)", PRIORITY_LCD_CLOCKLESS);
    } else {
        FL_DBG_F("ESP32-S3: LCD_CLOCKLESS driver creation deferred");
    }
#else
    (void)manager;
#endif
}

/// @brief Add I2S LCD_CAM driver if supported by platform
static void addI2sIfPossible(ChannelManager& manager) FL_NO_EXCEPT {
#if FASTLED_ESP32_HAS_I2S_LCD_CAM
    // I2S LCD_CAM driver uses LCD_CAM peripheral via I80 bus (ESP32-S3 only).
    // Experimental driver - uses transpose encoding for parallel LED output.
    BusTraits<Bus::FLEX_IO, 0>::registerWithManager();
    auto driver = manager.findDriverByName(fl::string::from_literal("LCD_CLOCKLESS"));
    if (driver) {
        manager.addDriver(PRIORITY_I2S, driver);
        FL_DBG_F("ESP32-S3: Added I2S LCD_CAM driver (priority %s)", PRIORITY_I2S);
    } else {
        FL_DBG_F("ESP32-S3: I2S LCD_CAM driver creation failed");
    }
#else
    (void)manager;  // Suppress unused parameter warning
#endif
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for ESP32 (no-op, post-#2428).
///
/// Drivers do NOT auto-register with `ChannelManager`. Only the
/// platform-default driver TU (named by the legacy clockless controller's
/// Phase 5b pre-bind via `BusTraits<DefaultBus<Chipset>>::instancePtr()`)
/// links into the binary. Users who need additional drivers at runtime call
/// `fl::enableDrivers<fl::Bus::X, ...>()` or `FastLED.enableAllDrivers()`.
void initChannelDrivers() FL_NO_EXCEPT {
    // Intentionally empty. See `fl::enableDrivers<>()` / `enableAllDrivers()`
    // for the opt-in registration path.
}

} // namespace platforms

} // namespace fl

#endif // FL_IS_ESP32
