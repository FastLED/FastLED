/// @file lcd_spi_peripheral_mock.cpp
/// @brief Unit tests for LCD_CAM SPI mock peripheral

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {

void resetMockState() {
    auto &mock = LcdSpiPeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

FL_TEST_CASE("LcdSpiPeripheralMock - basic initialization") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    FL_CHECK_FALSE(mock.isInitialized());

    LcdSpiConfig config;
    config.num_lanes = 4;
    config.clock_gpio = 18;
    config.clock_hz = 6000000;
    config.max_transfer_bytes = 4096;
    config.data_gpios[0] = 1;
    config.data_gpios[1] = 2;
    config.data_gpios[2] = 3;
    config.data_gpios[3] = 4;

    FL_CHECK(mock.initialize(config));
    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.isEnabled());

    const auto &stored = mock.getConfig();
    FL_CHECK(stored.clock_hz == 6000000);
    FL_CHECK(stored.num_lanes == 4);
    FL_CHECK(stored.clock_gpio == 18);
}

FL_TEST_CASE("LcdSpiPeripheralMock - invalid configuration") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config;
    config.clock_hz = 6000000;
    config.num_lanes = 0;
    FL_CHECK_FALSE(mock.initialize(config));

    config.num_lanes = 17;
    FL_CHECK_FALSE(mock.initialize(config));
}

FL_TEST_CASE("LcdSpiPeripheralMock - buffer allocation") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 4096);
    FL_REQUIRE(mock.initialize(config));

    u16 *buffer = mock.allocateBuffer(1024);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 512; i++) {
        buffer[i] = static_cast<u16>(i);
    }
    for (size_t i = 0; i < 512; i++) {
        FL_CHECK(buffer[i] == static_cast<u16>(i));
    }

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("LcdSpiPeripheralMock - basic transmit") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(4, 18, 6000000, 4096);
    FL_REQUIRE(mock.initialize(config));

    u16 *buffer = mock.allocateBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 128; i++) {
        buffer[i] = 0xAAAA;
    }

    FL_CHECK(mock.transmit(buffer, 256));
    FL_CHECK(mock.isBusy()); // Async: still busy after transmit

    mock.simulateTransmitComplete();
    FL_CHECK_FALSE(mock.isBusy());

    const auto &history = mock.getTransmitHistory();
    FL_CHECK(history.size() == 1);
    FL_CHECK(history[0].size_bytes == 256);
    FL_CHECK(mock.getTransmitCount() == 1);

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("LcdSpiPeripheralMock - transmit data capture") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(2, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u16 *buffer = mock.allocateBuffer(64);
    FL_REQUIRE(buffer != nullptr);

    for (size_t i = 0; i < 32; i++) {
        buffer[i] = static_cast<u16>(0x1234 + i);
    }

    FL_REQUIRE(mock.transmit(buffer, 64));
    mock.simulateTransmitComplete();

    fl::span<const u16> last_data = mock.getLastTransmitData();
    FL_REQUIRE(last_data.size() == 32);

    for (size_t i = 0; i < last_data.size(); i++) {
        FL_CHECK(last_data[i] == static_cast<u16>(0x1234 + i));
    }

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("LcdSpiPeripheralMock - transmit failure injection") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u16 *buffer = mock.allocateBuffer(256);
    FL_REQUIRE(buffer != nullptr);

    mock.setTransmitFailure(true);
    FL_CHECK_FALSE(mock.transmit(buffer, 256));

    mock.setTransmitFailure(false);
    FL_CHECK(mock.transmit(buffer, 256));

    mock.freeBuffer(buffer);
}

FL_TEST_CASE("LcdSpiPeripheralMock - transmit without initialization") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    u16 dummy[16] = {0};
    FL_CHECK_FALSE(mock.transmit(dummy, sizeof(dummy)));
}

FL_TEST_CASE("LcdSpiPeripheralMock - reset clears all state") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));

    u16 *buffer = mock.allocateBuffer(256);
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

FL_TEST_CASE("LcdSpiPeripheralMock - deinitialize") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 1024);
    FL_REQUIRE(mock.initialize(config));
    FL_CHECK(mock.isInitialized());

    mock.deinitialize();
    FL_CHECK_FALSE(mock.isInitialized());
}


//=============================================================================
// Issue #2270 — owner-aware teardown on cross-driver switch
//=============================================================================

