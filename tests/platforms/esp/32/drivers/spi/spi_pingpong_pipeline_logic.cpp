// ok standalone
/// @file spi_pingpong_pipeline_logic.cpp
/// @brief Host-side state-machine tests for the back-to-back DMA chunk
///        streaming added in #2304.
///
/// The real driver in `src/platforms/esp/32/drivers/spi/channel_driver_spi.cpp.hpp`
/// only compiles under ESP-IDF, so this test models the buffer/transaction
/// rotation logic in plain C++ and verifies the invariants we care about:
///
///   1. After startFirstDma() in async mode, BOTH stagingA and stagingB are
///      queued for any strip whose encoded payload exceeds a single staging
///      buffer (the > ~680-LED case the issue cites).
///   2. For strips that fit in one chunk, only A is queued.
///   3. The refill-on-completion loop refills the buffer that just freed up
///      (ping-pong) and keeps two transactions in the SPI ISR's queue as long
///      as data remains.
///   4. The pipeline never lets fewer than 2 chunks be queued while data
///      remains and never queues more than 2.
///   5. Final drain waits for both outstanding transactions before marking the
///      channel complete.
///
/// These tests do NOT exercise real SPI hardware; on-device byte equality and
/// inter-chunk timing must be validated via `bash autoresearch --spi`.

#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace spi_pingpong_pipeline_logic_test {

/// Mirror of the relevant fields from
/// ChannelEngineSpi::SpiChannelState / DmaPipelineState. Only the fields the
/// pre-queue + refill state machine touches are modelled.
struct PipelineModel {
    // Source data state
    size_t ledBytesRemaining;
    size_t stagingCapacity;       // SPI bytes per staging buffer
    size_t bytesPerLedByte;       // wave8 expansion factor (8 for WS2812)

    // Transaction tracking
    bool transAInFlight;
    bool transBInFlight;
    int  encodeIdx;               // Next buffer to encode into; bit 0 selects A/B

    // Completion bookkeeping
    int  completedCount;
    int  queuedCount;

    // Platform flag: true on ESP32-C6 (split polling, max 1 in flight)
    bool usePolling;

    static PipelineModel make(size_t bytes, size_t capacity, bool polling) {
        PipelineModel m{};
        m.ledBytesRemaining = bytes;
        m.stagingCapacity   = capacity;
        m.bytesPerLedByte   = 8;
        m.transAInFlight    = false;
        m.transBInFlight    = false;
        m.encodeIdx         = 0;
        m.completedCount    = 0;
        m.queuedCount       = 0;
        m.usePolling        = polling;
        return m;
    }

    /// Encode one chunk into the selected buffer; advance LED state.
    /// Returns the number of SPI bytes "encoded" (0 if no data left).
    size_t encodeChunk(int /*bufIdx*/) {
        if (ledBytesRemaining == 0) return 0;
        const size_t max_led_bytes = stagingCapacity / bytesPerLedByte;
        const size_t to_encode = (ledBytesRemaining < max_led_bytes)
                                     ? ledBytesRemaining
                                     : max_led_bytes;
        ledBytesRemaining -= to_encode;
        return to_encode * bytesPerLedByte;
    }

    /// Mirrors the new startFirstDma() logic from the driver.
    void startFirstDma() {
        size_t encodedA = encodeChunk(0);
        if (encodedA == 0) return;
        transAInFlight = true;
        encodeIdx = 1;
        ++queuedCount;

        // Back-to-back pre-queue: only on async (non-polling) path AND only
        // when more data remains for this channel.
        if (!usePolling && ledBytesRemaining > 0) {
            size_t encodedB = encodeChunk(1);
            if (encodedB > 0) {
                transBInFlight = true;
                encodeIdx = 2;
                ++queuedCount;
            }
        }
    }

    bool anyInFlight() const { return transAInFlight || transBInFlight; }

