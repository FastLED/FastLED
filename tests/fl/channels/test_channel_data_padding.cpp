/// @file test_channel_data_padding.cpp
/// @brief Unit test for ChannelData padding generator functionality
///
/// Demonstrates how padding generators can be used to extend buffers
/// for protocols that require alignment (e.g., UCS7604's divisible-by-3 requirement)

#include "test.h"
#include "fl/channels/channel_data.h"
#include "fl/chipsets/led_timing.h"
#include "ftl/vector.h"

using namespace fl;

namespace {

/// @brief Helper to build a UCS7604-style preamble (15 bytes)
void buildUCS7604Preamble(fl::vector_psram<uint8_t>& buffer) {
    buffer.push_back(0xFF); // Sync pattern
    buffer.push_back(0xFF);
    buffer.push_back(0xFF);
    buffer.push_back(0xFF);
    buffer.push_back(0xFF);
    buffer.push_back(0xFF);
    buffer.push_back(0x00); // Header
    buffer.push_back(0x02);
    buffer.push_back(0x03); // Mode: 8-bit @ 800kHz
    buffer.push_back(0x0F); // R current
    buffer.push_back(0x0F); // G current
    buffer.push_back(0x0F); // B current
    buffer.push_back(0x0F); // W current
    buffer.push_back(0x00); // Reserved
    buffer.push_back(0x00);
}

/// @brief UCS7604-style padding generator
///
/// Extends the buffer to the exact target size by inserting zero bytes
/// after the 15-byte preamble (before LED data)
void ucs7604PaddingGenerator(fl::vector_psram<uint8_t>& buffer, size_t targetSize) {
    constexpr size_t PREAMBLE_LEN = 15;

    size_t current_size = buffer.size();
    if (current_size >= targetSize) {
        return; // Already at or above target size
    }

    size_t padding_needed = targetSize - current_size;

    // Insert padding after preamble (before LED data)
    // We insert at the same position each time, which shifts the LED data to the right
    for (size_t i = 0; i < padding_needed; ++i) {
        auto insert_pos = buffer.begin() + PREAMBLE_LEN;
        buffer.insert(insert_pos, 0x00);
    }
}

} // anonymous namespace

TEST_CASE("ChannelData padding - no generator is no-op") {
    // Create ChannelData with WS2812 timing
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0x01);
    buffer.push_back(0x02);
    buffer.push_back(0x03);

    channelData->applyPadding(10); // Target size larger than current

    // Buffer should be unchanged (no padding generator configured)
    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer[0] == 0x01);
    REQUIRE(buffer[1] == 0x02);
    REQUIRE(buffer[2] == 0x03);
}

TEST_CASE("ChannelData padding - extends to target size") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    // Set a simple padding generator that pads with 0xFF to reach target size
    channelData->setPaddingGenerator([](fl::vector_psram<uint8_t>& buf, size_t targetSize) {
        while (buf.size() < targetSize) {
            buf.push_back(0xFF);
        }
    });

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);

    channelData->applyPadding(5); // Extend to 5 bytes

    // Buffer should be extended to exactly 5 bytes
    REQUIRE(buffer.size() == 5);
    REQUIRE(buffer[0] == 0xAA);
    REQUIRE(buffer[1] == 0xBB);
    REQUIRE(buffer[2] == 0xFF);
    REQUIRE(buffer[3] == 0xFF);
    REQUIRE(buffer[4] == 0xFF);
}

TEST_CASE("ChannelData padding - target equals current size") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    channelData->setPaddingGenerator([](fl::vector_psram<uint8_t>& buf, size_t targetSize) {
        while (buf.size() < targetSize) {
            buf.push_back(0xFF);
        }
    });

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);
    buffer.push_back(0xCC);

    channelData->applyPadding(3); // Target equals current size

    // Buffer should remain unchanged
    REQUIRE(buffer.size() == 3);
    REQUIRE(buffer[0] == 0xAA);
    REQUIRE(buffer[1] == 0xBB);
    REQUIRE(buffer[2] == 0xCC);
}

