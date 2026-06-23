// IWYU pragma: private

/// @file spi_hw_manager_esp32.cpp.hpp
/// @brief ESP32-specific SPI hardware initialization
///
/// This file provides lazy initialization of ESP32-specific SPI hardware drivers.
/// SpiHw1 (single-lane SPI) is registered on first access via platforms::initSpiHardware().
///
/// Note: SpiHw16 (16-lane I2S parallel mode) has been replaced by
/// I2S_SPI and LCD_SPI channel drivers.

#include "fl/stl/compiler_control.h"
#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

#include "fl/log/log.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/shared/spi_hw_1.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

// SpiHw16 (I2S parallel) removed - replaced by I2S_SPI/LCD_SPI channel drivers

namespace fl {

// Forward declare singleton getters from spi_hw_1_esp32.cpp.hpp
// These are defined in spi_hw_1_esp32.cpp.hpp, included via platforms/esp/32/drivers/spi/_build.hpp
extern fl::shared_ptr<SpiHw1>& getController2() FL_NOEXCEPT;
#if SOC_SPI_PERIPH_NUM > 2
extern fl::shared_ptr<SpiHw1>& getController3() FL_NOEXCEPT;
#endif

namespace detail {

/// @brief SPI hardware priority constants for ESP32
constexpr int PRIORITY_HW_1 = 5;    ///< Standard single-lane SPI

/// @brief Add SpiHw1 (single-lane SPI) if supported
static void addSpiHw1IfPossible() FL_NOEXCEPT {
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

}  // namespace detail

namespace platforms {

/// @brief Initialize SPI hardware for ESP32
///
/// Called lazily on first access to SpiHw1::getAll() or SpiHw16::getAll().
/// Registers platform-specific SPI hardware instances with the appropriate registries.
///
/// Uses a static initialization flag to ensure initialization happens only once,
/// even if called from multiple SpiHw* classes.
void initSpiHardware() FL_NOEXCEPT {
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (sInitialized) {
        return;  // Already initialized
    }
    sInitialized = true;

    FL_DBG("ESP32: Initializing SPI hardware");

    // Register SPI hardware
    // Note: SpiHw16 (I2S parallel) replaced by I2S_SPI/LCD_SPI channel drivers
    detail::addSpiHw1IfPossible();   // Priority 5 (single-lane SPI)

    FL_DBG("ESP32: SPI hardware initialized");
}

}  // namespace platforms

}  // namespace fl

#endif  // FL_IS_ESP32
