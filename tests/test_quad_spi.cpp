// Comprehensive QuadSPI tests for ESP32 parallel LED output
// Tests bit-interleaving, mock driver, controller integration, and performance

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/quad_spi_transposer.h"
#include "platforms/stub/quad_spi_driver.h"
#include <chrono>

using namespace fl;

// ============================================================================
// Test-only QuadSPI controller (inlined from helpers/quad_spi_test_controller.h)
// ============================================================================

/// Test-only QuadSPI controller that uses MockQuadSPIDriver
/// @tparam SPI_BUS_NUM SPI bus number (ignored in tests)
/// @tparam SPI_CLOCK_HZ Clock frequency in Hz
template<uint8_t SPI_BUS_NUM = 2, uint32_t SPI_CLOCK_HZ = 10000000>
class QuadSPITestController {
private:
    MockQuadSPIDriver mMockDriver;
    QuadSPITransposer mTransposer;
    fl::vector<fl::vector<uint8_t>> mLaneBuffers;
    fl::vector<uint8_t> mPaddingBytes;
    fl::vector<uint8_t> mInterleavedDMABuffer;
    size_t mMaxLaneBytes = 0;
    uint8_t mNumLanes = 0;

public:
    QuadSPITestController() = default;

    void begin() {
        mMockDriver.setClockSpeed(SPI_CLOCK_HZ);
    }

    template<typename CONTROLLER_TYPE>
    void addLane(uint8_t lane_id, size_t num_leds) {
        if (lane_id >= 4) return;

        size_t bytes_needed = CONTROLLER_TYPE::calculateBytes(num_leds);
        uint8_t padding = CONTROLLER_TYPE::getPaddingByte();

        if (mLaneBuffers.size() <= lane_id) {
            mLaneBuffers.resize(lane_id + 1);
            mPaddingBytes.resize(lane_id + 1);
        }

        mLaneBuffers[lane_id].resize(bytes_needed);
        mPaddingBytes[lane_id] = padding;

        if (bytes_needed > mMaxLaneBytes) {
            mMaxLaneBytes = bytes_needed;
        }

        mNumLanes = fl::fl_max(mNumLanes, static_cast<uint8_t>(lane_id + 1));
    }

    void finalize() {
        for (size_t i = 0; i < mLaneBuffers.size(); ++i) {
            if (mLaneBuffers[i].size() < mMaxLaneBytes) {
                mLaneBuffers[i].resize(mMaxLaneBytes, mPaddingBytes[i]);
            }
        }

        mInterleavedDMABuffer.resize(mMaxLaneBytes * 4);
    }

    fl::vector<uint8_t>* getLaneBuffer(uint8_t lane_id) {
        if (lane_id >= mLaneBuffers.size()) return nullptr;
        return &mLaneBuffers[lane_id];
    }

    void transmit() {
        mTransposer.reset();

        for (size_t i = 0; i < mLaneBuffers.size(); ++i) {
            if (!mLaneBuffers[i].empty()) {
                mTransposer.addLane(i, mLaneBuffers[i], mPaddingBytes[i]);
            }
        }

        auto interleaved = mTransposer.transpose();
        if (!interleaved.empty()) {
            mMockDriver.transmitDMA(interleaved.data(), interleaved.size());
        }
    }

    void waitComplete() {
        // Mock driver completes instantly
    }

    uint8_t getNumLanes() const { return mNumLanes; }
    size_t getMaxLaneBytes() const { return mMaxLaneBytes; }

    MockQuadSPIDriver& getMockDriver() { return mMockDriver; }
};

// Alias for tests
template<uint8_t SPI_BUS_NUM = 2, uint32_t SPI_CLOCK_HZ = 10000000>
using QuadSPIController = QuadSPITestController<SPI_BUS_NUM, SPI_CLOCK_HZ>;

// ============================================================================
// Test helper functions (inlined from helpers/apa102_test_helpers.h)
// ============================================================================

