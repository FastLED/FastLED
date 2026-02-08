#include "platforms/shared/spi_types.h"
#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "platforms/shared/spi_hw_16.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/log.h"
#include "fl/promise.h"
#include "fl/result.h"
#include "fl/slice.h"
#include "fl/spi/config.h"
#include "fl/spi/device.h"
#include "fl/spi/transaction.h"
#include "fl/stl/cstring.h"
#include "fl/stl/move.h"
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/strstream.h"
#include "fl/stl/vector.h"

using namespace fl::spi;
using fl::SPIError;
using fl::DMABuffer;

// ============================================================================
// Test Fixture - Reset SPIBusManager before each test
// ============================================================================
// NOTE: Fixture temporarily disabled to investigate crash issues
// The Device destructors should handle all cleanup automatically via unregisterDevice()
// struct SPITestFixture {
//     SPITestFixture() {
//         fl::getSPIBusManager().reset();
//     }
// };

// ============================================================================
// Lazy Initialization Verification Tests
// ============================================================================

FL_TEST_CASE("Lazy Init - SPI stub instances are registered") {
    // Verify that lazy initialization successfully registers all SPI stub instances
    // This confirms the platform::initSpiHwNInstances() mechanism works correctly

    auto spi1_instances = fl::SpiHw1::getAll();
    auto spi2_instances = fl::SpiHw2::getAll();
    auto spi4_instances = fl::SpiHw4::getAll();
    auto spi8_instances = fl::SpiHw8::getAll();
    auto spi16_instances = fl::SpiHw16::getAll();

    FL_CHECK(spi1_instances.size() == 2);
    FL_CHECK(spi2_instances.size() == 2);
    FL_CHECK(spi4_instances.size() == 2);
    FL_CHECK(spi8_instances.size() == 2);
    FL_CHECK(spi16_instances.size() == 2);
}

// ============================================================================
// fl::Result<T> Tests
// ============================================================================

