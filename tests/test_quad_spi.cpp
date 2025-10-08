// Comprehensive QuadSPI tests for ESP32 parallel LED output
// Tests bit-interleaving, mock driver, controller integration, and performance

#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_transposer_quad.h"
#include "platforms/shared/spi_quad.h"
#include "platforms/stub/spi_quad_stub.h"
#include <chrono>

using namespace fl;

// ============================================================================
// Test Setup/Teardown - Reset mock drivers before/after each test
// ============================================================================

// Per-test fixture that resets mock drivers
struct SPIQuadTestFixture {
    SPIQuadTestFixture() {
        resetAllStubs();
    }

    // Don't reset in destructor - tests need to extract data after transmit
    // ~SPIQuadTestFixture() {
    //     resetAllStubs();
    // }

    static void resetAllStubs() {
        auto controllers = SPIQuad::getAll();
        for (int i = 0; i < controllers.size(); ++i) {
            SPIQuadStub* stub = toStub(controllers[i]);
            if (stub) {
                // Call end() to clear initialization state
                stub->end();
                // Then reset transmission state
                stub->reset();
                // Initialize with default config for tests that don't specify
                // Tests that need different settings should call end() + begin()
                SPIQuad::Config config;
                config.bus_num = i + 2;  // Bus 2, 3, etc.
                config.clock_speed_hz = 20000000;
                config.clock_pin = 14;
                config.data0_pin = 13;
                config.data1_pin = 12;
                config.data2_pin = 27;
                config.data3_pin = 33;
                stub->begin(config);
            }
        }
    }
};

// Global fixture that runs once at test suite start
struct SPIQuadGlobalFixture {
    SPIQuadGlobalFixture() {
        SPIQuadTestFixture::resetAllStubs();
    }
};

// Apply global fixture
static SPIQuadGlobalFixture g_fixture;

// ============================================================================
// Test-only QuadSPI controller (inlined from helpers/quad_spi_test_controller.h)
// ============================================================================

/// Test-only QuadSPI controller that uses SPIQuad interface
/// @tparam SPI_BUS_NUM SPI bus number (ignored in tests)
/// @tparam SPI_CLOCK_HZ Clock frequency in Hz
template<uint8_t SPI_BUS_NUM = 2, uint32_t SPI_CLOCK_HZ = 10000000>
class QuadSPITestController {
private:
    SPIQuad* mMockDriver;
    fl::vector<fl::vector<uint8_t>> mLaneBuffers;
    fl::vector<fl::vector<uint8_t>> mPaddingFrames;  // Changed from single bytes to frames
    fl::vector<uint8_t> mInterleavedDMABuffer;
    size_t mMaxLaneBytes = 0;
    uint8_t mNumLanes = 0;
    bool mFinalized = false;

public:
    QuadSPITestController() {
        // Get mock SPI controller from factory
        const auto& controllers = SPIQuad::getAll();
        if (!controllers.empty()) {
            mMockDriver = controllers[0];  // Use first available
        } else {
            mMockDriver = nullptr;
        }
    }

    void begin() {
        if (mMockDriver) {
            SPIQuad::Config config;
            config.bus_num = SPI_BUS_NUM;
            config.clock_speed_hz = SPI_CLOCK_HZ;
            config.clock_pin = 14;
            config.data0_pin = 13;
            config.data1_pin = 12;
            config.data2_pin = 27;
            config.data3_pin = 33;
            mMockDriver->begin(config);
        }
    }

    template<typename CONTROLLER_TYPE>
    void addLane(uint8_t lane_id, size_t num_leds) {
        if (lane_id >= 4) return;

        size_t bytes_needed = CONTROLLER_TYPE::calculateBytes(num_leds);
        auto padding_frame = CONTROLLER_TYPE::getPaddingLEDFrame();

        if (mLaneBuffers.size() <= lane_id) {
            mLaneBuffers.resize(lane_id + 1);
            mPaddingFrames.resize(lane_id + 1);
        }

        mLaneBuffers[lane_id].resize(bytes_needed);
        // Copy padding frame to vector
        mPaddingFrames[lane_id].clear();
        for (size_t i = 0; i < padding_frame.size(); ++i) {
            mPaddingFrames[lane_id].push_back(padding_frame[i]);
        }

        if (bytes_needed > mMaxLaneBytes) {
            mMaxLaneBytes = bytes_needed;
        }

        mNumLanes = fl::fl_max(mNumLanes, static_cast<uint8_t>(lane_id + 1));
    }

    void finalize() {
        if (mFinalized) {
            return;
        }

        // Check for empty lanes (like real controller)
        if (mLaneBuffers.empty()) {
            return;  // Don't finalize
        }

        // Check if all buffers are empty
        bool has_data = false;
        for (const auto& buf : mLaneBuffers) {
            if (!buf.empty()) {
                has_data = true;
                break;
            }
        }
        if (!has_data) {
            return;  // Don't finalize
        }

        // Check for DMA size limits (like real controller)
        constexpr size_t MAX_DMA_TRANSFER = 65536;
        size_t total_size = mMaxLaneBytes * 4;
        if (total_size > MAX_DMA_TRANSFER) {
            mMaxLaneBytes = MAX_DMA_TRANSFER / 4;
        }

        // Pad all lanes to max size
        for (size_t i = 0; i < mLaneBuffers.size(); ++i) {
            if (mLaneBuffers[i].size() < mMaxLaneBytes) {
                // Padding is now handled by transposer during transpose(), not by buffer resize
                // Just ensure buffer is properly sized
                size_t old_size = mLaneBuffers[i].size();
                // Don't resize - transposer will handle padding with full LED frames
                (void)old_size;  // Suppress unused warning
            }
        }

        mInterleavedDMABuffer.resize(mMaxLaneBytes * 4);
        mFinalized = true;
    }

    fl::vector<uint8_t>* getLaneBuffer(uint8_t lane_id) {
        if (lane_id >= mLaneBuffers.size()) return nullptr;
        return &mLaneBuffers[lane_id];
    }

    void transmit() {
        if (!mMockDriver) return;

        // Setup lanes using optional
        fl::optional<SPITransposerQuad::LaneData> lanes[4];
        for (size_t i = 0; i < mLaneBuffers.size() && i < 4; ++i) {
            if (!mLaneBuffers[i].empty()) {
                lanes[i] = SPITransposerQuad::LaneData{
                    fl::span<const uint8_t>(mLaneBuffers[i].data(), mLaneBuffers[i].size()),
                    fl::span<const uint8_t>(mPaddingFrames[i].data(), mPaddingFrames[i].size())
                };
            }
        }

        // Allocate output buffer
        mInterleavedDMABuffer.resize(mMaxLaneBytes * 4);

        // Perform transpose
        const char* error = nullptr;
        bool success = SPITransposerQuad::transpose(lanes, mMaxLaneBytes,
            fl::span<uint8_t>(mInterleavedDMABuffer.data(), mInterleavedDMABuffer.size()), &error);

        if (success && !mInterleavedDMABuffer.empty()) {
            mMockDriver->transmitAsync(fl::span<const uint8_t>(mInterleavedDMABuffer.data(), mInterleavedDMABuffer.size()));
        }
    }

