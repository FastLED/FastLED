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
#include "test.h"
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

FL_TEST_CASE("SpiPeripheralMock - initialize bus") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Valid configuration
    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(config));
    FL_CHECK(mock.isInitialized());

    // Verify stored configuration
    const auto& stored = mock.getBusConfig();
    FL_CHECK_EQ(stored.mosi_pin, 23);
    FL_CHECK_EQ(stored.sclk_pin, 18);
}

FL_TEST_CASE("SpiPeripheralMock - initialize bus with invalid config") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Invalid: missing SCLK pin
    SpiBusConfig config(/*mosi=*/23, /*sclk=*/-1);
    FL_CHECK_FALSE(mock.initializeBus(config));
    FL_CHECK_FALSE(mock.isInitialized());
}

FL_TEST_CASE("SpiPeripheralMock - double initialization fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(config));
    FL_CHECK(mock.isInitialized());

    // Second initialization should fail
    FL_CHECK_FALSE(mock.initializeBus(config));
}

FL_TEST_CASE("SpiPeripheralMock - free bus") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(config));
    FL_CHECK(mock.freeBus());
    FL_CHECK_FALSE(mock.isInitialized());
}

FL_TEST_CASE("SpiPeripheralMock - free uninitialized bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    FL_CHECK_FALSE(mock.freeBus());
}

FL_TEST_CASE("SpiPeripheralMock - free bus with device attached fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Cannot free bus while device is attached
    FL_CHECK_FALSE(mock.freeBus());

    // Must remove device first
    FL_CHECK(mock.removeDevice());
    FL_CHECK(mock.freeBus());
}

//=============================================================================
// Device Management Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - add device") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));
    FL_CHECK(mock.hasDevice());

    // Verify stored configuration
    const auto& stored = mock.getDeviceConfig();
    FL_CHECK_EQ(stored.clock_speed_hz, 2500000);
    FL_CHECK_EQ(stored.queue_size, 3);
}

FL_TEST_CASE("SpiPeripheralMock - add device without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK_FALSE(mock.addDevice(device_config));
    FL_CHECK_FALSE(mock.hasDevice());
}

FL_TEST_CASE("SpiPeripheralMock - add device with invalid config fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    // Invalid: zero clock speed
    SpiDeviceConfig bad_clock(/*clock_hz=*/0, /*queue_size=*/3);
    FL_CHECK_FALSE(mock.addDevice(bad_clock));

    // Invalid: zero queue size
    SpiDeviceConfig bad_queue(/*clock_hz=*/2500000, /*queue_size=*/0);
    FL_CHECK_FALSE(mock.addDevice(bad_queue));
}

FL_TEST_CASE("SpiPeripheralMock - double add device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Second add should fail
    FL_CHECK_FALSE(mock.addDevice(device_config));
}

FL_TEST_CASE("SpiPeripheralMock - remove device") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));
    FL_CHECK(mock.hasDevice());

    FL_CHECK(mock.removeDevice());
    FL_CHECK_FALSE(mock.hasDevice());
}

FL_TEST_CASE("SpiPeripheralMock - remove non-existent device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    FL_CHECK_FALSE(mock.removeDevice());
}

//=============================================================================
// Transaction Queuing Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - queue single transaction") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Queue transaction
    uint8_t buffer[16] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    FL_CHECK(mock.queueTransaction(trans));

    // Verify transaction was queued
    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 1);
    FL_CHECK_EQ(mock.getTransactionCount(), 1);
}

FL_TEST_CASE("SpiPeripheralMock - queue transaction without device fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    // No device added
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    FL_CHECK_FALSE(mock.queueTransaction(trans));
}

FL_TEST_CASE("SpiPeripheralMock - queue transaction without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // No bus initialized
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    SpiTrans trans(buffer, 4);
    FL_CHECK_FALSE(mock.queueTransaction(trans));
}

