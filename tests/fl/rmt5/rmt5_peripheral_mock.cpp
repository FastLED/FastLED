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
#include "test.h"

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

FL_TEST_CASE("RMT5 mock - create and delete channel") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Create channel
    void* channel_handle = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    bool success = mock.createTxChannel(config, &channel_handle);

    FL_CHECK(success);
    FL_CHECK(channel_handle != nullptr);
    FL_CHECK(mock.getChannelCount() == 1);

    // Delete channel
    success = mock.deleteChannel(channel_handle);
    FL_CHECK(success);
    FL_CHECK(mock.getChannelCount() == 0);
}

FL_TEST_CASE("RMT5 mock - create multiple channels") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* ch1 = nullptr;
    void* ch2 = nullptr;
    void* ch3 = nullptr;

    Rmt5ChannelConfig config1(18, 40000000, 64, 1, false, 0);
    Rmt5ChannelConfig config2(19, 40000000, 64, 1, false, 0);
    Rmt5ChannelConfig config3(20, 40000000, 64, 1, true, 0);  // DMA

    FL_CHECK(mock.createTxChannel(config1, &ch1));
    FL_CHECK(mock.createTxChannel(config2, &ch2));
    FL_CHECK(mock.createTxChannel(config3, &ch3));

    FL_CHECK(mock.getChannelCount() == 3);

    // Verify channels are distinct
    FL_CHECK(ch1 != ch2);
    FL_CHECK(ch2 != ch3);
    FL_CHECK(ch1 != ch3);

    // Cleanup
    mock.deleteChannel(ch1);
    mock.deleteChannel(ch2);
    mock.deleteChannel(ch3);
    FL_CHECK(mock.getChannelCount() == 0);
}

FL_TEST_CASE("RMT5 mock - enable and disable channel") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);

    // Initially disabled
    FL_CHECK_FALSE(mock.isChannelEnabled(channel));

    // Enable
    bool success = mock.enableChannel(channel);
    FL_CHECK(success);
    FL_CHECK(mock.isChannelEnabled(channel));

    // Disable
    success = mock.disableChannel(channel);
    FL_CHECK(success);
    FL_CHECK_FALSE(mock.isChannelEnabled(channel));

    // Cleanup
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - invalid channel handle") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* invalid_handle = reinterpret_cast<void*>(0x12345678);

    // Operations on invalid handle should fail
    FL_CHECK_FALSE(mock.enableChannel(invalid_handle));
    FL_CHECK_FALSE(mock.disableChannel(invalid_handle));
    FL_CHECK_FALSE(mock.deleteChannel(invalid_handle));
    FL_CHECK_FALSE(mock.isChannelEnabled(invalid_handle));
}

//=============================================================================
// Test Suite: Encoder Management
//=============================================================================

FL_TEST_CASE("RMT5 mock - create and delete encoder") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    FL_CHECK(encoder != nullptr);
    FL_CHECK(mock.getEncoderCount() == 1);

    // Delete encoder
    mock.deleteEncoder(encoder);
    FL_CHECK(mock.getEncoderCount() == 0);
}

FL_TEST_CASE("RMT5 mock - create multiple encoders") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    ChipsetTiming timing = getWS2812Timing();
    void* enc1 = mock.createEncoder(timing, 40000000);
    void* enc2 = mock.createEncoder(timing, 40000000);
    void* enc3 = mock.createEncoder(timing, 20000000);  // Different resolution

    FL_CHECK(mock.getEncoderCount() == 3);

    // Verify encoders are distinct
    FL_CHECK(enc1 != enc2);
    FL_CHECK(enc2 != enc3);
    FL_CHECK(enc1 != enc3);

    // Cleanup
    mock.deleteEncoder(enc1);
    mock.deleteEncoder(enc2);
    mock.deleteEncoder(enc3);
    FL_CHECK(mock.getEncoderCount() == 0);
}

//=============================================================================
// Test Suite: DMA Buffer Management
//=============================================================================

FL_TEST_CASE("RMT5 mock - allocate and free DMA buffer") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    uint8_t* buffer = mock.allocateDmaBuffer(100);
    FL_CHECK(buffer != nullptr);

    // Verify alignment (64-byte)
    FL_CHECK((reinterpret_cast<uintptr_t>(buffer) % 64) == 0);

    // Free buffer
    mock.freeDmaBuffer(buffer);
}

