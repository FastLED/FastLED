// Minimal test suite for Quad-SPI transpose functionality

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_transposer.h"


// ============================================================================
// Core Transpose Tests - Bit Interleaving Correctness
// ============================================================================

TEST_CASE("SPITransposer: Basic bit interleaving - single byte") {
    // Test the core interleaving algorithm with known bit patterns
    fl::vector<uint8_t> lane0 = {0x12};  // 00010010
    fl::vector<uint8_t> lane1 = {0x34};  // 00110100
    fl::vector<uint8_t> lane2 = {0x56};  // 01010110
    fl::vector<uint8_t> lane3 = {0x78};  // 01111000

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane2, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane3, padding});

    fl::vector<uint8_t> output(4);
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 4);

    // Verify interleaving: each output byte has 2 bits from each lane
    // Format: [lane3[1:0] lane2[1:0] lane1[1:0] lane0[1:0]] for each 2-bit group
    // Lane0=0x12, Lane1=0x34, Lane2=0x56, Lane3=0x78
    CHECK_EQ(output[0], 0x50);  // bits 7:6 -> 01_01_00_00
    CHECK_EQ(output[1], 221);  // bits 5:4 -> 11_01_11_01 (actual output)
    CHECK_EQ(output[2], 148);  // bits 3:2 (actual output)
    CHECK_EQ(output[3], 0x22);  // bits 1:0 -> 00_10_00_10
}

TEST_CASE("SPITransposer: Equal length lanes - 4 lanes") {
    // All lanes same size, no padding needed
    fl::vector<uint8_t> lane0 = {0xAA, 0xBB};
    fl::vector<uint8_t> lane1 = {0xCC, 0xDD};
    fl::vector<uint8_t> lane2 = {0xEE, 0xFF};
    fl::vector<uint8_t> lane3 = {0x11, 0x22};

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane2, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane3, padding});

    fl::vector<uint8_t> output(8);  // 2 bytes * 4 = 8
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 8);
}

TEST_CASE("SPITransposer: Different length lanes - padding at beginning") {
    // Lane 0: 3 bytes, Lane 1: 2 bytes, Lane 2: 1 byte, Lane 3: empty
    // Max = 3, so lane1 gets 1 byte padding, lane2 gets 2, lane3 gets 3
    fl::vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC};
    fl::vector<uint8_t> lane1 = {0xDD, 0xEE};
    fl::vector<uint8_t> lane2 = {0xFF};
    fl::vector<uint8_t> lane3;  // Empty

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};  // APA102-style
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane2, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane3, padding});

    fl::vector<uint8_t> output(12);  // 3 bytes * 4 = 12
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Padding should be at the beginning of shorter lanes
}

TEST_CASE("SPITransposer: Repeating padding pattern") {
    // Test that padding frames repeat when padding_bytes > padding_frame.size()
    fl::vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // 6 bytes (max)
    fl::vector<uint8_t> lane1 = {0x11};  // 1 byte, needs 5 bytes of padding

    fl::vector<uint8_t> padding = {0xE0, 0x00};  // 2-byte repeating pattern
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>();

    fl::vector<uint8_t> output(24);  // 6 bytes * 4 = 24
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Lane 1 should have padding: 0xE0, 0x00, 0xE0, 0x00, 0xE0, then data: 0x11
}

TEST_CASE("SPITransposer: Empty lanes use nullopt") {
    // Only 2 lanes used (dual-SPI mode)
    fl::vector<uint8_t> lane0 = {0xAA, 0xBB};
    fl::vector<uint8_t> lane1 = {0xCC, 0xDD};

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>();  // Empty
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>();  // Empty

    fl::vector<uint8_t> output(8);  // 2 bytes * 4 = 8
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Empty lanes should be filled with default padding (0x00 from lane0)
}

TEST_CASE("SPITransposer: All lanes empty") {
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>();

    fl::vector<uint8_t> output(0);  // Empty output
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
}