FL_TEST_CASE("SpiPeripheralMock - queue multiple transactions") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Queue 3 transactions (queue size is 3)
    uint8_t buffer1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t buffer2[4] = {0x55, 0x66, 0x77, 0x88};
    uint8_t buffer3[4] = {0x99, 0xAA, 0xBB, 0xCC};

    FL_CHECK(mock.queueTransaction(SpiTrans(buffer1, 4)));
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer2, 4)));
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer3, 4)));

    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 3);
    FL_CHECK_EQ(mock.getTransactionCount(), 3);
}

FL_TEST_CASE("SpiPeripheralMock - queue overflow") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup with queue size 2
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/2);
    FL_CHECK(mock.addDevice(device_config));

    // Queue 2 transactions successfully
    uint8_t buffer1[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t buffer2[4] = {0x55, 0x66, 0x77, 0x88};
    uint8_t buffer3[4] = {0x99, 0xAA, 0xBB, 0xCC};

    FL_CHECK(mock.queueTransaction(SpiTrans(buffer1, 4)));
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer2, 4)));

    // Third transaction should fail (queue full)
    FL_CHECK_FALSE(mock.queueTransaction(SpiTrans(buffer3, 4)));
    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 2);
}

FL_TEST_CASE("SpiPeripheralMock - transaction failure injection") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Inject failure
    mock.setTransactionFailure(true);

    // Transaction should fail
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    FL_CHECK_FALSE(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Reset failure flag
    mock.setTransactionFailure(false);

    // Transaction should succeed
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
}

//=============================================================================
// Transaction History Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - capture transaction data") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Queue transaction with known data
    uint8_t buffer[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    SpiTrans trans(buffer, 4);
    FL_CHECK(mock.queueTransaction(trans));

    // Verify data was captured
    const auto& history = mock.getTransactionHistory();
    FL_CHECK_EQ(history.size(), 1);

    const auto& record = history[0];
    FL_CHECK_EQ(record.length_bits, 32);  // 4 bytes * 8 bits
    FL_CHECK_EQ(record.buffer_copy.size(), 4);
    FL_CHECK_EQ(record.buffer_copy[0], 0xDE);
    FL_CHECK_EQ(record.buffer_copy[1], 0xAD);
    FL_CHECK_EQ(record.buffer_copy[2], 0xBE);
    FL_CHECK_EQ(record.buffer_copy[3], 0xEF);
}

FL_TEST_CASE("SpiPeripheralMock - get last transaction data") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // No transactions yet
    auto last_data = mock.getLastTransactionData();
    FL_CHECK(last_data.empty());

    // Queue transaction
    uint8_t buffer[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Verify last data
    last_data = mock.getLastTransactionData();
    FL_CHECK_EQ(last_data.size(), 4);
    FL_CHECK_EQ(last_data[0], 0xCA);
    FL_CHECK_EQ(last_data[1], 0xFE);
    FL_CHECK_EQ(last_data[2], 0xBA);
    FL_CHECK_EQ(last_data[3], 0xBE);
}

FL_TEST_CASE("SpiPeripheralMock - clear transaction history") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Queue some transactions
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    FL_CHECK_EQ(mock.getTransactionHistory().size(), 2);

    // Clear history
    mock.clearTransactionHistory();

    FL_CHECK_EQ(mock.getTransactionHistory().size(), 0);
    FL_CHECK_EQ(mock.getTransactionCount(), 0);
    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 0);
}

//=============================================================================
// Callback Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - register callback") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    // Register callback
    void* callback = reinterpret_cast<void*>(testCallback);
    FL_CHECK(mock.registerCallback(callback, nullptr));
}

FL_TEST_CASE("SpiPeripheralMock - register callback without bus fails") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    void* callback = reinterpret_cast<void*>(testCallback);
    FL_CHECK_FALSE(mock.registerCallback(callback, nullptr));
}

FL_TEST_CASE("SpiPeripheralMock - simulate transaction complete triggers callback") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    // Register callback
    gCallbackCount = 0;
    void* callback = reinterpret_cast<void*>(testCallback);
    FL_CHECK(mock.registerCallback(callback, nullptr));

    // Queue transaction
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Manually trigger completion
    mock.simulateTransactionComplete();

    // Verify callback was called
    FL_CHECK_EQ(gCallbackCount, 1);
}

