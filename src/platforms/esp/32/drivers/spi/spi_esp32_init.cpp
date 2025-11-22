/// @file spi_esp32_init.cpp
/// @brief Centralized initialization for ESP32 SPI controllers (1/2/4/8-way)
///
/// This file contains a single static constructor that registers all SPI hardware
/// instances for ESP32 platforms. This replaces the individual static constructors
/// that were previously scattered across spi_hw_1_esp32.cpp, spi_hw_2_esp32.cpp,
/// spi_hw_4_esp32.cpp, and spi_hw_8_esp32.cpp.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "platforms/shared/spi_hw_1.h"
#include "fl/shared_ptr.h"
#include "fl/compiler_control.h"

// Include soc_caps.h if available (ESP-IDF 4.0+)
#include "fl/has_include.h"
#if FL_HAS_INCLUDE(<soc/soc_caps.h>)
  #include "soc/soc_caps.h"
#endif

// Determine SPI3_HOST availability using SOC capability macro
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

namespace fl {

// Forward declaration of the ESP32 Single-SPI implementation class
class SPISingleESP32;

namespace {

// ============================================================================
// SpiHw1 (Single-lane SPI) Instance Management
// ============================================================================

// Singleton getters for SpiHw1 controller instances
// These reference the SPISingleESP32 instances defined in spi_hw_1_esp32.cpp
extern fl::shared_ptr<SPISingleESP32>& getController2();
#if SOC_SPI_PERIPH_NUM > 2
extern fl::shared_ptr<SPISingleESP32>& getController3();
#endif

// ============================================================================
// Centralized Registration via Single Static Constructor
// ============================================================================

/// Register all ESP32 SPI hardware instances at static initialization time
FL_CONSTRUCTOR
static void registerAllESP32SpiInstances() {
    // SpiHw1 (Single-lane): Register SPI2_HOST and SPI3_HOST for single-strip configurations
    SpiHw1::registerInstance(getController2());
    #if SOC_SPI_PERIPH_NUM > 2
    SpiHw1::registerInstance(getController3());
    #endif

    // Note: ESP32 does not register SpiHw2/4/8/16 instances here.
    // For parallel strips (2+ strips), ESP32 uses the I2S peripheral via SpiHw16,
    // which is registered in the I2S driver initialization code.
    // ESP32's SPI dual/quad/octal modes are designed for QSPI flash, not parallel LED strips.
}

}  // anonymous namespace

}  // namespace fl

#endif  // FL_IS_ESP32
