/// @file spi_hw_manager_nrf52.cpp.hpp
/// @brief nRF52 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all nRF52 SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_nrf52.cpp.hpp::initSpiHw2Instances()
/// - spi_hw_4_nrf52.cpp.hpp::initSpiHw4Instances()
///
/// Platform support:
/// - nRF52832/nRF52840 (Adafruit Feather nRF52): SpiHw2 and SpiHw4
/// - Uses Timer/PPI peripherals for synchronized multi-lane SPI
///
// allow-include-after-namespace

#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_ADAFRUIT)

#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/arm/nrf52/spi_hw_2_nrf52.h"
#include "platforms/arm/nrf52/spi_hw_4_nrf52.h"
#include "fl/dbg.h"

namespace fl {
namespace detail {

/// Priority constants for SPI hardware (higher = preferred)
constexpr int PRIORITY_SPI_HW_4 = 7;   // Higher (4-lane quad-SPI)
constexpr int PRIORITY_SPI_HW_2 = 6;   // Lower (2-lane dual-SPI)

/// @brief Register nRF52 SpiHw2 instances
static void addSpiHw2IfPossible() {
    FL_DBG("nRF52: Registering SpiHw2 instances");

    // nRF52 has multiple Timer/PPI combinations available
    static auto controller0 = fl::make_shared<SPIDualNRF52>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIDualNRF52>(1, "SPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    FL_DBG("nRF52: SpiHw2 instances registered");
}

/// @brief Register nRF52 SpiHw4 instances
static void addSpiHw4IfPossible() {
    FL_DBG("nRF52: Registering SpiHw4 instances");

    // nRF52 has multiple Timer/PPI combinations available
    static auto controller0 = fl::make_shared<SPIQuadNRF52>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIQuadNRF52>(1, "SPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);

    FL_DBG("nRF52: SpiHw4 instances registered");
}

}  // namespace detail

namespace platform {

/// @brief Unified nRF52 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority (highest to lowest):
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes (Timer/PPI-based)
/// - SpiHw2 (priority 6): Dual-SPI, 2 parallel lanes (Timer/PPI-based)
///
/// Platform availability:
/// - nRF52832/nRF52840: Both SpiHw2 and SpiHw4 (via Timer/PPI peripherals)
void initSpiHardware() {
    FL_DBG("nRF52: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw4IfPossible();  // Priority 7
    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("nRF52: SPI hardware initialized");
}

}  // namespace platform
}  // namespace fl

#endif  // nRF52 platform guards
