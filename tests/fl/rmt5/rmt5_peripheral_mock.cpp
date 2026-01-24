/// @file rmt5_peripheral_mock.cpp
/// @brief Mock peripheral lifecycle tests for RMT5
///
/// Tests the mock peripheral implementation in isolation:
/// - Channel creation and deletion
/// - Enable/disable transitions
/// - DMA buffer allocation/deallocation
/// - Encoder creation and deletion
/// - Transmission data capture
/// - Error injection
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/shared/mock/esp/32/drivers/rmt5_peripheral_mock.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"

using namespace fl;
using namespace fl::detail;

namespace {

/// @brief Helper to get WS2812 timing
static ChipsetTiming getWS2812Timing() {
    ChipsetTiming timing;
    timing.T1 = 350;
    timing.T2 = 800;
    timing.T3 = 450;
    timing.RESET = 50;
    timing.name = "WS2812B";
    return timing;
}

/// @brief Reset mock state between tests
void resetMockState() {
    auto& mock = Rmt5PeripheralMock::instance();
    mock.reset();
}

} // anonymous namespace

//=============================================================================
// Test Suite: Channel Lifecycle
//=============================================================================

TEST_CASE("RMT5 mock - create and delete channel") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Create channel
    void* channel_handle = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    bool success = mock.createTxChannel(config, &channel_handle);

    CHECK(success);
    CHECK(channel_handle != nullptr);
    CHECK(mock.getChannelCount() == 1);

    // Delete channel
    success = mock.deleteChannel(channel_handle);
    CHECK(success);
    CHECK(mock.getChannelCount() == 0);
}

TEST_CASE("RMT5 mock - create multiple channels") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* ch1 = nullptr;
    void* ch2 = nullptr;
    void* ch3 = nullptr;

    Rmt5ChannelConfig config1(18, 40000000, 64, 1, false, 0);
    Rmt5ChannelConfig config2(19, 40000000, 64, 1, false, 0);
    Rmt5ChannelConfig config3(20, 40000000, 64, 1, true, 0);  // DMA

    CHECK(mock.createTxChannel(config1, &ch1));
    CHECK(mock.createTxChannel(config2, &ch2));
    CHECK(mock.createTxChannel(config3, &ch3));

    CHECK(mock.getChannelCount() == 3);

    // Verify channels are distinct
    CHECK(ch1 != ch2);
    CHECK(ch2 != ch3);
    CHECK(ch1 != ch3);

    // Cleanup
    mock.deleteChannel(ch1);
    mock.deleteChannel(ch2);
    mock.deleteChannel(ch3);
    CHECK(mock.getChannelCount() == 0);
}

TEST_CASE("RMT5 mock - enable and disable channel") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);

    // Initially disabled
    CHECK_FALSE(mock.isChannelEnabled(channel));

    // Enable
    bool success = mock.enableChannel(channel);
    CHECK(success);
    CHECK(mock.isChannelEnabled(channel));

    // Disable
    success = mock.disableChannel(channel);
    CHECK(success);
    CHECK_FALSE(mock.isChannelEnabled(channel));

    // Cleanup
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - invalid channel handle") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* invalid_handle = reinterpret_cast<void*>(0x12345678);

    // Operations on invalid handle should fail
    CHECK_FALSE(mock.enableChannel(invalid_handle));
    CHECK_FALSE(mock.disableChannel(invalid_handle));
    CHECK_FALSE(mock.deleteChannel(invalid_handle));
    CHECK_FALSE(mock.isChannelEnabled(invalid_handle));
}

//=============================================================================
// Test Suite: Encoder Management
//=============================================================================

TEST_CASE("RMT5 mock - create and delete encoder") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    CHECK(encoder != nullptr);
    CHECK(mock.getEncoderCount() == 1);

    // Delete encoder
    mock.deleteEncoder(encoder);
    CHECK(mock.getEncoderCount() == 0);
}

TEST_CASE("RMT5 mock - create multiple encoders") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    ChipsetTiming timing = getWS2812Timing();
    void* enc1 = mock.createEncoder(timing, 40000000);
    void* enc2 = mock.createEncoder(timing, 40000000);
    void* enc3 = mock.createEncoder(timing, 20000000);  // Different resolution

    CHECK(mock.getEncoderCount() == 3);

    // Verify encoders are distinct
    CHECK(enc1 != enc2);
    CHECK(enc2 != enc3);
    CHECK(enc1 != enc3);

    // Cleanup
    mock.deleteEncoder(enc1);
    mock.deleteEncoder(enc2);
    mock.deleteEncoder(enc3);
    CHECK(mock.getEncoderCount() == 0);
}

//=============================================================================
// Test Suite: DMA Buffer Management
//=============================================================================

TEST_CASE("RMT5 mock - allocate and free DMA buffer") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    uint8_t* buffer = mock.allocateDmaBuffer(100);
    CHECK(buffer != nullptr);

    // Verify alignment (64-byte)
    CHECK((reinterpret_cast<uintptr_t>(buffer) % 64) == 0);

    // Free buffer
    mock.freeDmaBuffer(buffer);
}