namespace TestHelpers {

// Generate APA102 protocol data for testing
inline fl::vector<uint8_t> generateAPA102ProtocolData(int num_leds, uint8_t pattern_byte) {
    fl::vector<uint8_t> data;
    data.push_back(0x00); data.push_back(0x00); data.push_back(0x00); data.push_back(0x00);
    for (int i = 0; i < num_leds; i++) {
        data.push_back(0xFF); data.push_back(pattern_byte); data.push_back(pattern_byte); data.push_back(pattern_byte);
    }
    int end_frame_bytes = fl::fl_max(4, (num_leds + 15) / 16);
    for (int i = 0; i < end_frame_bytes; i++) { data.push_back(0xFF); }
    return data;
}

inline size_t calculateAPA102Size(int num_leds) {
    return 4 + (num_leds * 4) + fl::fl_max(4, (num_leds + 15) / 16);
}

// De-interleave quad-SPI data
inline fl::vector<fl::vector<uint8_t>> deinterleaveLanes(
    const fl::vector<uint8_t>& interleaved, const fl::vector<size_t>& lane_sizes) {
    fl::vector<fl::vector<uint8_t>> lanes(4);
    for (size_t i = 0; i < 4; i++) {
        lanes[i].resize(lane_sizes[i], 0);
    }
    for (size_t out_idx = 0; out_idx < interleaved.size(); out_idx++) {
        uint8_t interleaved_byte = interleaved[out_idx];
        size_t in_byte_idx = out_idx / 4;
        size_t nibble_idx = out_idx % 4;
        for (size_t lane = 0; lane < 4; lane++) {
            if (in_byte_idx < lane_sizes[lane]) {
                uint8_t bits = (interleaved_byte >> (lane * 2)) & 0b11;
                lanes[lane][in_byte_idx] |= (bits << ((3 - nibble_idx) * 2));
            }
        }
    }
    return lanes;
}

inline bool verifyBitInterleaving(const fl::vector<uint8_t>& interleaved,
                                  const fl::vector<fl::vector<uint8_t>>& original_lanes) {
    fl::vector<size_t> lane_sizes;
    for (const auto& lane : original_lanes) { lane_sizes.push_back(lane.size()); }
    auto recovered = deinterleaveLanes(interleaved, lane_sizes);
    for (size_t i = 0; i < original_lanes.size(); i++) {
        if (recovered[i].size() != original_lanes[i].size()) return false;
        for (size_t j = 0; j < original_lanes[i].size(); j++) {
            if (recovered[i][j] != original_lanes[i][j]) return false;
        }
    }
    return true;
}

inline fl::vector<size_t> getLaneSizes(const fl::vector<fl::vector<uint8_t>>& lanes) {
    fl::vector<size_t> sizes;
    for (const auto& lane : lanes) { sizes.push_back(lane.size()); }
    return sizes;
}

inline bool validateStartFrameInterleaving(const fl::vector<uint8_t>& interleaved,
                                           size_t start_idx, size_t num_bytes) {
    for (size_t i = start_idx; i < start_idx + num_bytes && i < interleaved.size(); i++) {
        if (interleaved[i] != 0x00) return false;
    }
    return true;
}

inline bool validateEndFrameInterleaving(const fl::vector<uint8_t>& interleaved,
                                         size_t start_idx, size_t num_bytes) {
    for (size_t i = start_idx; i < start_idx + num_bytes && i < interleaved.size(); i++) {
        if (interleaved[i] != 0xFF) return false;
    }
    return true;
}

inline bool validateBrightnessInterleaving(const fl::vector<uint8_t>& interleaved,
                                           size_t start_idx, size_t num_bytes) {
    for (size_t i = start_idx; i < start_idx + num_bytes && i < interleaved.size(); i++) {
        if (interleaved[i] != 0xFF) return false;
    }
    return true;
}

inline bool validatePaddingRegion(const fl::vector<uint8_t>& interleaved,
                                  size_t actual_size, size_t max_size, uint8_t expected_padding) {
    size_t padding_start = (actual_size / 4) * 4;
    for (size_t i = padding_start; i < (max_size / 4) * 4 && i < interleaved.size(); i++) {
        if (interleaved[i] != expected_padding) return false;
    }
    return true;
}

inline fl::vector<uint8_t> generateLPD8806ProtocolData(int num_leds, uint8_t pattern_byte) {
    fl::vector<uint8_t> data;
    for (int i = 0; i < num_leds; i++) {
        data.push_back(0x80 | (pattern_byte & 0x7F));
        data.push_back(0x80 | (pattern_byte & 0x7F));
        data.push_back(0x80 | (pattern_byte & 0x7F));
    }
    int latch_bytes = fl::fl_max(1, (num_leds + 31) / 32);
    for (int i = 0; i < latch_bytes; i++) { data.push_back(0x00); }
    return data;
}

inline size_t calculateLPD8806Size(int num_leds) {
    return (num_leds * 3) + fl::fl_max(1, (num_leds + 31) / 32);
}

inline fl::vector<uint8_t> generateWS2801ProtocolData(int num_leds, uint8_t pattern_byte) {
    fl::vector<uint8_t> data;
    for (int i = 0; i < num_leds; i++) {
        data.push_back(pattern_byte); data.push_back(pattern_byte); data.push_back(pattern_byte);
    }
    data.push_back(0x00);
    return data;
}

inline size_t calculateWS2801Size(int num_leds) {
    return num_leds * 3 + 1;
}

inline fl::vector<uint8_t> generateP9813ProtocolData(int num_leds, uint8_t pattern_byte) {
    fl::vector<uint8_t> data;
    data.push_back(0x00); data.push_back(0x00); data.push_back(0x00); data.push_back(0x00);
    for (int i = 0; i < num_leds; i++) {
        data.push_back(0xC0); data.push_back(pattern_byte); data.push_back(pattern_byte); data.push_back(pattern_byte);
    }
    data.push_back(0x00); data.push_back(0x00); data.push_back(0x00); data.push_back(0x00);
    return data;
}

inline size_t calculateP9813Size(int num_leds) {
    return 4 + (num_leds * 4) + 4;
}

}  // namespace TestHelpers

// ============================================================================
// SECTION 1: Chipset Padding Bytes (7 tests)
// ============================================================================

TEST_CASE("QuadSPI: APA102 padding byte") {
    uint8_t padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0xFF);
}