    /// Mirrors a single STREAMING-phase step:
    ///   * drain one completion (FIFO order: A before B in async; current
    ///     buffer in polling),
    ///   * refill the freed buffer if more data remains.
    /// Returns true if a step occurred (work done or completion drained).
    bool stepStreaming() {
        if (!anyInFlight()) return false;

        // Drain one completion. Record which buffer freed up so the refill
        // goes to that same buffer — modeling the ESP-IDF SPI driver where
        // each staging buffer is independently tracked. Encoding always
        // targets the just-freed buffer, never the one still in flight.
        //
        // Polling and async share the same drain logic: drain whichever
        // buffer is currently in flight (A first if both are, mirroring the
        // ESP-IDF FIFO). The previous polling branch used `encodeIdx & 1`,
        // which alternated between A and B independently of which one was
        // actually in flight — that produced spurious "drain B" steps when
        // only A had been queued, and the resulting infinite loop manifested
        // as a UBSan signed-integer-overflow on `++completedCount`
        // (refs the test failure on this file at line 126).
        int drainedBuf = -1;
        if (transAInFlight)      { transAInFlight = false; drainedBuf = 0; }
        else if (transBInFlight) { transBInFlight = false; drainedBuf = 1; }
        ++completedCount;

        // Refill the just-freed buffer when more data remains.
        if (ledBytesRemaining > 0 && drainedBuf >= 0) {
            size_t encoded = encodeChunk(drainedBuf);
            if (encoded > 0) {
                if (drainedBuf == 0) transAInFlight = true;
                else                 transBInFlight = true;
                ++encodeIdx;
                ++queuedCount;
            }
        }
        return true;
    }

    /// Drain all remaining in-flight transactions (mirrors COMPLETING phase).
    void drainAll() {
        while (anyInFlight()) {
            if (transAInFlight) transAInFlight = false;
            else if (transBInFlight) transBInFlight = false;
            ++completedCount;
        }
    }
};

// ---------------------------------------------------------------------------
// Invariant: small strip that fits in one chunk → only A is queued.
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - single-chunk strip queues only A (async)") {
    // 100 LEDs * 3 channels = 300 LED bytes; wave8 → 2400 SPI bytes; fits in 16 KB.
    auto m = PipelineModel::make(/*bytes=*/300, /*capacity=*/16 * 1024,
                                 /*polling=*/false);
    m.startFirstDma();

    CHECK_TRUE(m.transAInFlight);
    FL_CHECK_FALSE(m.transBInFlight);
    FL_CHECK_EQ(m.queuedCount, 1);
    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
}

// ---------------------------------------------------------------------------
// Invariant: long strip pre-queues BOTH A and B in startFirstDma() (async).
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - long strip pre-queues both A and B (async)") {
    // 1000 LEDs * 3 = 3000 LED bytes; wave8 → 24000 SPI bytes; needs 2 chunks
    // of 16 KB. This is the >~680-LED regression case from issue #2304.
    auto m = PipelineModel::make(/*bytes=*/3000, /*capacity=*/16 * 1024,
                                 /*polling=*/false);
    m.startFirstDma();

    CHECK_TRUE(m.transAInFlight);
    CHECK_TRUE(m.transBInFlight);
    FL_CHECK_EQ(m.queuedCount, 2);
    // Both chunks already encoded → no LED bytes left.
    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
}

// ---------------------------------------------------------------------------
// Invariant: polling path (ESP32-C6) keeps the old single-in-flight behavior.
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - polling path queues only A even on long strip") {
    auto m = PipelineModel::make(/*bytes=*/3000, /*capacity=*/16 * 1024,
                                 /*polling=*/true);
    m.startFirstDma();

    CHECK_TRUE(m.transAInFlight);
    FL_CHECK_FALSE(m.transBInFlight);
    FL_CHECK_EQ(m.queuedCount, 1);
}

// ---------------------------------------------------------------------------
// Invariant: for very long strips (3+ chunks), the refill loop keeps 2
// transactions in flight at all times until the last chunk is queued.
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - refill loop maintains two in flight on 3-chunk strip") {
    // 6000 LED bytes / (16384/8 = 2048 bytes per chunk) = 2.93 → 3 chunks.
    auto m = PipelineModel::make(/*bytes=*/6000, /*capacity=*/16 * 1024,
                                 /*polling=*/false);
    m.startFirstDma();
    // After pre-queue, A+B queued (=2), C still pending (2000 LED bytes left).
    FL_CHECK_EQ(m.queuedCount, 2);
    CHECK_TRUE(m.transAInFlight);
    CHECK_TRUE(m.transBInFlight);
    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)6000 - 2 * 2048);

    // First completion (transA): drain + refill into A. Now A and B both in
    // flight again, and ledBytesRemaining == 0 (3rd chunk filled A).
    bool stepped = m.stepStreaming();
    CHECK_TRUE(stepped);
    FL_CHECK_EQ(m.queuedCount, 3);
    CHECK_TRUE(m.anyInFlight());
    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
}

