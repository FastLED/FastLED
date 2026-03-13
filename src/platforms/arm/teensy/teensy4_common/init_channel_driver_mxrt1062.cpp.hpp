// IWYU pragma: private

/// @file init_channel_driver_mxrt1062.cpp.hpp
/// @brief Teensy 4.x-specific channel driver initialization
///
/// This file provides lazy initialization of Teensy 4.x-specific channel drivers
/// (SPI hardware) in priority order. Engines are registered on first access
/// to ChannelManager::instance().
///
/// Priority Order:
/// - SPI_UNIFIED (6-7): True SPI hardware (quad/dual-lane)
///
/// Architecture Pattern:
/// This follows the ESP32 pattern:
/// 1. Collect all SpiHw instances via ::getAll()
/// 2. Wrap them in SpiChannelEngineAdapter
/// 3. Register unified adapter with ChannelManager

#include "fl/stl/compiler_control.h"

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/channels/manager.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/arm/teensy/teensy4_common/init_channel_driver.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.h"
namespace fl {

namespace detail {

/// @brief Add HW SPI drivers if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelManager& manager) {
    FL_DBG("Teensy 4.x: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw4 controllers (higher priority: 7)
    // ========================================================================
    const auto& hw4Controllers = SpiHw4::getAll();
    FL_DBG("Teensy 4.x: Found " << hw4Controllers.size() << " SpiHw4 controllers");

    for (const auto& ctrl : hw4Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(7);
            names.push_back(ctrl->getName());  // "SPI", "SPI1", "SPI2"
        }
    }

    // ========================================================================
    // Collect SpiHw2 controllers (lower priority: 6)
    // ========================================================================
    const auto& hw2Controllers = SpiHw2::getAll();
    FL_DBG("Teensy 4.x: Found " << hw2Controllers.size() << " SpiHw2 controllers");

    for (const auto& ctrl : hw2Controllers) {
        if (ctrl) {
            controllers.push_back(ctrl);
            priorities.push_back(6);
            names.push_back(ctrl->getName());  // "SPI", "SPI1", "SPI2"
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

            manager.addDriver(maxPriority, adapter);

            FL_DBG("Teensy 4.x: Registered unified SPI driver with "
                   << controllers.size() << " controllers (priority "
                   << maxPriority << ")");
        } else {
            FL_WARN("Teensy 4.x: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG("Teensy 4.x: No SPI hardware controllers available");
    }
}

/// @brief Add FlexIO2 engine for clockless LED strips on FlexIO-capable pins
static void addFlexIOIfPossible(ChannelManager& manager) {
    FL_DBG("Teensy 4.x: Registering FlexIO channel driver");

    auto engine = fl::make_shared<ChannelEngineFlexIO>();
    manager.addDriver(8, engine);

    FL_DBG("Teensy 4.x: Registered FlexIO driver (priority 8)");
}

/// @brief Add ObjectFLED DMA engine for clockless LED strips
static void addObjectFLEDIfPossible(ChannelManager& manager) {
    FL_DBG("Teensy 4.x: Registering ObjectFLED channel driver");

    auto engine = fl::make_shared<ChannelEngineObjectFLED>();
    manager.addDriver(5, engine);

    FL_DBG("Teensy 4.x: Registered ObjectFLED driver (priority 5)");
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for Teensy 4.x
///
/// Called lazily on first access to ChannelManager::instance().
/// Registers platform-specific drivers (SPI hardware, ObjectFLED DMA) with the bus manager.
void initChannelDrivers() {
    FL_DBG("Teensy 4.x: Lazy initialization of channel drivers");

    auto& manager = channelManager();

    // Register FlexIO engine for clockless strips on FlexIO-capable pins (priority 8)
    detail::addFlexIOIfPossible(manager);

    // Register ObjectFLED DMA engine for clockless strips (priority 5, fallback)
    detail::addObjectFLEDIfPossible(manager);

    // Register true SPI hardware (priority 6-7)
    detail::addSpiHardwareIfPossible(manager);

    FL_DBG("Teensy 4.x: Channel drivers initialized");
}

} // namespace platforms

} // namespace fl

#endif // FL_IS_TEENSY_4X