TEST_CASE("QuadSPI: LPD8806 padding byte") {
    uint8_t padding = LPD8806Controller<3, 4, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE("QuadSPI: WS2801 padding byte") {
    uint8_t padding = WS2801Controller<5, 6, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE("QuadSPI: P9813 padding byte") {
    uint8_t padding = P9813Controller<7, 8, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE("QuadSPI: SK9822 inherits APA102 padding") {
    uint8_t sk9822_padding = SK9822Controller<1, 2, RGB>::getPaddingByte();
    uint8_t apa102_padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(sk9822_padding, apa102_padding);
    CHECK_EQ(sk9822_padding, 0xFF);
}

TEST_CASE("QuadSPI: HD107 inherits APA102 padding") {
    uint8_t hd107_padding = HD107Controller<1, 2, RGB>::getPaddingByte();
    uint8_t apa102_padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(hd107_padding, apa102_padding);
    CHECK_EQ(hd107_padding, 0xFF);
}

TEST_CASE("QuadSPI: Mixed chipsets have correct padding") {
    CHECK_EQ(APA102Controller<1, 2, RGB>::getPaddingByte(), 0xFF);
    CHECK_EQ(LPD8806Controller<3, 4, RGB>::getPaddingByte(), 0x00);
    CHECK_EQ(WS2801Controller<5, 6, RGB>::getPaddingByte(), 0x00);
    CHECK_EQ(P9813Controller<7, 8, RGB>::getPaddingByte(), 0x00);
}

// ============================================================================
// SECTION 2: calculateBytes() Tests (7 tests)
// ============================================================================

TEST_CASE("QuadSPI: calculateBytes() is constexpr") {
    constexpr size_t apa102_size = APA102Controller<1, 2, RGB>::calculateBytes(100);
    constexpr size_t lpd8806_size = LPD8806Controller<3, 4, RGB>::calculateBytes(100);
    constexpr size_t ws2801_size = WS2801Controller<5, 6, RGB>::calculateBytes(100);
    constexpr size_t p9813_size = P9813Controller<7, 8, RGB>::calculateBytes(100);

    CHECK(apa102_size > 0);
    CHECK(lpd8806_size > 0);
    CHECK(ws2801_size > 0);
    CHECK(p9813_size > 0);
}

TEST_CASE("QuadSPI: APA102 calculateBytes") {
    // APA102: 4 + (num_leds * 4) + (4 * ((num_leds / 32) + 1))
    size_t bytes_10 = APA102Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = APA102Controller<1, 2, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 48);   // 4 + 40 + 4 = 48
    CHECK_EQ(bytes_100, 420); // 4 + 400 + 16 = 420
}

TEST_CASE("QuadSPI: LPD8806 calculateBytes") {
    // LPD8806: (num_leds * 3) + ((num_leds * 3 + 63) / 64)
    size_t bytes_10 = LPD8806Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = LPD8806Controller<3, 4, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 31);  // 30 + 1 = 31
    CHECK_EQ(bytes_100, 305); // 300 + 5 = 305
}

TEST_CASE("QuadSPI: WS2801 calculateBytes") {
    // WS2801: num_leds * 3 (no overhead)
    size_t bytes_10 = WS2801Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = WS2801Controller<5, 6, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 30);
    CHECK_EQ(bytes_100, 300);
}

TEST_CASE("QuadSPI: P9813 calculateBytes") {
    // P9813: 4 + (num_leds * 4) + 4
    size_t bytes_10 = P9813Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = P9813Controller<7, 8, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 48);   // 4 + 40 + 4 = 48
    CHECK_EQ(bytes_100, 408); // 4 + 400 + 4 = 408
}

TEST_CASE("QuadSPI: calculateBytes() for mixed chipset scenario") {
    // Simulate SpecialDrivers/ESP/QuadSPI/MultiChipset example:
    size_t lane0 = APA102Controller<1, 2, RGB>::calculateBytes(60);
    size_t lane1 = LPD8806Controller<3, 4, RGB>::calculateBytes(40);
    size_t lane2 = WS2801Controller<5, 6, RGB>::calculateBytes(80);
    size_t lane3 = APA102Controller<7, 8, RGB>::calculateBytes(100);

    size_t max_bytes = fl::fl_max(fl::fl_max(lane0, lane1), fl::fl_max(lane2, lane3));
    CHECK_EQ(max_bytes, 420);  // Lane 3 (APA102 100 LEDs) is largest
}

TEST_CASE("QuadSPI: calculateBytes() edge case - zero LEDs") {
    CHECK_EQ(APA102Controller<1, 2, RGB>::calculateBytes(0), 8);
    CHECK_EQ(LPD8806Controller<3, 4, RGB>::calculateBytes(0), 0);
    CHECK_EQ(WS2801Controller<5, 6, RGB>::calculateBytes(0), 0);
    CHECK_EQ(P9813Controller<7, 8, RGB>::calculateBytes(0), 8);
}

// ============================================================================
// SECTION 3: Bit-Interleaving Transpose Tests (13 tests)
// ============================================================================

TEST_CASE("QuadSPI: Basic 4-lane transpose with variable LED counts") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(1, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(2, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(7, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();

    size_t max_size = lane3.size();
    CHECK_EQ(interleaved.size(), max_size * 4);
}

TEST_CASE("QuadSPI: Transpose with equal-length lanes") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(5, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(5, 0x22);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(5, 0x33);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(5, 0x44);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane0.size() * 4);
}

TEST_CASE("QuadSPI: Single-lane transpose (degraded mode)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(3, 0xAB);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane0.size() * 4);
}

TEST_CASE("QuadSPI: Dual-lane transpose") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(4, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(6, 0x22);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane1.size() * 4);
}