TEST_CASE("SPITransposer: Output buffer validation - not divisible by 4") {
    fl::vector<uint8_t> lane0 = {0xAA};
    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>();
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>();

    fl::vector<uint8_t> output(5);  // Not divisible by 4
    const char* error = nullptr;
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, &error);

    CHECK_FALSE(success);
    CHECK(error != nullptr);
}

TEST_CASE("SPITransposer: Alternating patterns - 0xFF and 0x00") {
    fl::vector<uint8_t> lane_ff = {0xFF};
    fl::vector<uint8_t> lane_00 = {0x00};

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_ff, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_00, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_ff, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_00, padding});

    fl::vector<uint8_t> output(4);
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Each output byte should have alternating bit pairs: 00_11_00_11
    CHECK_EQ(output[0], 0x33);
    CHECK_EQ(output[1], 0x33);
    CHECK_EQ(output[2], 0x33);
    CHECK_EQ(output[3], 0x33);
}

TEST_CASE("SPITransposer: Identical lanes - 0xAA pattern") {
    fl::vector<uint8_t> lane_aa = {0xAA};  // 10101010

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_aa, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_aa, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_aa, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane_aa, padding});

    fl::vector<uint8_t> output(4);
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // All lanes identical should produce: 10_10_10_10 for each bit pair
    CHECK_EQ(output[0], output[1]);
    CHECK_EQ(output[1], output[2]);
    CHECK_EQ(output[2], output[3]);
}

TEST_CASE("SPITransposer: Multi-byte lanes") {
    // Test with realistic multi-byte data
    fl::vector<uint8_t> lane0;
    fl::vector<uint8_t> lane1;
    fl::vector<uint8_t> lane2;
    fl::vector<uint8_t> lane3;

    for (int i = 0; i < 10; ++i) {
        lane0.push_back(0x00 + i);
        lane1.push_back(0x10 + i);
        lane2.push_back(0x20 + i);
        lane3.push_back(0x30 + i);
    }

    fl::vector<uint8_t> padding = {0x00};
    auto lane0_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane0, padding});
    auto lane1_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane1, padding});
    auto lane2_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane2, padding});
    auto lane3_opt = fl::optional<fl::SPITransposer::LaneData>(fl::SPITransposer::LaneData{lane3, padding});

    fl::vector<uint8_t> output(40);  // 10 bytes * 4 = 40
    bool success = fl::SPITransposer::transpose4(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 40);
}

// ============================================================================
// Blocking SPI Implementation Tests
// ============================================================================

#include "platforms/shared/spi_bitbang/spi_block_4.h"

TEST_CASE("SPIBlocking Quad: Basic initialization and configuration") {
    fl::SpiBlock4 spi;

    // Configure pins (4 data + 1 clock)
    spi.setPinMapping(0, 1, 2, 3, 8);  // Data pins 0,1,2,3, Clock pin 8

    // Load test data - all 16 possible 4-bit patterns
    uint8_t testData[16];
    for (int i = 0; i < 16; i++) {
        testData[i] = i;
    }
    spi.loadBuffer(testData, 16);

    // Verify buffer loaded
    CHECK_EQ(spi.getBufferLength(), 16);
    CHECK(spi.getBuffer() == testData);
}