    void waitComplete() {
        if (mMockDriver) {
            mMockDriver->waitComplete();
        }
    }

    uint8_t getNumLanes() const { return mNumLanes; }
    size_t getMaxLaneBytes() const { return mMaxLaneBytes; }

    bool isFinalized() const {
        return mFinalized;
    }

    SPIQuad* getMockDriver() { return mMockDriver; }
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

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102 padding byte") {
    uint8_t padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0xFF);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: LPD8806 padding byte") {
    uint8_t padding = LPD8806Controller<3, 4, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: WS2801 padding byte") {
    uint8_t padding = WS2801Controller<5, 6, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: P9813 padding byte") {
    uint8_t padding = P9813Controller<7, 8, RGB>::getPaddingByte();
    CHECK_EQ(padding, 0x00);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: SK9822 inherits APA102 padding") {
    uint8_t sk9822_padding = SK9822Controller<1, 2, RGB>::getPaddingByte();
    uint8_t apa102_padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(sk9822_padding, apa102_padding);
    CHECK_EQ(sk9822_padding, 0xFF);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: HD107 inherits APA102 padding") {
    uint8_t hd107_padding = HD107Controller<1, 2, RGB>::getPaddingByte();
    uint8_t apa102_padding = APA102Controller<1, 2, RGB>::getPaddingByte();
    CHECK_EQ(hd107_padding, apa102_padding);
    CHECK_EQ(hd107_padding, 0xFF);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mixed chipsets have correct padding") {
    CHECK_EQ(APA102Controller<1, 2, RGB>::getPaddingByte(), 0xFF);
    CHECK_EQ(LPD8806Controller<3, 4, RGB>::getPaddingByte(), 0x00);
    CHECK_EQ(WS2801Controller<5, 6, RGB>::getPaddingByte(), 0x00);
    CHECK_EQ(P9813Controller<7, 8, RGB>::getPaddingByte(), 0x00);
}

// ============================================================================
// SECTION 2: calculateBytes() Tests (7 tests)
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: calculateBytes() is constexpr") {
    constexpr size_t apa102_size = APA102Controller<1, 2, RGB>::calculateBytes(100);
    constexpr size_t lpd8806_size = LPD8806Controller<3, 4, RGB>::calculateBytes(100);
    constexpr size_t ws2801_size = WS2801Controller<5, 6, RGB>::calculateBytes(100);
    constexpr size_t p9813_size = P9813Controller<7, 8, RGB>::calculateBytes(100);

    CHECK(apa102_size > 0);
    CHECK(lpd8806_size > 0);
    CHECK(ws2801_size > 0);
    CHECK(p9813_size > 0);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102 calculateBytes") {
    // APA102: 4 + (num_leds * 4) + (4 * ((num_leds / 32) + 1))
    size_t bytes_10 = APA102Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = APA102Controller<1, 2, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 48);   // 4 + 40 + 4 = 48
    CHECK_EQ(bytes_100, 420); // 4 + 400 + 16 = 420
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: LPD8806 calculateBytes") {
    // LPD8806: (num_leds * 3) + ((num_leds * 3 + 63) / 64)
    size_t bytes_10 = LPD8806Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = LPD8806Controller<3, 4, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 31);  // 30 + 1 = 31
    CHECK_EQ(bytes_100, 305); // 300 + 5 = 305
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: WS2801 calculateBytes") {
    // WS2801: num_leds * 3 (no overhead)
    size_t bytes_10 = WS2801Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = WS2801Controller<5, 6, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 30);
    CHECK_EQ(bytes_100, 300);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: P9813 calculateBytes") {
    // P9813: 4 + (num_leds * 4) + 4
    size_t bytes_10 = P9813Controller<1, 2, RGB>::calculateBytes(10);
    size_t bytes_100 = P9813Controller<7, 8, RGB>::calculateBytes(100);

    CHECK_EQ(bytes_10, 48);   // 4 + 40 + 4 = 48
    CHECK_EQ(bytes_100, 408); // 4 + 400 + 4 = 408
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: calculateBytes() for mixed chipset scenario") {
    // Simulate SpecialDrivers/ESP/QuadSPI/MultiChipset example:
    size_t lane0 = APA102Controller<1, 2, RGB>::calculateBytes(60);
    size_t lane1 = LPD8806Controller<3, 4, RGB>::calculateBytes(40);
    size_t lane2 = WS2801Controller<5, 6, RGB>::calculateBytes(80);
    size_t lane3 = APA102Controller<7, 8, RGB>::calculateBytes(100);

    size_t max_bytes = fl::fl_max(fl::fl_max(lane0, lane1), fl::fl_max(lane2, lane3));
    CHECK_EQ(max_bytes, 420);  // Lane 3 (APA102 100 LEDs) is largest
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: calculateBytes() edge case - zero LEDs") {
    CHECK_EQ(APA102Controller<1, 2, RGB>::calculateBytes(0), 8);
    CHECK_EQ(LPD8806Controller<3, 4, RGB>::calculateBytes(0), 0);
    CHECK_EQ(WS2801Controller<5, 6, RGB>::calculateBytes(0), 0);
    CHECK_EQ(P9813Controller<7, 8, RGB>::calculateBytes(0), 8);
}

