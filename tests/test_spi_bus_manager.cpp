#include "doctest.h"
#include "platforms/shared/spi_bus_manager.h"

using namespace fl;

// Mock controller for testing
struct MockController {
    uint8_t id;
    MockController(uint8_t i) : id(i) {}
};

TEST_CASE("SPIBusManager: Single device registration") {
    SPIBusManager manager;

    MockController ctrl(1);
    SPIBusHandle handle = manager.registerDevice(14, 13, &ctrl);

    CHECK(handle.is_valid);
    CHECK(handle.bus_id == 0);
    CHECK(handle.lane_id == 0);
    CHECK(manager.getNumBuses() == 1);
}

TEST_CASE("SPIBusManager: Multiple devices on same clock pin") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3);

    // Register 3 devices on same clock pin
    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);

    CHECK(h1.is_valid);
    CHECK(h2.is_valid);
    CHECK(h3.is_valid);

    // All should be on same bus
    CHECK(h1.bus_id == h2.bus_id);
    CHECK(h2.bus_id == h3.bus_id);

    // Different lanes
    CHECK(h1.lane_id == 0);
    CHECK(h2.lane_id == 1);
    CHECK(h3.lane_id == 2);

    // Only one bus created
    CHECK(manager.getNumBuses() == 1);
}

TEST_CASE("SPIBusManager: Multiple devices on different clock pins") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3);

    // Register devices on different clock pins
    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(18, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(22, 33, &ctrl3);

    CHECK(h1.is_valid);
    CHECK(h2.is_valid);
    CHECK(h3.is_valid);

    // Different buses
    CHECK(h1.bus_id != h2.bus_id);
    CHECK(h2.bus_id != h3.bus_id);

    // All on lane 0 (first device on each bus)
    CHECK(h1.lane_id == 0);
    CHECK(h2.lane_id == 0);
    CHECK(h3.lane_id == 0);

    // Three buses created
    CHECK(manager.getNumBuses() == 3);
}

TEST_CASE("SPIBusManager: NULL controller registration") {
    SPIBusManager manager;

    SPIBusHandle handle = manager.registerDevice(14, 13, nullptr);

    CHECK_FALSE(handle.is_valid);
}

TEST_CASE("SPIBusManager: Too many buses") {
    SPIBusManager manager;

    MockController ctrls[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Register 8 buses (max)
    for (uint8_t i = 0; i < 8; i++) {
        SPIBusHandle h = manager.registerDevice(10 + i, 20, &ctrls[i]);
        CHECK(h.is_valid);
    }

    CHECK(manager.getNumBuses() == 8);

    // 9th bus should fail
    SPIBusHandle h9 = manager.registerDevice(99, 20, &ctrls[8]);
    CHECK_FALSE(h9.is_valid);
}

TEST_CASE("SPIBusManager: Too many devices on one bus") {
    SPIBusManager manager;

    MockController ctrls[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Register 8 devices (max per bus)
    for (uint8_t i = 0; i < 8; i++) {
        SPIBusHandle h = manager.registerDevice(14, 20 + i, &ctrls[i]);
        CHECK(h.is_valid);
    }

    // 9th device on same clock pin should fail
    SPIBusHandle h9 = manager.registerDevice(14, 99, &ctrls[8]);
    CHECK_FALSE(h9.is_valid);
}

TEST_CASE("SPIBusManager: Single device initialization") {
    SPIBusManager manager;

    MockController ctrl(1);
    SPIBusHandle handle = manager.registerDevice(14, 13, &ctrl);

    bool result = manager.initialize();

    CHECK(result);
    CHECK(manager.isDeviceEnabled(handle));

    const SPIBusInfo* bus = manager.getBusInfo(handle.bus_id);
    REQUIRE(bus != nullptr);
    CHECK(bus->bus_type == SPIBusType::SINGLE_SPI);
    CHECK(bus->is_initialized);
}

TEST_CASE("SPIBusManager: Quad-SPI promotion with 3 devices") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3);

    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);

    bool result = manager.initialize();

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    // Quad-SPI should be promoted for 3 devices
    CHECK(bus->bus_type == SPIBusType::QUAD_SPI);
    CHECK(result == true);
    CHECK(manager.isDeviceEnabled(h1));
    CHECK(manager.isDeviceEnabled(h2));
    CHECK(manager.isDeviceEnabled(h3));
}

