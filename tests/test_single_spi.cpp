// Test suite for Single-SPI functionality

#include "test.h"
#include "FastLED.h"

using namespace fl;

#ifdef FASTLED_TESTING
#include "platforms/stub/spi_single_stub.h"

// ============================================================================
// Hardware Interface Tests
// ============================================================================

TEST_CASE("SPISingle: Hardware initialization") {
    const auto& controllers = SPISingle::getAll();
    CHECK(controllers.size() > 0);

    SPISingle* single = controllers[0];
    CHECK(single != nullptr);

    SPISingle::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 40000000;
    config.clock_pin = 18;
    config.data_pin = 23;

    CHECK(single->begin(config));
    CHECK(single->isInitialized());
    CHECK_EQ(single->getBusId(), 0);

    single->end();
    CHECK_FALSE(single->isInitialized());
}

TEST_CASE("SPISingle: Blocking transmission behavior") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    SPISingle::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 40000000;
    config.clock_pin = 18;
    config.data_pin = 23;

    CHECK(single->begin(config));

    vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};

    // transmitAsync should be BLOCKING - completes immediately
    CHECK(single->transmitAsync(fl::span<const uint8_t>(data)));

    // Should NOT be busy after transmitAsync returns (because it's blocking)
    CHECK_FALSE(single->isBusy());

    // waitComplete should return immediately
    CHECK(single->waitComplete());
    CHECK_FALSE(single->isBusy());

    single->end();
}

TEST_CASE("SPISingle: Empty buffer transmission") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    SPISingle::Config config;
    config.bus_num = 0;
    CHECK(single->begin(config));

    vector<uint8_t> empty_data;
    CHECK(single->transmitAsync(fl::span<const uint8_t>(empty_data)));

    single->end();
}

TEST_CASE("SPISingle: Multiple transmissions") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    SPISingle::Config config;
    config.bus_num = 0;
    CHECK(single->begin(config));

    // First transmission
    vector<uint8_t> data1 = {0xAA, 0xBB};
    CHECK(single->transmitAsync(fl::span<const uint8_t>(data1)));
    CHECK_FALSE(single->isBusy());  // Blocking, so not busy

    // Second transmission (should work immediately since first is complete)
    vector<uint8_t> data2 = {0xCC, 0xDD};
    CHECK(single->transmitAsync(fl::span<const uint8_t>(data2)));
    CHECK_FALSE(single->isBusy());

    single->end();
}

TEST_CASE("SPISingle: Transmission without initialization fails") {
    const auto& controllers = SPISingle::getAll();
    SPISingleStub* stub = toStub(controllers[0]);

    stub->reset();
    stub->end();  // Ensure not initialized

    vector<uint8_t> data = {0x12, 0x34};
    CHECK_FALSE(stub->transmitAsync(fl::span<const uint8_t>(data)));
}

TEST_CASE("SPISingle: Stub inspection") {
    const auto& controllers = SPISingle::getAll();
    SPISingleStub* stub = toStub(controllers[0]);
    CHECK(stub != nullptr);

    stub->reset();

    SPISingle::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 20000000;
    CHECK(stub->begin(config));
    CHECK_EQ(stub->getClockSpeed(), 20000000);

    vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
    CHECK(stub->transmitAsync(fl::span<const uint8_t>(test_data)));

    const auto& transmitted = stub->getLastTransmission();
    CHECK_EQ(transmitted.size(), 4);
    CHECK_EQ(transmitted[0], 0xAA);
    CHECK_EQ(transmitted[1], 0xBB);
    CHECK_EQ(transmitted[2], 0xCC);
    CHECK_EQ(transmitted[3], 0xDD);

    CHECK_EQ(stub->getTransmissionCount(), 1);

    stub->end();
}

TEST_CASE("SPISingle: Transmission count tracking") {
    const auto& controllers = SPISingle::getAll();
    SPISingleStub* stub = toStub(controllers[0]);

    stub->reset();

    SPISingle::Config config;
    CHECK(stub->begin(config));

    CHECK_EQ(stub->getTransmissionCount(), 0);

    vector<uint8_t> data = {0x11, 0x22};
    stub->transmitAsync(fl::span<const uint8_t>(data));
    CHECK_EQ(stub->getTransmissionCount(), 1);

    stub->transmitAsync(fl::span<const uint8_t>(data));
    CHECK_EQ(stub->getTransmissionCount(), 2);

    stub->transmitAsync(fl::span<const uint8_t>(data));
    CHECK_EQ(stub->getTransmissionCount(), 3);

    stub->reset();
    CHECK_EQ(stub->getTransmissionCount(), 0);

    stub->end();
}

TEST_CASE("SPISingle: Bus ID validation") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    SPISingle::Config config;
    config.bus_num = 0;
    CHECK(single->begin(config));
    CHECK_EQ(single->getBusId(), 0);

    single->end();
}

TEST_CASE("SPISingle: Name retrieval") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    const char* name = single->getName();
    CHECK(name != nullptr);
    // Just verify it's a valid string, don't check exact value
    CHECK(strlen(name) > 0);
}

TEST_CASE("SPISingle: Multiple controllers available") {
    const auto& controllers = SPISingle::getAll();

    // Should have at least 2 mock controllers in test environment
    CHECK(controllers.size() >= 2);

    // Verify they're distinct instances
    if (controllers.size() >= 2) {
        CHECK(controllers[0] != controllers[1]);
        CHECK(controllers[0]->getBusId() != controllers[1]->getBusId());
    }
}

