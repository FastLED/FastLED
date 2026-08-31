// IWYU pragma: private

/// @file spi_hw_manager_samd51.cpp.hpp
/// @brief SAMD51 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all SAMD51 SPI hardware initialization into a single
/// manager following the ESP32 channel_manager pattern.
///
/// Platform support:
/// - SAMD51 (Feather M4, Metro M4): SpiHw4 via the native QSPI peripheral
/// - SpiHw2 remains unavailable until the SERCOM implementation drives two lanes
///
// allow-include-after-namespace

#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD51)

#include "platforms/shared/spi_hw_4.h"
#include "fl/log/log.h"

namespace fl {
namespace detail {

/// Priority constant for SPI hardware
constexpr int PRIORITY_SPI_HW_4 = 7;   // Higher (4-lane quad-SPI)

/// @brief Register SAMD51 SpiHw4 instances
static void addSpiHw4IfPossible() {
    // Note: SPIQuadSAMD51 class is defined in spi_hw_4_samd51.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG_F("SAMD51: Registering SpiHw4 instances");

    // SAMD51 has one QSPI peripheral.
    static auto controller0 = fl::make_shared<SPIQuadSAMD51>(0, "QSPI");

    SpiHw4::registerInstance(controller0);

    FL_DBG_F("SAMD51: SpiHw4 instances registered");
}

}  // namespace detail

namespace platforms {

/// @brief Unified SAMD51 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority:
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes
///
/// Platform availability:
/// - SAMD51: SpiHw4 via the single native QSPI peripheral
void initSpiHardware() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    FL_DBG_F("SAMD51: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw4IfPossible();  // Priority 7
    // SPIDualSAMD51 currently transmits only on data0, so it must not be
    // advertised as a two-lane controller. SpiHw2 correctly remains empty.

    FL_DBG_F("SAMD51: SPI hardware initialized");
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_SAMD51
