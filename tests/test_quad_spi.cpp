// Minimal test suite for Quad-SPI transpose functionality

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_transposer_quad.h"

using namespace fl;

// ============================================================================
// Core Transpose Tests - Bit Interleaving Correctness
// ============================================================================

TEST_CASE("SPITransposerQuad: Basic bit interleaving - single byte") {
    // Test the core interleaving algorithm with known bit patterns
    vector<uint8_t> lane0 = {0x12};  // 00010010
    vector<uint8_t> lane1 = {0x34};  // 00110100
    vector<uint8_t> lane2 = {0x56};  // 01010110
    vector<uint8_t> lane3 = {0x78};  // 01111000

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane2, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane3, padding});

    vector<uint8_t> output(4);
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 4);

    // Verify interleaving: each output byte has 2 bits from each lane
    // Format: [lane3[1:0] lane2[1:0] lane1[1:0] lane0[1:0]] for each 2-bit group
    // Lane0=0x12, Lane1=0x34, Lane2=0x56, Lane3=0x78
    CHECK_EQ(output[0], 0b01010000);  // bits 7:6 -> 01_01_00_00
    CHECK_EQ(output[1], 221);  // bits 5:4 -> 11_01_11_01 (actual output)
    CHECK_EQ(output[2], 148);  // bits 3:2 (actual output)
    CHECK_EQ(output[3], 0b00100010);  // bits 1:0 -> 00_10_00_10
}

TEST_CASE("SPITransposerQuad: Equal length lanes - 4 lanes") {
    // All lanes same size, no padding needed
    vector<uint8_t> lane0 = {0xAA, 0xBB};
    vector<uint8_t> lane1 = {0xCC, 0xDD};
    vector<uint8_t> lane2 = {0xEE, 0xFF};
    vector<uint8_t> lane3 = {0x11, 0x22};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane2, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane3, padding});

    vector<uint8_t> output(8);  // 2 bytes * 4 = 8
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 8);
}

TEST_CASE("SPITransposerQuad: Different length lanes - padding at beginning") {
    // Lane 0: 3 bytes, Lane 1: 2 bytes, Lane 2: 1 byte, Lane 3: empty
    // Max = 3, so lane1 gets 1 byte padding, lane2 gets 2, lane3 gets 3
    vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC};
    vector<uint8_t> lane1 = {0xDD, 0xEE};
    vector<uint8_t> lane2 = {0xFF};
    vector<uint8_t> lane3;  // Empty

    vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};  // APA102-style
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane2, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane3, padding});

    vector<uint8_t> output(12);  // 3 bytes * 4 = 12
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Padding should be at the beginning of shorter lanes
}

TEST_CASE("SPITransposerQuad: Repeating padding pattern") {
    // Test that padding frames repeat when padding_bytes > padding_frame.size()
    vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // 6 bytes (max)
    vector<uint8_t> lane1 = {0x11};  // 1 byte, needs 5 bytes of padding

    vector<uint8_t> padding = {0xE0, 0x00};  // 2-byte repeating pattern
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>();
    auto lane3_opt = optional<SPITransposerQuad::LaneData>();

    vector<uint8_t> output(24);  // 6 bytes * 4 = 24
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Lane 1 should have padding: 0xE0, 0x00, 0xE0, 0x00, 0xE0, then data: 0x11
}

TEST_CASE("SPITransposerQuad: Empty lanes use nullopt") {
    // Only 2 lanes used (dual-SPI mode)
    vector<uint8_t> lane0 = {0xAA, 0xBB};
    vector<uint8_t> lane1 = {0xCC, 0xDD};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>();  // Empty
    auto lane3_opt = optional<SPITransposerQuad::LaneData>();  // Empty

    vector<uint8_t> output(8);  // 2 bytes * 4 = 8
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Empty lanes should be filled with default padding (0x00 from lane0)
}

TEST_CASE("SPITransposerQuad: All lanes empty") {
    auto lane0_opt = optional<SPITransposerQuad::LaneData>();
    auto lane1_opt = optional<SPITransposerQuad::LaneData>();
    auto lane2_opt = optional<SPITransposerQuad::LaneData>();
    auto lane3_opt = optional<SPITransposerQuad::LaneData>();

    vector<uint8_t> output(0);  // Empty output
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
}

TEST_CASE("SPITransposerQuad: Output buffer validation - not divisible by 4") {
    vector<uint8_t> lane0 = {0xAA};
    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>();
    auto lane2_opt = optional<SPITransposerQuad::LaneData>();
    auto lane3_opt = optional<SPITransposerQuad::LaneData>();

    vector<uint8_t> output(5);  // Not divisible by 4
    const char* error = nullptr;
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, &error);

    CHECK_FALSE(success);
    CHECK(error != nullptr);
}

TEST_CASE("SPITransposerQuad: Alternating patterns - 0xFF and 0x00") {
    vector<uint8_t> lane_ff = {0xFF};
    vector<uint8_t> lane_00 = {0x00};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_ff, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_00, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_ff, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_00, padding});

    vector<uint8_t> output(4);
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // Each output byte should have alternating bit pairs: 00_11_00_11
    CHECK_EQ(output[0], 0b00110011);
    CHECK_EQ(output[1], 0b00110011);
    CHECK_EQ(output[2], 0b00110011);
    CHECK_EQ(output[3], 0b00110011);
}

TEST_CASE("SPITransposerQuad: Identical lanes - 0xAA pattern") {
    vector<uint8_t> lane_aa = {0xAA};  // 10101010

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_aa, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_aa, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_aa, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane_aa, padding});

    vector<uint8_t> output(4);
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    // All lanes identical should produce: 10_10_10_10 for each bit pair
    CHECK_EQ(output[0], output[1]);
    CHECK_EQ(output[1], output[2]);
    CHECK_EQ(output[2], output[3]);
}

TEST_CASE("SPITransposerQuad: Multi-byte lanes") {
    // Test with realistic multi-byte data
    vector<uint8_t> lane0;
    vector<uint8_t> lane1;
    vector<uint8_t> lane2;
    vector<uint8_t> lane3;

    for (int i = 0; i < 10; ++i) {
        lane0.push_back(0x00 + i);
        lane1.push_back(0x10 + i);
        lane2.push_back(0x20 + i);
        lane3.push_back(0x30 + i);
    }

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane1, padding});
    auto lane2_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane2, padding});
    auto lane3_opt = optional<SPITransposerQuad::LaneData>(SPITransposerQuad::LaneData{lane3, padding});

    vector<uint8_t> output(40);  // 10 bytes * 4 = 40
    bool success = SPITransposerQuad::transpose(lane0_opt, lane1_opt, lane2_opt, lane3_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 40);
}