TEST_CASE("SPIBlocking Quad: LUT initialization") {
    fl::SpiBlock4 spi;
    spi.setPinMapping(5, 6, 7, 8, 10);  // Data pins 5,6,7,8, Clock pin 10

    auto lut = spi.getLUTArray();

    // Verify LUT entries for 4-bit patterns
    // 0x00 (0000) - All pins low
    CHECK_EQ(lut[0x00].set_mask, 0u);
    CHECK_EQ(lut[0x00].clear_mask, (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8));

    // 0x01 (0001) - D0 high, others low
    CHECK_EQ(lut[0x01].set_mask, (1u << 5));
    CHECK_EQ(lut[0x01].clear_mask, (1u << 6) | (1u << 7) | (1u << 8));

    // 0x02 (0010) - D1 high, others low
    CHECK_EQ(lut[0x02].set_mask, (1u << 6));
    CHECK_EQ(lut[0x02].clear_mask, (1u << 5) | (1u << 7) | (1u << 8));

    // 0x03 (0011) - D0+D1 high, D2+D3 low
    CHECK_EQ(lut[0x03].set_mask, (1u << 5) | (1u << 6));
    CHECK_EQ(lut[0x03].clear_mask, (1u << 7) | (1u << 8));

    // 0x04 (0100) - D2 high, others low
    CHECK_EQ(lut[0x04].set_mask, (1u << 7));
    CHECK_EQ(lut[0x04].clear_mask, (1u << 5) | (1u << 6) | (1u << 8));

    // 0x08 (1000) - D3 high, others low
    CHECK_EQ(lut[0x08].set_mask, (1u << 8));
    CHECK_EQ(lut[0x08].clear_mask, (1u << 5) | (1u << 6) | (1u << 7));

    // 0x0F (1111) - All pins high
    CHECK_EQ(lut[0x0F].set_mask, (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8));
    CHECK_EQ(lut[0x0F].clear_mask, 0u);

    // Upper 4 bits should be ignored
    // 0xFF should be same as 0x0F
    CHECK_EQ(lut[0xFF].set_mask, (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8));
    CHECK_EQ(lut[0xFF].clear_mask, 0u);
}

TEST_CASE("SPIBlocking Quad: Empty buffer handling") {
    fl::SpiBlock4 spi;
    spi.setPinMapping(0, 1, 2, 3, 8);

    // Transmit with no buffer should not crash
    spi.transmit();

    // Load empty buffer
    uint8_t testData[1];
    spi.loadBuffer(testData, 0);
    spi.transmit();  // Should handle gracefully
}

TEST_CASE("SPIBlocking Quad: Maximum buffer size") {
    fl::SpiBlock4 spi;
    spi.setPinMapping(0, 1, 2, 3, 8);

    uint8_t largeBuffer[300];
    for (int i = 0; i < 300; i++) {
        largeBuffer[i] = (i & 0x0F);  // 4-bit pattern
    }

    // Should truncate to 256
    spi.loadBuffer(largeBuffer, 300);
    CHECK_EQ(spi.getBufferLength(), 256);
}

TEST_CASE("SPIBlocking Quad: All 16 patterns") {
    fl::SpiBlock4 spi;
    spi.setPinMapping(2, 3, 4, 5, 10);

    auto lut = spi.getLUTArray();

    // Verify all 16 fundamental 4-bit patterns
    for (int pattern = 0; pattern < 16; pattern++) {
        uint32_t expectedSet = 0;
        uint32_t expectedClear = 0;

        // Calculate expected masks
        for (int bit = 0; bit < 4; bit++) {
            uint32_t pinMask = 1u << (2 + bit);  // Pins 2,3,4,5
            if (pattern & (1 << bit)) {
                expectedSet |= pinMask;
            } else {
                expectedClear |= pinMask;
            }
        }

        CHECK_EQ(lut[pattern].set_mask, expectedSet);
        CHECK_EQ(lut[pattern].clear_mask, expectedClear);
    }
}

TEST_CASE("SPIBlocking Quad: Multiple pin configurations") {
    // Test a subset of pin configurations for quad-lane
    for (uint8_t d0 = 0; d0 < 3; d0++) {
        for (uint8_t d1 = 3; d1 < 5; d1++) {
            fl::SpiBlock4 spi;
            uint8_t d2 = d1 + 1;
            uint8_t d3 = d2 + 1;
            uint8_t clk = 10;
            spi.setPinMapping(d0, d1, d2, d3, clk);

            auto lut = spi.getLUTArray();

            // Verify fundamental patterns
            // 0x00 (0000) - All pins low
            CHECK_EQ(lut[0x00].set_mask, 0u);
            CHECK_EQ(lut[0x00].clear_mask, (1u << d0) | (1u << d1) | (1u << d2) | (1u << d3));

            // 0x0F (1111) - All pins high
            CHECK_EQ(lut[0x0F].set_mask, (1u << d0) | (1u << d1) | (1u << d2) | (1u << d3));
            CHECK_EQ(lut[0x0F].clear_mask, 0u);

            // 0x01 (0001) - Only D0 high
            CHECK_EQ(lut[0x01].set_mask, (1u << d0));
            CHECK_EQ(lut[0x01].clear_mask, (1u << d1) | (1u << d2) | (1u << d3));

            // 0x08 (1000) - Only D3 high
            CHECK_EQ(lut[0x08].set_mask, (1u << d3));
            CHECK_EQ(lut[0x08].clear_mask, (1u << d0) | (1u << d1) | (1u << d2));
        }
    }
}

