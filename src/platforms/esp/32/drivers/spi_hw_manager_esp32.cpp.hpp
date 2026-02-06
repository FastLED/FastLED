/// @file spi_hw_manager_esp32.cpp.hpp
/// @brief ESP32-specific SPI hardware initialization
///
/// This file provides lazy initialization of ESP32-specific SPI hardware drivers
/// (SpiHw1, SpiHw16) in priority order. Drivers are registered on first access
/// via platforms::initSpiHardware().
///
/// Priority Order:
/// - SpiHw16 (9): Highest priority, 16-lane I2S parallel mode (ESP32, ESP32-S2 only)
/// - SpiHw1 (5): Standard single-lane SPI (all ESP32 variants)

#include "fl/compiler_control.h"
#ifdef FL_IS_ESP32

#include "fl/dbg.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/shared/spi_hw_1.h"
#include "fl/stl/shared_ptr.h"

// Include SpiHw16 only on platforms that support I2S parallel mode
// ESP32-S3 and newer use LCD_CAM peripheral instead of I2S parallel mode
#if defined(ESP32) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) && !defined(FL_IS_ESP_32C3) && !defined(FL_IS_ESP_32C5) && !defined(FL_IS_ESP_32C6) && !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4)
#include "platforms/shared/spi_hw_16.h"
#include "platforms/esp/32/drivers/i2s/spi_hw_i2s_esp32.h"
#define FASTLED_ESP32_HAS_SPI_HW_16 1
#else
#define FASTLED_ESP32_HAS_SPI_HW_16 0
#endif

namespace fl {

// Forward declare singleton getters from spi_hw_1_esp32.cpp.hpp
// These are defined in spi_hw_1_esp32.cpp.hpp, included via platforms/esp/32/drivers/spi/_build.hpp
extern fl::shared_ptr<SpiHw1>& getController2();
#if SOC_SPI_PERIPH_NUM > 2
extern fl::shared_ptr<SpiHw1>& getController3();
#endif

namespace detail {

/// @brief SPI hardware priority constants for ESP32
/// @note Higher values = higher precedence (SpiHw16: 9, SpiHw1: 5)
constexpr int PRIORITY_HW_16 = 9;   ///< Highest (16-lane I2S parallel mode)
constexpr int PRIORITY_HW_1 = 5;    ///< Standard single-lane SPI

/// @brief Add SpiHw1 (single-lane SPI) if supported
static void addSpiHw1IfPossible() {
#ifdef FL_IS_ESP32
    FL_DBG("ESP32: Registering SpiHw1 controllers");

    // Register SPI2_HOST controller
    fl::shared_ptr<SpiHw1> ctrl2 = getController2();
    if (ctrl2) {
        SpiHw1::registerInstance(ctrl2);
        FL_DBG("ESP32: Registered SpiHw1 controller (SPI2)");
    }

    // Register SPI3_HOST controller if available (ESP32, ESP32-S2, ESP32-S3)
#if SOC_SPI_PERIPH_NUM > 2
    fl::shared_ptr<SpiHw1> ctrl3 = getController3();
    if (ctrl3) {
        SpiHw1::registerInstance(ctrl3);
        FL_DBG("ESP32: Registered SpiHw1 controller (SPI3)");
    }
#endif

    FL_DBG("ESP32: SpiHw1 registration complete");
#endif
}

/// @brief Add SpiHw16 (16-lane I2S parallel mode) if supported
static void addSpiHw16IfPossible() {
#if FASTLED_ESP32_HAS_SPI_HW_16
    FL_DBG("ESP32: Registering SpiHw16 (I2S parallel mode)");

    // Create single I2S0 controller instance
    static auto i2s0_controller = fl::make_shared<SpiHwI2SESP32>(0);
    SpiHw16::registerInstance(i2s0_controller);

    FL_DBG("ESP32: Registered SpiHw16 controller (I2S0)");
#endif
}

}  // namespace detail

namespace platforms {

/// @brief Initialize SPI hardware for ESP32
///
/// Called lazily on first access to SpiHw1::getAll() or SpiHw16::getAll().
/// Registers platform-specific SPI hardware instances with the appropriate registries.
///
/// Uses a static initialization flag to ensure initialization happens only once,
/// even if called from multiple SpiHw* classes.
void initSpiHardware() {
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (sInitialized) {
        return;  // Already initialized
    }
    sInitialized = true;

    FL_DBG("ESP32: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw16IfPossible();  // Priority 9 (16-lane I2S)
    detail::addSpiHw1IfPossible();   // Priority 5 (single-lane SPI)

    FL_DBG("ESP32: SPI hardware initialized");
}

}  // namespace platforms

}  // namespace fl

#endif  // FL_IS_ESP32