TEST_CASE("QuadSPI: Mixed chipset transpose with different padding") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(3, 0xAA);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(5, 0xBB);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateP9813ProtocolData(6, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));
    CHECK_EQ(interleaved.size(), max_size * 4);
}

TEST_CASE("QuadSPI: APA102-only transpose (all 0xFF padding)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(2, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(3, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(5, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE("QuadSPI: LPD8806-only transpose (all 0x00 padding)") {
    auto lane0 = TestHelpers::generateLPD8806ProtocolData(3, 0x11);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(4, 0x22);
    auto lane2 = TestHelpers::generateLPD8806ProtocolData(5, 0x33);
    auto lane3 = TestHelpers::generateLPD8806ProtocolData(6, 0x44);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE("QuadSPI: WS2801-only transpose") {
    auto lane0 = TestHelpers::generateWS2801ProtocolData(5, 0xAA);
    auto lane1 = TestHelpers::generateWS2801ProtocolData(7, 0xBB);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(6, 0xCC);
    auto lane3 = TestHelpers::generateWS2801ProtocolData(8, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane3.size() * 4);  // lane3 is longest (8 LEDs)
}

TEST_CASE("QuadSPI: P9813-only transpose") {
    auto lane0 = TestHelpers::generateP9813ProtocolData(4, 0x55);
    auto lane1 = TestHelpers::generateP9813ProtocolData(5, 0x66);
    auto lane2 = TestHelpers::generateP9813ProtocolData(6, 0x77);
    auto lane3 = TestHelpers::generateP9813ProtocolData(7, 0x88);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE("QuadSPI: Empty transpose") {
    QuadSPITransposer transposer;
    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), 0);
}

TEST_CASE("QuadSPI: Reset and reuse transposer") {
    QuadSPITransposer transposer;

    // First use
    auto lane0_a = TestHelpers::generateAPA102ProtocolData(5, 0xAA);
    transposer.addLane(0, lane0_a, 0xFF);
    auto result_a = transposer.transpose();
    CHECK_EQ(result_a.size(), lane0_a.size() * 4);

    // Reset and reuse
    transposer.reset();
    auto lane0_b = TestHelpers::generateAPA102ProtocolData(10, 0xBB);
    transposer.addLane(0, lane0_b, 0xFF);
    auto result_b = transposer.transpose();
    CHECK_EQ(result_b.size(), lane0_b.size() * 4);
}

TEST_CASE("QuadSPI: Large buffer transpose (stress test)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(150, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(200, 0x22);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(175, 0x33);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(250, 0x44);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE("QuadSPI: Interleave order validation") {
    fl::vector<uint8_t> lane0 = {0xAA};
    fl::vector<uint8_t> lane1 = {0xBB};
    fl::vector<uint8_t> lane2 = {0xCC};
    fl::vector<uint8_t> lane3 = {0xDD};

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), 4);

    // Verify round-trip extraction works correctly
    MockQuadSPIDriver driver;
    driver.transmitDMA(interleaved.data(), interleaved.size());
    auto extracted = driver.extractLanes(4, 1);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][0], 0xBB);
    CHECK_EQ(extracted[2][0], 0xCC);
    CHECK_EQ(extracted[3][0], 0xDD);
}

// ============================================================================
// SECTION 4: Mock Driver Tests (13 tests)
// ============================================================================

TEST_CASE("QuadSPI: Mock driver initialization") {
    MockQuadSPIDriver driver;
    CHECK_EQ(driver.getClockSpeed(), 40000000);
    CHECK_EQ(driver.getTransmissionCount(), 0);
    CHECK(!driver.isTransmissionActive());
}

TEST_CASE("QuadSPI: Mock driver clock speed configuration") {
    MockQuadSPIDriver driver;
    driver.setClockSpeed(10000000);
    CHECK_EQ(driver.getClockSpeed(), 10000000);

    driver.setClockSpeed(40000000);
    CHECK_EQ(driver.getClockSpeed(), 40000000);
}

TEST_CASE("QuadSPI: Mock driver DMA transmission tracking") {
    MockQuadSPIDriver driver;
    driver.setClockSpeed(40000000);

    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    driver.transmitDMA(test_data, sizeof(test_data));

    CHECK_EQ(driver.getTransmissionCount(), 1);
    CHECK(driver.isTransmissionActive());
}

TEST_CASE("QuadSPI: Mock driver timing estimation accuracy") {
    MockQuadSPIDriver driver;
    driver.setClockSpeed(10000000);

    uint64_t time_10mhz = driver.estimateTransmissionTimeMicros(100);
    CHECK_EQ(time_10mhz, 80);  // 100 bytes * 8 bits / 10 MHz = 80µs

    driver.setClockSpeed(40000000);
    uint64_t time_40mhz = driver.estimateTransmissionTimeMicros(100);
    CHECK_EQ(time_40mhz, 20);  // 4× faster
}

TEST_CASE("QuadSPI: Mock driver timing edge cases") {
    MockQuadSPIDriver driver;
    driver.setClockSpeed(1000000);

    CHECK_EQ(driver.estimateTransmissionTimeMicros(0), 0);
    CHECK_EQ(driver.estimateTransmissionTimeMicros(1), 8);
    CHECK_EQ(driver.estimateTransmissionTimeMicros(1000), 8000);
}

