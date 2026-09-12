// src/fl/channels/validation.h
//
// Validation test logic - stateless single-test execution
// Extracted from examples/validation for unit testing

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/optional.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/channels/rx/types.h"

// Forward declarations for detail modules
namespace fl {
namespace validation {
    class RxTest;  // IWYU pragma: keep
    class ResultFormatter;  // IWYU pragma: keep
    class Platform;  // IWYU pragma: keep
}
}

namespace fl {

namespace validation {

/// @brief Whether an RMT validation capture should use ESP-IDF loopback.
///
/// ESP-IDF's internal RMT path only connects TX and RX channels configured on
/// the same GPIO. Distinct pins require a physical jumper and must leave the
/// RX channel attached to its external GPIO input.
inline bool useRmtInternalLoopback(bool is_rmt_driver, int tx_pin,
                                   int rx_pin) FL_NO_EXCEPT {
    return is_rmt_driver && tx_pin == rx_pin;
}

/// @brief Select the independent capture backend used by AutoResearch.
///
/// ESP32-C6 RMT RX does not preserve PARLIO's sub-microsecond low phases even
/// though the same pad's GPIO input sees them. An explicit caller override
/// remains authoritative for diagnostics.
///
/// The GPIO ISR timestamp backend was used here originally (#3880) but
/// cannot work: its measured capture ceiling is ~2 us between edges
/// (min=2000 ns, under1us=0 across 1137 intervals of a 100-LED frame)
/// while WS2812 needs ~300 ns resolution. PARLIO RX oversamples the pin
/// into DMA at 16 MHz (62.5 ns) with no per-edge interrupt, which is the
/// only path on C6 fast enough (FastLED#3586).
inline RxBackend resolveCaptureBackend(RxBackend requested_backend,
                                       bool has_explicit_override,
                                       bool is_parlio_driver,
                                       bool is_esp32_c6) FL_NO_EXCEPT {
    if (!has_explicit_override && is_parlio_driver && is_esp32_c6) {
        return RxBackend::PARLIO_RX;
    }
    return requested_backend;
}

/// @brief Compute the RX edge-buffer capacity for a validation frame.
///
/// GPIO ISR RX requires a power-of-two circular buffer. Size that buffer from
/// the frame under test (two edges per bit plus framing headroom), rather than
/// from AutoResearch's oversized shared byte buffer.
inline size_t captureEdgeCapacity(size_t shared_buffer_bytes,
                                  size_t expected_data_bytes,
                                  RxBackend backend) FL_NO_EXCEPT {
    constexpr size_t kEdgesPerByte = 16;
    constexpr size_t kFramingEdges = 2;
    const size_t max_size = static_cast<size_t>(-1);

    if (backend != RxBackend::ISR) {
        if (shared_buffer_bytes > max_size / 8) {
            return 0;
        }
        return shared_buffer_bytes * 8;
    }

    if (expected_data_bytes > (max_size - kFramingEdges) / kEdgesPerByte) {
        return 0;
    }
    const size_t required =
        expected_data_bytes * kEdgesPerByte + kFramingEdges;
    size_t capacity = 1;
    while (capacity < required) {
        if (capacity > max_size / 2) {
            return 0;
        }
        capacity *= 2;
    }
    return capacity;
}

/// @brief Largest frame, in wire bytes, an RP PIO RX capture can hold.
///
/// Two independent ceilings bound a capture, and each is the binding one in
/// its own regime -- which is why no single constant, and no single model,
/// predicts both (FastLED#4371):
///
///  1. **Edge pool.** The capture appends one `EdgeTime` per signal phase into
///     a fixed pool: two phases per bit, eight bits per wire byte. Past
///     `edge_capacity / 16` bytes it overflows.
///  2. **Sample-time budget.** The sampler writes `samples_per_dma_word`
///     samples per DMA word at a fixed clock, and the word count is
///     `(edge_capacity + 1) / 2 + dma_tail_words` for every frame large enough
///     for either ceiling to matter. That is a constant quantity of *wall
///     clock*, not of data. The frame, plus the trailing idle the capture must
///     observe before it will terminate, plus the gap between arming and the
///     first edge, all have to fit inside it.
///
/// At a 1225 ns bit period the pool binds first (300 bytes, 2.9 ms); at
/// 2500 ns the clock binds first (3.84 ms, 189 bytes). Measured on an RP2350W
/// across WS2812B-V5, WS2818, WS2814, UCS7604-800KHZ and WS2811-400KHZ, the
/// smaller of the two reproduces every pass/fail boundary to the byte.
///
/// Shorter frames are not shortchanged by using the clamped word count: below
/// the clamp the budget grows by 16 words per byte while the frame grows by
/// only 8 bit-periods, so it never becomes the binding constraint there.
///
/// @param edge_capacity        Phase slots in the capture pool
///                             (`kRpPioRxEdgeCapacity`)
/// @param dma_tail_words       Reset-tail words added to the DMA transfer
///                             (`kRpPioRxDmaTailWords`)
/// @param samples_per_dma_word Pin samples packed into one DMA word
///                             (`kRpPioRxSamplesPerDmaWord`)
/// @param sample_period_ns     Nanoseconds per sample (1e9 / `kRpPioRxClockHz`)
/// @param idle_tail_ns         Trailing idle the capture must see to finish
///                             (`RxChannelConfig::signal_range_max_ns`)
/// @param arming_lead_in_ns    Reserve for the arm-to-first-edge gap
/// @param bit_period_ns        T1 + T2 + T3 of the chipset under test
/// @return Maximum wire bytes, or 0 if nothing fits
inline size_t rpPioMaxWireBytes(size_t edge_capacity,
                                size_t dma_tail_words,
                                u32 samples_per_dma_word,
                                u32 sample_period_ns,
                                u32 idle_tail_ns,
                                u32 arming_lead_in_ns,
                                u32 bit_period_ns) FL_NO_EXCEPT {
    constexpr size_t kPhasesPerByte = 16;  // 8 bits x (high + low)
    constexpr u64 kBitsPerByte = 8;

    if (bit_period_ns == 0 || edge_capacity == 0 || samples_per_dma_word == 0 ||
        sample_period_ns == 0) {
        return 0;
    }

    const size_t by_pool = edge_capacity / kPhasesPerByte;

    const u64 dma_words =
        static_cast<u64>((edge_capacity + 1u) / 2u) + dma_tail_words;
    const u64 budget_ns =
        dma_words * samples_per_dma_word * sample_period_ns;
    const u64 reserved_ns =
        static_cast<u64>(idle_tail_ns) + arming_lead_in_ns;
    if (budget_ns <= reserved_ns) {
        return 0;
    }
    const u64 by_clock =
        (budget_ns - reserved_ns) / (kBitsPerByte * bit_period_ns);

    if (static_cast<u64>(by_pool) < by_clock) {
        return by_pool;
    }
    return static_cast<size_t>(by_clock);
}

}  // namespace validation

/// @brief Single test configuration - fully stateless
struct SingleTestConfig {
    string driver_name;           ///< Driver to test (e.g., "PARLIO", "RMT")
    vector<int> lane_sizes;       ///< LED count per lane [100, 100, 200]
    string pattern;               ///< Test pattern name (default: "MSB_LSB_A")
    int iterations;               ///< Number of test iterations (default: 1)
    int pin_tx;                   ///< TX pin (base pin for multi-lane)
    int pin_rx;                   ///< RX pin

