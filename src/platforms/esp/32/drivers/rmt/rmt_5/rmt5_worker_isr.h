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
    // === Availability Flag Pointer ===
    // Pointer to the worker's availability flag (nullptr = slot free, non-null = busy)
    // ISR sets *mAvailableFlag = true when transmission completes
    // Accessed by: ISR (write), main thread (read/write)
    volatile bool* mAvailableFlag;

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

    // Constructor
    RmtWorkerIsrData()
        : mAvailableFlag(nullptr)
        , mChannelId(0xFF)  // Invalid channel ID
        , mWhichHalf(0)
        , mCur(0)
        , mRMT_mem_start(nullptr)
        , mRMT_mem_ptr(nullptr)
        , mPixelData(nullptr)
        , mNumBytes(0)
        , mNibbleLut{}
    {}

    /**
     * Configure ISR data for transmission
     *
     * @param available_flag Pointer to worker's availability flag (ISR signals completion by setting to true)
     * @param channel_id Hardware RMT channel ID
     * @param rmt_mem_start Pointer to start of RMT channel memory
     * @param pixel_data Pointer to pixel data to transmit
     * @param num_bytes Number of bytes to transmit
     * @param nibble_lut Pre-built nibble lookup table
     */
    void config(
        volatile bool* available_flag,
        uint8_t channel_id,
        volatile rmt_item32_t* rmt_mem_start,
        const uint8_t* pixel_data,
        int num_bytes,
        const rmt_nibble_lut_t& nibble_lut
    ) {
        mAvailableFlag = available_flag;
        mChannelId = channel_id;
        mWhichHalf = 0;
        mCur = 0;
        mRMT_mem_start = rmt_mem_start;
        mRMT_mem_ptr = rmt_mem_start;
        mPixelData = pixel_data;
        mNumBytes = num_bytes;

        // Copy nibble lookup table
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 4; j++) {
                mNibbleLut[i][j].val = nibble_lut[i][j].val;
            }
        }
    }

    /**
     * Reset buffer state for new transmission
     * Called before starting transmission
     */
    void resetBufferState() {
        mWhichHalf = 0;
        mCur = 0;
        mRMT_mem_ptr = mRMT_mem_start;
    }
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
