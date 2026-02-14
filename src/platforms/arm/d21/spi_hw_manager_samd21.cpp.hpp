// IWYU pragma: private

/// @file spi_hw_manager_samd21.cpp.hpp
/// @brief SAMD21 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all SAMD21 SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_samd21.cpp.hpp::initSpiHw2Instances()
///
/// Platform support:
/// - SAMD21 (Arduino Zero, Feather M0): SpiHw2 only (dual-lane SPI)
/// - Uses SERCOM peripherals with DMA support
///
// allow-include-after-namespace

#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD21)

#include "platforms/shared/spi_hw_2.h"
#include "fl/dbg.h"

namespace fl {
namespace detail {

/// Priority constant for SPI hardware
constexpr int PRIORITY_SPI_HW_2 = 6;   // Dual-SPI (only mode available on SAMD21)

/// @brief Register SAMD21 SpiHw2 instances
static void addSpiHw2IfPossible() {
    // Note: SPIDualSAMD21 class is defined in spi_hw_2_samd21.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG("SAMD21: Registering SpiHw2 instances");

    // SAMD21 typically has 2-3 SERCOM peripherals available for SPI
    static auto controller0 = fl::make_shared<SPIDualSAMD21>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIDualSAMD21>(1, "SPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    FL_DBG("SAMD21: SpiHw2 instances registered");
}

}  // namespace detail

namespace platforms {

/// @brief Unified SAMD21 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers.
///
/// Platform availability:
/// - SAMD21: SpiHw2 only (dual-lane via SERCOM)
void initSpiHardware() {
    FL_DBG("SAMD21: Initializing SPI hardware");

    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("SAMD21: SPI hardware initialized");
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_SAMD21
