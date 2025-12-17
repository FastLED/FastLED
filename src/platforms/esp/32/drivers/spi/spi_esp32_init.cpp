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
#include "platforms/esp/32/drivers/spi/spi_hw_base.h"  // Common ESP32 SPI definitions
#include "fl/stl/shared_ptr.h"
#include "fl/compiler_control.h"

namespace fl {

// Forward declare the singleton getter functions from spi_hw_1_esp32.cpp
class SpiHw1;  // Full declaration from spi_hw_1.h above
extern fl::shared_ptr<SpiHw1>& getController2();
#if SOC_SPI_PERIPH_NUM > 2
extern fl::shared_ptr<SpiHw1>& getController3();
#endif

namespace {

// ============================================================================
// Centralized Registration via Single Static Constructor
// ============================================================================

/// Register all ESP32 SPI hardware instances at static initialization time
FL_CONSTRUCTOR
static void registerAllESP32SpiInstances() {
    // SpiHw1 (Single-lane): Register SPI2_HOST and SPI3_HOST for single-strip configurations
    // Access singleton instances from spi_hw_1_esp32.cpp
    // Get references and copy to ensure proper shared_ptr type conversion
    fl::shared_ptr<SpiHw1> ctrl2 = getController2();
    SpiHw1::registerInstance(ctrl2);
    #if SOC_SPI_PERIPH_NUM > 2
    fl::shared_ptr<SpiHw1> ctrl3 = getController3();
    SpiHw1::registerInstance(ctrl3);
    #endif

    // Note: ESP32 does not register SpiHw2/4/8/16 instances here.
    // For parallel strips (2+ strips), ESP32 uses the I2S peripheral via SpiHw16,
    // which is registered in the I2S driver initialization code.
    // ESP32's SPI dual/quad/octal modes are designed for QSPI flash, not parallel LED strips.
}

}  // anonymous namespace

}  // namespace fl

#endif  // FL_IS_ESP32
