#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "fl/force_inline.h"
#include "fl/slice.h"
#include "rmt5_worker_isr.h"

FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
FL_EXTERN_C_END

namespace fl {

// Forward declaration
class RmtWorker;

/**
 * RmtWorkerIsrMgr - Global ISR data manager for RMT workers
 *
 * Manages a static pool of ISR data structures, one per hardware RMT channel.
 * Workers register to acquire an ISR data slot before transmission and
 * release it upon completion.
 *
 * Architecture:
 * - Static array of ISR data (one per hardware channel)
 * - Workers acquire ISR data pointer during transmission via registerChannel()
 * - ISR accesses data through manager's getIsrData(channel_id)
 * - Registration includes copying pre-built nibble LUT
 */
class RmtWorkerIsrMgr {
public:
    // Get the singleton instance
    static RmtWorkerIsrMgr& getInstance();

    /**
     * Register worker for transmission on specific channel
     * Configures all ISR data fields for transmission
     *
     * @param channel_id Hardware RMT channel ID (0 to SOC_RMT_CHANNELS_PER_GROUP-1)
     * @param worker Pointer to worker requesting registration
     * @param rmt_mem RMT channel memory buffer (span of volatile rmt_item32_t)
     * @param pixel_data Pixel data to transmit (span of const uint8_t)
     * @param nibble_lut Pre-built nibble lookup table (will be copied)
     * @return Const pointer to assigned ISR data, or nullptr if channel occupied
     */
    const RmtWorkerIsrData* registerChannel(
        uint8_t channel_id,
        RmtWorker* worker,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const rmt_nibble_lut_t& nibble_lut
    );

    /**
     * Unregister worker from channel
     * Clears worker pointer and resets ISR data state
     *
     * @param channel_id Hardware RMT channel ID
     */
    void unregisterChannel(uint8_t channel_id);

    /**
     * Get ISR data for specific channel (used by global ISR)
     *
     * @param channel_id Hardware RMT channel ID
     * @return Pointer to ISR data, or nullptr if invalid channel
     */
    RmtWorkerIsrData* getIsrData(uint8_t channel_id);

    /**
     * Check if channel is currently occupied
     *
     * @param channel_id Hardware RMT channel ID
     * @return true if channel has active worker, false otherwise
     */
    bool isChannelOccupied(uint8_t channel_id) const;

    /**
     * Fill next half of RMT buffer (called from ISR context)
     *
     * @param channel_id Hardware RMT channel ID
     */
    static void IRAM_ATTR fillNextHalf(uint8_t channel_id);

    /**
     * Reset buffer state after filling both halves
     * Called before starting transmission
     *
     * @param channel_id Hardware RMT channel ID
     */
    static void resetBufferState(uint8_t channel_id);

    /**
     * Start RMT transmission for channel
     * Uses direct register access for platform-specific initialization
     *
     * @param channel_id Hardware RMT channel ID
     */
    static void IRAM_ATTR tx_start(uint8_t channel_id);

    /**
     * Convert byte to RMT symbols using nibble lookup table
     * Helper function for fillNextHalf()
     *
     * @param byte_val Byte value to convert
     * @param lut Nibble lookup table
     * @param out Output pointer to write 8 RMT items
     */
    static FASTLED_FORCE_INLINE void IRAM_ATTR convertByteToRmt(
        uint8_t byte_val,
        const rmt_nibble_lut_t& lut,
        volatile rmt_item32_t* out
    );

    /**
     * Shared global ISR - processes all channels in one pass
     * Called by ESP-IDF interrupt system
     *
     * @param arg User argument (unused)
     */
    static void IRAM_ATTR sharedGlobalISR(void* arg);

    /**
     * Allocate interrupt for channel
     * Registers worker in global map and allocates shared ISR if needed
     *
     * @param channel_id Hardware RMT channel ID
     * @param worker Pointer to worker
     * @return true if successful, false on error
     */
    bool allocateInterrupt(uint8_t channel_id, RmtWorker* worker);

    /**
     * Deallocate interrupt for channel
     * Unregisters worker from global map
     *
     * @param channel_id Hardware RMT channel ID
     */
    void deallocateInterrupt(uint8_t channel_id);

private:
    // Singleton - private constructor
    RmtWorkerIsrMgr();
    ~RmtWorkerIsrMgr() = default;

    // Delete copy/move constructors
    RmtWorkerIsrMgr(const RmtWorkerIsrMgr&) = delete;
    RmtWorkerIsrMgr& operator=(const RmtWorkerIsrMgr&) = delete;
    RmtWorkerIsrMgr(RmtWorkerIsrMgr&&) = delete;
    RmtWorkerIsrMgr& operator=(RmtWorkerIsrMgr&&) = delete;

    // ISR data pool - one per hardware channel
    // DRAM attribute for ISR access
    RmtWorkerIsrData mIsrDataArray[SOC_RMT_CHANNELS_PER_GROUP];

    // Global worker registry - maps channel ID to active worker
    // Used by ISR to access worker for completion signaling
    RmtWorker* mWorkerRegistry[SOC_RMT_CHANNELS_PER_GROUP];

    // Interrupt allocation tracking per channel
    bool mInterruptAllocated[SOC_RMT_CHANNELS_PER_GROUP];

    // Global interrupt handle (shared by all channels)
    intr_handle_t mGlobalInterruptHandle;

    // Maximum channel number
    uint8_t mMaxChannel;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