FL_TEST_CASE("fl::Result<void, SPIError> basic operations") {
    FL_SUBCASE("Default construction creates failure state") {
        fl::Result<void, SPIError> r;
        FL_CHECK(!r.ok());
        FL_CHECK(r.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("Success creation") {
        fl::Result<void, SPIError> r = fl::Result<void, SPIError>::success();
        FL_CHECK(r.ok());
        FL_CHECK(static_cast<bool>(r));  // Explicit bool conversion
    }

    FL_SUBCASE("Failure creation with error code") {
        fl::Result<void, SPIError> r = fl::Result<void, SPIError>::failure(SPIError::BUFFER_TOO_LARGE);
        FL_CHECK(!r.ok());
        FL_CHECK(r.error() == SPIError::BUFFER_TOO_LARGE);
    }

    FL_SUBCASE("Failure creation with error code and message") {
        fl::Result<void, SPIError> r = fl::Result<void, SPIError>::failure(SPIError::ALLOCATION_FAILED, "Out of memory");
        FL_CHECK(!r.ok());
        FL_CHECK(r.error() == SPIError::ALLOCATION_FAILED);
        FL_CHECK(r.message() != nullptr);
        FL_CHECK(fl::strcmp(r.message(), "Out of memory") == 0);
    }
}

FL_TEST_CASE("fl::Result<int> value type operations") {
    FL_SUBCASE("Success with value") {
        fl::Result<int, SPIError> r = fl::Result<int, SPIError>::success(42);
        FL_CHECK(r.ok());
        FL_CHECK(r.value() == 42);
    }

    FL_SUBCASE("Failure with no value") {
        fl::Result<int, SPIError> r = fl::Result<int, SPIError>::failure(SPIError::BUSY, "Device busy");
        FL_CHECK(!r.ok());
        FL_CHECK(r.error() == SPIError::BUSY);
        FL_CHECK(r.message() != nullptr);
    }

    FL_SUBCASE("Value modification") {
        fl::Result<int, SPIError> r = fl::Result<int, SPIError>::success(10);
        r.value() = 20;
        FL_CHECK(r.value() == 20);
    }
}

FL_TEST_CASE("fl::Result<Transaction, SPIError> example usage") {
    // This tests that Result works with complex types
    // (Transaction will be properly implemented in later iterations)

    FL_SUBCASE("Failure case") {
        fl::Result<int, SPIError> r = fl::Result<int, SPIError>::failure(SPIError::NOT_INITIALIZED);
        FL_CHECK(!r.ok());
        FL_CHECK(!static_cast<bool>(r));  // Bool conversion
    }
}

// ============================================================================
// Config Tests
// ============================================================================

FL_TEST_CASE("Config construction") {
    FL_SUBCASE("Basic construction with default values") {
        Config cfg(18, 23);
        FL_CHECK(cfg.clock_pin == 18);
        FL_CHECK(cfg.data_pins.size() == 1);
        FL_CHECK(cfg.data_pins[0] == 23);
        FL_CHECK(cfg.clock_speed_hz == 0xffffffff);  // As fast as possible
        FL_CHECK(cfg.spi_mode == 0);
    }

    FL_SUBCASE("Configuration modification") {
        Config cfg(5, 6);
        cfg.clock_speed_hz = 20000000;  // 20 MHz
        cfg.spi_mode = 1;

        FL_CHECK(cfg.clock_speed_hz == 20000000);
        FL_CHECK(cfg.spi_mode == 1);
    }
}

// ============================================================================
// Device Construction Tests (Basic)
// ============================================================================

FL_TEST_CASE("Device construction (basic)") {
    FL_SUBCASE("Device can be constructed") {
        Config cfg(18, 23);
        Device spi(cfg);

        // Device should not be ready until begin() is called
        FL_CHECK(!spi.isReady());
    }

    FL_SUBCASE("Device configuration access") {
        Config cfg(5, 6);
        cfg.clock_speed_hz = 15000000;
        Device spi(cfg);

        const Config& stored_cfg = spi.getConfig();
        FL_CHECK(stored_cfg.clock_pin == 5);
        FL_CHECK(stored_cfg.data_pins.size() == 1);
        FL_CHECK(stored_cfg.data_pins[0] == 6);
        FL_CHECK(stored_cfg.clock_speed_hz == 15000000);
    }
}

// ============================================================================
// Device Initialization Tests (Iteration 2)
// ============================================================================

FL_TEST_CASE("Device initialization with begin()") {
    FL_SUBCASE("Device starts not ready") {
        Config cfg(18, 23);
        Device spi(cfg);
        FL_CHECK(!spi.isReady());
    }

    FL_SUBCASE("begin() initializes device") {
        Config cfg(18, 23);
        Device spi(cfg);

        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(!result);  // No error means success
        FL_CHECK(spi.isReady());
    }

    FL_SUBCASE("Double begin() is idempotent") {
        Config cfg(18, 23);
        Device spi(cfg);

        fl::optional<fl::Error> result1 = spi.begin();
        FL_CHECK(!result1);  // No error means success
        FL_CHECK(spi.isReady());

        // Second begin() should also succeed
        fl::optional<fl::Error> result2 = spi.begin();
        FL_CHECK(!result2);  // No error means success
        FL_CHECK(spi.isReady());
    }

    FL_SUBCASE("end() shuts down device") {
        Config cfg(18, 23);
        Device spi(cfg);

        spi.begin();
        FL_CHECK(spi.isReady());

        spi.end();
        FL_CHECK(!spi.isReady());
    }

    FL_SUBCASE("Multiple begin/end cycles") {
        Config cfg(18, 23);
        Device spi(cfg);

        // First cycle
        spi.begin();
        FL_CHECK(spi.isReady());
        spi.end();
        FL_CHECK(!spi.isReady());

        // Second cycle
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(!result);  // No error means success
        FL_CHECK(spi.isReady());
        spi.end();
        FL_CHECK(!spi.isReady());
    }

    // NOTE: write(), read(), transfer() methods are not implemented in Device class
    // Device only provides writeAsync() and zero-copy DMA API
    // FL_SUBCASE("Operations fail when not initialized") {
    //     Config cfg(18, 23);
    //     Device spi(cfg);
    //
    //     // Try operations without calling begin()
    //     uint8_t data[4] = {1, 2, 3, 4};
    //     uint8_t buffer[4] = {0};
    //
    //     fl::Result<void, SPIError> write_result = spi.write(data, 4);
    //     FL_CHECK(!write_result.ok());
    //     FL_CHECK(write_result.error() == SPIError::NOT_INITIALIZED);
    //
    //     fl::Result<void, SPIError> read_result = spi.read(buffer, 4);
    //     FL_CHECK(!read_result.ok());
    //     FL_CHECK(read_result.error() == SPIError::NOT_INITIALIZED);
    //
    //     fl::Result<void, SPIError> transfer_result = spi.transfer(data, buffer, 4);
    //     FL_CHECK(!transfer_result.ok());
    //     FL_CHECK(transfer_result.error() == SPIError::NOT_INITIALIZED);
    // }
}

FL_TEST_CASE("Device destructor cleanup") {
    FL_SUBCASE("Destructor cleans up initialized device") {
        Config cfg(18, 23);
        {
            Device spi(cfg);
            spi.begin();
            FL_CHECK(spi.isReady());
            // Destructor should be called here
        }
        // If we get here without crashes, RAII cleanup worked
        FL_CHECK(true);
    }

    FL_SUBCASE("Destructor handles uninitialized device") {
        Config cfg(18, 23);
        {
            Device spi(cfg);
            // Never call begin()
            FL_CHECK(!spi.isReady());
            // Destructor should handle this gracefully
        }
        FL_CHECK(true);
    }
}

FL_TEST_CASE("Device state transitions") {
    Config cfg(18, 23);
    Device spi(cfg);

    FL_SUBCASE("Initial state") {
        FL_CHECK(!spi.isReady());
        FL_CHECK(!spi.isBusy());
    }

    FL_SUBCASE("After begin()") {
        spi.begin();
        FL_CHECK(spi.isReady());
        FL_CHECK(!spi.isBusy());
    }

    FL_SUBCASE("After end()") {
        spi.begin();
        spi.end();
        FL_CHECK(!spi.isReady());
        FL_CHECK(!spi.isBusy());
    }
}

FL_TEST_CASE("Device configuration updates") {
    FL_SUBCASE("Clock speed can be updated") {
        Config cfg(18, 23);
        cfg.clock_speed_hz = 10000000;
        Device spi(cfg);

        fl::optional<fl::Error> result = spi.setClockSpeed(20000000);
        FL_CHECK(!result);  // No error means success

        const Config& stored = spi.getConfig();
        FL_CHECK(stored.clock_speed_hz == 20000000);
    }

    FL_SUBCASE("Configuration persists after begin()") {
        Config cfg(18, 23);
        cfg.clock_speed_hz = 15000000;
        cfg.spi_mode = 2;

        Device spi(cfg);
        spi.begin();

        const Config& stored = spi.getConfig();
        FL_CHECK(stored.clock_pin == 18);
        FL_CHECK(stored.data_pins.size() == 1);
        FL_CHECK(stored.data_pins[0] == 23);
        FL_CHECK(stored.clock_speed_hz == 15000000);
        FL_CHECK(stored.spi_mode == 2);
    }
}

FL_TEST_CASE("Multiple devices on different pins") {
    FL_SUBCASE("Two devices can coexist") {
        // Use different clock pins to avoid conflicts with previous tests
        // Note: SPIBusManager is a global singleton and not reset between tests
        Config cfg1(10, 11);
        Config cfg2(12, 13);

        Device spi1(cfg1);
        Device spi2(cfg2);

        fl::optional<fl::Error> result1 = spi1.begin();
        fl::optional<fl::Error> result2 = spi2.begin();

        FL_CHECK(!result1);  // No error means success
        FL_CHECK(!result2);  // No error means success
        FL_CHECK(spi1.isReady());
        FL_CHECK(spi2.isReady());

        // Cleanup
        spi1.end();
        spi2.end();
    }
}

// ============================================================================
// Device Write Operations Tests (Iteration 3)
// ============================================================================
// NOTE: Device class does not have write(), read(), transfer() methods
// It only provides writeAsync() and zero-copy DMA API (acquireBuffer/transmit)
// These tests are commented out

/*
FL_TEST_CASE("Device write operations") {
    FL_SUBCASE("Basic write succeeds") {
        Config cfg(14, 15);
        Device spi(cfg);

        fl::Result<void, SPIError> begin_result = spi.begin();
        FL_REQUIRE(begin_result.ok());

        uint8_t data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        fl::Result<void, SPIError> write_result = spi.write(data, 4);

        FL_CHECK(write_result.ok());
        FL_CHECK(!spi.isBusy());  // Should be complete after blocking write

        spi.end();
    }

    FL_SUBCASE("Write fails without begin()") {
        Config cfg(14, 15);
        Device spi(cfg);

        uint8_t data[4] = {1, 2, 3, 4};
        fl::Result<void, SPIError> result = spi.write(data, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("Write fails with null data") {
        Config cfg(14, 15);
        Device spi(cfg);
        spi.begin();

        fl::Result<void, SPIError> result = spi.write(nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Write fails with zero size") {
        Config cfg(14, 15);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {1, 2, 3, 4};
        fl::Result<void, SPIError> result = spi.write(data, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Multiple writes work correctly") {
        Config cfg(14, 15);
        Device spi(cfg);
        spi.begin();

        uint8_t data1[4] = {0x11, 0x22, 0x33, 0x44};
        uint8_t data2[8] = {0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};

        fl::Result<void, SPIError> result1 = spi.write(data1, 4);
        FL_CHECK(result1.ok());

        fl::Result<void, SPIError> result2 = spi.write(data2, 8);
        FL_CHECK(result2.ok());

        spi.end();
    }

    FL_SUBCASE("Write with varying buffer sizes") {
        Config cfg(14, 15);
        Device spi(cfg);
        spi.begin();

        // Small buffer
        uint8_t small[2] = {0x01, 0x02};
        fl::Result<void, SPIError> result_small = spi.write(small, 2);
        FL_CHECK(result_small.ok());

        // Medium buffer
        uint8_t medium[64];
        for (size_t i = 0; i < 64; i++) medium[i] = static_cast<uint8_t>(i);
        fl::Result<void, SPIError> result_medium = spi.write(medium, 64);
        FL_CHECK(result_medium.ok());

        // Large buffer
        uint8_t large[256];
        for (size_t i = 0; i < 256; i++) large[i] = static_cast<uint8_t>(i);
        fl::Result<void, SPIError> result_large = spi.write(large, 256);
        FL_CHECK(result_large.ok());

        spi.end();
    }
}
*/

FL_TEST_CASE("Device buffer acquisition") {
    FL_SUBCASE("acquireBuffer returns valid buffer") {
        Config cfg(16, 17);
        Device spi(cfg);
        spi.begin();

        DMABuffer buffer = spi.acquireBuffer(64);

        FL_CHECK(buffer.ok());
        FL_CHECK(buffer.size() == 64);

        spi.end();
    }

    FL_SUBCASE("acquireBuffer fails without begin()") {
        Config cfg(16, 17);
        Device spi(cfg);

        DMABuffer buffer = spi.acquireBuffer(64);

        FL_CHECK(!buffer.ok());
        FL_CHECK(buffer.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("Multiple buffer acquisitions work") {
        Config cfg(16, 17);
        Device spi(cfg);
        spi.begin();

        DMABuffer buffer1 = spi.acquireBuffer(32);
        FL_CHECK(buffer1.ok());

        DMABuffer buffer2 = spi.acquireBuffer(64);
        FL_CHECK(buffer2.ok());

        spi.end();
    }
}

FL_TEST_CASE("Device transmit operations") {
    FL_SUBCASE("Transmit with valid buffer succeeds (blocking)") {
        Config cfg(16, 17);
        Device spi(cfg);
        spi.begin();

        DMABuffer buffer = spi.acquireBuffer(16);
        FL_REQUIRE(buffer.ok());

        // Fill buffer with test data
        fl::span<uint8_t> data = buffer.data();
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = static_cast<uint8_t>(i);
        }

        fl::optional<fl::Error> result = spi.transmit(buffer, false);  // Blocking
        FL_CHECK(!result);  // No error means success
        FL_CHECK(!spi.isBusy());

        spi.end();
    }

    FL_SUBCASE("Transmit fails without begin()") {
        Config cfg(16, 17);
        Device spi(cfg);

        DMABuffer buffer(64);
        fl::optional<fl::Error> result = spi.transmit(buffer, false);

        FL_CHECK(result.has_value());  // Error present means failure
    }

    FL_SUBCASE("Transmit with invalid buffer fails") {
        Config cfg(16, 17);
        Device spi(cfg);
        spi.begin();

        DMABuffer invalid_buffer(SPIError::ALLOCATION_FAILED);
        fl::optional<fl::Error> result = spi.transmit(invalid_buffer, false);

        FL_CHECK(result.has_value());  // Error present means failure

        spi.end();
    }
}

FL_TEST_CASE("Device busy state and waitComplete") {
    FL_SUBCASE("Device not busy after initialization") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        FL_CHECK(!spi.isBusy());

        spi.end();
    }

    FL_SUBCASE("Device not busy after blocking write") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {1, 2, 3, 4};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
        if (!result.ok()) {
            FL_DBG("writeAsync failed with error: " << result.error());
        }
        FL_REQUIRE(result.ok());  // REQUIRE instead of CHECK to stop if fails
        Transaction txn = fl::move(result.value());
        FL_CHECK(txn.wait());  // Wait for completion (blocking)
        FL_CHECK(!spi.isBusy());

        spi.end();
    }

    FL_SUBCASE("waitComplete returns true when not busy") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        bool completed = spi.waitComplete(1000);
        FL_CHECK(completed);

        spi.end();
    }
}

