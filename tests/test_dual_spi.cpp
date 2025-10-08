// Minimal test suite for Dual-SPI transpose functionality

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_transposer_dual.h"

using namespace fl;

// ============================================================================
// Core Transpose Tests - Bit Interleaving Correctness
// ============================================================================

TEST_CASE("SPITransposerDual: Basic bit interleaving - single byte") {
    // Test the core interleaving algorithm with known bit patterns
    vector<uint8_t> lane0 = {0xAB};  // 10101011
    vector<uint8_t> lane1 = {0x12};  // 00010010

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(2);
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 2);

    // Verify interleaving: each output byte has 4 bits from each lane
    // Format: [lane1_nibble lane0_nibble]
    // Lane0=0xAB (hi=0xA, lo=0xB), Lane1=0x12 (hi=0x1, lo=0x2)
    CHECK_EQ(output[0], 0x1A);  // bits 7:4 -> lane1=0x1, lane0=0xA
    CHECK_EQ(output[1], 0x2B);  // bits 3:0 -> lane1=0x2, lane0=0xB
}

TEST_CASE("SPITransposerDual: Equal length lanes - 2 lanes") {
    // Both lanes same size, no padding needed
    vector<uint8_t> lane0 = {0xAA, 0xBB};
    vector<uint8_t> lane1 = {0xCC, 0xDD};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(4);  // 2 bytes * 2 = 4
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 4);
}

TEST_CASE("SPITransposerDual: Different length lanes - padding at beginning") {
    // Lane 0: 3 bytes, Lane 1: 2 bytes
    // Max = 3, so lane1 gets 1 byte padding
    vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC};
    vector<uint8_t> lane1 = {0xDD, 0xEE};

    vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};  // APA102-style
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(6);  // 3 bytes * 2 = 6
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    // Padding should be at the beginning of shorter lane
}

TEST_CASE("SPITransposerDual: Repeating padding pattern") {
    // Test that padding frames repeat when padding_bytes > padding_frame.size()
    vector<uint8_t> lane0 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // 6 bytes (max)
    vector<uint8_t> lane1 = {0x11};  // 1 byte, needs 5 bytes of padding

    vector<uint8_t> padding = {0xE0, 0x00};  // 2-byte repeating pattern
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(12);  // 6 bytes * 2 = 12
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    // Lane 1 should have padding: 0xE0, 0x00, 0xE0, 0x00, 0xE0, then data: 0x11
}

TEST_CASE("SPITransposerDual: Empty lane uses nullopt") {
    // Only 1 lane used
    vector<uint8_t> lane0 = {0xAA, 0xBB};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>();  // Empty

    vector<uint8_t> output(4);  // 2 bytes * 2 = 4
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    // Empty lane should be filled with default padding (0x00 from lane0)
}

TEST_CASE("SPITransposerDual: All lanes empty") {
    auto lane0_opt = optional<SPITransposerDual::LaneData>();
    auto lane1_opt = optional<SPITransposerDual::LaneData>();

    vector<uint8_t> output(0);  // Empty output
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
}

TEST_CASE("SPITransposerDual: Output buffer validation - not divisible by 2") {
    vector<uint8_t> lane0 = {0xAA};
    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>();

    vector<uint8_t> output(3);  // Not divisible by 2
    const char* error = nullptr;
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, &error);

    CHECK_FALSE(success);
    CHECK(error != nullptr);
}

TEST_CASE("SPITransposerDual: Alternating patterns - 0xFF and 0x00") {
    vector<uint8_t> lane_ff = {0xFF};
    vector<uint8_t> lane_00 = {0x00};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane_ff, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane_00, padding});

    vector<uint8_t> output(2);
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    // Lane0=0xFF (hi=0xF, lo=0xF), Lane1=0x00 (hi=0x0, lo=0x0)
    // Output should be: [0x0F, 0x0F]
    CHECK_EQ(output[0], 0x0F);
    CHECK_EQ(output[1], 0x0F);
}

TEST_CASE("SPITransposerDual: Identical lanes - 0xAA pattern") {
    vector<uint8_t> lane_aa = {0xAA};  // 10101010

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane_aa, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane_aa, padding});

    vector<uint8_t> output(2);
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    // Both lanes identical: Lane0=0xAA (hi=0xA, lo=0xA), Lane1=0xAA
    // Output should be: [0xAA, 0xAA]
    CHECK_EQ(output[0], 0xAA);
    CHECK_EQ(output[1], 0xAA);
}

TEST_CASE("SPITransposerDual: Multi-byte lanes") {
    // Test with realistic multi-byte data
    vector<uint8_t> lane0;
    vector<uint8_t> lane1;

    for (int i = 0; i < 10; ++i) {
        lane0.push_back(0x00 + i);
        lane1.push_back(0x10 + i);
    }

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(20);  // 10 bytes * 2 = 20
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);
    CHECK_EQ(output.size(), 20);
}

