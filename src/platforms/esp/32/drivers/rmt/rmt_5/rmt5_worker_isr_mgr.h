#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "fl/force_inline.h"
#include "fl/slice.h"
#include "fl/chipsets/led_timing.h"
#include "fl/result.h"
#include "rmt5_worker_isr.h"

FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
#include "esp_attr.h"
FL_EXTERN_C_END

namespace fl {

// Forward declaration
class RmtWorker;

/**
 * Opaque handle for registered RMT ISR channel
 * Wraps channel_id but prevents direct access to ISR data
 */
struct RmtIsrHandle {
    uint8_t channel_id;

    explicit RmtIsrHandle(uint8_t id) : channel_id(id) {}
};

/**
 * Error codes for RMT channel registration
 */
enum class RmtRegisterError : uint8_t {
    INVALID_CHANNEL,        ///< Channel ID out of valid range
    CHANNEL_OCCUPIED,       ///< Channel already in use by another worker
    INTERRUPT_ALLOC_FAILED  ///< Failed to allocate interrupt
};

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
     * Register worker for transmission on specific channel and start transmission
     * Configures all ISR data fields, builds LUT from timing config, fills buffers, and starts hardware transmission
     *
     * @param channel_id Hardware RMT channel ID (0 to SOC_RMT_CHANNELS_PER_GROUP-1)
     * @param completed Pointer to worker's completion flag (ISR signals completion)
     * @param rmt_mem RMT channel memory buffer (span of volatile rmt_item32_t)
     * @param pixel_data Pixel data to transmit (span of const uint8_t)
     * @param timing Chipset timing configuration (T1, T2, T3, RESET in nanoseconds)
     * @return Result containing handle on success, or error code on failure
     */
    virtual Result<RmtIsrHandle, RmtRegisterError> registerChannel(
        uint8_t channel_id,
        volatile bool* completed,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const ChipsetTiming& timing
    ) = 0;

    /**
     * Unregister worker from channel
     * Clears worker pointer and resets ISR data state
     *
     * @param handle Handle returned from registerChannel
     */
    virtual void unregisterChannel(const RmtIsrHandle& handle) = 0;

protected:
    // Protected constructor for singleton pattern
    RmtWorkerIsrMgr() = default;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