// Note: Buffer caching is handled internally by the hardware controller (SpiHw1/2/4/8),
// not at the Device level. This simplifies the API and ensures proper synchronization.

// ============================================================================
// Device Read Operations (Iteration 4)
// ============================================================================
// NOTE: Device class does not have read() method (TX-only API)
// These tests are commented out

/*
FL_TEST_CASE("Device read operations") {
    FL_SUBCASE("Basic read succeeds") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        fl::Result<void, SPIError> result = spi.read(buffer, 4);

        FL_CHECK(result.ok());
        // Note: Current implementation fills buffer with 0x00 until hardware RX is supported
        FL_CHECK(buffer[0] == 0x00);
        FL_CHECK(buffer[1] == 0x00);
        FL_CHECK(buffer[2] == 0x00);
        FL_CHECK(buffer[3] == 0x00);

        spi.end();
    }

    FL_SUBCASE("Read fails without begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        uint8_t buffer[4];
        fl::Result<void, SPIError> result = spi.read(buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("Read fails with null buffer") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        fl::Result<void, SPIError> result = spi.read(nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Read fails with zero size") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t buffer[4];
        fl::Result<void, SPIError> result = spi.read(buffer, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Multiple reads work correctly") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t buffer1[2];
        uint8_t buffer2[4];

        fl::Result<void, SPIError> result1 = spi.read(buffer1, 2);
        FL_CHECK(result1.ok());

        fl::Result<void, SPIError> result2 = spi.read(buffer2, 4);
        FL_CHECK(result2.ok());

        spi.end();
    }

    FL_SUBCASE("Read with varying buffer sizes") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        // Small buffer
        uint8_t small[2];
        FL_CHECK(spi.read(small, 2).ok());

        // Medium buffer
        uint8_t medium[64];
        FL_CHECK(spi.read(medium, 64).ok());

        // Large buffer
        uint8_t large[256];
        FL_CHECK(spi.read(large, 256).ok());

        spi.end();
    }
}
*/