TEST_CASE("QuadSPI: Mock driver lane extraction basic") {
    MockQuadSPIDriver driver;
    QuadSPITransposer transposer;

    fl::vector<uint8_t> lane0_data = {0xAA};
    fl::vector<uint8_t> lane1_data = {0xBB};
    fl::vector<uint8_t> lane2_data = {0xCC};
    fl::vector<uint8_t> lane3_data = {0xDD};

    transposer.addLane(0, lane0_data, 0x00);
    transposer.addLane(1, lane1_data, 0x00);
    transposer.addLane(2, lane2_data, 0x00);
    transposer.addLane(3, lane3_data, 0x00);

    auto interleaved = transposer.transpose();
    driver.transmitDMA(interleaved.data(), interleaved.size());

    auto extracted = driver.extractLanes(4, 1);
    CHECK_EQ(extracted.size(), 4);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][0], 0xBB);
    CHECK_EQ(extracted[2][0], 0xCC);
    CHECK_EQ(extracted[3][0], 0xDD);
}

TEST_CASE("QuadSPI: Mock driver lane extraction multi-byte") {
    MockQuadSPIDriver driver;
    QuadSPITransposer transposer;

    fl::vector<uint8_t> lane0 = {0x11, 0x22, 0x33};
    fl::vector<uint8_t> lane1 = {0x44, 0x55, 0x66};
    fl::vector<uint8_t> lane2 = {0x77, 0x88, 0x99};
    fl::vector<uint8_t> lane3 = {0xAA, 0xBB, 0xCC};

    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    driver.transmitDMA(interleaved.data(), interleaved.size());

    auto extracted = driver.extractLanes(4, 3);
    CHECK_EQ(extracted[0][0], 0x11);
    CHECK_EQ(extracted[0][2], 0x33);
    CHECK_EQ(extracted[3][1], 0xBB);
}

TEST_CASE("QuadSPI: Mock driver lane extraction with padding") {
    MockQuadSPIDriver driver;
    QuadSPITransposer transposer;

    fl::vector<uint8_t> lane0 = {0xAA};
    fl::vector<uint8_t> lane1 = {0xBB, 0xCC};
    fl::vector<uint8_t> lane2 = {0xDD, 0xEE, 0xFF};
    fl::vector<uint8_t> lane3 = {0x11, 0x22};

    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0x00);
    transposer.addLane(2, lane2, 0x00);
    transposer.addLane(3, lane3, 0x00);

    auto interleaved = transposer.transpose();
    driver.transmitDMA(interleaved.data(), interleaved.size());

    auto extracted = driver.extractLanes(4, 3);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[0][1], 0xFF);  // APA102 padding
    CHECK_EQ(extracted[1][2], 0x00);  // LPD8806 padding
}

TEST_CASE("QuadSPI: Mock driver dual-SPI mode") {
    MockQuadSPIDriver driver;
    QuadSPITransposer transposer;

    fl::vector<uint8_t> lane0 = {0xAA, 0xBB};
    fl::vector<uint8_t> lane1 = {0xCC, 0xDD};

    transposer.addLane(0, lane0, 0x00);
    transposer.addLane(1, lane1, 0x00);

    auto interleaved = transposer.transpose();
    driver.transmitDMA(interleaved.data(), interleaved.size());

    auto extracted = driver.extractLanes(4, 2);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][1], 0xDD);
    CHECK_EQ(extracted[2][0], 0x00);  // Unused lane
    CHECK_EQ(extracted[3][1], 0x00);  // Unused lane
}

TEST_CASE("QuadSPI: Mock driver round-trip validation") {
    MockQuadSPIDriver driver;
    QuadSPITransposer transposer;

    fl::vector<uint8_t> lane0_pattern;
    fl::vector<uint8_t> lane1_pattern;
    fl::vector<uint8_t> lane2_pattern;
    fl::vector<uint8_t> lane3_pattern;

    for (int i = 0; i < 10; ++i) {
        lane0_pattern.push_back(0x00 + i);
        lane1_pattern.push_back(0x10 + i);
        lane2_pattern.push_back(0x20 + i);
        lane3_pattern.push_back(0x30 + i);
    }

    transposer.addLane(0, lane0_pattern, 0xFF);
    transposer.addLane(1, lane1_pattern, 0xFF);
    transposer.addLane(2, lane2_pattern, 0xFF);
    transposer.addLane(3, lane3_pattern, 0xFF);

    auto interleaved = transposer.transpose();
    driver.transmitDMA(interleaved.data(), interleaved.size());

    auto extracted = driver.extractLanes(4, 10);
    for (int i = 0; i < 10; ++i) {
        CHECK_EQ(extracted[0][i], lane0_pattern[i]);
        CHECK_EQ(extracted[1][i], lane1_pattern[i]);
        CHECK_EQ(extracted[2][i], lane2_pattern[i]);
        CHECK_EQ(extracted[3][i], lane3_pattern[i]);
    }
}

TEST_CASE("QuadSPI: Mock driver extraction edge cases") {
    MockQuadSPIDriver driver;

    auto empty_extract = driver.extractLanes(4, 0);
    CHECK_EQ(empty_extract.size(), 4);
    for (const auto& lane : empty_extract) {
        CHECK_EQ(lane.size(), 0);
    }
}

