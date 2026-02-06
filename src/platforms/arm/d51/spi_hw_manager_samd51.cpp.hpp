/// @file spi_hw_manager_samd51.cpp.hpp
/// @brief SAMD51 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all SAMD51 SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_samd51.cpp.hpp::initSpiHw2Instances()
/// - spi_hw_4_samd51.cpp.hpp::initSpiHw4Instances()
///
/// Platform support:
/// - SAMD51 (Feather M4, Metro M4): SpiHw2 and SpiHw4 (dual/quad-lane SPI)
/// - Uses SERCOM peripherals with DMA support
///
// allow-include-after-namespace

#if defined(ARDUINO_SAMD51) || defined(__SAMD51__) || defined(__SAMD51J19A__) || \
    defined(__SAMD51J20A__) || defined(__SAMD51G19A__) || \
    defined(ADAFRUIT_FEATHER_M4_EXPRESS) || defined(ADAFRUIT_METRO_M4_EXPRESS)

#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "fl/dbg.h"

namespace fl {
namespace detail {

/// Priority constants for SPI hardware (higher = preferred)
constexpr int PRIORITY_SPI_HW_4 = 7;   // Higher (4-lane quad-SPI)
constexpr int PRIORITY_SPI_HW_2 = 6;   // Lower (2-lane dual-SPI)

/// @brief Register SAMD51 SpiHw2 instances
static void addSpiHw2IfPossible() {
    // Note: SPIDualSAMD51 class is defined in spi_hw_2_samd51.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG("SAMD51: Registering SpiHw2 instances");

    // SAMD51 has multiple SERCOM peripherals available for SPI
    static auto controller0 = fl::make_shared<SPIDualSAMD51>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIDualSAMD51>(1, "SPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    FL_DBG("SAMD51: SpiHw2 instances registered");
}

/// @brief Register SAMD51 SpiHw4 instances
static void addSpiHw4IfPossible() {
    // Note: SPIQuadSAMD51 class is defined in spi_hw_4_samd51.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG("SAMD51: Registering SpiHw4 instances");

    // SAMD51 has multiple SERCOM peripherals available for SPI
    static auto controller0 = fl::make_shared<SPIQuadSAMD51>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIQuadSAMD51>(1, "SPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);

    FL_DBG("SAMD51: SpiHw4 instances registered");
}

}  // namespace detail

namespace platforms {

/// @brief Unified SAMD51 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority (highest to lowest):
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes
/// - SpiHw2 (priority 6): Dual-SPI, 2 parallel lanes
///
/// Platform availability:
/// - SAMD51: Both SpiHw2 and SpiHw4 (via SERCOM)
void initSpiHardware() {
    FL_DBG("SAMD51: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw4IfPossible();  // Priority 7
    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("SAMD51: SPI hardware initialized");
}

}  // namespace platforms
}  // namespace fl

#endif  // SAMD51 platform guards
