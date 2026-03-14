// IWYU pragma: private

/// @file init_channel_driver_samd21.cpp.hpp
/// @brief SAMD21-specific channel driver initialization
///
/// This file provides lazy initialization of SAMD21-specific channel drivers
/// (SPI hardware) in priority order. Engines are registered on first access
/// to ChannelManager::instance().
///
/// Priority Order:
/// - SPI_UNIFIED (6): True SPI hardware (dual-lane via SERCOM)

#include "fl/stl/compiler_control.h"

#if defined(ARDUINO_SAMD_ZERO) || defined(ADAFRUIT_FEATHER_M0) || \
    defined(ARDUINO_SAMD_FEATHER_M0) || defined(ARDUINO_SAM_ZERO) || \
    defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E18A__)

#include "fl/channels/manager.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/system/log.h"
#include "fl/system/log.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/arm/d21/init_channel_driver.h"

namespace fl {

namespace detail {

/// @brief Add HW SPI drivers if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelManager& manager) {
    FL_DBG("SAMD21: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // ========================================================================
    // Collect SpiHw2 controllers (priority: 6)
    // ========================================================================
    const auto& hw2Controllers = SpiHw2::getAll();
    FL_DBG("SAMD21: Found " << hw2Controllers.size() << " SpiHw2 controllers");

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

            FL_DBG("SAMD21: Registered unified SPI driver with "
                   << controllers.size() << " controllers (priority "
                   << maxPriority << ")");
        } else {
            FL_WARN("SAMD21: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG("SAMD21: No SPI hardware controllers available");
    }
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for SAMD21
///
/// Called lazily on first access to ChannelManager::instance().
/// Registers platform-specific drivers (SPI hardware) with the bus manager.
void initChannelDrivers() {
    FL_DBG("SAMD21: Lazy initialization of channel drivers");

    auto& manager = channelManager();

    // Register true SPI hardware (priority 6)
    detail::addSpiHardwareIfPossible(manager);

    FL_DBG("SAMD21: Channel drivers initialized");
}

} // namespace platforms

} // namespace fl

#endif // SAMD21 platform guards