TEST_CASE("QuadSPI: Mock driver state persistence") {
    MockQuadSPIDriver driver;
    driver.setClockSpeed(25000000);

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    driver.transmitDMA(data, sizeof(data));

    CHECK_EQ(driver.getClockSpeed(), 25000000);
    CHECK_EQ(driver.getTransmissionCount(), 1);
    CHECK(driver.isTransmissionActive());
}

TEST_CASE("QuadSPI: Mock driver buffer capture") {
    MockQuadSPIDriver driver;

    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x04, 0x05, 0x06, 0x07};

    driver.transmitDMA(data1, sizeof(data1));
    CHECK_EQ(driver.getTransmissionCount(), 1);

    driver.transmitDMA(data2, sizeof(data2));
    CHECK_EQ(driver.getTransmissionCount(), 2);
}

// ============================================================================
// SECTION 5: Controller Integration Tests (10 tests combined)
// ============================================================================

TEST_CASE("QuadSPI: Controller uses mock driver in test mode") {
    QuadSPIController<2, 40000000> controller;
    controller.begin();

    controller.addLane<APA102Controller<1, 2, RGB>>(0, 60);
    controller.addLane<APA102Controller<3, 4, RGB>>(1, 100);
    controller.addLane<APA102Controller<5, 6, RGB>>(2, 80);
    controller.addLane<APA102Controller<7, 8, RGB>>(3, 120);

    controller.finalize();

    CHECK_EQ(controller.getNumLanes(), 4);
    CHECK_EQ(controller.getMaxLaneBytes(), 500);

    auto* buf0 = controller.getLaneBuffer(0);
    CHECK(buf0 != nullptr);
    for (size_t i = 0; i < buf0->size(); ++i) (*buf0)[i] = 0xAA;

    controller.transmit();
    controller.waitComplete();

    auto& mockDriver = controller.getMockDriver();
    CHECK_EQ(mockDriver.getTransmissionCount(), 1);
    CHECK(mockDriver.isTransmissionActive());
}

TEST_CASE("QuadSPI: Controller calculates correct byte counts") {
    QuadSPIController<2, 10000000> controller;

    controller.addLane<APA102Controller<1, 2, RGB>>(0, 50);
    controller.addLane<LPD8806Controller<3, 4, RGB>>(1, 40);
    controller.addLane<WS2801Controller<5, 6, RGB>>(2, 60);
    controller.addLane<P9813Controller<7, 8, RGB>>(3, 30);

    controller.finalize();

    size_t expected_max = fl::fl_max(
        fl::fl_max(APA102Controller<1, 2, RGB>::calculateBytes(50),
                  LPD8806Controller<3, 4, RGB>::calculateBytes(40)),
        fl::fl_max(WS2801Controller<5, 6, RGB>::calculateBytes(60),
                  P9813Controller<7, 8, RGB>::calculateBytes(30))
    );

    CHECK_EQ(controller.getMaxLaneBytes(), expected_max);
}

TEST_CASE("QuadSPI: Controller preserves protocol-safe padding") {
    QuadSPIController<2, 20000000> controller;

    controller.addLane<APA102Controller<1, 2, RGB>>(0, 10);
    controller.addLane<WS2801Controller<3, 4, RGB>>(1, 20);
    controller.finalize();

    auto* buf0 = controller.getLaneBuffer(0);
    auto* buf1 = controller.getLaneBuffer(1);

    size_t apa102_actual = APA102Controller<1, 2, RGB>::calculateBytes(10);
    for (size_t i = 0; i < apa102_actual; ++i) (*buf0)[i] = 0x11;

    size_t ws2801_actual = WS2801Controller<3, 4, RGB>::calculateBytes(20);
    for (size_t i = 0; i < ws2801_actual; ++i) (*buf1)[i] = 0x22;

    controller.transmit();

    auto& mockDriver = controller.getMockDriver();
    auto extracted = mockDriver.extractLanes(4, controller.getMaxLaneBytes());

    size_t max_bytes = controller.getMaxLaneBytes();
    if (apa102_actual < max_bytes) {
        CHECK_EQ(extracted[0][apa102_actual], 0xFF);
    }
    if (ws2801_actual < max_bytes) {
        CHECK_EQ(extracted[1][ws2801_actual], 0x00);
    }
}

TEST_CASE("QuadSPI: Controller handles empty configuration") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();
    controller.finalize();

    CHECK_EQ(controller.getNumLanes(), 0);
    CHECK_EQ(controller.getMaxLaneBytes(), 0);

    controller.transmit();
    controller.waitComplete();
}

TEST_CASE("QuadSPI: Controller single lane operation") {
    QuadSPIController<2, 40000000> controller;
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 50);
    controller.finalize();

    CHECK_EQ(controller.getNumLanes(), 1);

    auto* buf = controller.getLaneBuffer(0);
    for (size_t i = 0; i < buf->size(); ++i) (*buf)[i] = 0xAB;

    controller.transmit();
    controller.waitComplete();

    auto& mockDriver = controller.getMockDriver();
    CHECK_EQ(mockDriver.getTransmissionCount(), 1);
}