// ============================================================================
// Device Transfer Operations (Iteration 4)
// ============================================================================
// NOTE: Device class does not have transfer() method (TX-only API)
// These tests are commented out

/*
FL_TEST_CASE("Device transfer operations") {
    FL_SUBCASE("Basic transfer succeeds") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint8_t rx_buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};

        fl::Result<void, SPIError> result = spi.transfer(tx_data, rx_buffer, 4);

        FL_CHECK(result.ok());
        // Note: Current implementation fills RX buffer with 0x00 until hardware RX is supported
        // But TX data should still be transmitted
        FL_CHECK(rx_buffer[0] == 0x00);
        FL_CHECK(rx_buffer[1] == 0x00);
        FL_CHECK(rx_buffer[2] == 0x00);
        FL_CHECK(rx_buffer[3] == 0x00);

        spi.end();
    }

    FL_SUBCASE("Transfer fails without begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        uint8_t tx_data[4] = {1, 2, 3, 4};
        uint8_t rx_buffer[4];

        fl::Result<void, SPIError> result = spi.transfer(tx_data, rx_buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("Transfer fails with null tx_data") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t rx_buffer[4];
        fl::Result<void, SPIError> result = spi.transfer(nullptr, rx_buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Transfer fails with null rx_buffer") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {1, 2, 3, 4};
        fl::Result<void, SPIError> result = spi.transfer(tx_data, nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Transfer fails with zero size") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {1, 2, 3, 4};
        uint8_t rx_buffer[4];
        fl::Result<void, SPIError> result = spi.transfer(tx_data, rx_buffer, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Multiple transfers work correctly") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx1[2] = {0x01, 0x02};
        uint8_t rx1[2];
        uint8_t tx2[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint8_t rx2[4];

        fl::Result<void, SPIError> result1 = spi.transfer(tx1, rx1, 2);
        FL_CHECK(result1.ok());

        fl::Result<void, SPIError> result2 = spi.transfer(tx2, rx2, 4);
        FL_CHECK(result2.ok());

        spi.end();
    }

    FL_SUBCASE("Transfer with varying buffer sizes") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        // Small buffer
        uint8_t tx_small[2] = {0x01, 0x02};
        uint8_t rx_small[2];
        FL_CHECK(spi.transfer(tx_small, rx_small, 2).ok());

        // Medium buffer
        uint8_t tx_medium[64];
        uint8_t rx_medium[64];
        for (size_t i = 0; i < 64; i++) tx_medium[i] = i & 0xFF;
        FL_CHECK(spi.transfer(tx_medium, rx_medium, 64).ok());

        // Large buffer
        uint8_t tx_large[256];
        uint8_t rx_large[256];
        for (size_t i = 0; i < 256; i++) tx_large[i] = i & 0xFF;
        FL_CHECK(spi.transfer(tx_large, rx_large, 256).ok());

        spi.end();
    }

    FL_SUBCASE("Transfer transmits TX data correctly") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {0x12, 0x34, 0x56, 0x78};
        uint8_t rx_buffer[4];

        fl::Result<void, SPIError> result = spi.transfer(tx_data, rx_buffer, 4);
        FL_CHECK(result.ok());

        // We can't directly verify that TX data was transmitted since we don't have
        // access to the stub's internal state from here, but we can verify that
        // the transfer operation completed successfully

        spi.end();
    }
}
*/