//=============================================================================
// DMA Memory Allocation Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - allocate DMA buffer") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Allocate buffer
    uint8_t* buffer = mock.allocateDma(128);
    FL_CHECK(buffer != nullptr);

    // Write to buffer (verify it's usable)
    buffer[0] = 0x42;
    FL_CHECK_EQ(buffer[0], 0x42);

    // Free buffer
    mock.freeDma(buffer);
}

FL_TEST_CASE("SpiPeripheralMock - allocate DMA buffer with alignment") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Allocate non-aligned size (should be rounded up)
    uint8_t* buffer = mock.allocateDma(17);  // Not a multiple of 4
    FL_CHECK(buffer != nullptr);

    // Verify 4-byte alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
    FL_CHECK_EQ(addr % 4, 0);

    mock.freeDma(buffer);
}

FL_TEST_CASE("SpiPeripheralMock - free nullptr is safe") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Should not crash
    mock.freeDma(nullptr);
}

//=============================================================================
// State Inspection Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - canQueueTransaction") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Not ready initially
    FL_CHECK_FALSE(mock.canQueueTransaction());

    // Setup
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/2);
    FL_CHECK(mock.addDevice(device_config));

    // Now ready
    FL_CHECK(mock.canQueueTransaction());

    // Queue 2 transactions (queue size is 2)
    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    // Queue full
    FL_CHECK_FALSE(mock.canQueueTransaction());
}

FL_TEST_CASE("SpiPeripheralMock - reset clears all state") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Setup and queue transaction
    SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
    FL_CHECK(mock.initializeBus(bus_config));

    SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
    FL_CHECK(mock.addDevice(device_config));

    uint8_t buffer[4] = {0x12, 0x34, 0x56, 0x78};
    FL_CHECK(mock.queueTransaction(SpiTrans(buffer, 4)));

    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.hasDevice());
    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 1);

    // Reset
    mock.reset();

    // All state cleared
    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(mock.hasDevice());
    FL_CHECK_EQ(mock.getQueuedTransactionCount(), 0);
    FL_CHECK_EQ(mock.getTransactionCount(), 0);
}

//=============================================================================
// Multi-Lane Configuration Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - dual-lane bus configuration") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Dual-lane configuration
    SpiBusConfig config(/*data0=*/23, /*data1=*/19, /*sclk=*/18, /*max_sz=*/4096);
    FL_CHECK(mock.initializeBus(config));

    const auto& stored = mock.getBusConfig();
    FL_CHECK_EQ(stored.mosi_pin, 23);  // Data0 → MOSI
    FL_CHECK_EQ(stored.miso_pin, 19);  // Data1 → MISO
    FL_CHECK_EQ(stored.sclk_pin, 18);
}

FL_TEST_CASE("SpiPeripheralMock - quad-lane bus configuration") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    // Quad-lane configuration
    SpiBusConfig config(/*data0=*/23, /*data1=*/19, /*data2=*/22, /*data3=*/21, /*sclk=*/18, /*max_sz=*/4096);
    FL_CHECK(mock.initializeBus(config));

    const auto& stored = mock.getBusConfig();
    FL_CHECK_EQ(stored.mosi_pin, 23);
    FL_CHECK_EQ(stored.miso_pin, 19);
    FL_CHECK_EQ(stored.data2_pin, 22);
    FL_CHECK_EQ(stored.data3_pin, 21);
    FL_CHECK_EQ(stored.sclk_pin, 18);
}

//=============================================================================
// Platform Utility Tests
//=============================================================================

FL_TEST_CASE("SpiPeripheralMock - getMicroseconds") {
    SpiPeripheralFixture fixture;
    auto& mock = fixture.mock;

    uint64_t t1 = mock.getMicroseconds();
    mock.delay(1);  // Delay 1ms
    uint64_t t2 = mock.getMicroseconds();

    // Time should advance (may not be exactly 1000us due to scheduling)
    FL_CHECK(t2 > t1);
}