// ============================================================================
// SECTION 3: Bit-Interleaving Transpose Tests (13 tests)
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Basic 4-lane transpose with variable LED counts") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(1, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(2, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(7, 0xDD);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    const char* error = nullptr;
    bool success = SPITransposerQuad::transpose(lanes, max_size, interleaved, &error);

    CHECK(success);
    CHECK_EQ(interleaved.size(), max_size * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Transpose with equal-length lanes") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(5, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(5, 0x22);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(5, 0x33);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(5, 0x44);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane0.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane0.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Single-lane transpose (degraded mode)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(3, 0xAB);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};

    size_t max_size = lane0.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane0.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Dual-lane transpose") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(4, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(6, 0x22);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};

    size_t max_size = lane1.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane1.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mixed chipset transpose with different padding") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(3, 0xAA);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(5, 0xBB);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateP9813ProtocolData(6, 0xDD);

    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00}};
    lanes[1] = SPITransposerQuad::LaneData{lane1, fl::vector<uint8_t>{0x00}};
    lanes[2] = SPITransposerQuad::LaneData{lane2, fl::vector<uint8_t>{0x00}};
    lanes[3] = SPITransposerQuad::LaneData{lane3, fl::vector<uint8_t>{0x00}};

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    CHECK_EQ(interleaved.size(), max_size * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102-only transpose (all 0xFF padding)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(2, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(3, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(4, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(5, 0xDD);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: LPD8806-only transpose (all 0x00 padding)") {
    auto lane0 = TestHelpers::generateLPD8806ProtocolData(3, 0x11);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(4, 0x22);
    auto lane2 = TestHelpers::generateLPD8806ProtocolData(5, 0x33);
    auto lane3 = TestHelpers::generateLPD8806ProtocolData(6, 0x44);

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: WS2801-only transpose") {
    auto lane0 = TestHelpers::generateWS2801ProtocolData(5, 0xAA);
    auto lane1 = TestHelpers::generateWS2801ProtocolData(7, 0xBB);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(6, 0xCC);
    auto lane3 = TestHelpers::generateWS2801ProtocolData(8, 0xDD);

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane3.size() * 4);  // lane3 is longest (8 LEDs)
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: P9813-only transpose") {
    auto lane0 = TestHelpers::generateP9813ProtocolData(4, 0x55);
    auto lane1 = TestHelpers::generateP9813ProtocolData(5, 0x66);
    auto lane2 = TestHelpers::generateP9813ProtocolData(6, 0x77);
    auto lane3 = TestHelpers::generateP9813ProtocolData(7, 0x88);

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Empty transpose") {
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    fl::vector<uint8_t> interleaved(0);
    SPITransposerQuad::transpose(lanes, 0, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), 0);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Reset and reuse transposer") {
    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};

    // First use
    auto lane0_a = TestHelpers::generateAPA102ProtocolData(5, 0xAA);
    fl::optional<SPITransposerQuad::LaneData> lanes_a[4];
    lanes_a[0] = SPITransposerQuad::LaneData{lane0_a, padding};
    fl::vector<uint8_t> result_a(lane0_a.size() * 4);
    SPITransposerQuad::transpose(lanes_a, lane0_a.size(), result_a, nullptr);
    CHECK_EQ(result_a.size(), lane0_a.size() * 4);

    // Reuse (stateless API doesn't need reset)
    auto lane0_b = TestHelpers::generateAPA102ProtocolData(10, 0xBB);
    fl::optional<SPITransposerQuad::LaneData> lanes_b[4];
    lanes_b[0] = SPITransposerQuad::LaneData{lane0_b, padding};
    fl::vector<uint8_t> result_b(lane0_b.size() * 4);
    SPITransposerQuad::transpose(lanes_b, lane0_b.size(), result_b, nullptr);
    CHECK_EQ(result_b.size(), lane0_b.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Large buffer transpose (stress test)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(150, 0x11);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(200, 0x22);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(175, 0x33);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(250, 0x44);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = lane3.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane3.size() * 4);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Interleave order validation") {
    fl::vector<uint8_t> lane0 = {0xAA};
    fl::vector<uint8_t> lane1 = {0xBB};
    fl::vector<uint8_t> lane2 = {0xCC};
    fl::vector<uint8_t> lane3 = {0xDD};

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    fl::vector<uint8_t> interleaved(1 * 4);
    SPITransposerQuad::transpose(lanes, 1, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), 4);

    // Verify round-trip extraction works correctly
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = driver->extractLanes(4, 1);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][0], 0xBB);
    CHECK_EQ(extracted[2][0], 0xCC);
    CHECK_EQ(extracted[3][0], 0xDD);
}

// ============================================================================
// SECTION 4: Mock Driver Tests (13 tests)
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver initialization") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    CHECK_EQ(driver->getClockSpeed(), 20000000);  // Default clock speed
    CHECK_EQ(driver->getTransmissionCount(), 0);
    CHECK(!driver->isTransmissionActive());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver clock speed configuration") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 10000000;
    driver->end();  // End first to allow re-initialization
    driver->begin(config);
    CHECK_EQ(driver->getClockSpeed(), 10000000);

    config.clock_speed_hz = 40000000;
    driver->end();  // End before changing clock speed
    driver->begin(config);
    CHECK_EQ(driver->getClockSpeed(), 40000000);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver DMA transmission tracking") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->reset();
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 40000000;
    driver->begin(config);

    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    driver->transmitAsync(fl::span<const uint8_t>(test_data, sizeof(test_data)));

    CHECK_EQ(driver->getTransmissionCount(), 1);
    CHECK(driver->isTransmissionActive());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver transmission reset") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 10000000;
    driver->begin(config);

    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    driver->transmitAsync(fl::span<const uint8_t>(test_data, sizeof(test_data)));
    CHECK_EQ(driver->getTransmissionCount(), 1);

    driver->reset();
    CHECK_EQ(driver->getTransmissionCount(), 0);
    CHECK(!driver->isTransmissionActive());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver multiple transmissions") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 40000000;
    driver->begin(config);

    uint8_t data1[] = {0x11, 0x22};
    driver->transmitAsync(fl::span<const uint8_t>(data1, sizeof(data1)));
    CHECK_EQ(driver->getTransmissionCount(), 1);

    uint8_t data2[] = {0x33, 0x44, 0x55};
    driver->transmitAsync(fl::span<const uint8_t>(data2, sizeof(data2)));
    CHECK_EQ(driver->getTransmissionCount(), 2);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver lane extraction basic") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

    fl::vector<uint8_t> lane0_data = {0xAA};
    fl::vector<uint8_t> lane1_data = {0xBB};
    fl::vector<uint8_t> lane2_data = {0xCC};
    fl::vector<uint8_t> lane3_data = {0xDD};

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0_data, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1_data, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2_data, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3_data, padding};

    fl::vector<uint8_t> interleaved(1 * 4);
    SPITransposerQuad::transpose(lanes, 1, interleaved, nullptr);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = driver->extractLanes(4, 1);
    CHECK_EQ(extracted.size(), 4);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][0], 0xBB);
    CHECK_EQ(extracted[2][0], 0xCC);
    CHECK_EQ(extracted[3][0], 0xDD);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver lane extraction multi-byte") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

    fl::vector<uint8_t> lane0 = {0x11, 0x22, 0x33};
    fl::vector<uint8_t> lane1 = {0x44, 0x55, 0x66};
    fl::vector<uint8_t> lane2 = {0x77, 0x88, 0x99};
    fl::vector<uint8_t> lane3 = {0xAA, 0xBB, 0xCC};

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    fl::vector<uint8_t> interleaved(3 * 4);
    SPITransposerQuad::transpose(lanes, 3, interleaved, nullptr);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = driver->extractLanes(4, 3);
    CHECK_EQ(extracted[0][0], 0x11);
    CHECK_EQ(extracted[0][2], 0x33);
    CHECK_EQ(extracted[3][1], 0xBB);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver lane extraction with padding") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

    fl::vector<uint8_t> lane0 = {0xAA};
    fl::vector<uint8_t> lane1 = {0xBB, 0xCC};
    fl::vector<uint8_t> lane2 = {0xDD, 0xEE, 0xFF};
    fl::vector<uint8_t> lane3 = {0x11, 0x22};

    fl::vector<uint8_t> padding0 = {0xE0, 0x00, 0x00, 0x00};
    fl::vector<uint8_t> padding1 = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding0};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding1};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding1};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding1};

    size_t max_size = 3;
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = driver->extractLanes(4, 3);
    // Padding now goes at BEGINNING using repeating LED frame pattern
    // Lane 0: max=3, data=1, padding=2 bytes from {0xE0,0x00,0x00,0x00}
    CHECK_EQ(extracted[0][0], 0xE0);  // APA102 padding frame[0]
    CHECK_EQ(extracted[0][1], 0x00);  // APA102 padding frame[1]
    CHECK_EQ(extracted[0][2], 0xAA);  // Original data at end
    // Lane 1: max=3, data=2, padding=1 byte from {0x00}
    CHECK_EQ(extracted[1][0], 0x00);  // Single-byte padding
    CHECK_EQ(extracted[1][1], 0xBB);  // Original data
    CHECK_EQ(extracted[1][2], 0xCC);  // Original data
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver dual-SPI mode") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    fl::vector<uint8_t> lane0 = {0xAA, 0xBB};
    fl::vector<uint8_t> lane1 = {0xCC, 0xDD};

    fl::vector<uint8_t> padding = {0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};

    size_t max_size = fl::fl_max(lane0.size(), lane1.size());
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = driver->extractLanes(4, 2);
    CHECK_EQ(extracted[0][0], 0xAA);
    CHECK_EQ(extracted[1][1], 0xDD);
    CHECK_EQ(extracted[2][0], 0x00);  // Unused lane
    CHECK_EQ(extracted[3][1], 0x00);  // Unused lane
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver round-trip validation") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

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

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0_pattern, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1_pattern, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2_pattern, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3_pattern, padding};

    size_t max_size = 10;
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = driver->extractLanes(4, 10);
    for (int i = 0; i < 10; ++i) {
        CHECK_EQ(extracted[0][i], lane0_pattern[i]);
        CHECK_EQ(extracted[1][i], lane1_pattern[i]);
        CHECK_EQ(extracted[2][i], lane2_pattern[i]);
        CHECK_EQ(extracted[3][i], lane3_pattern[i]);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver extraction edge cases") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

    auto empty_extract = driver->extractLanes(4, 0);
    CHECK_EQ(empty_extract.size(), 4);
    for (const auto& lane : empty_extract) {
        CHECK_EQ(lane.size(), 0);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver state persistence") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 25000000;
    driver->end();  // End first to allow re-initialization with new clock speed
    driver->begin(config);

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    driver->transmitAsync(fl::span<const uint8_t>(data, sizeof(data)));

    CHECK_EQ(driver->getClockSpeed(), 25000000);
    CHECK_EQ(driver->getTransmissionCount(), 1);
    CHECK(driver->isTransmissionActive());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Mock driver buffer capture") {
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);

    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x04, 0x05, 0x06, 0x07};

    driver->transmitAsync(fl::span<const uint8_t>(data1, sizeof(data1)));
    CHECK_EQ(driver->getTransmissionCount(), 1);

    driver->transmitAsync(fl::span<const uint8_t>(data2, sizeof(data2)));
    CHECK_EQ(driver->getTransmissionCount(), 2);
}