TEST_CASE("UCS7604 padding - no padding needed") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    auto channelData = ChannelData::create(5, timing);
    channelData->setPaddingGenerator(ucs7604PaddingGenerator);

    auto& buffer = channelData->getData();
    buffer.clear();

    // Build preamble (15 bytes)
    buildUCS7604Preamble(buffer);

    // Add 3 RGB LEDs (3 * 3 = 9 bytes)
    buffer.push_back(0xFF); buffer.push_back(0x00); buffer.push_back(0x00); // Red
    buffer.push_back(0x00); buffer.push_back(0xFF); buffer.push_back(0x00); // Green
    buffer.push_back(0x00); buffer.push_back(0x00); buffer.push_back(0xFF); // Blue

    REQUIRE(buffer.size() == 24); // 15 + 9

    channelData->applyPadding(24); // Target = current size

    // No padding needed
    REQUIRE(buffer.size() == 24);
    REQUIRE(buffer[23] == 0xFF); // Last byte unchanged
}

TEST_CASE("UCS7604 padding - add 3 bytes") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    auto channelData = ChannelData::create(5, timing);
    channelData->setPaddingGenerator(ucs7604PaddingGenerator);

    auto& buffer = channelData->getData();
    buffer.clear();

    // Build preamble (15 bytes)
    buildUCS7604Preamble(buffer);

    // Add 3 RGB LEDs (3 * 3 = 9 bytes)
    for (int i = 0; i < 3; ++i) {
        buffer.push_back(0x10 + i);
        buffer.push_back(0x20 + i);
        buffer.push_back(0x30 + i);
    }

    REQUIRE(buffer.size() == 24); // 15 + 9

    channelData->applyPadding(27); // Extend to 27

    // Should add 3 bytes of padding after preamble
    REQUIRE(buffer.size() == 27);

    // Preamble should be intact
    REQUIRE(buffer[0] == 0xFF);
    REQUIRE(buffer[14] == 0x00);

    // 3 bytes of padding at index 15-17
    REQUIRE(buffer[15] == 0x00);
    REQUIRE(buffer[16] == 0x00);
    REQUIRE(buffer[17] == 0x00);

    // LED data moved to index 18-26
    REQUIRE(buffer[18] == 0x10);
    REQUIRE(buffer[26] == 0x32);
}

TEST_CASE("UCS7604 padding - insert after preamble") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    auto channelData = ChannelData::create(5, timing);
    channelData->setPaddingGenerator(ucs7604PaddingGenerator);

    auto& buffer = channelData->getData();
    buffer.clear();

    // Build preamble (15 bytes)
    buildUCS7604Preamble(buffer);

    // Add LED data (4 bytes)
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);
    buffer.push_back(0xCC);
    buffer.push_back(0xDD);

    REQUIRE(buffer.size() == 19);

    channelData->applyPadding(21); // Extend to 21 (divisible by 3)

    // Should add 2 bytes of padding
    REQUIRE(buffer.size() == 21);

    // Verify preamble is intact (first 15 bytes)
    REQUIRE(buffer[0] == 0xFF);
    REQUIRE(buffer[14] == 0x00);

    // Verify padding inserted at index 15 and 16
    REQUIRE(buffer[15] == 0x00);
    REQUIRE(buffer[16] == 0x00);

    // Verify LED data moved after padding
    REQUIRE(buffer[17] == 0xAA);
    REQUIRE(buffer[18] == 0xBB);
    REQUIRE(buffer[19] == 0xCC);
    REQUIRE(buffer[20] == 0xDD);
}

TEST_CASE("UCS7604 RGBW padding - 2 bytes needed") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    auto channelData = ChannelData::create(5, timing);
    channelData->setPaddingGenerator(ucs7604PaddingGenerator);

    auto& buffer = channelData->getData();
    buffer.clear();

    // Build preamble (15 bytes)
    buildUCS7604Preamble(buffer);

    // Add 4 RGBW LEDs (4 * 4 = 16 bytes)
    for (int i = 0; i < 4; ++i) {
        buffer.push_back(0x11);
        buffer.push_back(0x22);
        buffer.push_back(0x33);
        buffer.push_back(0x44);
    }

    REQUIRE(buffer.size() == 31); // 15 + 16

    channelData->applyPadding(33); // Extend to 33 (divisible by 3)

    // Should add 2 bytes of padding
    REQUIRE(buffer.size() == 33);

    // Verify padding at index 15-16
    REQUIRE(buffer[15] == 0x00);
    REQUIRE(buffer[16] == 0x00);

    // Verify first RGBW LED moved to index 17-20
    REQUIRE(buffer[17] == 0x11);
    REQUIRE(buffer[20] == 0x44);
}