// ============================================================================
// Async Write Tests (Iteration 5)
// ============================================================================

FL_TEST_CASE("Device writeAsync operations") {
    FL_SUBCASE("Basic writeAsync succeeds and returns Transaction") {
        Config cfg(18, 19);
        Device spi(cfg);
        fl::optional<fl::Error> begin_result = spi.begin();
        FL_REQUIRE(!begin_result);  // Must succeed for test to continue

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);

        FL_REQUIRE(result.ok());  // Must succeed to safely access value()

        // Get transaction and wait for completion
        Transaction txn = fl::move(result.value());
        FL_CHECK(txn.wait());
        FL_CHECK(txn.isDone());
        FL_CHECK(!txn.isPending());
        FL_CHECK(!txn.getResult());  // No error means success

        spi.end();
    }

    FL_SUBCASE("writeAsync fails without begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("writeAsync fails with null data") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        fl::Result<Transaction, SPIError> result = spi.writeAsync(nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("writeAsync fails with zero size") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("Multiple sequential async writes work") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        // First async write
        uint8_t data1[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result1 = spi.writeAsync(data1, 4);
        FL_CHECK(result1.ok());
        Transaction txn1 = fl::move(result1.value());
        FL_CHECK(txn1.wait());
        FL_CHECK(txn1.isDone());

        // Second async write (after first completes)
        uint8_t data2[4] = {0x05, 0x06, 0x07, 0x08};
        fl::Result<Transaction, SPIError> result2 = spi.writeAsync(data2, 4);
        FL_CHECK(result2.ok());
        Transaction txn2 = fl::move(result2.value());
        FL_CHECK(txn2.wait());
        FL_CHECK(txn2.isDone());

        spi.end();
    }

    FL_SUBCASE("writeAsync with varying buffer sizes") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        // Small buffer
        uint8_t small[2] = {0x01, 0x02};
        fl::Result<Transaction, SPIError> r1 = spi.writeAsync(small, 2);
        FL_CHECK(r1.ok());
        Transaction t1 = fl::move(r1.value());
        FL_CHECK(t1.wait());

        // Medium buffer
        uint8_t medium[64];
        for (size_t i = 0; i < 64; i++) medium[i] = i & 0xFF;
        fl::Result<Transaction, SPIError> r2 = spi.writeAsync(medium, 64);
        FL_CHECK(r2.ok());
        Transaction t2 = fl::move(r2.value());
        FL_CHECK(t2.wait());

        // Large buffer
        uint8_t large[256];
        for (size_t i = 0; i < 256; i++) large[i] = i & 0xFF;
        fl::Result<Transaction, SPIError> r3 = spi.writeAsync(large, 256);
        FL_CHECK(r3.ok());
        Transaction t3 = fl::move(r3.value());
        FL_CHECK(t3.wait());

        spi.end();
    }

    FL_SUBCASE("Transaction auto-waits on destruction") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};

        {
            fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
            FL_CHECK(result.ok());
            Transaction txn = fl::move(result.value());
            // Transaction destructor will auto-wait
        }

        // After transaction is destroyed, we should be able to start another
        fl::Result<Transaction, SPIError> result2 = spi.writeAsync(data, 4);
        FL_CHECK(result2.ok());

        spi.end();
    }
}

