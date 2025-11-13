#pragma once

#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_isr_mgr.h"

namespace fl {
namespace rmt5_threshold {

/**
 * RmtWorkerIsrMgrThreshold - Threshold Variant
 *
 * Classic RMT threshold interrupt approach using RMT peripheral interrupts.
 * - Uses byte-level filling (8 items at a time) with ping-pong buffer strategy
 * - RMT peripheral fires interrupt when buffer reaches 50% empty
 * - Lower CPU overhead but susceptible to interrupt latency
 * - Safety margin: 8 items (1 byte worth)
 */
class RmtWorkerIsrMgrThreshold {
public:
    static RmtWorkerIsrMgrThreshold& getInstance();

    // Public API matching RmtWorkerIsrMgr interface
    Result<RmtIsrHandle, RmtRegisterError> startTransmission(
        uint8_t channel_id,
        volatile bool* completed,
        fl::span<volatile rmt_item32_t> rmt_mem,
        fl::span<const uint8_t> pixel_data,
        const ChipsetTiming& timing
    );

    void stopTransmission(const RmtIsrHandle& handle);
};

// Type alias for dispatcher compatibility
using RmtWorkerIsrMgrImpl = RmtWorkerIsrMgrThreshold;

} // namespace rmt5_threshold
} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