TEST_CASE("SPIBusManager: Quad-SPI promotion with 4 devices") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3), ctrl4(4);

    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);
    SPIBusHandle h4 = manager.registerDevice(14, 25, &ctrl4);

    bool result = manager.initialize();

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    // Quad-SPI should be promoted for 4 devices
    CHECK(bus->bus_type == SPIBusType::QUAD_SPI);
    CHECK(result == true);
    CHECK(manager.isDeviceEnabled(h1));
    CHECK(manager.isDeviceEnabled(h2));
    CHECK(manager.isDeviceEnabled(h3));
    CHECK(manager.isDeviceEnabled(h4));
}

TEST_CASE("SPIBusManager: Conflict resolution - 2 devices, no multi-SPI") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2);

    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);

    bool result = manager.initialize();

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    // Should try Quad-SPI (but only 2 devices)
    // Current implementation promotes 3-4 devices to Quad
    // For 2 devices, should use Dual-SPI (not implemented yet)
    // So it will fail and disable second device
    CHECK_FALSE(manager.isDeviceEnabled(h2));

    // First device should always be enabled
    CHECK(manager.isDeviceEnabled(h1));
}

TEST_CASE("SPIBusManager: Reset functionality") {
    SPIBusManager manager;

    MockController ctrl(1);
    SPIBusHandle handle = manager.registerDevice(14, 13, &ctrl);

    CHECK(manager.getNumBuses() == 1);
    CHECK(handle.is_valid);

    manager.reset();

    CHECK(manager.getNumBuses() == 0);

    // Register again after reset
    SPIBusHandle handle2 = manager.registerDevice(14, 13, &ctrl);
    CHECK(handle2.is_valid);
    CHECK(manager.getNumBuses() == 1);
}

TEST_CASE("SPIBusManager: isDeviceEnabled with invalid handle") {
    SPIBusManager manager;

    SPIBusHandle invalid_handle;  // Default constructor creates invalid handle
    CHECK_FALSE(manager.isDeviceEnabled(invalid_handle));
}

TEST_CASE("SPIBusManager: transmit with invalid handle") {
    SPIBusManager manager;

    uint8_t data[10] = {0};
    SPIBusHandle invalid_handle;

    // Should not crash
    manager.transmit(invalid_handle, data, 10);
    manager.waitComplete(invalid_handle);
}

TEST_CASE("SPIBusManager: Multiple buses with mixed device counts") {
    SPIBusManager manager;

    MockController ctrls[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    // Bus 1: Single device on pin 14
    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrls[0]);

    // Bus 2: Three devices on pin 18 (Quad-SPI candidate)
    SPIBusHandle h2_1 = manager.registerDevice(18, 19, &ctrls[1]);
    SPIBusHandle h2_2 = manager.registerDevice(18, 20, &ctrls[2]);
    SPIBusHandle h2_3 = manager.registerDevice(18, 21, &ctrls[3]);

    // Bus 3: Two devices on pin 22 (Dual-SPI candidate)
    SPIBusHandle h3_1 = manager.registerDevice(22, 23, &ctrls[4]);
    (void)manager.registerDevice(22, 24, &ctrls[5]);

    CHECK(manager.getNumBuses() == 3);

    (void)manager.initialize();

    // Bus 1: Should be single SPI
    const SPIBusInfo* bus1 = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus1 != nullptr);
    CHECK(bus1->bus_type == SPIBusType::SINGLE_SPI);

    // Bus 2: Should be Quad-SPI
    const SPIBusInfo* bus2 = manager.getBusInfo(h2_1.bus_id);
    REQUIRE(bus2 != nullptr);
    CHECK(bus2->bus_type == SPIBusType::QUAD_SPI);
    CHECK(manager.isDeviceEnabled(h2_1));
    CHECK(manager.isDeviceEnabled(h2_2));
    CHECK(manager.isDeviceEnabled(h2_3));
}

TEST_CASE("SPIBusManager: getBusInfo with invalid bus_id") {
    SPIBusManager manager;

    const SPIBusInfo* bus = manager.getBusInfo(99);
    CHECK(bus == nullptr);
}

