/// @file init_channel_engine_rp.cpp.hpp
/// @brief RP2040/RP2350-specific channel engine initialization
///
/// This file provides lazy initialization of RP2040/RP2350-specific channel engines
/// (SPI hardware) in priority order. Engines are registered on first access
/// to ChannelBusManager::instance().
///
/// Priority Order:
/// - SPI_UNIFIED (6-8): True SPI hardware (octal/quad/dual-lane via PIO)
///
/// Architecture Pattern:
/// This follows the ESP32 pattern:
/// 1. Collect all SpiHw instances via ::getAll()
/// 2. Wrap them in SpiChannelEngineAdapter
/// 3. Register unified adapter with ChannelBusManager

#include "fl/compiler_control.h"

#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)

#include "fl/channels/bus_manager.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "platforms/arm/rp/rpcommon/init_channel_engine.h"

namespace fl {

namespace detail {

/// @brief Add HW SPI engines if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelBusManager& manager) {
    FL_DBG("RP2040/RP2350: Registering unified HW SPI channel engine");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw8 controllers (highest priority: 8)
    // ========================================================================
    const auto& hw8Controllers = SpiHw8::getAll();
    FL_DBG("RP2040/RP2350: Found " << hw8Controllers.size() << " SpiHw8 controllers");

    for (const auto& ctrl : hw8Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(8);
            names.push_back(ctrl->getName());  // "SPI0" or "SPI1"
        }
    }

    // ========================================================================
    // Collect SpiHw4 controllers (medium priority: 7)
    // ========================================================================
    const auto& hw4Controllers = SpiHw4::getAll();
    FL_DBG("RP2040/RP2350: Found " << hw4Controllers.size() << " SpiHw4 controllers");

    for (const auto& ctrl : hw4Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(7);
            names.push_back(ctrl->getName());  // "SPI0" or "SPI1"
        }
    }

    // ========================================================================
    // Collect SpiHw2 controllers (lower priority: 6)
    // ========================================================================
    const auto& hw2Controllers = SpiHw2::getAll();
    FL_DBG("RP2040/RP2350: Found " << hw2Controllers.size() << " SpiHw2 controllers");

    for (const auto& ctrl : hw2Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(6);
            names.push_back(ctrl->getName());  // "SPI0" or "SPI1"
        }
    }

    // ========================================================================
    // Create unified adapter with all controllers
    // ========================================================================
    if (!controllers.empty()) {
        auto adapter = SpiChannelEngineAdapter::create(
            controllers,
            priorities,
            names,
            "SPI_UNIFIED"
        );

        if (adapter) {
            // Register with highest priority found
            int maxPriority = priorities[0];
            for (size_t i = 1; i < priorities.size(); i++) {
                if (priorities[i] > maxPriority) {
                    maxPriority = priorities[i];
                }
            }

            manager.addEngine(maxPriority, adapter, "SPI_UNIFIED");

            FL_DBG("RP2040/RP2350: Registered unified SPI engine with "
                   << controllers.size() << " controllers (priority "
                   << maxPriority << ")");
        } else {
            FL_WARN("RP2040/RP2350: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG("RP2040/RP2350: No SPI hardware controllers available");
    }
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel engines for RP2040/RP2350
///
/// Called lazily on first access to ChannelBusManager::instance().
/// Registers platform-specific engines (SPI hardware) with the bus manager.
void initChannelEngines() {
    FL_DBG("RP2040/RP2350: Lazy initialization of channel engines");

    auto& manager = channelBusManager();

    // Register true SPI hardware (priority 6-8)
    detail::addSpiHardwareIfPossible(manager);

    FL_DBG("RP2040/RP2350: Channel engines initialized");
}

} // namespace platforms

} // namespace fl

#endif // PICO_RP2040 || PICO_RP2350 || ARDUINO_ARCH_RP2040 || ARDUINO_ARCH_RP2350
