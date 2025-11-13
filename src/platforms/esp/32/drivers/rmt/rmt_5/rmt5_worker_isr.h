#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker_lut.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_tx.h"
FL_EXTERN_C_END

namespace fl {

// Forward declaration
class RmtWorker;

/**
 * RmtWorkerIsrData - ISR-optimized data structure for RMT worker
 *
 * This structure contains all data that needs to be accessed from
 * interrupt context for fast buffer refill operations. By grouping
 * this data together, we minimize pointer chasing in the ISR.
 *
 * Memory Layout Strategy:
 * - Cache line aligned (32 bytes) for optimal ESP32 cache performance
 * - Hot data (accessed every byte/iteration) placed first for best cache locality
 * - Frequently accessed lookup table at the beginning (256 bytes, 8 cache lines)
 * - Member ordering optimized for ISR access patterns:
 *   1. mNibbleLut: Accessed every byte conversion (HOT)
 *   2. Hot pointers/counters: Accessed every loop iteration
 *   3. Cold data: Accessed rarely (initialization, completion)
 */
struct alignas(32) RmtWorkerIsrData {
    // === Lookup Table (HOT - First in structure for optimal cache access) ===
    // Nibble lookup table for fast bit-to-RMT conversion (256 bytes = 8 cache lines)
    // Maps each nibble value (0x0-0xF) to 4 RMT items (MSB first: bit3, bit2, bit1, bit0)
    // Used for both high nibble (bits 7-4) and low nibble (bits 3-0)
    // Accessed by: ISR (read - EVERY BYTE), main thread (write on configure)
    // Placed first for guaranteed alignment at structure start
    alignas(32) rmt_nibble_lut_t mNibbleLut;

    // === Hot Data (Accessed every loop iteration) ===

    // Pointer to pixel data (NOT owned by worker, just a reference)
    // Accessed by: ISR (read - EVERY BYTE), main thread (write on transmit)
    const uint8_t* mPixelData;

    // Current write pointer in RMT memory (updated during buffer refill)
    // Accessed by: ISR (read/write - EVERY BYTE), main thread (write on init)
    volatile rmt_item32_t* mRMT_mem_ptr;

    // Current byte position in pixel data (updated during buffer refill)
    // Accessed by: ISR (read/write - EVERY ITERATION), main thread (write on init)
    int mCur;

    // Total number of bytes to transmit
    // Accessed by: ISR (read - EVERY ITERATION), main thread (write on transmit)
    int mNumBytes;

    // === Warm Data (Accessed periodically) ===

    // Start of RMT channel memory (base address for this channel)
    // Accessed by: ISR (read - on buffer wrap), main thread (write on configure)
    volatile rmt_item32_t* mRMT_mem_start;

    // Reset pulse duration in RMT ticks (LOW level after data transmission)
    // Required for chipsets like WS2812 to latch data and reset internal state
    // uint32_t to support high-frequency clocks (40MHz+) where reset time may exceed uint16_t max
    // For chained reset pulses, this value is split across multiple RMT items in fillNextHalf()
    // Stores the template value (set in config), and is used as working counter during transmission
    // Decremented in ISR as reset chunks are written
    // NOTE: Does NOT need restoration in tx_start() - startTransmission() is called before each
    // transmission and properly initializes this value via config()
    // Accessed by: ISR (read/write - at end of transmission), main thread (read/write)
    uint32_t mResetTicksRemaining;

    // Which half of the ping-pong buffer (0 or 1)
    // Accessed by: ISR (read/write - on buffer wrap), main thread (write on init)
    uint8_t mWhichHalf;

    // === Cold Data (Accessed rarely) ===

    // Indicates whether this worker is actively processing data
    // Set to true when transmission starts, false when complete
    // Used by ISR to determine which channels to process (not the availability flag!)
    // Accessed by: ISR (read - at start), main thread (write)
    bool mEnabled;

    // Hardware Channel ID (0-7 on ESP32, 0-3 on ESP32-S2/S3/C3/C6)
    // Used by global ISR to determine which worker handles which channel
    // Accessed by: Main thread (write), ISR (rarely)
    uint8_t mChannelId;

    // Padding byte for alignment (unused)
    uint8_t _padding;

    // Pointer to the worker's completion flag (nullptr = slot free, non-null = busy)
    // ISR sets *mCompleted = true when transmission completes (completion signal only)
    // NOT used to determine if worker should process - that's what mEnabled is for
    // Accessed by: ISR (write - at completion), main thread (read/write)
    volatile bool* mCompleted;

    // Constructor
    RmtWorkerIsrData()
        : mNibbleLut{}
        , mPixelData(nullptr)
        , mRMT_mem_ptr(nullptr)
        , mCur(0)
        , mNumBytes(0)
        , mRMT_mem_start(nullptr)
        , mResetTicksRemaining(0)
        , mWhichHalf(0)
        , mEnabled(false)
        , mChannelId(0xFF)  // Invalid channel ID
        , _padding(0)
        , mCompleted(nullptr)
    {}

    /**
     * Configure ISR data for transmission
     *
     * @param completed Pointer to worker's completion flag (ISR signals completion by setting to true)
     * @param channel_id Hardware RMT channel ID
     * @param rmt_mem_start Pointer to start of RMT channel memory
     * @param pixel_data Pointer to pixel data to transmit
     * @param num_bytes Number of bytes to transmit
     * @param nibble_lut Pre-built nibble lookup table
     * @param reset_ticks Reset pulse duration in RMT ticks (uint32_t for 40MHz+ clock support)
     */
    void config(
        volatile bool* completed,
        uint8_t channel_id,
        volatile rmt_item32_t* rmt_mem_start,
        const uint8_t* pixel_data,
        int num_bytes,
        const rmt_nibble_lut_t& nibble_lut,
        uint32_t reset_ticks
    ) {
        mEnabled = false;  // Will be set to true when transmission starts
        mCompleted = completed;
        mChannelId = channel_id;
        mWhichHalf = 0;
        mCur = 0;
        mRMT_mem_start = rmt_mem_start;
        mRMT_mem_ptr = rmt_mem_start;
        mPixelData = pixel_data;
        mNumBytes = num_bytes;
        mResetTicksRemaining = reset_ticks;  // Store value

        // Copy nibble lookup table
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 4; j++) {
                mNibbleLut[i][j].val = nibble_lut[i][j].val;
            }
        }
    }
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
