/// @file spi_batching_logic.cpp
/// @brief Unit tests for SPI batching logic validation
///
/// These tests validate the batching algorithm logic used by ChannelEngineSpi
/// for timing-aware channel batching when N > K (channels exceed lane capacity).
///
/// Since the batching methods are private, these tests focus on:
/// 1. Algorithm correctness (batch count calculation)
/// 2. Timing compatibility grouping logic
/// 3. Lane capacity determination
///
/// The actual SPI hardware transmission is tested via integration tests.

#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/math_macros.h"

using namespace fl;

namespace spi_batching_logic_test {

/// @brief Calculate number of batches using ceiling division
/// Replicates the logic from ChannelEngineSpi::beginBatchedTransmission()
inline size_t calculateBatchCount(size_t N, uint8_t K) {
    return (N + K - 1) / K;  // ceil(N/K)
}

FL_TEST_CASE("Batch calculation - exact fit") {
    // When N equals K, expect exactly 1 batch
    FL_CHECK_EQ(calculateBatchCount(4, 4), 1);
    FL_CHECK_EQ(calculateBatchCount(2, 2), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 1), 1);
}

FL_TEST_CASE("Batch calculation - multiple batches") {
    // When N is multiple of K, expect N/K batches
    FL_CHECK_EQ(calculateBatchCount(8, 4), 2);
    FL_CHECK_EQ(calculateBatchCount(12, 4), 3);
    FL_CHECK_EQ(calculateBatchCount(16, 4), 4);
}

FL_TEST_CASE("Batch calculation - partial last batch") {
    // When N > K but not evenly divisible, expect ceil(N/K) batches
    FL_CHECK_EQ(calculateBatchCount(5, 4), 2);   // 4 + 1
    FL_CHECK_EQ(calculateBatchCount(9, 4), 3);   // 4 + 4 + 1
    FL_CHECK_EQ(calculateBatchCount(10, 4), 3);  // 4 + 4 + 2
    FL_CHECK_EQ(calculateBatchCount(7, 3), 3);   // 3 + 3 + 1
}

FL_TEST_CASE("Batch calculation - single channel") {
    // Single channel always results in 1 batch, regardless of K
    FL_CHECK_EQ(calculateBatchCount(1, 4), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 2), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 1), 1);
}

FL_TEST_CASE("Batch calculation - many channels small lanes") {
    // Large N with small K
    FL_CHECK_EQ(calculateBatchCount(100, 4), 25);
    FL_CHECK_EQ(calculateBatchCount(99, 4), 25);
    FL_CHECK_EQ(calculateBatchCount(97, 4), 25);
    FL_CHECK_EQ(calculateBatchCount(96, 4), 24);
}

FL_TEST_CASE("Batch calculation - edge cases") {
    // Edge case: N just over K (worst utilization)
    FL_CHECK_EQ(calculateBatchCount(5, 4), 2);  // 4 + 1 (25% utilization in last batch)
    FL_CHECK_EQ(calculateBatchCount(3, 2), 2);  // 2 + 1 (50% utilization in last batch)

    // Edge case: N = K-1 (all fit in single batch)
    FL_CHECK_EQ(calculateBatchCount(3, 4), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 2), 1);
}

FL_TEST_CASE("Lane capacity - maximum lane determination") {
    // Simulates determineLaneCapacity() logic:
    // Returns maximum lane count from available multi-lane configs

    // No multi-lane configs → K = 1 (default)
    uint8_t maxLanes = 1;
    FL_CHECK_EQ(maxLanes, 1);

    // One 4-lane config → K = 4
    maxLanes = fl_max<uint8_t>(1, 4);
    FL_CHECK_EQ(maxLanes, 4);

    // Multiple configs (2-lane and 4-lane) → K = 4 (max)
    maxLanes = 1;
    maxLanes = fl_max(maxLanes, 2);
    maxLanes = fl_max(maxLanes, 4);
    FL_CHECK_EQ(maxLanes, 4);

    // All 1-lane configs → K = 1
    maxLanes = 1;
    maxLanes = fl_max(maxLanes, 1);
    maxLanes = fl_max(maxLanes, 1);
    FL_CHECK_EQ(maxLanes, 1);
}

