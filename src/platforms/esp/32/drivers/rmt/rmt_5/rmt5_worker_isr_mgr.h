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
#include "esp_attr.h"
FL_EXTERN_C_END

namespace fl {

// Forward declaration
class RmtWorker;

/**
 * RmtWorkerIsrMgr - Global ISR data manager interface for RMT workers
 *
 * Manages a static pool of ISR data structures, one per hardware RMT channel.
 * Workers register to acquire an ISR data slot before transmission and
 * release it upon completion.
 *
 * Architecture:
 * - Static array of ISR data (one per hardware channel)
 * - Workers acquire ISR data pointer during transmission via registerChannel()
 * - ISR accesses data through internal implementation
 * - Registration includes copying pre-built nibble LUT
 */
class RmtWorkerIsrMgr {
public:
    // Get the singleton instance
    static RmtWorkerIsrMgr& getInstance();

    // Virtual destructor for interface
    virtual ~RmtWorkerIsrMgr() = default;

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
    virtual const RmtWorkerIsrData* registerChannel(
        uint8_t channel_id,
        RmtWorker* worker,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const rmt_nibble_lut_t& nibble_lut
    ) = 0;

    /**
     * Unregister worker from channel
     * Clears worker pointer and resets ISR data state
     *
     * @param channel_id Hardware RMT channel ID
     */
    virtual void unregisterChannel(uint8_t channel_id) = 0;

    /**
     * Start RMT transmission for channel
     * Fills buffers and starts hardware transmission
     *
     * @param channel_id Hardware RMT channel ID
     */
    virtual void startTransmission(uint8_t channel_id) = 0;

protected:
    // Protected constructor for singleton pattern
    RmtWorkerIsrMgr() = default;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