TEST_CASE("SPITransposerDual: Verify bit-level interleaving") {
    // Detailed verification of bit interleaving logic
    vector<uint8_t> lane0 = {0b11001010};  // 0xCA
    vector<uint8_t> lane1 = {0b01010011};  // 0x53

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane1, padding});

    vector<uint8_t> output(2);
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);

    // Lane0=0xCA (hi=0xC, lo=0xA), Lane1=0x53 (hi=0x5, lo=0x3)
    // Output byte 0: [lane1_hi lane0_hi] = [0x5 0xC] = 0x5C
    // Output byte 1: [lane1_lo lane0_lo] = [0x3 0xA] = 0x3A
    CHECK_EQ(output[0], 0x5C);
    CHECK_EQ(output[1], 0x3A);
}

TEST_CASE("SPITransposerDual: Zero padding for missing lanes") {
    // Test that missing lanes get filled with zeros
    vector<uint8_t> lane0 = {0xFF, 0xAA};

    vector<uint8_t> padding = {0x00};
    auto lane0_opt = optional<SPITransposerDual::LaneData>(SPITransposerDual::LaneData{lane0, padding});
    auto lane1_opt = optional<SPITransposerDual::LaneData>();  // Missing

    vector<uint8_t> output(4);
    bool success = SPITransposerDual::transpose(lane0_opt, lane1_opt, output, nullptr);

    CHECK(success);

    // Lane0=0xFF, Lane1=0x00 (default)
    // Output[0]: [0x0 0xF] = 0x0F
    // Output[1]: [0x0 0xF] = 0x0F
    CHECK_EQ(output[0], 0x0F);
    CHECK_EQ(output[1], 0x0F);

    // Lane0=0xAA, Lane1=0x00
    // Output[2]: [0x0 0xA] = 0x0A
    // Output[3]: [0x0 0xA] = 0x0A
    CHECK_EQ(output[2], 0x0A);
    CHECK_EQ(output[3], 0x0A);
}

// ============================================================================
// Hardware Interface Tests (using stub)
// ============================================================================

#ifdef FASTLED_TESTING
#include "platforms/stub/spi_dual_stub.h"

TEST_CASE("SPIDual: Hardware initialization") {
    const auto& controllers = SPIDual::getAll();
    CHECK(controllers.size() > 0);

    SPIDual* dual = controllers[0];
    CHECK(dual != nullptr);

    SPIDual::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 40000000;
    config.clock_pin = 18;
    config.data0_pin = 23;
    config.data1_pin = 19;

    CHECK(dual->begin(config));
    CHECK(dual->isInitialized());
    CHECK_EQ(dual->getBusId(), 0);

    dual->end();
    CHECK_FALSE(dual->isInitialized());
}

TEST_CASE("SPIDual: Async transmission") {
    const auto& controllers = SPIDual::getAll();
    SPIDual* dual = controllers[0];

    SPIDual::Config config;
    config.bus_num = 0;
    config.clock_speed_hz = 40000000;
    config.clock_pin = 18;
    config.data0_pin = 23;
    config.data1_pin = 19;

    CHECK(dual->begin(config));

    vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};
    CHECK(dual->transmitAsync(fl::span<const uint8_t>(data)));
    CHECK(dual->isBusy());

    CHECK(dual->waitComplete());
    CHECK_FALSE(dual->isBusy());

    dual->end();
}

TEST_CASE("SPIDual: Stub inspection") {
    const auto& controllers = SPIDual::getAll();
    SPIDualStub* stub = toStub(controllers[0]);
    CHECK(stub != nullptr);

    stub->reset();

    SPIDual::Config config;
    config.bus_num = 0;
    CHECK(stub->begin(config));

    vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
    CHECK(stub->transmitAsync(fl::span<const uint8_t>(test_data)));

    const auto& transmitted = stub->getLastTransmission();
    CHECK_EQ(transmitted.size(), 4);
    CHECK_EQ(transmitted[0], 0xAA);
    CHECK_EQ(transmitted[1], 0xBB);

    stub->end();
}

TEST_CASE("SPIDual: Extract lanes from interleaved data") {
    const auto& controllers = SPIDual::getAll();
    SPIDualStub* stub = toStub(controllers[0]);

    stub->reset();

    SPIDual::Config config;
    CHECK(stub->begin(config));

    // Create interleaved data manually
    // Lane0=0xAB, Lane1=0x12
    // Interleaved: [0x1A, 0x2B]
    vector<uint8_t> interleaved = {0x1A, 0x2B};
    stub->transmitAsync(fl::span<const uint8_t>(interleaved));

    auto lanes = stub->extractLanes(2, 1);
    CHECK_EQ(lanes.size(), 2);
    CHECK_EQ(lanes[0].size(), 1);
    CHECK_EQ(lanes[1].size(), 1);

    CHECK_EQ(lanes[0][0], 0xAB);
    CHECK_EQ(lanes[1][0], 0x12);

    stub->end();
}

#endif  // FASTLED_TESTING