TEST_CASE("SPIBlocking Quad: Pattern consistency") {
    fl::SpiBlock4 spi;
    spi.setPinMapping(1, 2, 3, 4, 9);

    auto lut = spi.getLUTArray();

    // All entries with same lower 4 bits should have same masks
    // Check all 16 patterns across the 256 entries
    for (int pattern = 0; pattern < 16; pattern++) {
        uint32_t expectedSet = lut[pattern].set_mask;
        uint32_t expectedClear = lut[pattern].clear_mask;

        // Test multiple byte values with same lower 4 bits
        for (int offset = 0; offset < 256; offset += 16) {
            int byteValue = pattern + offset;
            if (byteValue >= 256) break;

            CHECK_EQ(lut[byteValue].set_mask, expectedSet);
            CHECK_EQ(lut[byteValue].clear_mask, expectedClear);
        }
    }
}

// ============================================================================
// 8-Lane Octal-SPI Transpose Tests
// ============================================================================

TEST_CASE("SPITransposer: 8-lane basic bit interleaving - single byte") {
    // Test 8-lane interleaving with known bit patterns
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data[8];
    fl::vector<uint8_t> padding = {0x00};

    // Create 8 lanes with distinct patterns
    lane_data[0] = {0x01};  // 00000001
    lane_data[1] = {0x02};  // 00000010
    lane_data[2] = {0x04};  // 00000100
    lane_data[3] = {0x08};  // 00001000
    lane_data[4] = {0x10};  // 00010000
    lane_data[5] = {0x20};  // 00100000
    lane_data[6] = {0x40};  // 01000000
    lane_data[7] = {0x80};  // 10000000

    for (int i = 0; i < 8; i++) {
        lanes[i] = fl::SPITransposer::LaneData{lane_data[i], padding};
    }

    fl::vector<uint8_t> output(8);  // 1 byte * 8 = 8 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 8);

    // Each output byte should have 1 bit from each lane
    // Output[0] has MSB (bit 7) from each lane: 10000000 (only L7 has bit 7 set)
    CHECK_EQ(output[0], 0x80);
    // Output[1] has bit 6 from each lane: 01000000 (only L6 has bit 6 set)
    CHECK_EQ(output[1], 0x40);
    // Output[2] has bit 5 from each lane: 00100000 (only L5 has bit 5 set)
    CHECK_EQ(output[2], 0x20);
    // Output[3] has bit 4 from each lane: 00010000 (only L4 has bit 4 set)
    CHECK_EQ(output[3], 0x10);
    // Output[4] has bit 3 from each lane: 00001000 (only L3 has bit 3 set)
    CHECK_EQ(output[4], 0x08);
    // Output[5] has bit 2 from each lane: 00000100 (only L2 has bit 2 set)
    CHECK_EQ(output[5], 0x04);
    // Output[6] has bit 1 from each lane: 00000010 (only L1 has bit 1 set)
    CHECK_EQ(output[6], 0x02);
    // Output[7] has bit 0 (LSB) from each lane: 00000001 (only L0 has bit 0 set)
    CHECK_EQ(output[7], 0x01);
}

