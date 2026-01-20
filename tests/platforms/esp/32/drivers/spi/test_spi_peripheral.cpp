/// @file test_spi_peripheral.cpp
/// @brief Unit tests for SPI peripheral mock implementation
///
/// These tests validate the SpiPeripheralMock behavior for unit testing.
/// The mock simulates ESP32 SPI hardware without requiring real hardware.
///
/// Test coverage:
/// 1. Bus lifecycle (initialize, free)
/// 2. Device management (add, remove)
/// 3. Transaction queuing and completion
/// 4. Callback registration and triggering
/// 5. DMA memory allocation
/// 6. Transaction history capture
/// 7. Error handling and state validation

#include "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/spi/ispi_peripheral.h"

using namespace fl;
using namespace fl::detail;

// Disambiguate SpiTransaction from fl::SpiTransaction (in fl/spi.h)
using SpiTrans = fl::detail::SpiTransaction;

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/// @brief Reset mock to clean state before each test
struct SpiPeripheralFixture {
    SpiPeripheralMock& mock = SpiPeripheralMock::instance();

    SpiPeripheralFixture() {
        // Reset mock to clean state
        mock.reset();
        mock.clearTransactionHistory();
    }

    ~SpiPeripheralFixture() {
        // Cleanup after test
        if (mock.hasDevice()) {
            mock.removeDevice();
        }
        if (mock.isInitialized()) {
            mock.freeBus();
        }
        mock.reset();
    }
};

/// @brief Callback function for transaction completion testing
static int gCallbackCount = 0;
static void testCallback(void* trans) {
    (void)trans;  // Unused
    gCallbackCount++;
}

//=============================================================================
// Bus Lifecycle Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - initialize bus") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Valid configuration
    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(config));
    CHECK(mock.isInitialized());

    // Verify stored configuration
    const auto& stored = mock.getBusConfig();
    CHECK_EQ(stored.mosi_pin, 23);
    CHECK_EQ(stored.sclk_pin, 18);
}

TEST_CASE("SpiPeripheralMock - initialize bus with invalid config") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Invalid: missing SCLK pin
    SpiBusConfig config(/*mosi=*/23, /*sclk=*/-1);
    CHECK_FALSE(mock.initializeBus(config));
    CHECK_FALSE(mock.isInitialized());
}

TEST_CASE("SpiPeripheralMock - double initialization fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(config));
    CHECK(mock.isInitialized());

    // Second initialization should fail
    CHECK_FALSE(mock.initializeBus(config));
}

TEST_CASE("SpiPeripheralMock - free bus") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(config));
    CHECK(mock.freeBus());
    CHECK_FALSE(mock.isInitialized());
}

TEST_CASE("SpiPeripheralMock - free uninitialized bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    CHECK_FALSE(mock.freeBus());
}

TEST_CASE("SpiPeripheralMock - free bus with device attached fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Cannot free bus while device is attached
    CHECK_FALSE(mock.freeBus());

    // Must remove device first
    CHECK(mock.removeDevice());
    CHECK(mock.freeBus());
}

//=============================================================================
// Device Management Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - add device") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));
    CHECK(mock.hasDevice());

    // Verify stored configuration
    const auto& stored = mock.getDeviceConfig();
    CHECK_EQ(stored.clock_speed_hz, 2500000);
    CHECK_EQ(stored.queue_size, 3);
}

TEST_CASE("SpiPeripheralMock - add device without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK_FALSE(mock.addDevice(device_config));
    CHECK_FALSE(mock.hasDevice());
}

TEST_CASE("SpiPeripheralMock - add device with invalid config fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    // Invalid: zero clock speed
    SpiDeviceConfig bad_clock(/*clock_hz=*/0, /*queue_size=*/3);
    CHECK_FALSE(mock.addDevice(bad_clock));

    // Invalid: zero queue size
    SpiDeviceConfig bad_queue(/*clock_hz=*/2500000, /*queue_size=*/0);
    CHECK_FALSE(mock.addDevice(bad_queue));
}

TEST_CASE("SpiPeripheralMock - double add device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Second add should fail
    CHECK_FALSE(mock.addDevice(device_config));
}

TEST_CASE("SpiPeripheralMock - remove device") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));
    CHECK(mock.hasDevice());

    CHECK(mock.removeDevice());
    CHECK_FALSE(mock.hasDevice());
}