// ============================================================================
// SECTION 5: Controller Integration Tests (10 tests combined)
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Controller uses mock driver in test mode") {
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
    // Don't call waitComplete() - it clears the transmission active flag

    SPIQuadStub* mockDriver = toStub(controller.getMockDriver());
    CHECK_EQ(mockDriver->getTransmissionCount(), 1);
    CHECK(mockDriver->isTransmissionActive());

    // Clean up by completing the transmission
    controller.waitComplete();
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Controller calculates correct byte counts") {
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

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Controller preserves protocol-safe padding") {
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

    SPIQuadStub* mockDriver = toStub(controller.getMockDriver());
    auto extracted = mockDriver->extractLanes(4, controller.getMaxLaneBytes());

    size_t max_bytes = controller.getMaxLaneBytes();
    // Padding now goes at BEGINNING using repeating LED frame pattern
    // APA102 frame: {0xE0, 0x00, 0x00, 0x00}, WS2801 frame: {0x00, 0x00, 0x00}
    if (apa102_actual < max_bytes) {
        // Padding is at the BEGINNING, actual data comes after
        size_t padding_bytes = max_bytes - apa102_actual;
        CHECK_EQ(extracted[0][0], 0xE0);  // First padding byte is frame[0]
        // Actual data starts after padding
        CHECK_EQ(extracted[0][padding_bytes], 0x11);  // First actual data byte
    }
    if (ws2801_actual < max_bytes) {
        // WS2801 padding is at beginning
        size_t padding_bytes = max_bytes - ws2801_actual;
        if (padding_bytes > 0) {
            CHECK_EQ(extracted[1][0], 0x00);  // WS2801 frame[0]
        }
        // Actual data starts after padding
        CHECK_EQ(extracted[1][padding_bytes], 0x22);  // First actual data byte
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Controller handles empty configuration") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();
    controller.finalize();

    CHECK_EQ(controller.getNumLanes(), 0);
    CHECK_EQ(controller.getMaxLaneBytes(), 0);

    controller.transmit();
    controller.waitComplete();
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Controller single lane operation") {
    QuadSPIController<2, 40000000> controller;
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 50);
    controller.finalize();

    CHECK_EQ(controller.getNumLanes(), 1);

    auto* buf = controller.getLaneBuffer(0);
    for (size_t i = 0; i < buf->size(); ++i) (*buf)[i] = 0xAB;

    controller.transmit();
    controller.waitComplete();

    SPIQuadStub* mockDriver = toStub(controller.getMockDriver());
    CHECK_EQ(mockDriver->getTransmissionCount(), 1);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Integration - 4-lane APA102 different lengths") {
    const int num_leds_lane0 = 60;
    const int num_leds_lane1 = 100;
    const int num_leds_lane2 = 80;
    const int num_leds_lane3 = 120;

    auto lane0_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane0, 0xAA);
    auto lane1_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane1, 0xBB);
    auto lane2_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane2, 0xCC);
    auto lane3_data = TestHelpers::generateAPA102ProtocolData(num_leds_lane3, 0xDD);

    auto padding_frame = APA102Controller<1, 2, RGB>::getPaddingLEDFrame();
    CHECK_EQ(padding_frame.size(), 4);

    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0_data, padding_frame};
    lanes[1] = SPITransposerQuad::LaneData{lane1_data, padding_frame};
    lanes[2] = SPITransposerQuad::LaneData{lane2_data, padding_frame};
    lanes[3] = SPITransposerQuad::LaneData{lane3_data, padding_frame};

    size_t max_size = fl::fl_max(fl::fl_max(lane0_data.size(), lane1_data.size()),
                                 fl::fl_max(lane2_data.size(), lane3_data.size()));
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    size_t max_lane_size = lane3_data.size();
    CHECK_EQ(interleaved.size(), max_lane_size * 4);

    SPIQuadStub* mock_driver = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 40000000;
    mock_driver->begin(config);
    mock_driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    CHECK_EQ(mock_driver->getTransmissionCount(), 1);
    CHECK(mock_driver->isTransmissionActive());

    auto extracted = mock_driver->extractLanes(4, max_lane_size);

    // Padding now goes at BEGINNING, so lanes have: [padding...][original data]
    size_t padding0 = max_lane_size - lane0_data.size();
    for (size_t i = 0; i < lane0_data.size(); ++i) {
        CHECK_EQ(extracted[0][padding0 + i], lane0_data[i]);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Integration - Mixed chipsets on different lanes") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(10, 0x11);
    auto lane1 = TestHelpers::generateLPD8806ProtocolData(15, 0x22);
    auto lane2 = TestHelpers::generateWS2801ProtocolData(12, 0x33);
    auto lane3 = TestHelpers::generateP9813ProtocolData(8, 0x44);

    auto padding0 = APA102Controller<1, 2, RGB>::getPaddingLEDFrame();
    auto padding1 = LPD8806Controller<3, 4, RGB>::getPaddingLEDFrame();
    auto padding2 = WS2801Controller<5, 6, RGB>::getPaddingLEDFrame();
    auto padding3 = P9813Controller<7, 8, RGB>::getPaddingLEDFrame();

    CHECK_EQ(padding0.size(), 4);
    CHECK_EQ(padding1.size(), 3);
    CHECK_EQ(padding2.size(), 3);
    CHECK_EQ(padding3.size(), 4);

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));

    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding0};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding1};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding2};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding3};
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    SPIQuadStub* mock = toStub(SPIQuad::getAll()[0]);
    mock->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = mock->extractLanes(4, max_size);

    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0[i]);
    }

    if (lane0.size() < max_size) {
        CHECK_EQ(extracted[0][lane0.size()], 0xFF);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Integration - Transmission verification") {
    const int num_leds = 300;

    auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    SPIQuadStub* mock = toStub(SPIQuad::getAll()[0]);
    SPIQuad::Config config;
    config.bus_num = 2;
    config.clock_speed_hz = 40000000;
    mock->begin(config);

    mock->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    CHECK_EQ(mock->getTransmissionCount(), 1);
    CHECK(mock->isTransmissionActive());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Integration - Single lane degraded mode") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(50, 0xAB);

    fl::vector<uint8_t> padding = {0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};

    size_t max_size = lane0.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);
    CHECK_EQ(interleaved.size(), lane0.size() * 4);

    SPIQuadStub* mock = toStub(SPIQuad::getAll()[0]);
    mock->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = mock->extractLanes(4, lane0.size());

    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][i], lane0[i]);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Integration - Dual-SPI mode (2 lanes)") {
    auto lane0 = TestHelpers::generateAPA102ProtocolData(40, 0xC0);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(60, 0xC1);

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};

    size_t max_size = fl::fl_max(lane0.size(), lane1.size());
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    SPIQuadStub* mock = toStub(SPIQuad::getAll()[0]);
    mock->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));

    auto extracted = mock->extractLanes(4, max_size);

    // Padding now goes at BEGINNING using repeating APA102 frame {0xE0, 0x00, 0x00, 0x00}
    size_t padding0 = max_size - lane0.size();

    // Check padding at beginning of lane 0 - repeating 4-byte APA102 frame
    const uint8_t apa102_frame[] = {0xE0, 0x00, 0x00, 0x00};
    for (size_t i = 0; i < padding0; ++i) {
        CHECK_EQ(extracted[0][i], apa102_frame[i % 4]);
    }

    // Check original data at end of lane 0
    for (size_t i = 0; i < lane0.size(); ++i) {
        CHECK_EQ(extracted[0][padding0 + i], lane0[i]);
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

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Bit-interleaving speed") {
    const size_t led_counts[] = {10, 50, 100, 200, 300};

    for (size_t num_leds : led_counts) {
        auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
        auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
        auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
        auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

                fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
        uint64_t transpose_time = measureMicroseconds([&]() {
            fl::optional<SPITransposerQuad::LaneData> lanes[4];
            lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
            lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
            lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
            lanes[3] = SPITransposerQuad::LaneData{lane3, padding};
            size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                         fl::fl_max(lane2.size(), lane3.size()));
            fl::vector<uint8_t> result(max_size * 4);
            SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
        });

        if (num_leds == 10) {
            CHECK(transpose_time < 500);  // Lenient for test system variance
        } else if (num_leds == 300) {
            CHECK(transpose_time < 5000);  // Lenient for test system variance
        }
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Different clock speeds") {
    const size_t num_leds = 100;
    const uint32_t clock_speeds[] = {10000000, 20000000, 40000000};

    auto lane0 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    auto lane1 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xBB);
    auto lane2 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xCC);
    auto lane3 = TestHelpers::generateAPA102ProtocolData(num_leds, 0xDD);

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    for (uint32_t clock_speed : clock_speeds) {
        SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
        SPIQuad::Config config;
        config.bus_num = 2;
        config.clock_speed_hz = clock_speed;
        driver->end();  // End before each iteration to allow re-initialization
        driver->begin(config);

        driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
        CHECK_EQ(driver->getClockSpeed(), clock_speed);
        CHECK(driver->isTransmissionActive());
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Full controller workflow") {
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

    SPIQuadStub* driver = toStub(controller.getMockDriver());
    CHECK_EQ(driver->getTransmissionCount(), 1);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Theoretical speedup vs serial") {
    const size_t num_leds = 100;
    const uint32_t serial_speed = 6000000;
    const uint32_t parallel_speed = 40000000;

    size_t bytes_per_strip = APA102Controller<1, 2, RGB>::calculateBytes(num_leds);
    uint64_t serial_time_per_strip = (bytes_per_strip * 8 * 1000000ULL) / serial_speed;
    uint64_t total_serial_time = serial_time_per_strip * 4;

    auto lane_data = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);
    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane_data, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane_data, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane_data, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane_data, padding};

    size_t max_size = lane_data.size();
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    uint64_t parallel_time = (interleaved.size() * 8 * 1000000ULL) / parallel_speed;

    float speedup = static_cast<float>(total_serial_time) / static_cast<float>(parallel_time);

    CHECK(speedup > 5.0f);
    CHECK(speedup < 40.0f);
    CHECK(parallel_time < total_serial_time);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Memory footprint") {
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

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Performance - Cache-friendly sequential access") {
    const size_t num_leds = 50;
    auto lane_data = TestHelpers::generateAPA102ProtocolData(num_leds, 0xAA);

    // Warm up cache
    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00};
    for (int i = 0; i < 10; ++i) {
        fl::optional<SPITransposerQuad::LaneData> lanes[4];
        lanes[0] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[1] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[2] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[3] = SPITransposerQuad::LaneData{lane_data, padding};
        size_t max_size = lane_data.size();
        fl::vector<uint8_t> result(max_size * 4);
        SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
    }

    uint64_t final_time = measureMicroseconds([&]() {
        fl::optional<SPITransposerQuad::LaneData> lanes[4];
        lanes[0] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[1] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[2] = SPITransposerQuad::LaneData{lane_data, padding};
        lanes[3] = SPITransposerQuad::LaneData{lane_data, padding};
        size_t max_size = lane_data.size();
        fl::vector<uint8_t> result(max_size * 4);
        SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
    });

    CHECK(final_time < 1000);  // Very lenient - just checking it's not pathologically slow
}