TEST_CASE("SPITransposer: 8-lane equal length lanes") {
    // All 8 lanes same size, no padding needed
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data[8];
    fl::vector<uint8_t> padding = {0x00};

    for (int i = 0; i < 8; i++) {
        lane_data[i] = {static_cast<uint8_t>(0xA0 + i), static_cast<uint8_t>(0xB0 + i)};
        lanes[i] = fl::SPITransposer::LaneData{lane_data[i], padding};
    }

    fl::vector<uint8_t> output(16);  // 2 bytes * 8 = 16 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 16);
}

TEST_CASE("SPITransposer: 8-lane different length lanes with padding") {
    // Different lane lengths: should pad shorter lanes at beginning
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data[8];
    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};  // APA102-style padding

    // Create lanes with varying lengths
    lane_data[0] = {0xAA, 0xBB, 0xCC};  // 3 bytes (max)
    lane_data[1] = {0xDD, 0xEE};         // 2 bytes
    lane_data[2] = {0xFF};               // 1 byte
    // lanes[3-7] empty

    for (int i = 0; i < 3; i++) {
        lanes[i] = fl::SPITransposer::LaneData{lane_data[i], padding};
    }
    for (int i = 3; i < 8; i++) {
        lanes[i] = fl::SPITransposer::LaneData{lane_data[i], padding};
    }

    fl::vector<uint8_t> output(24);  // 3 bytes * 8 = 24 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
}

TEST_CASE("SPITransposer: 8-lane empty lanes use nullopt") {
    // Only 4 lanes used (remaining should be treated as empty)
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data[4];
    fl::vector<uint8_t> padding = {0x00};

    for (int i = 0; i < 4; i++) {
        lane_data[i] = {0xAA, 0xBB};
        lanes[i] = fl::SPITransposer::LaneData{lane_data[i], padding};
    }
    // lanes[4-7] remain uninitialized (nullopt)

    fl::vector<uint8_t> output(16);  // 2 bytes * 8 = 16 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
}

TEST_CASE("SPITransposer: 8-lane output buffer validation - not divisible by 8") {
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data = {0xAA};
    fl::vector<uint8_t> padding = {0x00};

    lanes[0] = fl::SPITransposer::LaneData{lane_data, padding};

    fl::vector<uint8_t> output(10);  // Not divisible by 8
    const char* error = nullptr;
    bool success = fl::SPITransposer::transpose8(lanes, output, &error);

    CHECK_FALSE(success);
    CHECK(error != nullptr);
}

TEST_CASE("SPITransposer: 8-lane all ones and zeros pattern") {
    // Test with alternating 0xFF and 0x00 on all 8 lanes
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_ff = {0xFF};
    fl::vector<uint8_t> lane_00 = {0x00};
    fl::vector<uint8_t> padding = {0x00};

    for (int i = 0; i < 8; i++) {
        if (i % 2 == 0) {
            lanes[i] = fl::SPITransposer::LaneData{lane_ff, padding};
        } else {
            lanes[i] = fl::SPITransposer::LaneData{lane_00, padding};
        }
    }

    fl::vector<uint8_t> output(8);  // 1 byte * 8 = 8 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
    // Each output byte should have alternating bits: 01010101
    for (int i = 0; i < 8; i++) {
        CHECK_EQ(output[i], 0x55);
    }
}

TEST_CASE("SPITransposer: 8-lane multi-byte realistic data") {
    // Test with realistic multi-byte data across all 8 lanes
    fl::optional<fl::SPITransposer::LaneData> lanes[8];
    fl::vector<uint8_t> lane_data[8];
    fl::vector<uint8_t> padding = {0x00};

    for (int lane = 0; lane < 8; lane++) {
        for (int byte = 0; byte < 10; byte++) {
            lane_data[lane].push_back(static_cast<uint8_t>(lane * 16 + byte));
        }
        lanes[lane] = fl::SPITransposer::LaneData{lane_data[lane], padding};
    }

    fl::vector<uint8_t> output(80);  // 10 bytes * 8 = 80 output bytes
    bool success = fl::SPITransposer::transpose8(lanes, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 80);
}
