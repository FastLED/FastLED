// IWYU pragma: private

/// @file init_channel_driver_rp.cpp.hpp
/// @brief RP2040/RP2350-specific channel driver initialization
///
/// This file provides lazy initialization of RP2040/RP2350-specific channel drivers
/// (SPI hardware) in priority order. Engines are registered on first access
/// to ChannelManager::instance().
///
/// Priority Order:
/// - SPI_UNIFIED: True single-lane SPI channel hardware
///
/// Architecture Pattern:
/// This follows the ESP32 pattern:
/// 1. Collect all SpiHw instances via ::getAll()
/// 2. Wrap them in SpiChannelEngineAdapter
/// 3. Register unified adapter with ChannelManager

#include "fl/stl/compiler_control.h"
#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "fl/channels/manager.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/arm/rp/rpcommon/init_channel_driver.h"
#include "platforms/arm/rp/rpcommon/rp_uart_bus_traits.h"
#include "platforms/arm/rp/rpcommon/rp_spi_bus_traits.h"
#include "platforms/arm/rp/rpcommon/rp_pio_tx_bus_traits.h"

namespace fl {

namespace detail {

/// @brief Add HW SPI drivers if supported by platform (UNIFIED VERSION)
static void addSpiHardwareIfPossible(ChannelManager& manager) {
    FL_DBG_F("RP2040/RP2350: Registering unified HW SPI channel driver");

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    // RP2040's PIO-based SpiHw2/4/8 controllers are intentionally not added
    // here. They are multi-lane backends for MultiLaneDevice, which owns the
    // lane buffers and performs the required bit transposition before DMA.
    // SpiChannelEngineAdapter receives independent ChannelData buffers and
    // cannot safely reinterpret them as a parallel transfer.

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

            FL_DBG_F("RP2040/RP2350: Registered unified SPI driver with %s controllers (priority %s)", controllers.size(), maxPriority);
        } else {
            FL_WARN_F("RP2040/RP2350: Failed to create unified SPI adapter");
        }
    } else {
        FL_DBG_F("RP2040/RP2350: No SPI hardware controllers available");
    }
}

} // namespace detail

namespace platforms {

/// @brief Initialize channel drivers for RP2040/RP2350
///
/// Called lazily on first access to ChannelManager::instance().
/// Registers platform-specific drivers (SPI hardware) with the bus manager.
void initChannelDrivers() {
    FL_DBG_F("RP2040/RP2350: Lazy initialization of channel drivers");

    auto& manager = channelManager();

    // Register true SPI hardware (priority 6-8)
    detail::addSpiHardwareIfPossible(manager);

    // RP2040 has two PL011 UART blocks. They are explicit Bus::UART lanes;
    // USB CDC remains the sketch/RPC transport and is never registered here.
    BusTraits<Bus::UART, 0>::registerWithManager();
    BusTraits<Bus::UART, 1>::registerWithManager();
    // Keep fixed PL022 SPI separate from the PIO multi-lane adapter above.
    // Explicit Bus::SPI instance selection names SPI0 or SPI1 and therefore
    // cannot be silently routed to a PIO parallel-SPI implementation.
    BusTraits<Bus::SPI, 0>::registerWithManager();
    BusTraits<Bus::SPI, 1>::registerWithManager();
    BusTraits<Bus::FLEX_IO, 0>::registerWithManager();
    BusTraits<Bus::FLEX_IO, 1>::registerWithManager();

    FL_DBG_F("RP2040/RP2350: Channel drivers initialized");
}

} // namespace platforms

} // namespace fl

#endif // FL_IS_RP2040 || FL_IS_RP2350
