/// @file i2s_spi_peripheral_mock.cpp
/// @brief Unit tests for I2S parallel SPI mock peripheral

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {

void resetMockState() {
    auto &mock = I2sSpiPeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

//=============================================================================
// Initialization
//=============================================================================

FL_TEST_CASE("I2sSpiPeripheralMock - basic initialization") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    FL_CHECK_FALSE(mock.isInitialized());

    I2sSpiConfig config;
    config.num_lanes = 4;
    config.clock_gpio = 18;
    config.clock_hz = 6000000;
    config.max_transfer_bytes = 4096;
    config.data_gpios[0] = 1;
    config.data_gpios[1] = 2;
    config.data_gpios[2] = 3;
    config.data_gpios[3] = 4;

    bool success = mock.initialize(config);
    FL_CHECK(success);
    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.isEnabled());

    const auto &stored = mock.getConfig();
    FL_CHECK(stored.clock_hz == 6000000);
    FL_CHECK(stored.num_lanes == 4);
    FL_CHECK(stored.clock_gpio == 18);
}

FL_TEST_CASE("I2sSpiPeripheralMock - invalid configuration") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config;
    config.clock_hz = 6000000;
    config.num_lanes = 0;
    config.max_transfer_bytes = 4096;

    FL_CHECK_FALSE(mock.initialize(config));
    FL_CHECK_FALSE(mock.isInitialized());

    config.num_lanes = 17;
    FL_CHECK_FALSE(mock.initialize(config));
}

//=============================================================================
// Buffer Management
//=============================================================================

FL_TEST_CASE("I2sSpiPeripheralMock - buffer allocation") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(1, 18, 6000000, 4096);
    FL_REQUIRE(mock.initialize(config));

    u8 *buffer = mock.allocateBuffer(1024);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 1024; i++) {
        buffer[i] = static_cast<u8>(i & 0xFF);
    }
    for (size_t i = 0; i < 1024; i++) {
        FL_CHECK(buffer[i] == static_cast<u8>(i & 0xFF));
    }

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("I2sSpiPeripheralMock - free null buffer is safe") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();
    mock.freeBuffer(nullptr);
}

//=============================================================================
// Transmission
//=============================================================================

FL_TEST_CASE("I2sSpiPeripheralMock - basic transmit") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(4, 18, 6000000, 4096);
    FL_REQUIRE(mock.initialize(config));

    u8 *buffer = mock.allocateBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 256; i++) {
        buffer[i] = 0xAA;
    }

    bool success = mock.transmit(buffer, 256);
    FL_CHECK(success);
    FL_CHECK(mock.isBusy()); // Async: still busy after transmit

    mock.simulateTransmitComplete();
    FL_CHECK_FALSE(mock.isBusy());

    const auto &history = mock.getTransmitHistory();
    FL_CHECK(history.size() == 1);
    FL_CHECK(history[0].size_bytes == 256);
    FL_CHECK(mock.getTransmitCount() == 1);

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("I2sSpiPeripheralMock - transmit data capture") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(2, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u8 *buffer = mock.allocateBuffer(64);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 64; i++) {
        buffer[i] = static_cast<u8>(0x12 + i);
    }

    FL_REQUIRE(mock.transmit(buffer, 64));
    mock.simulateTransmitComplete();

    fl::span<const u8> last_data = mock.getLastTransmitData();
    FL_REQUIRE(last_data.size() == 64);

    for (size_t i = 0; i < last_data.size(); i++) {
        FL_CHECK(last_data[i] == static_cast<u8>(0x12 + i));
    }

    mock.freeBuffer(buffer);
}

//=============================================================================
// Error Injection
//=============================================================================

FL_TEST_CASE("I2sSpiPeripheralMock - transmit failure injection") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u8 *buffer = mock.allocateBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    mock.setTransmitFailure(true);
    FL_CHECK_FALSE(mock.transmit(buffer, 256));

    mock.setTransmitFailure(false);
    FL_CHECK(mock.transmit(buffer, 256));

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("I2sSpiPeripheralMock - transmit without initialization") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    u8 dummy[16] = {0};
    FL_CHECK_FALSE(mock.transmit(dummy, sizeof(dummy)));
}

//=============================================================================
// State
//=============================================================================

FL_TEST_CASE("I2sSpiPeripheralMock - reset clears all state") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u8 *buffer = mock.allocateBuffer(256);
    FL_REQUIRE(buffer != nullptr);
    FL_REQUIRE(mock.transmit(buffer, 256));
    mock.simulateTransmitComplete();
    mock.freeBuffer(buffer);

    mock.reset();

    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(mock.isEnabled());
    FL_CHECK_FALSE(mock.isBusy());
    FL_CHECK(mock.getTransmitCount() == 0);
    FL_CHECK(mock.getTransmitHistory().size() == 0);
}

FL_TEST_CASE("I2sSpiPeripheralMock - deinitialize") {
    resetMockState();
    auto &mock = I2sSpiPeripheralMock::instance();

    I2sSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));
    FL_CHECK(mock.isInitialized());

    mock.deinitialize();
    FL_CHECK_FALSE(mock.isInitialized());
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
