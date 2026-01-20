/// @file test_channel_data_padding.cpp
/// @brief Unit test for ChannelData padding generator functionality
///
/// Demonstrates how padding generators can be used to extend buffers
/// for protocols that require alignment (e.g., UCS7604's divisible-by-3 requirement)

#include "fl/channels/data.h"
#include "fl/stl/cstring.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/vector.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/slice.h"
#include "fl/stl/move.h"

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
/// Writes source data to destination with padding inserted after the 15-byte preamble.
/// Layout: [PREAMBLE (15 bytes)][PADDING (zeros)][LED DATA]
void ucs7604PaddingGenerator(fl::span<const uint8_t> src, fl::span<uint8_t> dst) {
    constexpr size_t PREAMBLE_LEN = 15;

    size_t srcSize = src.size();
    size_t dstSize = dst.size();

    if (dstSize < srcSize) {
        return; // Invalid: destination too small
    }

    size_t paddingSize = dstSize - srcSize;

    // Copy preamble (first 15 bytes)
    size_t preambleBytes = (srcSize < PREAMBLE_LEN) ? srcSize : PREAMBLE_LEN;
    fl::memcopy(dst.data(), src.data(), preambleBytes);

    // Insert padding zeros after preamble
    if (paddingSize > 0 && srcSize >= PREAMBLE_LEN) {
        fl::fill(dst.begin() + PREAMBLE_LEN, dst.begin() + PREAMBLE_LEN + paddingSize, uint8_t(0));
    }

    // Copy LED data after padding
    if (srcSize > PREAMBLE_LEN) {
        size_t ledDataSize = srcSize - PREAMBLE_LEN;
        fl::memcopy(dst.data() + PREAMBLE_LEN + paddingSize, src.data() + PREAMBLE_LEN, ledDataSize);
    }
}

} // anonymous namespace

TEST_CASE("writeWithPadding - no padding generator, exact size") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);
    buffer.push_back(0xCC);

    // Destination buffer with exact size
    uint8_t dst[3] = {0};
    channelData->writeWithPadding(fl::span<uint8_t>(dst, 3));

    REQUIRE(dst[0] == 0xAA);
    REQUIRE(dst[1] == 0xBB);
    REQUIRE(dst[2] == 0xCC);
}

TEST_CASE("writeWithPadding - no padding generator, left-pad with zeros") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);

    // Destination buffer larger than data (should left-pad with zeros)
    uint8_t dst[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    channelData->writeWithPadding(fl::span<uint8_t>(dst, 5));

    REQUIRE(dst[0] == 0x00); // Left-padding (transmitted first to non-existent pixels)
    REQUIRE(dst[1] == 0x00);
    REQUIRE(dst[2] == 0x00);
    REQUIRE(dst[3] == 0xAA); // Actual data
    REQUIRE(dst[4] == 0xBB);
}

TEST_CASE("writeWithPadding - with padding generator") {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "WS2812");
    auto channelData = ChannelData::create(5, timing);

    // Padding generator that right-pads with 0xFF
    channelData->setPaddingGenerator([](fl::span<const uint8_t> src, fl::span<uint8_t> dst) {
        fl::memcopy(dst.data(), src.data(), src.size());
        fl::fill(dst.begin() + src.size(), dst.end(), uint8_t(0xFF));
    });

    auto& buffer = channelData->getData();
    buffer.clear();
    buffer.push_back(0xAA);
    buffer.push_back(0xBB);

    // Destination buffer larger than data
    uint8_t dst[5] = {0};
    channelData->writeWithPadding(fl::span<uint8_t>(dst, 5));

    REQUIRE(dst[0] == 0xAA);
    REQUIRE(dst[1] == 0xBB);
    REQUIRE(dst[2] == 0xFF); // Padded with 0xFF
    REQUIRE(dst[3] == 0xFF);
    REQUIRE(dst[4] == 0xFF);
}

TEST_CASE("writeWithPadding - UCS7604 complex padding") {
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

    // Write to destination with padding to 21 bytes
    uint8_t dst[21] = {0};
    channelData->writeWithPadding(fl::span<uint8_t>(dst, 21));

    // Verify preamble is intact (first 15 bytes)
    REQUIRE(dst[0] == 0xFF);
    REQUIRE(dst[14] == 0x00);

    // Verify padding inserted at index 15 and 16
    REQUIRE(dst[15] == 0x00);
    REQUIRE(dst[16] == 0x00);

    // Verify LED data moved after padding
    REQUIRE(dst[17] == 0xAA);
    REQUIRE(dst[18] == 0xBB);
    REQUIRE(dst[19] == 0xCC);
    REQUIRE(dst[20] == 0xDD);

    // Original buffer should remain unchanged
    REQUIRE(buffer.size() == 19);
    REQUIRE(buffer[15] == 0xAA);
}