FL_TEST_CASE("LcdSpiPeripheralMock - same owner reuse does not deinit") {
    // Regression test for #2270: when the same driver re-initializes with
    // the same config, the peripheral should NOT force a tear-down.
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 1024);
    config.owner = LcdSpiOwnerDriver::LCD_SPI;
    FL_REQUIRE(mock.initialize(config));
    size_t deinitsBefore = mock.getDeinitCount();

    // Same owner, re-initialize -> fast path (no extra deinit).
    FL_REQUIRE(mock.initialize(config));
    FL_CHECK(mock.getDeinitCount() == deinitsBefore);
}

FL_TEST_CASE("LcdSpiPeripheralMock - cross-driver switch forces deinit") {
    // Regression test for #2270: switching from LCD_SPI to LCD_CLOCKLESS
    // MUST force a tear-down even if the hardware shape happens to match.
    // Otherwise the previous driver's ISR callback and ring buffers leak
    // into the new driver's session and the second switch silently fails.
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig spiConfig(1, 18, 6000000, 1024);
    spiConfig.owner = LcdSpiOwnerDriver::LCD_SPI;
    FL_REQUIRE(mock.initialize(spiConfig));
    size_t deinitsBefore = mock.getDeinitCount();

    // Switch to the clockless driver with identical lanes/clock/size.
    // This is the bug's fingerprint: same shape, different owner.
    LcdSpiConfig clocklessConfig(1, 18, 6000000, 1024);
    clocklessConfig.owner = LcdSpiOwnerDriver::LCD_CLOCKLESS;
    FL_REQUIRE(mock.initialize(clocklessConfig));

    FL_CHECK(mock.getDeinitCount() == deinitsBefore + 1);
    FL_CHECK(mock.getConfig().owner == LcdSpiOwnerDriver::LCD_CLOCKLESS);
}

FL_TEST_CASE(
    "LcdSpiPeripheralMock - repeated cross-driver alternation never fails") {
    // Regression test for #2270: the issue's failure table shows that
    // switches 3 and 4 in an SPI -> CLOCKLESS -> SPI -> CLOCKLESS
    // sequence start failing. After the fix every switch must deinit
    // the previous owner and initialize successfully.
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig spiConfig(1, 18, 6000000, 1024);
    spiConfig.owner = LcdSpiOwnerDriver::LCD_SPI;

    LcdSpiConfig clocklessConfig(1, 18, 6000000, 1024);
    clocklessConfig.owner = LcdSpiOwnerDriver::LCD_CLOCKLESS;

    // Simulate the exact failing sequence from the issue table:
    // 1. LCD_SPI       2. LCD_CLOCKLESS
    // 3. LCD_SPI       4. LCD_CLOCKLESS
    size_t baseDeinits = mock.getDeinitCount();

    FL_REQUIRE(mock.initialize(spiConfig));       // #1 - cold init
    FL_REQUIRE(mock.initialize(clocklessConfig)); // #2 - switch
    FL_REQUIRE(mock.initialize(spiConfig));       // #3 - switch back
    FL_REQUIRE(mock.initialize(clocklessConfig)); // #4 - switch again

    // Each cross-driver switch must trigger one deinit (#2, #3, #4 = 3).
    FL_CHECK(mock.getDeinitCount() == baseDeinits + 3);

    // Final state: clockless owns the peripheral.
    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.getConfig().owner == LcdSpiOwnerDriver::LCD_CLOCKLESS);
}

FL_TEST_CASE(
    "LcdSpiPeripheralMock - legacy caller without owner field still works") {
    // Backward compatibility: existing callers that don't set `owner`
    // leave it as NONE. The singleton should treat NONE as "don't force
    // tear-down on owner mismatch" so legacy tests / wrappers still
    // hit the fast path when the shape is compatible.
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();

    LcdSpiConfig config(1, 18, 6000000, 1024);
    // owner intentionally left as LcdSpiOwnerDriver::NONE (default).
    FL_REQUIRE(mock.initialize(config));
    size_t deinitsBefore = mock.getDeinitCount();

    // Re-initialize with identical NONE-owner config -> no deinit.
    FL_REQUIRE(mock.initialize(config));
    FL_CHECK(mock.getDeinitCount() == deinitsBefore);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