TEST_CASE("SPISingle: Large data transmission") {
    const auto& controllers = SPISingle::getAll();
    SPISingleStub* stub = toStub(controllers[0]);

    stub->reset();

    SPISingle::Config config;
    CHECK(stub->begin(config));

    // Create large data buffer
    vector<uint8_t> large_data;
    for (size_t i = 0; i < 1000; ++i) {
        large_data.push_back(static_cast<uint8_t>(i & 0xFF));
    }

    CHECK(stub->transmitAsync(fl::span<const uint8_t>(large_data)));

    const auto& transmitted = stub->getLastTransmission();
    CHECK_EQ(transmitted.size(), 1000);

    // Verify data integrity
    for (size_t i = 0; i < 1000; ++i) {
        CHECK_EQ(transmitted[i], static_cast<uint8_t>(i & 0xFF));
    }

    stub->end();
}

TEST_CASE("SPISingle: Configuration parameter validation") {
    const auto& controllers = SPISingle::getAll();
    SPISingle* single = controllers[0];

    SPISingle::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 10000000;  // 10 MHz
    config.clock_pin = 14;
    config.data_pin = 13;
    config.max_transfer_sz = 4096;

    CHECK(single->begin(config));
    CHECK(single->isInitialized());

    single->end();
}

TEST_CASE("SPISingle: Reset clears transmission history") {
    const auto& controllers = SPISingle::getAll();
    SPISingleStub* stub = toStub(controllers[0]);

    stub->reset();

    SPISingle::Config config;
    CHECK(stub->begin(config));

    vector<uint8_t> data = {0xFF, 0xEE, 0xDD};
    stub->transmitAsync(fl::span<const uint8_t>(data));

    CHECK_EQ(stub->getTransmissionCount(), 1);
    CHECK_EQ(stub->getLastTransmission().size(), 3);

    stub->reset();

    CHECK_EQ(stub->getTransmissionCount(), 0);
    CHECK_EQ(stub->getLastTransmission().size(), 0);

    stub->end();
}

#endif  // FASTLED_TESTING

// ============================================================================
// Blocking SPI Implementation Tests
// ============================================================================

#include "platforms/esp/32/parallel_spi/parallel_spi_blocking_single.hpp"

TEST_CASE("SPIBlocking Single: Basic initialization and configuration") {
    SingleSPI_Blocking_ESP32 spi;

    // Configure pins
    spi.setPinMapping(0, 8);  // Data pin 0, Clock pin 8

    // Load test data
    uint8_t testData[8] = {0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00};
    spi.loadBuffer(testData, 8);

    // Verify buffer loaded
    CHECK_EQ(spi.getBufferLength(), 8);
    CHECK(spi.getBuffer() == testData);
}

TEST_CASE("SPIBlocking Single: LUT initialization") {
    SingleSPI_Blocking_ESP32 spi;
    spi.setPinMapping(5, 10);  // Data pin 5, Clock pin 10

    PinMaskEntry* lut = spi.getLUTArray();

    // Verify LUT entries for bit 0
    // When byte value has bit 0 clear, data pin should be cleared
    CHECK_EQ(lut[0x00].set_mask, 0u);
    CHECK_EQ(lut[0x00].clear_mask, (1u << 5));

    // When byte value has bit 0 set, data pin should be set
    CHECK_EQ(lut[0x01].set_mask, (1u << 5));
    CHECK_EQ(lut[0x01].clear_mask, 0u);

    // Upper 7 bits should be ignored - same as 0x01
    CHECK_EQ(lut[0xFF].set_mask, (1u << 5));
    CHECK_EQ(lut[0xFF].clear_mask, 0u);

    // Even values (bit 0 clear) should clear pin
    CHECK_EQ(lut[0xAA].set_mask, 0u);
    CHECK_EQ(lut[0xAA].clear_mask, (1u << 5));
}

TEST_CASE("SPIBlocking Single: Empty buffer handling") {
    SingleSPI_Blocking_ESP32 spi;
    spi.setPinMapping(0, 8);

    // Transmit with no buffer should not crash
    spi.transmit();

    // Load empty buffer
    uint8_t testData[1];
    spi.loadBuffer(testData, 0);
    spi.transmit();  // Should handle gracefully
}

TEST_CASE("SPIBlocking Single: Maximum buffer size") {
    SingleSPI_Blocking_ESP32 spi;
    spi.setPinMapping(0, 8);

    uint8_t largeBuffer[300];
    for (int i = 0; i < 300; i++) {
        largeBuffer[i] = (i & 1);  // Alternating pattern
    }

    // Should truncate to 256
    spi.loadBuffer(largeBuffer, 300);
    CHECK_EQ(spi.getBufferLength(), 256);
}

TEST_CASE("SPIBlocking Single: Multiple pin configurations") {
    // Test different pin configurations
    for (uint8_t dataPin = 0; dataPin < 10; dataPin++) {
        for (uint8_t clkPin = 10; clkPin < 15; clkPin++) {
            SingleSPI_Blocking_ESP32 spi;
            spi.setPinMapping(dataPin, clkPin);

            PinMaskEntry* lut = spi.getLUTArray();

            // Verify data pin mask in LUT
            CHECK_EQ(lut[0x01].set_mask, (1u << dataPin));
            CHECK_EQ(lut[0x00].clear_mask, (1u << dataPin));
        }
    }
}