// ============================================================================
// Optimized bit-spreading algorithm tests
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - exact bit positions") {
    // This test verifies the EXACT bit positions in the output
    // Uses distinct bit patterns for each lane to verify correct interleaving
    // Use bit patterns where only specific 2-bit pairs are set
    fl::vector<uint8_t> lane0 = {0xC0};  // 11000000 (bits 7:6 = 11, rest 00)
    fl::vector<uint8_t> lane1 = {0x30};  // 00110000 (bits 5:4 = 11, rest 00)
    fl::vector<uint8_t> lane2 = {0x0C};  // 00001100 (bits 3:2 = 11, rest 00)
    fl::vector<uint8_t> lane3 = {0x03};  // 00000011 (bits 1:0 = 11, rest 00)

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = 1;
    fl::vector<uint8_t> result(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
    CHECK_EQ(result.size(), 4);

    // Verify exact bit positions in output
    // Each output byte format: [d1 d0 c1 c0 b1 b0 a1 a0]
    // where a=lane0, b=lane1, c=lane2, d=lane3

    // Output[0]: bits 7:6 from each lane
    //   lane3[7:6]=00, lane2[7:6]=00, lane1[7:6]=00, lane0[7:6]=11
    //   Expected: 0b00_00_00_11 = 0x03
    CHECK_EQ(result[0], 0x03);

    // Output[1]: bits 5:4 from each lane
    //   lane3[5:4]=00, lane2[5:4]=00, lane1[5:4]=11, lane0[5:4]=00
    //   Expected: 0b00_00_11_00 = 0x0C
    CHECK_EQ(result[1], 0x0C);

    // Output[2]: bits 3:2 from each lane
    //   lane3[3:2]=00, lane2[3:2]=11, lane1[3:2]=00, lane0[3:2]=00
    //   Expected: 0b00_11_00_00 = 0x30
    CHECK_EQ(result[2], 0x30);

    // Output[3]: bits 1:0 from each lane
    //   lane3[1:0]=11, lane2[1:0]=00, lane1[1:0]=00, lane0[1:0]=00
    //   Expected: 0b11_00_00_00 = 0xC0
    CHECK_EQ(result[3], 0xC0);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - known patterns") {
    // Test 1: All 0xAA (10101010)
    fl::vector<uint8_t> lane_aa = {0xAA};
    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane_aa, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane_aa, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane_aa, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane_aa, padding};

    size_t max_size = lane_aa.size();
    fl::vector<uint8_t> result_aa(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result_aa, nullptr);
    CHECK_EQ(result_aa.size(), 4);
    // All lanes identical should produce specific pattern
    CHECK_EQ(result_aa[0], result_aa[1]);
    CHECK_EQ(result_aa[1], result_aa[2]);
    CHECK_EQ(result_aa[2], result_aa[3]);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - alternating lanes") {
    // Test 2: Alternating 0xFF and 0x00
    fl::vector<uint8_t> lane_ff = {0xFF};
    fl::vector<uint8_t> lane_00 = {0x00};
    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane_ff, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane_00, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane_ff, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane_00, padding};

    size_t max_size = 1;
    fl::vector<uint8_t> result_alt(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result_alt, nullptr);
    CHECK_EQ(result_alt.size(), 4);

    // Verify lanes can be extracted
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(result_alt.data(), result_alt.size()));
    auto extracted = driver->extractLanes(4, 1);
    CHECK_EQ(extracted[0][0], 0xFF);
    CHECK_EQ(extracted[1][0], 0x00);
    CHECK_EQ(extracted[2][0], 0xFF);
    CHECK_EQ(extracted[3][0], 0x00);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - correctness check") {
    // Test known bit patterns and verify output
    fl::vector<uint8_t> lane0 = {0x12};  // 00010010
    fl::vector<uint8_t> lane1 = {0x34};  // 00110100
    fl::vector<uint8_t> lane2 = {0x56};  // 01010110
    fl::vector<uint8_t> lane3 = {0x78};  // 01111000

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};

    size_t max_size = 1;
    fl::vector<uint8_t> result(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
    CHECK_EQ(result.size(), 4);

    // Verify by extracting lanes back
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(result.data(), result.size()));
    auto extracted = driver->extractLanes(4, 1);
    CHECK_EQ(extracted[0][0], 0x12);
    CHECK_EQ(extracted[1][0], 0x34);
    CHECK_EQ(extracted[2][0], 0x56);
    CHECK_EQ(extracted[3][0], 0x78);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - large buffer") {
    // Test with larger buffer to verify performance improvement
    const size_t NUM_BYTES = 1000;  // Simulate 100 LEDs * 3 bytes + overhead
    fl::vector<uint8_t> large_buffer(NUM_BYTES, 0xAA);

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{large_buffer, padding};
    lanes[1] = SPITransposerQuad::LaneData{large_buffer, padding};
    lanes[2] = SPITransposerQuad::LaneData{large_buffer, padding};
    lanes[3] = SPITransposerQuad::LaneData{large_buffer, padding};
    size_t max_size = fl::fl_max(fl::fl_max(large_buffer.size(), large_buffer.size()),
                                 fl::fl_max(large_buffer.size(), large_buffer.size()));
    fl::vector<uint8_t> result (max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result, nullptr);

    // Verify output size is correct
    CHECK_EQ(result.size(), NUM_BYTES * 4);

    // Verify correctness by checking extracted lanes
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(result.data(), result.size()));
    auto extracted = driver->extractLanes(4, NUM_BYTES);
    for (size_t i = 0; i < NUM_BYTES; i++) {
        CHECK_EQ(extracted[0][i], 0xAA);
        CHECK_EQ(extracted[1][i], 0xAA);
        CHECK_EQ(extracted[2][i], 0xAA);
        CHECK_EQ(extracted[3][i], 0xAA);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Optimized bit spreading - mixed patterns") {
    // Test with different patterns on each lane
    fl::vector<uint8_t> lane0 = {0x11, 0x22, 0x33};
    fl::vector<uint8_t> lane1 = {0x44, 0x55, 0x66};
    fl::vector<uint8_t> lane2 = {0x77, 0x88, 0x99};
    fl::vector<uint8_t> lane3 = {0xAA, 0xBB, 0xCC};

    fl::vector<uint8_t> padding = fl::vector<uint8_t>{0x00};
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0, padding};
    lanes[1] = SPITransposerQuad::LaneData{lane1, padding};
    lanes[2] = SPITransposerQuad::LaneData{lane2, padding};
    lanes[3] = SPITransposerQuad::LaneData{lane3, padding};
    size_t max_size = fl::fl_max(fl::fl_max(lane0.size(), lane1.size()),
                                 fl::fl_max(lane2.size(), lane3.size()));
    fl::vector<uint8_t> result (max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, result, nullptr);
    CHECK_EQ(result.size(), 12);  // 3 bytes * 4 = 12 output bytes

    // Verify correctness
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(result.data(), result.size()));
    auto extracted = driver->extractLanes(4, 3);
    CHECK_EQ(extracted[0][0], 0x11);
    CHECK_EQ(extracted[0][1], 0x22);
    CHECK_EQ(extracted[0][2], 0x33);
    CHECK_EQ(extracted[1][0], 0x44);
    CHECK_EQ(extracted[2][1], 0x88);
    CHECK_EQ(extracted[3][2], 0xCC);
}

