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
 * - All ISR-accessed data in one cache-line-friendly structure
 * - Volatile members for ISR/main thread communication
 * - Lookup tables for fast bit-to-RMT conversion
 */
struct RmtWorkerIsrData {
    // === Enabled Flag ===
    // Indicates whether this worker is actively processing data
    // Set to true when transmission starts, false when complete
    // Used by ISR to determine which channels to process (not the availability flag!)
    // Accessed by: ISR (read), main thread (write)
    bool mEnabled;

    // === Completion Flag Pointer ===
    // Pointer to the worker's completion flag (nullptr = slot free, non-null = busy)
    // ISR sets *mCompleted = true when transmission completes (completion signal only)
    // NOT used to determine if worker should process - that's what mEnabled is for
    // Accessed by: ISR (write), main thread (read/write)
    volatile bool* mCompleted;

    // === Hardware Channel ID ===
    // Physical RMT channel ID (0-7 on ESP32, 0-3 on ESP32-S2/S3/C3/C6)
    // Used by global ISR to determine which worker handles which channel
    // Accessed by: Main thread (write), ISR (read)
    uint8_t mChannelId;

    // === Buffer Management ===
    // Which half of the ping-pong buffer (0 or 1)
    // Accessed by: ISR (read/write), main thread (write on init)
    uint8_t mWhichHalf;

    // Current byte position in pixel data (updated during buffer refill)
    // Accessed by: ISR (read/write), main thread (write on init)
    int mCur;

    // === RMT Memory Pointers ===
    // Start of RMT channel memory (base address for this channel)
    // Accessed by: ISR (read), main thread (write on configure)
    volatile rmt_item32_t* mRMT_mem_start;

    // Current write pointer in RMT memory (updated during buffer refill)
    // Accessed by: ISR (read/write), main thread (write on init)
    volatile rmt_item32_t* mRMT_mem_ptr;

    // === Transmission Data ===
    // Pointer to pixel data (NOT owned by worker, just a reference)
    // Accessed by: ISR (read), main thread (write on transmit)
    const uint8_t* mPixelData;

    // Total number of bytes to transmit
    // Accessed by: ISR (read), main thread (write on transmit)
    int mNumBytes;

    // === Lookup Table ===
    // Nibble lookup table for fast bit-to-RMT conversion
    // Maps each nibble value (0x0-0xF) to 4 RMT items (MSB first: bit3, bit2, bit1, bit0)
    // Used for both high nibble (bits 7-4) and low nibble (bits 3-0)
    // Accessed by: ISR (read), main thread (write on configure)
    rmt_nibble_lut_t mNibbleLut;

    // === Reset Pulse Timing ===
    // Reset pulse duration in RMT ticks (LOW level after data transmission)
    // Required for chipsets like WS2812 to latch data and reset internal state
    // uint32_t to support high-frequency clocks (40MHz+) where reset time may exceed uint16_t max
    // For chained reset pulses, this value is split across multiple RMT items in fillNextHalf()
    // Stores the template value (set in config), and is used as working counter during transmission
    // Decremented in ISR as reset chunks are written
    // NOTE: Does NOT need restoration in tx_start() - startTransmission() is called before each
    // transmission and properly initializes this value via config()
    // Accessed by: ISR (read/write), main thread (read/write)
    uint32_t mResetTicksRemaining;

    // Constructor
    RmtWorkerIsrData()
        : mEnabled(false)
        , mCompleted(nullptr)
        , mChannelId(0xFF)  // Invalid channel ID
        , mWhichHalf(0)
        , mCur(0)
        , mRMT_mem_start(nullptr)
        , mRMT_mem_ptr(nullptr)
        , mPixelData(nullptr)
        , mNumBytes(0)
        , mNibbleLut{}
        , mResetTicksRemaining(0)
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
