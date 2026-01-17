/// @file spi_force_software.cpp
/// @brief Test that SPIBusManager respects FASTLED_FORCE_SOFTWARE_SPI
///
/// This test verifies device registration works with the bus manager and that
/// the proper bus type is assigned. When FASTLED_FORCE_SOFTWARE_SPI is defined,
/// the bus manager forces SOFT_SPI mode while preserving the proxy architecture.

#include "test.h"
#include "platforms/shared/spi_bus_manager.h"

using namespace fl;

TEST_CASE("SPIBusManager - device registration and bus type") {
    SPIBusManager& manager = getSPIBusManager();
    manager.reset();

    // Register a device
    SPIBusHandle handle = manager.registerDevice(
        18,  // clock_pin
        23,  // data_pin
        1000000,  // speed
        (void*)0x1234  // dummy controller
    );

    CHECK(handle.is_valid);

    // Initialize
    manager.initialize();

    // Verify bus info is available
    const SPIBusInfo* bus = manager.getBusInfo(handle.bus_id);
    CHECK(bus != nullptr);

    // Verify bus type based on FASTLED_FORCE_SOFTWARE_SPI define
    // When the define is set, bus_type should be SOFT_SPI
    // When the define is NOT set, bus_type should be SINGLE_SPI for one device
#if defined(FASTLED_FORCE_SOFTWARE_SPI)
    CHECK(bus->bus_type == SPIBusType::SOFT_SPI);
#else
    CHECK(bus->bus_type == SPIBusType::SINGLE_SPI);
#endif

    manager.reset();
}