TEST_CASE("SPIBusManager: Device info tracking") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2);

    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    (void)manager.registerDevice(14, 27, &ctrl2);

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    CHECK(bus->num_devices == 2);
    CHECK(bus->devices[0].clock_pin == 14);
    CHECK(bus->devices[0].data_pin == 13);
    CHECK(bus->devices[0].controller == &ctrl1);

    CHECK(bus->devices[1].clock_pin == 14);
    CHECK(bus->devices[1].data_pin == 27);
    CHECK(bus->devices[1].controller == &ctrl2);
}

TEST_CASE("SPIBusManager: Device unregistration") {
    SPIBusManager manager;

    MockController ctrl(1);
    SPIBusHandle handle = manager.registerDevice(14, 13, &ctrl);

    CHECK(handle.is_valid);
    CHECK(manager.isDeviceEnabled(handle));

    // Unregister the device
    bool result = manager.unregisterDevice(handle);

    CHECK(result);
    CHECK_FALSE(manager.isDeviceEnabled(handle));
}

TEST_CASE("SPIBusManager: Unregister with invalid handle") {
    SPIBusManager manager;

    SPIBusHandle invalid_handle;  // Invalid by default
    bool result = manager.unregisterDevice(invalid_handle);

    CHECK_FALSE(result);
}

TEST_CASE("SPIBusManager: Quad-SPI release when all devices unregistered") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3);

    // Register 3 devices for Quad-SPI
    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);

    manager.initialize();

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    // Should be Quad-SPI initially
    CHECK(bus->bus_type == SPIBusType::QUAD_SPI);
    CHECK(bus->is_initialized);

    // Unregister devices one by one
    manager.unregisterDevice(h1);
    CHECK(bus->is_initialized);  // Still initialized (2 devices left)

    manager.unregisterDevice(h2);
    CHECK(bus->is_initialized);  // Still initialized (1 device left)

    manager.unregisterDevice(h3);
    // After last device is unregistered, bus should be released
    CHECK_FALSE(bus->is_initialized);
    CHECK(bus->bus_type == SPIBusType::SOFT_SPI);  // Reset to default
    CHECK(bus->hw_controller == nullptr);  // Hardware released
}

TEST_CASE("SPIBusManager: Partial unregistration - Quad-SPI remains active") {
    SPIBusManager manager;

    MockController ctrl1(1), ctrl2(2), ctrl3(3), ctrl4(4);

    // Register 4 devices for Quad-SPI
    SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
    SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
    SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);
    SPIBusHandle h4 = manager.registerDevice(14, 25, &ctrl4);

    manager.initialize();

    const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
    REQUIRE(bus != nullptr);

    CHECK(bus->bus_type == SPIBusType::QUAD_SPI);

    // Unregister 2 devices (2 remain)
    manager.unregisterDevice(h3);
    manager.unregisterDevice(h4);

    // Quad-SPI should still be active (2 devices left)
    CHECK(bus->is_initialized);
    CHECK(bus->bus_type == SPIBusType::QUAD_SPI);

    // First 2 devices should still be enabled
    CHECK(manager.isDeviceEnabled(h1));
    CHECK(manager.isDeviceEnabled(h2));

    // Last 2 devices should be deallocated
    CHECK_FALSE(manager.isDeviceEnabled(h3));
    CHECK_FALSE(manager.isDeviceEnabled(h4));
}

TEST_CASE("SPIBusManager: Double unregistration is safe") {
    SPIBusManager manager;

    MockController ctrl(1);
    SPIBusHandle handle = manager.registerDevice(14, 13, &ctrl);

    // First unregister should succeed
    bool result1 = manager.unregisterDevice(handle);
    CHECK(result1);

    // Second unregister should fail gracefully (already deallocated)
    bool result2 = manager.unregisterDevice(handle);
    CHECK_FALSE(result2);
}