// ============================================================================
// Buffer validation tests
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Buffer validation - empty lanes") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();

    // Finalize with no lanes should fail gracefully
    controller.finalize();
    CHECK(!controller.isFinalized());
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Buffer validation - exceeds DMA limit") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();

    // Create buffer larger than DMA limit (would be 70KB * 4 = 280KB > 256KB)
    // This should trigger truncation
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 23000);  // ~70KB

    controller.finalize();

    // Should truncate to DMA limit (65536 / 4 = 16384 bytes max per lane)
    CHECK(controller.isFinalized());
    CHECK(controller.getMaxLaneBytes() <= 16384);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Buffer validation - mismatched sizes") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();

    // Different LED counts per lane - should pad shorter lanes
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 25);   // Small
    controller.addLane<APA102Controller<3, 4, RGB>>(1, 100);  // Large

    controller.finalize();

    // Max lane bytes should be based on largest lane
    CHECK(controller.isFinalized());
    size_t max_bytes = controller.getMaxLaneBytes();
    CHECK(max_bytes == APA102Controller<1, 2, RGB>::calculateBytes(100));

    // Buffers are NOT pre-padded - they remain their original size
    // Padding is applied during transpose() using LED frames
    auto* buf0 = controller.getLaneBuffer(0);
    auto* buf1 = controller.getLaneBuffer(1);
    CHECK_EQ(buf0->size(), APA102Controller<1, 2, RGB>::calculateBytes(25));
    CHECK_EQ(buf1->size(), APA102Controller<1, 2, RGB>::calculateBytes(100));
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Buffer validation - all empty capture buffers") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();

    // Add lanes with 0 LEDs - still creates small buffers for protocol overhead
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 0);
    controller.addLane<APA102Controller<3, 4, RGB>>(1, 0);

    controller.finalize();

    // Should finalize even with 0 LEDs (protocol overhead bytes still present)
    CHECK(controller.isFinalized());
    // But max bytes should be very small
    CHECK(controller.getMaxLaneBytes() < 20);
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: Buffer validation - mixed empty and valid lanes") {
    QuadSPIController<2, 10000000> controller;
    controller.begin();

    // One valid lane, one with 0 LEDs
    controller.addLane<APA102Controller<1, 2, RGB>>(0, 100);
    controller.addLane<APA102Controller<3, 4, RGB>>(1, 0);

    controller.finalize();

    // Should finalize - at least one lane has data
    CHECK(controller.isFinalized());
}

