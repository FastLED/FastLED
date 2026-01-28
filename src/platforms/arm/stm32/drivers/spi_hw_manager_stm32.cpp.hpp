/// @file spi_hw_manager_stm32.cpp.hpp
/// @brief STM32 SPI Hardware Manager - Unified initialization
///
/// This file consolidates all STM32 SPI hardware initialization into a single
/// manager following the ESP32 channel_bus_manager pattern.
///
/// Replaces scattered initialization from:
/// - spi_hw_2_stm32.cpp.hpp::initSpiHw2Instances()
/// - spi_hw_4_stm32.cpp.hpp::initSpiHw4Instances()
/// - spi_hw_8_stm32.cpp.hpp::initSpiHw8Instances()
///
/// Platform support:
/// - STM32F2/F4/F7/H7/L4: Stream-based DMA (SpiHw2, SpiHw4, SpiHw8)
/// - STM32F1/G4/U5: Software fallback (channel-based DMA not yet implemented)
///

#include "platforms/arm/stm32/is_stm32.h"

#if defined(FL_IS_STM32)

#include "platforms/arm/stm32/stm32_capabilities.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "fl/dbg.h"
#include "fl/warn.h"

// Include SPI implementation files BEFORE opening fl::detail namespace
// to avoid nested namespace issues in unity build
#ifdef FL_STM32_HAS_SPI_HW_2
    #include "platforms/arm/stm32/drivers/spi_hw_2_stm32.cpp.hpp"
#endif

#ifdef FL_STM32_HAS_SPI_HW_4
    #include "platforms/arm/stm32/drivers/spi_hw_4_stm32.cpp.hpp"
#endif

#ifdef FL_STM32_HAS_SPI_HW_8
    #include "platforms/arm/stm32/drivers/spi_hw_8_stm32.cpp.hpp"
#endif

namespace fl {
namespace detail {

/// Priority constants for SPI hardware (higher = preferred)
constexpr int PRIORITY_SPI_HW_8 = 8;   // Highest (8-lane octal-SPI)
constexpr int PRIORITY_SPI_HW_4 = 7;   // Medium (4-lane quad-SPI)
constexpr int PRIORITY_SPI_HW_2 = 6;   // Lowest (2-lane dual-SPI)

/// @brief Register STM32 SpiHw2 instances if available
static void addSpiHw2IfPossible() {
#ifdef FL_STM32_HAS_SPI_HW_2
    FL_DBG("STM32: Registering SpiHw2 instances");

    // Create logical SPI buses based on available Timer/DMA resources
    static auto controller0 = fl::make_shared<SPIDualSTM32>(0, "DSPI0");
    static auto controller1 = fl::make_shared<SPIDualSTM32>(1, "DSPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    FL_DBG("STM32: SpiHw2 instances registered");
#else
    // No-op if FL_STM32_HAS_SPI_HW_2 not defined
    FL_DBG("STM32: SpiHw2 not available (stream-based DMA required)");
#endif
}

/// @brief Register STM32 SpiHw4 instances if available
static void addSpiHw4IfPossible() {
#ifdef FL_STM32_HAS_SPI_HW_4
    FL_DBG("STM32: Registering SpiHw4 instances");

    // Create logical SPI buses based on available Timer/DMA resources
    static auto controller0 = fl::make_shared<SPIQuadSTM32>(0, "QSPI0");
    static auto controller1 = fl::make_shared<SPIQuadSTM32>(1, "QSPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);

    FL_DBG("STM32: SpiHw4 instances registered");
#else
    // No-op if FL_STM32_HAS_SPI_HW_4 not defined
    FL_DBG("STM32: SpiHw4 not available (stream-based DMA required)");
#endif
}

/// @brief Register STM32 SpiHw8 instances if available
static void addSpiHw8IfPossible() {
#ifdef FL_STM32_HAS_SPI_HW_8
    FL_DBG("STM32: Registering SpiHw8 instances");

    // Create 2 logical octal-SPI buses
    static auto controller0 = fl::make_shared<SPIOctalSTM32>(0, "OSPI0");
    static auto controller1 = fl::make_shared<SPIOctalSTM32>(1, "OSPI1");

    SpiHw8::registerInstance(controller0);
    SpiHw8::registerInstance(controller1);

    FL_DBG("STM32: SpiHw8 instances registered");
#else
    // No-op if FL_STM32_HAS_SPI_HW_8 not defined
    FL_DBG("STM32: SpiHw8 not available (stream-based DMA required)");
#endif
}

}  // namespace detail

namespace platform {

/// @brief Unified STM32 SPI hardware initialization entry point
///
/// Called lazily on first access to SpiHw*::getAll().
/// Registers all available SPI hardware controllers in priority order.
///
/// Registration priority (highest to lowest):
/// - SpiHw8 (priority 8): Octal-SPI, 8 parallel lanes
/// - SpiHw4 (priority 7): Quad-SPI, 4 parallel lanes
/// - SpiHw2 (priority 6): Dual-SPI, 2 parallel lanes
///
/// Feature flag driven:
/// - FL_STM32_HAS_SPI_HW_8: Octal-SPI available
/// - FL_STM32_HAS_SPI_HW_4: Quad-SPI available
/// - FL_STM32_HAS_SPI_HW_2: Dual-SPI available
///
/// Platform availability:
/// - STM32F2/F4/F7/H7/L4: All three (stream-based DMA)
/// - STM32F1/G4/U5: None (channel-based DMA not yet implemented)
#if defined(FL_STM32_HAS_SPI_HW_2) || defined(FL_STM32_HAS_SPI_HW_4) || defined(FL_STM32_HAS_SPI_HW_8)
inline void initSpiHardware() {
    FL_DBG("STM32: Initializing SPI hardware");

    // Register in priority order (highest to lowest)
    detail::addSpiHw8IfPossible();  // Priority 8
    detail::addSpiHw4IfPossible();  // Priority 7
    detail::addSpiHw2IfPossible();  // Priority 6

    FL_DBG("STM32: SPI hardware initialized");
}
#endif  // Hardware SPI available

}  // namespace platform
}  // namespace fl

#endif  // FL_IS_STM32
