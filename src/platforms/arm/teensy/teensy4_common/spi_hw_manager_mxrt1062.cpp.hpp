/// @file spi_hw_manager_mxrt1062.cpp.hpp
/// @brief Teensy 4.x (IMXRT1062) SPI Hardware Manager - Unified initialization
///
/// This file consolidates all Teensy 4.x SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_mxrt1062.cpp.hpp::initSpiHw2Instances()
/// - spi_hw_4_mxrt1062.cpp.hpp::initSpiHw4Instances()
///
/// Platform support:
/// - Teensy 4.0/4.1: 3 LPSPI peripherals (SPI, SPI1, SPI2)
/// - Supports dual-mode (SpiHw2) and quad-mode (SpiHw4) via LPSPI WIDTH field
///
// allow-include-after-namespace

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "fl/dbg.h"
#include "fl/log.h"

// Note: SpiHw2MXRT1062 and SpiHw4MXRT1062 classes are defined in:
// - spi_hw_2_mxrt1062.cpp.hpp
// - spi_hw_4_mxrt1062.cpp.hpp
// These files are included by _build.hpp before this file, ensuring the classes are available.

namespace fl {
namespace detail {

/// Priority constants for SPI hardware (higher = preferred)
constexpr int PRIORITY_SPI_HW_4 = 7;   // Higher (4-lane quad-SPI)
constexpr int PRIORITY_SPI_HW_2 = 6;   // Lower (2-lane dual-SPI)

/// @brief Register Teensy 4.x SpiHw2 instances
static void addSpiHw2IfPossible() {
    // Note: SpiHw2MXRT1062 class is defined in spi_hw_2_mxrt1062.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG("Teensy 4.x: Registering SpiHw2 instances");

    // Teensy 4.x has 3 LPSPI peripherals available
    // SPI (bus 0), SPI1 (bus 1), SPI2 (bus 2)
    static auto controller0 = fl::make_shared<SpiHw2MXRT1062>(0, "SPI");
    static auto controller1 = fl::make_shared<SpiHw2MXRT1062>(1, "SPI1");
    static auto controller2 = fl::make_shared<SpiHw2MXRT1062>(2, "SPI2");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);
    SpiHw2::registerInstance(controller2);

    FL_DBG("Teensy 4.x: SpiHw2 instances registered");
}

/// @brief Register Teensy 4.x SpiHw4 instances
static void addSpiHw4IfPossible() {
    // Note: SpiHw4MXRT1062 class is defined in spi_hw_4_mxrt1062.cpp.hpp
    // which is included by _build.hpp before this file
    FL_DBG("Teensy 4.x: Registering SpiHw4 instances");

    // Teensy 4.x has 3 LPSPI peripherals available
    // SPI (bus 0), SPI1 (bus 1), SPI2 (bus 2)
    static auto controller0 = fl::make_shared<SpiHw4MXRT1062>(0, "SPI");
    static auto controller1 = fl::make_shared<SpiHw4MXRT1062>(1, "SPI1");
    static auto controller2 = fl::make_shared<SpiHw4MXRT1062>(2, "SPI2");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);
    SpiHw4::registerInstance(controller2);

    FL_DBG("Teensy 4.x: SpiHw4 instances registered");
}

}  // namespace detail

namespace platform {

/// @brief Unified Teensy 4.x SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority (highest to lowest):
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes
/// - SpiHw2 (priority 6): Dual-SPI, 2 parallel lanes
///
/// Platform availability:
/// - Teensy 4.0/4.1: Both SpiHw2 and SpiHw4 (3 LPSPI controllers)
void initSpiHardware() {
    FL_DBG("Teensy 4.x: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw4IfPossible();  // Priority 7
    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("Teensy 4.x: SPI hardware initialized");
}

}  // namespace platform
}  // namespace fl

#endif  // FL_IS_TEENSY_4X