// ============================================================================
// APA102 Padding Tests - Different Strip Lengths
// ============================================================================

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102 padding - different strip lengths padded to same size") {
    // Test that strips of different lengths (1, 3, 7, 13 LEDs) are all
    // padded to the same size in the interleaved format

    // Calculate byte sizes for each LED count using APA102 protocol
    // Formula: 4 (start frame) + (num_leds * 4) + (4 * ((num_leds / 32) + 1)) (end frame)
    size_t bytes_1_led = APA102Controller<1, 2, RGB>::calculateBytes(1);    // 12 bytes
    size_t bytes_3_leds = APA102Controller<1, 2, RGB>::calculateBytes(3);   // 20 bytes
    size_t bytes_7_leds = APA102Controller<1, 2, RGB>::calculateBytes(7);   // 36 bytes
    size_t bytes_13_leds = APA102Controller<1, 2, RGB>::calculateBytes(13); // 60 bytes

    // Verify our understanding of the formula
    CHECK_EQ(bytes_1_led, 12);
    CHECK_EQ(bytes_3_leds, 20);
    CHECK_EQ(bytes_7_leds, 36);
    CHECK_EQ(bytes_13_leds, 60);

    // Create lane data with different lengths
    fl::vector<uint8_t> lane0_data(bytes_13_leds, 0xAA); // 13 LEDs (longest)
    fl::vector<uint8_t> lane1_data(bytes_7_leds, 0xBB);  // 7 LEDs
    fl::vector<uint8_t> lane2_data(bytes_3_leds, 0xCC);  // 3 LEDs
    fl::vector<uint8_t> lane3_data(bytes_1_led, 0xDD);   // 1 LED (shortest)

    // Get black LED frame for APA102
    auto padding_frame = APA102Controller<1, 2, RGB>::getPaddingLEDFrame();
    CHECK_EQ(padding_frame.size(), 4);

    // Setup lanes
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0_data, padding_frame};
    lanes[1] = SPITransposerQuad::LaneData{lane1_data, padding_frame};
    lanes[2] = SPITransposerQuad::LaneData{lane2_data, padding_frame};
    lanes[3] = SPITransposerQuad::LaneData{lane3_data, padding_frame};

    // Perform transpose
    size_t max_size = bytes_13_leds;
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    // Verify output size: max_lane_bytes * 4
    size_t expected_size = bytes_13_leds * 4;
    CHECK_EQ(interleaved.size(), expected_size);

    // Extract lanes back using mock driver
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = driver->extractLanes(4, bytes_13_leds);

    // Verify all lanes are same size after extraction
    CHECK_EQ(extracted[0].size(), bytes_13_leds);
    CHECK_EQ(extracted[1].size(), bytes_13_leds);
    CHECK_EQ(extracted[2].size(), bytes_13_leds);
    CHECK_EQ(extracted[3].size(), bytes_13_leds);

    // Verify original data in each lane (no padding needed for longest)
    for (size_t i = 0; i < bytes_13_leds; i++) {
        CHECK_EQ(extracted[0][i], 0xAA); // Lane 0: full data
    }

    // Verify black LED padding at BEGINNING, original data at END

    // Lane 1: padding + original data
    size_t pad1 = bytes_13_leds - bytes_7_leds;
    for (size_t i = 0; i < pad1; i++) {
        CHECK_EQ(extracted[1][i], padding_frame[i % padding_frame.size()]); // Black LED padding
    }
    for (size_t i = 0; i < bytes_7_leds; i++) {
        CHECK_EQ(extracted[1][pad1 + i], 0xBB); // Original data
    }

    // Lane 2: padding + original data
    size_t pad2 = bytes_13_leds - bytes_3_leds;
    for (size_t i = 0; i < pad2; i++) {
        CHECK_EQ(extracted[2][i], padding_frame[i % padding_frame.size()]); // Black LED padding
    }
    for (size_t i = 0; i < bytes_3_leds; i++) {
        CHECK_EQ(extracted[2][pad2 + i], 0xCC); // Original data
    }

    // Lane 3: padding + original data
    size_t pad3 = bytes_13_leds - bytes_1_led;
    for (size_t i = 0; i < pad3; i++) {
        CHECK_EQ(extracted[3][i], padding_frame[i % padding_frame.size()]); // Black LED padding
    }
    for (size_t i = 0; i < bytes_1_led; i++) {
        CHECK_EQ(extracted[3][pad3 + i], 0xDD); // Original data
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102 padding - verify black LED padding source") {
    // Test that padding comes from controller's getPaddingLEDFrame()
    // and uses black LED frames for synchronized latching

    // Create two lanes of different lengths
    size_t bytes_5_leds = APA102Controller<1, 2, RGB>::calculateBytes(5);   // 28 bytes
    size_t bytes_10_leds = APA102Controller<1, 2, RGB>::calculateBytes(10); // 48 bytes

    fl::vector<uint8_t> lane0_data(bytes_10_leds, 0x11); // Longer
    fl::vector<uint8_t> lane1_data(bytes_5_leds, 0x22);  // Shorter
    fl::vector<uint8_t> empty_lane;

    // Get black LED frame from APA102 controller
    auto apa102_frame = APA102Controller<1, 2, RGB>::getPaddingLEDFrame();
    CHECK_EQ(apa102_frame.size(), 4); // 4 bytes per LED
    CHECK_EQ(apa102_frame[0], 0xE0);  // Brightness = 0
    CHECK_EQ(apa102_frame[1], 0x00);  // Blue = 0
    CHECK_EQ(apa102_frame[2], 0x00);  // Green = 0
    CHECK_EQ(apa102_frame[3], 0x00);  // Red = 0

    // Setup lanes with black LED padding
    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{lane0_data, apa102_frame};
    lanes[1] = SPITransposerQuad::LaneData{lane1_data, apa102_frame};
    lanes[2] = SPITransposerQuad::LaneData{empty_lane, apa102_frame}; // Empty lane
    lanes[3] = SPITransposerQuad::LaneData{empty_lane, apa102_frame}; // Empty lane

    size_t max_size = bytes_10_leds;
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    // Extract and verify padding
    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = driver->extractLanes(4, bytes_10_leds);

    size_t padding_bytes = bytes_10_leds - bytes_5_leds;

    // Lane 1: verify black LED padding at BEGINNING
    for (size_t i = 0; i < padding_bytes; i++) {
        uint8_t expected = apa102_frame[i % apa102_frame.size()];
        CHECK_EQ(extracted[1][i], expected);
    }

    // Lane 1: verify original data at END
    for (size_t i = 0; i < bytes_5_leds; i++) {
        CHECK_EQ(extracted[1][padding_bytes + i], 0x22);
    }

    // Lanes 2 and 3: completely padded with repeating black LED frames
    for (size_t i = 0; i < bytes_10_leds; i++) {
        uint8_t expected = apa102_frame[i % apa102_frame.size()];
        CHECK_EQ(extracted[2][i], expected);
        CHECK_EQ(extracted[3][i], expected);
    }
}