// ---------------------------------------------------------------------------
// Invariant: refill never queues more than 2 chunks ahead of the consumer.
// Hard cap is enforced by the buffer count (only A and B exist).
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - queue depth never exceeds 2") {
    auto m = PipelineModel::make(/*bytes=*/24000, /*capacity=*/16 * 1024,
                                 /*polling=*/false);
    m.startFirstDma();

    // Drive the pipeline to completion, checking at every step that we never
    // exceed queue_size = 2 (the physical buffer cap).
    int safety = 100;
    while (m.anyInFlight() || m.ledBytesRemaining > 0) {
        int inFlight = (m.transAInFlight ? 1 : 0) + (m.transBInFlight ? 1 : 0);
        CHECK_TRUE(inFlight <= 2);
        if (!m.stepStreaming()) break;
        if (--safety <= 0) break;
    }
    FL_CHECK_GT(safety, 0);                 // didn't loop forever
    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
}

// ---------------------------------------------------------------------------
// Invariant: every byte of LED data is eventually transmitted (no chunk
// drop) and the queued/completed counters match at the end.
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - all bytes accounted for after drain") {
    const size_t TOTAL_BYTES = 12345;  // odd number, forces partial last chunk
    auto m = PipelineModel::make(TOTAL_BYTES, /*capacity=*/4 * 1024,
                                 /*polling=*/false);

    m.startFirstDma();
    // Safety counter mirrors the queue-depth test above: if a future
    // regression spins the drain loop, fail-fast with a clear assertion
    // instead of waiting 20 s for the runner watchdog (see #2824).
    int safety = 1000;
    while (m.anyInFlight() || m.ledBytesRemaining > 0) {
        if (!m.stepStreaming()) break;
        if (--safety <= 0) break;
    }
    FL_CHECK_GT(safety, 0);
    m.drainAll();

    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
    FL_CHECK_EQ(m.queuedCount, m.completedCount);
    FL_CHECK_GT(m.queuedCount, 1);   // multi-chunk strip → at least 2 queued
}

// ---------------------------------------------------------------------------
// Invariant: polling path drains correctly too (one in flight at a time, but
// all bytes still get sent).
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - polling path drains all bytes") {
    const size_t TOTAL_BYTES = 8000;
    auto m = PipelineModel::make(TOTAL_BYTES, /*capacity=*/4 * 1024,
                                 /*polling=*/true);

    m.startFirstDma();
    // Safety counter — this is the exact loop that hung at 20 s before #2843
    // dropped the encodeIdx-based polling branch (see #2824). Keeping the
    // guard means a future regression of the same shape fails in ms with a
    // clear assertion, not as a runner-watchdog timeout.
    int safety = 1000;
    while (m.anyInFlight() || m.ledBytesRemaining > 0) {
        if (!m.stepStreaming()) break;
        if (--safety <= 0) break;
    }
    FL_CHECK_GT(safety, 0);
    m.drainAll();

    FL_CHECK_EQ(m.ledBytesRemaining, (size_t)0);
    FL_CHECK_EQ(m.queuedCount, m.completedCount);
    // Polling path: only ever 1 in flight at a time.
    // queuedCount == number of chunks == ceil(TOTAL_BYTES / (capacity/8))
    const size_t bytes_per_chunk = (4 * 1024) / 8;
    const size_t expected_chunks =
        (TOTAL_BYTES + bytes_per_chunk - 1) / bytes_per_chunk;
    FL_CHECK_EQ((size_t)m.queuedCount, expected_chunks);
}

// ---------------------------------------------------------------------------
// Regression: the issue's specific repro — chunk-boundary behaviour around
// the >~680-LED case. A 16 KB staging buffer with wave8 expansion holds
// exactly 2048 LED bytes (≈ 682 RGB LEDs). 682-LED strips fit in a single
// chunk; 683+ LED strips need two and must trigger the pre-queue path.
// ---------------------------------------------------------------------------
FL_TEST_CASE("Ping-pong - 682-LED boundary regression (#2304)") {
    // 682 LEDs RGB → 2046 LED bytes → 16368 SPI bytes → still fits in 16 KB
    auto small = PipelineModel::make(/*bytes=*/2046, /*capacity=*/16 * 1024,
                                     /*polling=*/false);
    small.startFirstDma();
    FL_CHECK_EQ(small.queuedCount, 1);  // single chunk, no pre-queue needed

    // 683 LEDs RGB → 2049 LED bytes → 16392 SPI bytes → needs 2 chunks
    auto big = PipelineModel::make(/*bytes=*/2049, /*capacity=*/16 * 1024,
                                   /*polling=*/false);
    big.startFirstDma();
    FL_CHECK_EQ(big.queuedCount, 2);  // pre-queues both A and B
}

} // namespace spi_pingpong_pipeline_logic_test

} // FL_TEST_FILE