    SingleTestConfig() FL_NO_EXCEPT
        : pattern("MSB_LSB_A")
        , iterations(1)
        , pin_tx(1)
        , pin_rx(0) {}
};

/// @brief Single test result - comprehensive pass/fail information
struct SingleTestResult {
    bool success;                 ///< RPC execution succeeded
    bool passed;                  ///< All validation tests passed
    int total_tests;              ///< Total validation tests run
    int passed_tests;             ///< Number of tests that passed
    u32 duration_ms;              ///< Test execution time (milliseconds)
    string driver;                ///< Driver tested
    int lane_count;               ///< Number of lanes tested
    vector<int> lane_sizes;       ///< LED counts per lane
    string pattern;               ///< Pattern tested

    // Optional failure info
    optional<string> error_message;       ///< Error message if !success
    optional<string> failure_pattern;     ///< Pattern that failed if !passed
    optional<string> failure_details;     ///< Failure details

    SingleTestResult() FL_NO_EXCEPT
        : success(false)
        , passed(false)
        , total_tests(0)
        , passed_tests(0)
        , duration_ms(0)
        , lane_count(0) {}
};

/// @brief Driver test result tracking (moved from ValidationTest.h)
struct DriverTestResult {
    fl::string driver_name;  ///< Driver name (e.g., "RMT", "SPI", "PARLIO")
    int total_tests;         ///< Total test count across all chipset timings
    int passed_tests;        ///< Passed test count across all chipset timings
    bool skipped;            ///< True if driver was skipped (e.g., failed to set exclusive)

    DriverTestResult(const char* name)
        : driver_name(name)
        , total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    DriverTestResult() FL_NO_EXCEPT
        : total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    /// @brief Check if all tests passed
    bool allPassed() const { return !skipped && total_tests > 0 && passed_tests == total_tests; }

    /// @brief Check if any tests failed
    bool anyFailed() const { return !skipped && total_tests > 0 && passed_tests < total_tests; }
};

/// @brief Run a single stateless validation test
/// @param config Test configuration
/// @return Test result with pass/fail information
SingleTestResult runSingleValidationTest(const SingleTestConfig& config);

}  // namespace fl
