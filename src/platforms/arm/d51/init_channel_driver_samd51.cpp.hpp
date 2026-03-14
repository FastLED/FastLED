// IWYU pragma: private

/// @file init_channel_driver_samd51.cpp.hpp
/// @brief SAMD51-specific channel driver initialization
///
/// This file provides lazy initialization of SAMD51-specific channel drivers
/// (SPI hardware) in priority order. Engines are registered on first access
/// to ChannelManager::instance().
///
/// Priority Order:
/// - SPI_UNIFIED (6-7): True SPI hardware (quad/dual-lane via SERCOM)

#include "fl/stl/compiler_control.h"
#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD51)

#include "fl/channels/manager.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/system/log.h"
#include "fl/system/log.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/arm/d51/init_channel_driver.h"

namespace fl {

namespace detail {

/// @brief Add HW SPI drivers if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelManager& manager) {
    FL_DBG("SAMD51: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw4 controllers (higher priority: 7)
    // ========================================================================
    const auto& hw4Controllers = SpiHw4::getAll();
    FL_DBG("SAMD51: Found " << hw4Controllers.size() << " SpiHw4 controllers");

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
    FL_DBG("SAMD51: Found " << hw2Controllers.size() << " SpiHw2 controllers");

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

            manager.addDriver(maxPriority, adapter);

            FL_DBG("SAMD51: Registered unified SPI driver with "
                   << controllers.size() << " controllers (priority "
                   << maxPriority << ")");
        } else {
            FL_WARN("SAMD51: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG("SAMD51: No SPI hardware controllers available");
    }
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for SAMD51
///
/// Called lazily on first access to ChannelManager::instance().
/// Registers platform-specific drivers (SPI hardware) with the bus manager.
void initChannelDrivers() {
    FL_DBG("SAMD51: Lazy initialization of channel drivers");

    auto& manager = channelManager();

    // Register true SPI hardware (priority 6-7)
    detail::addSpiHardwareIfPossible(manager);

    FL_DBG("SAMD51: Channel drivers initialized");
}

} // namespace platforms

} // namespace fl

#endif // FL_IS_SAMD51