TEST_CASE("QuadSPI: Integration - 4-lane APA102 different lengths") {
    const int num_leds_lane0 = 60;
    const int num_leds_lane1 = 100;
    const int num_leds_lane2 = 80;
    const int num_leds_lane3 = 120;

    auto lane0_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane0, 0xAA);
    auto lane1_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane1, 0xBB);
    auto lane2_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane2, 0xCC);
    auto lane3_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane3, 0xDD);

    uint8_t padding_byte = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(padding_byte, 0xFF);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0_data, padding_byte);
    transposer.addLane(1, lane1_data, padding_byte);
    transposer.addLane(2, lane2_data, padding_byte);
    transposer.addLane(3, lane3_data, padding_byte);

    auto interleaved = transposer.transpose();
    size_t max_lane_size = lane3_data.size();
    CHECK_EQ(interleaved.size(), max_lane_size * 4);

    MockQuadSPIDriver mock_driver;
    mock_driver.setClockSpeed(40000000);
    mock_driver.transmitDMA(interleaved.data(), interleaved.size());

    CHECK_EQ(mock_driver.getTransmissionCount(), 1);
    CHECK(mock_driver.isTransmissionActive());

    auto extracted = mock_driver.extractLanes(4, max_lane_size);
    for (size_t i = 0; i < lane0_data.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0_data[i]);
    }
}

TEST_CASE("QuadSPI: Integration - Mixed chipsets on different lanes") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(10, 0x11);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(15, 0x22);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(12, 0x33);
    auto lane3 = TestHelpers::generateP9813ProtocolData(8, 0x44);

    uint8_t padding0 = APA102Controller<1, 2, RGB>::getPaddingByte();
    uint8_t padding1 = LPD8806Controller<3, 4, RGB>::getPaddingByte();
    uint8_t padding2 = WS2801Controller<5, 6, RGB>::getPaddingByte();
    uint8_t padding3 = P9813Controller<7, 8, RGB>::getPaddingByte();

    CHECK_EQ(padding0, 0xFF);
    CHECK_EQ(padding1, 0x00);
    CHECK_EQ(padding2, 0x00);
    CHECK_EQ(padding3, 0x00);

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, padding0);
    transposer.addLane(1, lane1, padding1);
    transposer.addLane(2, lane2, padding2);
    transposer.addLane(3, lane3, padding3);

    auto interleaved = transposer.transpose();

    MockQuadSPIDriver mock;
    mock.transmitDMA(interleaved.data(), interleaved.size());
    auto extracted = mock.extractLanes(4, max_size);

    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0[i]);
    }

    if (lane0.size() < max_size) {
        CHECK_EQ(extracted[0][lane0.size()], 0xFF);
    }
}

TEST_CASE("QuadSPI: Integration - Performance estimation") {
    const int num_leds = 300;

    auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);

    auto interleaved = transposer.transpose();

    MockQuadSPIDriver mock;
    mock.setClockSpeed(40000000);
    uint64_t time_40mhz = mock.estimateTransmissionTimeMicros(interleaved.size());

    mock.setClockSpeed(20000000);
    uint64_t time_20mhz = mock.estimateTransmissionTimeMicros(interleaved.size());

    CHECK(time_40mhz < time_20mhz);
    CHECK_EQ(time_40mhz * 2, time_20mhz);
}

TEST_CASE("QuadSPI: Integration - Single lane degraded mode") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(50, 0xAB);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);

    auto interleaved = transposer.transpose();
    CHECK_EQ(interleaved.size(), lane0.size() * 4);

    MockQuadSPIDriver mock;
    mock.transmitDMA(interleaved.data(), interleaved.size());
    auto extracted = mock.extractLanes(4, lane0.size());

    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0[i]);
    }
}

TEST_CASE("QuadSPI: Integration - Dual-SPI mode (2 lanes)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(40, 0xC0);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(60, 0xC1);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);

    auto interleaved = transposer.transpose();

    MockQuadSPIDriver mock;
    mock.transmitDMA(interleaved.data(), interleaved.size());

    size_t max_size = lane1.size();
    auto extracted = mock.extractLanes(4, max_size);

    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0[i]);
    }
    for (size_t i = lane0.size(); i < max_size; ++i) {
        CHECK_EQ(extracted[0][i], 0xFF);
    }
}

// ============================================================================
// SECTION 6: Performance Tests (6 tests)
// ============================================================================

// Helper to measure execution time in microseconds
template<typename Func>
uint64_t measureMicroseconds(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

TEST_CASE("QuadSPI: Performance - Bit-interleaving speed") {
    const size_t led_counts[] = {10, 50, 100, 200, 300};

    for (size_t num_leds : led_counts) {
        auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
        auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
        auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
        auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

        QuadSPITransposer transposer;
        uint64_t transpose_time = measureMicroseconds([&]() {
            transposer.reset();
            transposer.addLane(0, lane0, 0xFF);
            transposer.addLane(1, lane1, 0xFF);
            transposer.addLane(2, lane2, 0xFF);
            transposer.addLane(3, lane3, 0xFF);
            auto result = transposer.transpose();
        });

        if (num_leds == 10) {
            CHECK(transpose_time < 500);  // Lenient for test system variance
        } else if (num_leds == 300) {
            CHECK(transpose_time < 5000);  // Lenient for test system variance
        }
    }
}

TEST_CASE("QuadSPI: Performance - Transmission time estimation") {
    const size_t num_leds = 100;
    const uint32_t clock_speeds[] = {10000000, 20000000, 40000000};

    auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane0, 0xFF);
    transposer.addLane(1, lane1, 0xFF);
    transposer.addLane(2, lane2, 0xFF);
    transposer.addLane(3, lane3, 0xFF);
    auto interleaved = transposer.transpose();

    for (uint32_t clock_speed : clock_speeds) {
        MockQuadSPIDriver driver;
        driver.setClockSpeed(clock_speed);

        uint64_t estimated_time = driver.estimateTransmissionTimeMicros(interleaved.size());

        if (clock_speed == 40000000) {
            CHECK(estimated_time < 500);
        }
    }
}