TEST_CASE_FIXTURE(SPIQuadTestFixture, "QuadSPI: APA102 padding - verify black LED padding at the BEGINNING") {
    // Test that black LED padding appears at the BEGINNING of shorter strips
    // for synchronized latching (all strips finish transmitting simultaneously)

    size_t bytes_2_leds = APA102Controller<1, 2, RGB>::calculateBytes(2);  // 16 bytes
    size_t bytes_6_leds = APA102Controller<1, 2, RGB>::calculateBytes(6);  // 32 bytes

    // Create lane with distinctive pattern: 0x01, 0x02, 0x03, ...
    fl::vector<uint8_t> short_lane;
    for (size_t i = 0; i < bytes_2_leds; i++) {
        short_lane.push_back(static_cast<uint8_t>(i + 1));
    }

    fl::vector<uint8_t> long_lane(bytes_6_leds, 0xEE);
    fl::vector<uint8_t> empty_lane;

    auto padding_frame = APA102Controller<1, 2, RGB>::getPaddingLEDFrame();

    fl::optional<SPITransposerQuad::LaneData> lanes[4];
    lanes[0] = SPITransposerQuad::LaneData{long_lane, padding_frame};
    lanes[1] = SPITransposerQuad::LaneData{short_lane, padding_frame};
    lanes[2] = SPITransposerQuad::LaneData{empty_lane, padding_frame};
    lanes[3] = SPITransposerQuad::LaneData{empty_lane, padding_frame};

    size_t max_size = bytes_6_leds;
    fl::vector<uint8_t> interleaved(max_size * 4);
    SPITransposerQuad::transpose(lanes, max_size, interleaved, nullptr);

    SPIQuadStub* driver = toStub(SPIQuad::getAll()[0]);
    driver->transmitAsync(fl::span<const uint8_t>(interleaved.data(), interleaved.size()));
    auto extracted = driver->extractLanes(4, bytes_6_leds);

    // Verify short lane has:
    // - Black LED padding at the BEGINNING
    // - Original data at the END

    size_t padding_bytes = bytes_6_leds - bytes_2_leds;

    // Check black LED padding is at the beginning
    // Black LED frame: {0xE0, 0x00, 0x00, 0x00} (repeated as needed)
    for (size_t i = 0; i < padding_bytes; i++) {
        uint8_t expected = padding_frame[i % padding_frame.size()];
        CHECK_EQ(extracted[1][i], expected);
    }

    // Check original data is at the end
    for (size_t i = 0; i < bytes_2_leds; i++) {
        CHECK_EQ(extracted[1][padding_bytes + i], static_cast<uint8_t>(i + 1));
    }
}