TEST_CASE("SpiPeripheralMock - remove non-existent device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    CHECK_FALSE(mock.removeDevice());
}

//=============================================================================
// Transaction Queuing Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - queue single transaction") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Queue transaction
    uint8_t buffer[16] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    CHECK(mock.queueTransaction(trans));

    // Verify transaction was queued
    CHECK_EQ(mock.getQueuedTransactionCount(), 1);
    CHECK_EQ(mock.getTransactionCount(), 1);
}

TEST_CASE("SpiPeripheralMock - queue transaction without device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    // No device added
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    CHECK_FALSE(mock.queueTransaction(trans));
}

TEST_CASE("SpiPeripheralMock - queue transaction without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // No bus initialized
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    CHECK_FALSE(mock.queueTransaction(trans));
}

TEST_CASE("SpiPeripheralMock - queue multiple transactions") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Queue 3 transactions (queue size is 3)
    uint8_t buffer1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t buffer2[4] = {0x55, 0x66, 0x77, 0x88};
    uint8_t buffer3[4] = {0x99, 0xAA, 0xBB, 0xCC};

    CHECK(mock.queueTransaction(SpiTrans(buffer1, 4)));
    CHECK(mock.queueTransaction(SpiTrans(buffer2, 4)));
    CHECK(mock.queueTransaction(SpiTrans(buffer3, 4)));

    CHECK_EQ(mock.getQueuedTransactionCount(), 3);
    CHECK_EQ(mock.getTransactionCount(), 3);
}

TEST_CASE("SpiPeripheralMock - queue overflow") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup with queue size 2
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/2);
    CHECK(mock.addDevice(device_config));

    // Queue 2 transactions successfully
    uint8_t buffer1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t buffer2[4] = {0x55, 0x66, 0x77, 0x88};
    uint8_t buffer3[4] = {0x99, 0xAA, 0xBB, 0xCC};

    CHECK(mock.queueTransaction(SpiTrans(buffer1, 4)));
    CHECK(mock.queueTransaction(SpiTrans(buffer2, 4)));

    // Third transaction should fail (queue full)
    CHECK_FALSE(mock.queueTransaction(SpiTrans(buffer3, 4)));
    CHECK_EQ(mock.getQueuedTransactionCount(), 2);
}

TEST_CASE("SpiPeripheralMock - transaction failure injection") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Inject failure
    mock.setTransactionFailure(true);

    // Transaction should fail
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    CHECK_FALSE(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Reset failure flag
    mock.setTransactionFailure(false);

    // Transaction should succeed
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
}

//=============================================================================
// Transaction History Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - capture transaction data") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Queue transaction with known data
    uint8_t buffer[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    SpiTrans trans(buffer, 4);
    CHECK(mock.queueTransaction(trans));

    // Verify data was captured
    const auto& history = mock.getTransactionHistory();
    CHECK_EQ(history.size(), 1);

    const auto& record = history[0];
    CHECK_EQ(record.length_bits, 32);  // 4 bytes * 8 bits
    CHECK_EQ(record.buffer_copy.size(), 4);
    CHECK_EQ(record.buffer_copy[0], 0xDE);
    CHECK_EQ(record.buffer_copy[1], 0xAD);
    CHECK_EQ(record.buffer_copy[2], 0xBE);
    CHECK_EQ(record.buffer_copy[3], 0xEF);
}

TEST_CASE("SpiPeripheralMock - get last transaction data") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // No transactions yet
    auto last_data = mock.getLastTransactionData();
    CHECK(last_data.empty());

    // Queue transaction
    uint8_t buffer[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Verify last data
    last_data = mock.getLastTransactionData();
    CHECK_EQ(last_data.size(), 4);
    CHECK_EQ(last_data[0], 0xCA);
    CHECK_EQ(last_data[1], 0xFE);
    CHECK_EQ(last_data[2], 0xBA);
    CHECK_EQ(last_data[3], 0xBE);
}

TEST_CASE("SpiPeripheralMock - clear transaction history") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Queue some transactions
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    CHECK_EQ(mock.getTransactionHistory().size(), 2);

    // Clear history
    mock.clearTransactionHistory();

    CHECK_EQ(mock.getTransactionHistory().size(), 0);
    CHECK_EQ(mock.getTransactionCount(), 0);
    CHECK_EQ(mock.getQueuedTransactionCount(), 0);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - register callback") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    // Register callback
    void* callback = reinterpret_cast<void*>(testCallback);
    CHECK(mock.registerCallback(callback, nullptr));
}