// ============================================================================
// Async Read Tests (Iteration 5)
// ============================================================================
// NOTE: Device class does not have readAsync() method (TX-only API)
// These tests are commented out

/*
FL_TEST_CASE("Device readAsync operations") {
    FL_SUBCASE("Basic readAsync succeeds and returns Transaction") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t buffer[4];
        fl::Result<Transaction, SPIError> result = spi.readAsync(buffer, 4);

        FL_CHECK(result.ok());

        Transaction txn = fl::move(result.value());
        FL_CHECK(txn.isDone());  // Should complete immediately (stub implementation)
        FL_CHECK(txn.getResult().ok());

        // Buffer should be filled with 0x00 (no RX data yet)
        for (size_t i = 0; i < 4; i++) {
            FL_CHECK(buffer[i] == 0x00);
        }

        spi.end();
    }

    FL_SUBCASE("readAsync fails without begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        uint8_t buffer[4];
        fl::Result<Transaction, SPIError> result = spi.readAsync(buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("readAsync fails with null buffer") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        fl::Result<Transaction, SPIError> result = spi.readAsync(nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("readAsync fails with zero size") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t buffer[4];
        fl::Result<Transaction, SPIError> result = spi.readAsync(buffer, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }
}
*/

// ============================================================================
// Async Transfer Tests (Iteration 5)
// ============================================================================
// NOTE: Device class does not have transferAsync() method (TX-only API)
// These tests are commented out

