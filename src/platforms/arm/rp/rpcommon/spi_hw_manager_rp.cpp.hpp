/// @file spi_hw_manager_rp.cpp.hpp
/// @brief RP2040/RP2350 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all RP2040/RP2350 SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_rp.cpp.hpp::initSpiHw2Instances()
/// - spi_hw_4_rp.cpp.hpp::initSpiHw4Instances()
/// - spi_hw_8_rp.cpp.hpp::initSpiHw8Instances()
///
/// Platform support:
/// - RP2040: 2 PIO blocks, 8 state machines total (PIO-based SPI)
/// - RP2350: 3 PIO blocks, 12 state machines total (PIO-based SPI)
/// - All lane counts use PIO for flexible pin assignment
///
// allow-include-after-namespace

#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)

#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "fl/dbg.h"

// Note: SPIDualRP2040, SPIQuadRP2040, and SpiHw8RP2040 classes are defined in:
// - spi_hw_2_rp.cpp.hpp
// - spi_hw_4_rp.cpp.hpp
// - spi_hw_8_rp.cpp.hpp
// These files are included by _build.hpp before this file, ensuring the classes are available.

namespace fl {
namespace detail {

/// Priority constants for SPI hardware (higher = preferred)
constexpr int PRIORITY_SPI_HW_8 = 8;   // Highest (8-lane octal-SPI)
constexpr int PRIORITY_SPI_HW_4 = 7;   // Medium (4-lane quad-SPI)
constexpr int PRIORITY_SPI_HW_2 = 6;   // Lowest (2-lane dual-SPI)

/// @brief Register RP2040/RP2350 SpiHw2 instances
static void addSpiHw2IfPossible() {
    FL_DBG("RP2040/RP2350: Registering SpiHw2 instances");

    // Create 2 logical SPI buses (each uses separate PIO state machine)
    static auto controller0 = fl::make_shared<SPIDualRP2040>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIDualRP2040>(1, "SPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    FL_DBG("RP2040/RP2350: SpiHw2 instances registered");
}

/// @brief Register RP2040/RP2350 SpiHw4 instances
static void addSpiHw4IfPossible() {
    FL_DBG("RP2040/RP2350: Registering SpiHw4 instances");

    // Create 2 logical SPI buses (each uses separate PIO state machine)
    static auto controller0 = fl::make_shared<SPIQuadRP2040>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIQuadRP2040>(1, "SPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);

    FL_DBG("RP2040/RP2350: SpiHw4 instances registered");
}

/// @brief Register RP2040/RP2350 SpiHw8 instances
static void addSpiHw8IfPossible() {
    FL_DBG("RP2040/RP2350: Registering SpiHw8 instances");

    // Create 2 logical SPI buses (each uses separate PIO state machine)
    static auto controller0 = fl::make_shared<SpiHw8RP2040>(0, "SPI0");
    static auto controller1 = fl::make_shared<SpiHw8RP2040>(1, "SPI1");

    SpiHw8::registerInstance(controller0);
    SpiHw8::registerInstance(controller1);

    FL_DBG("RP2040/RP2350: SpiHw8 instances registered");
}

}  // namespace detail

namespace platform {

/// @brief Unified RP2040/RP2350 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority (highest to lowest):
/// - SpiHw8 (priority 8): Octal-SPI, 8 parallel lanes (PIO-based)
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes (PIO-based)
/// - SpiHw2 (priority 6): Dual-SPI, 2 parallel lanes (PIO-based)
///
/// Platform availability:
/// - RP2040: All three (2 PIO blocks × 4 state machines = 8 total)
/// - RP2350: All three (3 PIO blocks × 4 state machines = 12 total)
void initSpiHardware() {
    FL_DBG("RP2040/RP2350: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw8IfPossible();  // Priority 8
    detail::addSpiHw4IfPossible();  // Priority 7
    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("RP2040/RP2350: SPI hardware initialized");
}

}  // namespace platform
}  // namespace fl

#endif  // PICO_RP2040 || PICO_RP2350 || ARDUINO_ARCH_RP2040 || ARDUINO_ARCH_RP2350