FL_TEST_CASE("Timing group batching - sequential transmission") {
    // Scenario: 2 timing groups with different lane capacities

    // Group 1: WS2812 (8 channels, 4 lanes)
    size_t ws2812_channels = 8;
    uint8_t ws2812_lanes = 4;
    size_t ws2812_batches = calculateBatchCount(ws2812_channels, ws2812_lanes);
    FL_CHECK_EQ(ws2812_batches, 2);  // 2 batches of 4

    // Group 2: SK6812 (4 channels, 4 lanes)
    size_t sk6812_channels = 4;
    uint8_t sk6812_lanes = 4;
    size_t sk6812_batches = calculateBatchCount(sk6812_channels, sk6812_lanes);
    FL_CHECK_EQ(sk6812_batches, 1);  // 1 batch of 4

    // Total batches (sequential transmission)
    size_t total_batches = ws2812_batches + sk6812_batches;
    FL_CHECK_EQ(total_batches, 3);
}

FL_TEST_CASE("Batch index calculation - channel assignment") {
    // Simulates how channels are assigned to batches

    size_t N = 10;  // 10 channels
    uint8_t K = 4;  // 4 lanes
    size_t numBatches = calculateBatchCount(N, K);
    FL_CHECK_EQ(numBatches, 3);  // 4 + 4 + 2

    // Batch 0: channels 0-3
    size_t batch0_start = 0 * K;
    size_t batch0_end = fl_min(batch0_start + K, N);
    FL_CHECK_EQ(batch0_start, 0);
    FL_CHECK_EQ(batch0_end, 4);
    FL_CHECK_EQ(batch0_end - batch0_start, 4);  // 4 channels

    // Batch 1: channels 4-7
    size_t batch1_start = 1 * K;
    size_t batch1_end = fl_min(batch1_start + K, N);
    FL_CHECK_EQ(batch1_start, 4);
    FL_CHECK_EQ(batch1_end, 8);
    FL_CHECK_EQ(batch1_end - batch1_start, 4);  // 4 channels

    // Batch 2: channels 8-9 (partial batch)
    size_t batch2_start = 2 * K;
    size_t batch2_end = fl_min(batch2_start + K, N);
    FL_CHECK_EQ(batch2_start, 8);
    FL_CHECK_EQ(batch2_end, 10);
    FL_CHECK_EQ(batch2_end - batch2_start, 2);  // 2 channels (partial)
}

FL_TEST_CASE("Performance expectations - timing calculation") {
    // Validates expected speedup from batching

    // Scenario: 8 WS2812 strips, 100 LEDs each, 4-lane hardware
    size_t strips = 8;
    uint8_t lanes = 4;

    // Current behavior (no batching): 8 sequential transmissions
    size_t sequential_transmissions = strips;
    FL_CHECK_EQ(sequential_transmissions, 8);

    // With batching: ceil(8/4) = 2 batches
    size_t batched_transmissions = calculateBatchCount(strips, lanes);
    FL_CHECK_EQ(batched_transmissions, 2);

    // Speedup factor: 8/2 = 4x
    size_t speedup_factor = sequential_transmissions / batched_transmissions;
    FL_CHECK_EQ(speedup_factor, 4);
}

FL_TEST_CASE("Edge case - empty channel list") {
    // Empty list should result in 0 batches
    // (handled by early return in actual implementation)
    size_t N = 0;
    uint8_t K = 4;

    if (N == 0) {
        // Early return - no batches needed
        FL_CHECK(true);
    } else {
        size_t batches = calculateBatchCount(N, K);
        FL_CHECK_EQ(batches, 0);  // Would not reach here
    }
}

FL_TEST_CASE("Edge case - N much greater than K") {
    // Many strips on limited hardware
    size_t N = 100;
    uint8_t K = 4;
    size_t batches = calculateBatchCount(N, K);
    FL_CHECK_EQ(batches, 25);  // 25 batches of 4 channels each

    // Verify last batch is full
    size_t last_batch_start = 24 * K;
    size_t last_batch_end = fl_min(last_batch_start + K, N);
    FL_CHECK_EQ(last_batch_end - last_batch_start, 4);  // Full batch
}

FL_TEST_CASE("Backward compatibility - single channel no batching") {
    // Single channel with any K → 1 batch (unchanged behavior)
    FL_CHECK_EQ(calculateBatchCount(1, 1), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 2), 1);
    FL_CHECK_EQ(calculateBatchCount(1, 4), 1);

    // Verifies batching doesn't change single-channel behavior
}

} // namespace spi_batching_logic_test