/*
FL_TEST_CASE("Device transferAsync operations") {
    FL_SUBCASE("Basic transferAsync succeeds and returns Transaction") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {0x12, 0x34, 0x56, 0x78};
        uint8_t rx_buffer[4];
        fl::Result<Transaction, SPIError> result = spi.transferAsync(tx_data, rx_buffer, 4);

        FL_CHECK(result.ok());

        Transaction txn = fl::move(result.value());
        FL_CHECK(txn.wait());
        FL_CHECK(txn.isDone());
        FL_CHECK(txn.getResult().ok());

        // RX buffer should be filled with 0x00 (no RX data yet)
        for (size_t i = 0; i < 4; i++) {
            FL_CHECK(rx_buffer[i] == 0x00);
        }

        spi.end();
    }

    FL_SUBCASE("transferAsync fails without begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        uint8_t tx_data[4] = {0x01, 0x02, 0x03, 0x04};
        uint8_t rx_buffer[4];
        fl::Result<Transaction, SPIError> result = spi.transferAsync(tx_data, rx_buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::NOT_INITIALIZED);
    }

    FL_SUBCASE("transferAsync fails with null tx_data") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t rx_buffer[4];
        fl::Result<Transaction, SPIError> result = spi.transferAsync(nullptr, rx_buffer, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("transferAsync fails with null rx_buffer") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.transferAsync(tx_data, nullptr, 4);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }

    FL_SUBCASE("transferAsync fails with zero size") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t tx_data[4] = {0x01, 0x02, 0x03, 0x04};
        uint8_t rx_buffer[4];
        fl::Result<Transaction, SPIError> result = spi.transferAsync(tx_data, rx_buffer, 0);

        FL_CHECK(!result.ok());
        FL_CHECK(result.error() == SPIError::ALLOCATION_FAILED);

        spi.end();
    }
}
*/

// ============================================================================
// Transaction Tests (Iteration 5)
// ============================================================================

FL_TEST_CASE("Transaction lifecycle") {
    FL_SUBCASE("Transaction isDone() and isPending() work correctly") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
        FL_CHECK(result.ok());

        Transaction txn = fl::move(result.value());

        // Wait for completion
        FL_CHECK(txn.wait());

        // After wait, should be done
        FL_CHECK(txn.isDone());
        FL_CHECK(!txn.isPending());

        spi.end();
    }

    FL_SUBCASE("Transaction cancel() marks as completed") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
        FL_CHECK(result.ok());

        Transaction txn = fl::move(result.value());

        // Cancel immediately
        bool cancelled = txn.cancel();
        FL_CHECK(cancelled);
        FL_CHECK(txn.isDone());
        FL_CHECK(!txn.isPending());

        // Calling cancel again should fail
        FL_CHECK(!txn.cancel());

        spi.end();
    }

    FL_SUBCASE("Transaction getResult() returns correct result") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
        FL_CHECK(result.ok());

        Transaction txn = fl::move(result.value());
        FL_CHECK(txn.wait());

        fl::optional<fl::Error> txn_result = txn.getResult();
        FL_CHECK(!txn_result);  // No error means success

        spi.end();
    }

    FL_SUBCASE("Transaction move semantics work") {
        Config cfg(18, 19);
        Device spi(cfg);
        spi.begin();

        uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
        fl::Result<Transaction, SPIError> result = spi.writeAsync(data, 4);
        FL_CHECK(result.ok());

        Transaction txn1 = fl::move(result.value());

        // Move to another transaction
        Transaction txn2 = fl::move(txn1);

        FL_CHECK(txn2.wait());
        FL_CHECK(txn2.isDone());

        spi.end();
    }
}