TEST_CASE("QuadSPI: Performance - Full controller workflow") {
    const size_t num_leds = 100;

    QuadSPIController<2, 40000000> controller;
    controller.begin();

    controller.addLane<APA102Controller<1, 2, RGB>>(0, num_leds);
    controller.addLane<APA102Controller<3, 4, RGB>>(1, num_leds);
    controller.addLane<APA102Controller<5, 6, RGB>>(2, num_leds);
    controller.addLane<APA102Controller<7, 8, RGB>>(3, num_leds);

    uint64_t finalize_time = measureMicroseconds([&]() {
        controller.finalize();
    });

    CHECK(finalize_time < 1000);

    for (uint8_t lane = 0; lane < 4; ++lane) {
        auto* buf = controller.getLaneBuffer(lane);
        for (size_t i = 0; i < buf->size(); ++i) {
            (*buf)[i] = 0xAA + lane;
        }
    }

    uint64_t transmit_time = measureMicroseconds([&]() {
        controller.transmit();
    });

    CHECK(transmit_time < 1000);

    auto& driver = controller.getMockDriver();
    CHECK_EQ(driver.getTransmissionCount(), 1);
}

TEST_CASE("QuadSPI: Performance - Theoretical speedup vs serial") {
    const size_t num_leds = 100;
    const uint32_t serial_speed = 6000000;
    const uint32_t parallel_speed = 40000000;

    size_t bytes_per_strip = APA102Controller<1, 2, RGB>::calculateBytes(num_leds);
    uint64_t serial_time_per_strip = (bytes_per_strip * 8 * 1000000ULL) / serial_speed;
    uint64_t total_serial_time = serial_time_per_strip * 4;

    QuadSPITransposer transposer;
    auto lane_data = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    transposer.addLane(0, lane_data, 0xFF);
    transposer.addLane(1, lane_data, 0xFF);
    transposer.addLane(2, lane_data, 0xFF);
    transposer.addLane(3, lane_data, 0xFF);
    auto interleaved = transposer.transpose();

    uint64_t parallel_time = (interleaved.size() * 8 * 1000000ULL) / parallel_speed;

    float speedup = static_cast<float>(total_serial_time) / static_cast<float>(parallel_time);

    CHECK(speedup > 5.0f);
    CHECK(speedup < 40.0f);
    CHECK(parallel_time < total_serial_time);
}

TEST_CASE("QuadSPI: Performance - Memory footprint") {
    const size_t num_leds = 150;

    QuadSPIController<2, 40000000> controller;
    controller.addLane<APA102Controller<1, 2, RGB>>(0, num_leds);
    controller.addLane<APA102Controller<3, 4, RGB>>(1, num_leds);
    controller.addLane<APA102Controller<5, 6, RGB>>(2, num_leds);
    controller.addLane<APA102Controller<7, 8, RGB>>(3, num_leds);

    controller.finalize();

    size_t bytes_per_lane = APA102Controller<1, 2, RGB>::calculateBytes(num_leds);
    size_t max_lane_bytes = controller.getMaxLaneBytes();

    size_t expected_memory = (max_lane_bytes * 4) + (max_lane_bytes * 4);

    CHECK_EQ(max_lane_bytes, bytes_per_lane);
    CHECK(expected_memory < 10000);

    for (uint8_t lane = 0; lane < 4; ++lane) {
        auto* buf = controller.getLaneBuffer(lane);
        CHECK_EQ(buf->size(), max_lane_bytes);
    }
}

TEST_CASE("QuadSPI: Performance - Cache-friendly sequential access") {
    const size_t num_leds = 50;
    auto lane_data = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);

    QuadSPITransposer transposer;
    transposer.addLane(0, lane_data, 0xFF);
    transposer.addLane(1, lane_data, 0xFF);
    transposer.addLane(2, lane_data, 0xFF);
    transposer.addLane(3, lane_data, 0xFF);

    // Warm up cache
    for (int i = 0; i < 10; ++i) {
        transposer.reset();
        transposer.addLane(0, lane_data, 0xFF);
        transposer.addLane(1, lane_data, 0xFF);
        transposer.addLane(2, lane_data, 0xFF);
        transposer.addLane(3, lane_data, 0xFF);
        auto result = transposer.transpose();
    }

    uint64_t final_time = measureMicroseconds([&]() {
        transposer.reset();
        transposer.addLane(0, lane_data, 0xFF);
        transposer.addLane(1, lane_data, 0xFF);
        transposer.addLane(2, lane_data, 0xFF);
        transposer.addLane(3, lane_data, 0xFF);
        auto result = transposer.transpose();
    });

    CHECK(final_time < 1000);  // Very lenient - just checking it's not pathologically slow
}