TEST_CASE("SPIBusManager: Quad-SPI controller reuse after release") {
    SPIBusManager manager;

    // First batch: Register 3 devices, initialize Quad-SPI
    {
        MockController ctrl1(1), ctrl2(2), ctrl3(3);

        SPIBusHandle h1 = manager.registerDevice(14, 13, &ctrl1);
        SPIBusHandle h2 = manager.registerDevice(14, 27, &ctrl2);
        SPIBusHandle h3 = manager.registerDevice(14, 33, &ctrl3);

        manager.initialize();

        const SPIBusInfo* bus = manager.getBusInfo(h1.bus_id);
        REQUIRE(bus != nullptr);
        CHECK(bus->bus_type == SPIBusType::QUAD_SPI);
        CHECK(bus->is_initialized);

        // Store the controller pointer for comparison
        void* first_controller = bus->hw_controller;
        CHECK(first_controller != nullptr);

        // Unregister all devices
        manager.unregisterDevice(h1);
        manager.unregisterDevice(h2);
        manager.unregisterDevice(h3);

        // Bus should be released
        CHECK_FALSE(bus->is_initialized);
        CHECK(bus->hw_controller == nullptr);
    }

    // Second batch: Register 4 new devices on a different clock pin
    // This should get a fresh Quad-SPI controller (possibly the same one that was released)
    {
        MockController ctrl4(4), ctrl5(5), ctrl6(6), ctrl7(7);

        SPIBusHandle h4 = manager.registerDevice(18, 19, &ctrl4);  // Different clock pin!
        SPIBusHandle h5 = manager.registerDevice(18, 20, &ctrl5);
        SPIBusHandle h6 = manager.registerDevice(18, 21, &ctrl6);
        SPIBusHandle h7 = manager.registerDevice(18, 22, &ctrl7);

        // Need to initialize this new bus
        manager.initialize();

        const SPIBusInfo* bus2 = manager.getBusInfo(h4.bus_id);
        REQUIRE(bus2 != nullptr);
        CHECK(bus2->bus_type == SPIBusType::QUAD_SPI);
        CHECK(bus2->is_initialized);
        CHECK(bus2->hw_controller != nullptr);  // Should have a valid controller

        // The controller should be successfully initialized (reused from the pool)
        // We can't directly test if it's the same instance, but we verify it works

        // Cleanup
        manager.unregisterDevice(h4);
        manager.unregisterDevice(h5);
        manager.unregisterDevice(h6);
        manager.unregisterDevice(h7);

        CHECK_FALSE(bus2->is_initialized);
    }
}

TEST_CASE("SPIBusManager: Multiple buses can share Quad-SPI controllers") {
    SPIBusManager manager;

    // Mock platform can handle 2 Quad-SPI groups simultaneously
    MockController ctrls[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    // Bus 1: 3 devices on clock pin 14
    SPIBusHandle b1_h1 = manager.registerDevice(14, 13, &ctrls[0]);
    SPIBusHandle b1_h2 = manager.registerDevice(14, 27, &ctrls[1]);
    SPIBusHandle b1_h3 = manager.registerDevice(14, 33, &ctrls[2]);

    // Bus 2: 4 devices on clock pin 18
    SPIBusHandle b2_h1 = manager.registerDevice(18, 19, &ctrls[3]);
    SPIBusHandle b2_h2 = manager.registerDevice(18, 20, &ctrls[4]);
    SPIBusHandle b2_h3 = manager.registerDevice(18, 21, &ctrls[5]);
    SPIBusHandle b2_h4 = manager.registerDevice(18, 22, &ctrls[6]);

    manager.initialize();

    const SPIBusInfo* bus1 = manager.getBusInfo(b1_h1.bus_id);
    const SPIBusInfo* bus2 = manager.getBusInfo(b2_h1.bus_id);

    REQUIRE(bus1 != nullptr);
    REQUIRE(bus2 != nullptr);

    // Both should be Quad-SPI
    CHECK(bus1->bus_type == SPIBusType::QUAD_SPI);
    CHECK(bus2->bus_type == SPIBusType::QUAD_SPI);

    // Both should be initialized
    CHECK(bus1->is_initialized);
    CHECK(bus2->is_initialized);

    // They should have different hardware controllers (different SPI peripherals)
    CHECK(bus1->hw_controller != nullptr);
    CHECK(bus2->hw_controller != nullptr);
    CHECK(bus1->hw_controller != bus2->hw_controller);  // Different peripherals

    // Cleanup first bus
    manager.unregisterDevice(b1_h1);
    manager.unregisterDevice(b1_h2);
    manager.unregisterDevice(b1_h3);

    // First bus should be released, second should still be active
    CHECK_FALSE(bus1->is_initialized);
    CHECK(bus2->is_initialized);

    // Cleanup second bus
    manager.unregisterDevice(b2_h1);
    manager.unregisterDevice(b2_h2);
    manager.unregisterDevice(b2_h3);
    manager.unregisterDevice(b2_h4);

    // Both should now be released
    CHECK_FALSE(bus2->is_initialized);
}
