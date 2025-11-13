#pragma once

#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_isr_mgr.h"

namespace fl {
namespace rmt5_timer {

/**
 * RmtWorkerIsrMgrTimer - Timer Variant
 *
 * Timer-driven interrupt approach using high-frequency GPTimer.
 * - Uses nibble-level filling (4 items at a time) for maximum buffer utilization
 * - Timer fires every 0.5µs unconditionally, checking all active channels
 * - Higher CPU overhead but more resistant to timing jitter
 * - Safety margin: 4 items (1 nibble worth)
 */
class RmtWorkerIsrMgrTimer {
public:
    static RmtWorkerIsrMgrTimer& getInstance();

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
using RmtWorkerIsrMgrImpl = RmtWorkerIsrMgrTimer;

} // namespace rmt5_timer
} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