// ============================================================================
// Configuration Management Tests (Iteration 7)
// ============================================================================

FL_TEST_CASE("Device configuration management") {
    FL_SUBCASE("getConfig() returns correct configuration") {
        Config cfg(18, 19);
        cfg.clock_speed_hz = 5000000;  // 5 MHz
        cfg.spi_mode = 0;

        Device spi(cfg);

        const Config& retrieved = spi.getConfig();
        FL_CHECK(retrieved.clock_pin == 18);
        FL_CHECK(retrieved.data_pins.size() == 1);
        FL_CHECK(retrieved.data_pins[0] == 19);
        FL_CHECK(retrieved.clock_speed_hz == 5000000);
        FL_CHECK(retrieved.spi_mode == 0);
    }

    FL_SUBCASE("setClockSpeed() updates configuration before begin()") {
        Config cfg(18, 19);
        cfg.clock_speed_hz = 10000000;  // 10 MHz

        Device spi(cfg);

        // Update clock speed before initialization
        fl::optional<fl::Error> result = spi.setClockSpeed(20000000);  // 20 MHz
        FL_CHECK(!result);  // No error means success

        // Verify configuration was updated
        const Config& retrieved = spi.getConfig();
        FL_CHECK(retrieved.clock_speed_hz == 20000000);
    }

    FL_SUBCASE("setClockSpeed() updates configuration after begin()") {
        Config cfg(18, 19);
        Device spi(cfg);

        FL_CHECK(!spi.begin());  // No error means success

        // Update clock speed after initialization
        fl::optional<fl::Error> result = spi.setClockSpeed(15000000);  // 15 MHz
        FL_CHECK(!result);  // No error means success

        // Verify configuration was updated
        const Config& retrieved = spi.getConfig();
        FL_CHECK(retrieved.clock_speed_hz == 15000000);

        // Note: Hardware clock speed won't change until next begin()
        // This is documented behavior

        spi.end();
    }

    FL_SUBCASE("setClockSpeed() with zero speed") {
        Config cfg(18, 19);
        Device spi(cfg);

        // Setting to zero should succeed (though may not be practical)
        fl::optional<fl::Error> result = spi.setClockSpeed(0);
        FL_CHECK(!result);  // No error means success
        FL_CHECK(spi.getConfig().clock_speed_hz == 0);
    }

    FL_SUBCASE("setClockSpeed() with very high speed") {
        Config cfg(18, 19);
        Device spi(cfg);

        // Setting to very high speed should succeed
        // (hardware will clamp to maximum supported speed)
        fl::optional<fl::Error> result = spi.setClockSpeed(80000000);  // 80 MHz
        FL_CHECK(!result);  // No error means success
        FL_CHECK(spi.getConfig().clock_speed_hz == 80000000);
    }
}

FL_TEST_CASE("SPI mode configuration") {
    FL_SUBCASE("Mode 0 (default) is accepted") {
        Config cfg(18, 19);
        cfg.spi_mode = 0;

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(!result);  // No error means success

        spi.end();
    }

    FL_SUBCASE("Mode 1 generates warning but initialization succeeds") {
        Config cfg(18, 19);
        cfg.spi_mode = 1;

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        // Should succeed despite warning (mode is ignored)
        FL_CHECK(!result);  // No error means success

        spi.end();
    }

    FL_SUBCASE("Mode 2 generates warning but initialization succeeds") {
        Config cfg(18, 19);
        cfg.spi_mode = 2;

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(!result);  // No error means success

        spi.end();
    }

    FL_SUBCASE("Mode 3 generates warning but initialization succeeds") {
        Config cfg(18, 19);
        cfg.spi_mode = 3;

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(!result);  // No error means success

        spi.end();
    }

    FL_SUBCASE("Invalid mode (>3) is rejected") {
        Config cfg(18, 19);
        cfg.spi_mode = 4;  // Invalid

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(result.has_value());  // Error present means failure
    }

    FL_SUBCASE("Invalid mode (255) is rejected") {
        Config cfg(18, 19);
        cfg.spi_mode = 255;  // Very invalid

        Device spi(cfg);
        fl::optional<fl::Error> result = spi.begin();
        FL_CHECK(result.has_value());  // Error present means failure
    }

    FL_SUBCASE("Mode configuration is preserved in getConfig()") {
        Config cfg(18, 19);
        cfg.spi_mode = 2;

        Device spi(cfg);

        const Config& retrieved = spi.getConfig();
        FL_CHECK(retrieved.spi_mode == 2);
    }
}