TEST_CASE("SpiPeripheralMock - register callback without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    void* callback = reinterpret_cast<void*>(testCallback);
    CHECK_FALSE(mock.registerCallback(callback, nullptr));
}

TEST_CASE("SpiPeripheralMock - simulate transaction complete triggers callback") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    // Register callback
    gCallbackCount = 0;
    void* callback = reinterpret_cast<void*>(testCallback);
    CHECK(mock.registerCallback(callback, nullptr));

    // Queue transaction
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Manually trigger completion
    mock.simulateTransactionComplete();

    // Verify callback was called
    CHECK_EQ(gCallbackCount, 1);
}

//=============================================================================
// DMA Memory Allocation Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - allocate DMA buffer") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Allocate buffer
    uint8_t* buffer = mock.allocateDma(128);
    CHECK(buffer != nullptr);

    // Write to buffer (verify it's usable)
    buffer[0] = 0x42;
    CHECK_EQ(buffer[0], 0x42);

    // Free buffer
    mock.freeDma(buffer);
}

TEST_CASE("SpiPeripheralMock - allocate DMA buffer with alignment") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Allocate non-aligned size (should be rounded up)
    uint8_t* buffer = mock.allocateDma(17);  // Not a multiple of 4
    CHECK(buffer != nullptr);

    // Verify 4-byte alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
    CHECK_EQ(addr % 4, 0);

    mock.freeDma(buffer);
}

TEST_CASE("SpiPeripheralMock - free nullptr is safe") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Should not crash
    mock.freeDma(nullptr);
}

//=============================================================================
// State Inspection Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - canQueueTransaction") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Not ready initially
    CHECK_FALSE(mock.canQueueTransaction());

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/2);
    CHECK(mock.addDevice(device_config));

    // Now ready
    CHECK(mock.canQueueTransaction());

    // Queue 2 transactions (queue size is 2)
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Queue full
    CHECK_FALSE(mock.canQueueTransaction());
}

TEST_CASE("SpiPeripheralMock - reset clears all state") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup and queue transaction
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    CHECK(mock.addDevice(device_config));

    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    CHECK(mock.isInitialized());
    CHECK(mock.hasDevice());
    CHECK_EQ(mock.getQueuedTransactionCount(), 1);

    // Reset
    mock.reset();

    // All state cleared
    CHECK_FALSE(mock.isInitialized());
    CHECK_FALSE(mock.hasDevice());
    CHECK_EQ(mock.getQueuedTransactionCount(), 0);
    CHECK_EQ(mock.getTransactionCount(), 0);
}

//=============================================================================
// Multi-Lane Configuration Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - dual-lane bus configuration") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Dual-lane configuration
    SpiBusConfig config(/*data0=*/23, /*data1=*/19, /*sclk=*/18, /*max_sz=*/4096);
    CHECK(mock.initializeBus(config));

    const auto& stored = mock.getBusConfig();
    CHECK_EQ(stored.mosi_pin, 23);  // Data0 → MOSI
    CHECK_EQ(stored.miso_pin, 19);  // Data1 → MISO
    CHECK_EQ(stored.sclk_pin, 18);
}

TEST_CASE("SpiPeripheralMock - quad-lane bus configuration") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Quad-lane configuration
    SpiBusConfig config(/*data0=*/23, /*data1=*/19, /*data2=*/22, /*data3=*/21, /*sclk=*/18, /*max_sz=*/4096);
    CHECK(mock.initializeBus(config));

    const auto& stored = mock.getBusConfig();
    CHECK_EQ(stored.mosi_pin, 23);
    CHECK_EQ(stored.miso_pin, 19);
    CHECK_EQ(stored.data2_pin, 22);
    CHECK_EQ(stored.data3_pin, 21);
    CHECK_EQ(stored.sclk_pin, 18);
}

//=============================================================================
// Platform Utility Tests
//=============================================================================

TEST_CASE("SpiPeripheralMock - getMicroseconds") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    uint64_t t1 = mock.getMicroseconds();
    mock.delay(1);  // Delay 1ms
    uint64_t t2 = mock.getMicroseconds();

    // Time should advance (may not be exactly 1000us due to scheduling)
    CHECK(t2 > t1);
}