FL_TEST_CASE("RMT5 mock - allocate multiple DMA buffers") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    uint8_t* buf1 = mock.allocateDmaBuffer(100);
    uint8_t* buf2 = mock.allocateDmaBuffer(200);
    uint8_t* buf3 = mock.allocateDmaBuffer(300);

    FL_CHECK(buf1 != nullptr);
    FL_CHECK(buf2 != nullptr);
    FL_CHECK(buf3 != nullptr);

    // Verify distinct buffers
    FL_CHECK(buf1 != buf2);
    FL_CHECK(buf2 != buf3);
    FL_CHECK(buf1 != buf3);

    // Cleanup
    mock.freeDmaBuffer(buf1);
    mock.freeDmaBuffer(buf2);
    mock.freeDmaBuffer(buf3);
}

FL_TEST_CASE("RMT5 mock - DMA buffer nullptr safety") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    // Should not crash
    mock.freeDmaBuffer(nullptr);
}

//=============================================================================
// Test Suite: Transmission Data Capture
//=============================================================================

FL_TEST_CASE("RMT5 mock - capture transmission data") {
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
    FL_CHECK(success);

    // Verify transmission was captured
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() == 1);

    const auto& record = history[0];
    FL_CHECK(record.buffer_size == 3);
    FL_CHECK(record.gpio_pin == 18);
    FL_CHECK(record.used_dma == false);
    FL_CHECK(verifyPixelData(record, fl::span<const uint8_t>(pixels, 3)));

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - multiple transmissions") {
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
    FL_REQUIRE(history.size() == 3);

    FL_CHECK(verifyPixelData(history[0], fl::span<const uint8_t>(pixels1, 3)));
    FL_CHECK(verifyPixelData(history[1], fl::span<const uint8_t>(pixels2, 3)));
    FL_CHECK(verifyPixelData(history[2], fl::span<const uint8_t>(pixels3, 3)));

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - clear transmission history") {
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

    FL_CHECK(mock.getTransmissionHistory().size() == 1);

    // Clear history
    mock.clearTransmissionHistory();
    FL_CHECK(mock.getTransmissionHistory().size() == 0);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - get last transmission data") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    // Initially empty
    FL_CHECK(mock.getLastTransmissionData().empty());

    // Transmit
    uint8_t pixels[] = {0xAA, 0xBB, 0xCC};
    mock.transmit(channel, encoder, pixels, sizeof(pixels));

    // Verify last transmission
    auto last_data = mock.getLastTransmissionData();
    FL_REQUIRE(last_data.size() == 3);
    FL_CHECK(last_data[0] == 0xAA);
    FL_CHECK(last_data[1] == 0xBB);
    FL_CHECK(last_data[2] == 0xCC);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

//=============================================================================
// Test Suite: Error Injection
//=============================================================================

FL_TEST_CASE("RMT5 mock - inject transmission failure") {
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
    FL_CHECK_FALSE(success);

    // No transmission should be captured
    FL_CHECK(mock.getTransmissionHistory().size() == 0);

    // Disable failure injection
    mock.setTransmitFailure(false);
    success = mock.transmit(channel, encoder, pixels, sizeof(pixels));
    FL_CHECK(success);
    FL_CHECK(mock.getTransmissionHistory().size() == 1);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - transmit without enabling channel") {
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
    FL_CHECK_FALSE(success);  // Should fail - channel not enabled

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

//=============================================================================
// Test Suite: Mock State Inspection
//=============================================================================

FL_TEST_CASE("RMT5 mock - transmission counter") {
    resetMockState();
    auto& mock = Rmt5PeripheralMock::instance();

    FL_CHECK(mock.getTransmissionCount() == 0);

    void* channel = nullptr;
    Rmt5ChannelConfig config(18, 40000000, 64, 1, false, 0);
    mock.createTxChannel(config, &channel);
    mock.enableChannel(channel);

    ChipsetTiming timing = getWS2812Timing();
    void* encoder = mock.createEncoder(timing, 40000000);

    uint8_t pixels[] = {0xFF, 0x00, 0x00};
    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    FL_CHECK(mock.getTransmissionCount() == 1);

    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    FL_CHECK(mock.getTransmissionCount() == 2);

    mock.transmit(channel, encoder, pixels, sizeof(pixels));
    FL_CHECK(mock.getTransmissionCount() == 3);

    // Cleanup
    mock.deleteEncoder(encoder);
    mock.deleteChannel(channel);
}

FL_TEST_CASE("RMT5 mock - reset state") {
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

    FL_CHECK(mock.getChannelCount() == 2);
    FL_CHECK(mock.getEncoderCount() == 2);

    // Reset
    mock.reset();

    // Verify everything cleared
    FL_CHECK(mock.getChannelCount() == 0);
    FL_CHECK(mock.getEncoderCount() == 0);
    FL_CHECK(mock.getTransmissionCount() == 0);
    FL_CHECK(mock.getTransmissionHistory().size() == 0);
}

#endif // FASTLED_STUB_IMPL