TEST_CASE("RMT5 mock - allocate multiple DMA buffers") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    uint8_t* buf1 = mock.allocateDmaBuffer(100);
    uint8_t* buf2 = mock.allocateDmaBuffer(200);
    uint8_t* buf3 = mock.allocateDmaBuffer(300);

    CHECK(buf1 != nullptr);
    CHECK(buf2 != nullptr);
    CHECK(buf3 != nullptr);

    // Verify distinct buffers
    CHECK(buf1 != buf2);
    CHECK(buf2 != buf3);
    CHECK(buf1 != buf3);

    // Cleanup
    mock.freeDmaBuffer(buf1);
    mock.freeDmaBuffer(buf2);
    mock.freeDmaBuffer(buf3);
}

TEST_CASE("RMT5 mock - DMA buffer nullptr safety") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Should not crash
    mock.freeDmaBuffer(nullptr);
}

//=============================================================================
// Test Suite: Transmission Data Capture
//=============================================================================

TEST_CASE("RMT5 mock - capture transmission data") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Create channel and encoder
    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    // Transmit pixel data
    uint8_t pixels[] = {0xFF, 0x00, 0x00};  // Red pixel
    bool success = mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK(success);

    // Verify transmission was captured
    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() == 1);

    const auto& record = history[0];
    CHECK(record.buffer_size == 3);
    CHECK(record.gpio_pin == 18);
    CHECK(record.used_dma == false);
    CHECK(verifyPixelData(record, fl::span<const uint8_t>(pixels, 3)));

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - multiple transmissions") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    // Transmit three different pixel patterns
    uint8_t pixels1[] = {0xFF, 0x00, 0x00};  // Red
    uint8_t pixels2[] = {0x00, 0xFF, 0x00};  // Green
    uint8_t pixels3[] = {0x00, 0x00, 0xFF};  // Blue

    mock.transmit(channel, encoder, pixels1, sizeof(pixels1));
    mock.transmit(channel, encoder, pixels2, sizeof(pixels2));
    mock.transmit(channel, encoder, pixels3, sizeof(pixels3));

    // Verify all three transmissions were captured
    const auto& history = mock.getTransmissionHistory();
    REQUIRE(history.size() == 3);

    CHECK(verifyPixelData(history[0], fl::span<const uint8_t>(pixels1, 3)));
    CHECK(verifyPixelData(history[1], fl::span<const uint8_t>(pixels2, 3)));
    CHECK(verifyPixelData(history[2], fl::span<const uint8_t>(pixels3, 3)));

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - clear transmission history") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    uint8_t pixels[] = {0xFF, 0x00, 0x00};
    mock.transmit(channel, encoder, pixels, sizeof(pixels));

    CHECK(mock.getTransmissionHistory().size() == 1);

    // Clear history
    mock.clearTransmissionHistory();
    CHECK(mock.getTransmissionHistory().size() == 0);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - get last transmission data") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    // Initially empty
    CHECK(mock.getLastTransmissionData().empty());

    // Transmit
    uint8_t pixels[] = {0xAA, 0xBB, 0xCC};
    mock.transmit(channel, encoder, pixels, sizeof(pixels));

    // Verify last transmission
    auto last_data = mock.getLastTransmissionData();
    REQUIRE(last_data.size() == 3);
    CHECK(last_data[0] == 0xAA);
    CHECK(last_data[1] == 0xBB);
    CHECK(last_data[2] == 0xCC);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

TEST_CASE("RMT5 mock - inject transmission failure") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    // Inject failure
    mock.setTransmitFailure(true);

    uint8_t pixels[] = {0xFF, 0x00, 0x00};
    bool success = mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK_FALSE(success);

    // No transmission should be captured
    CHECK(mock.getTransmissionHistory().size() == 0);

    // Disable failure injection
    mock.setTransmitFailure(false);
    success = mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK(success);
    CHECK(mock.getTransmissionHistory().size() == 1);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - transmit without enabling channel") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    // NOTE: Channel not enabled

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    uint8_t pixels[] = {0xFF, 0x00, 0x00};
    bool success = mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK_FALSE(success);  // Should fail - channel not enabled

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

//=============================================================================
// Test Suite: Mock State Inspection
//=============================================================================

TEST_CASE("RMT5 mock - transmission counter") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    CHECK(mock.getTransmissionCount() == 0);

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    uint8_t pixels[] = {0xFF, 0x00, 0x00};
    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK(mock.getTransmissionCount() == 1);

    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK(mock.getTransmissionCount() == 2);

    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    CHECK(mock.getTransmissionCount() == 3);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

TEST_CASE("RMT5 mock - reset state") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Create channels and encoders
    void* ch1 = nullptr;
    void* ch2 = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &ch1);
    mock.createTxChannel(config, &ch2);

    ChipsetTiming timing = getWS2812Timing();
    void* enc1 = mock.createEncoder(timing, 40000000);
    void* enc2 = mock.createEncoder(timing, 40000000);
    (void)enc1;  // Intentionally unused - only created to test reset
    (void)enc2;  // Intentionally unused - only created to test reset

    CHECK(mock.getChannelCount() == 2);
    CHECK(mock.getEncoderCount() == 2);

    // Reset
    mock.reset();

    // Verify everything cleared
    CHECK(mock.getChannelCount() == 0);
    CHECK(mock.getEncoderCount() == 0);
    CHECK(mock.getTransmissionCount() == 0);
    CHECK(mock.getTransmissionHistory().size() == 0);
}

#endif // FASTLED_STUB_IMPL
