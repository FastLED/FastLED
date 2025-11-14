#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

// RMT5 ISR Manager Dispatcher
//
// This file dispatches to the appropriate ISR manager implementation
// based on the FASTLED_RMT5_USE_TIMER_ISR define from common.h.
//
// Both implementations are compiled, but only one is used at runtime
// via compile-time selection. This allows the build system to remain
// simple while avoiding #include "*.cpp" anti-patterns.

#include "common.h"
#include "rmt5_worker_isr_mgr.h"

// Include both variant headers
#include "rmt5_worker_isr_mgr_timer.h"
#include "rmt5_worker_isr_mgr_threshold.h"

namespace fl {

// Select the active implementation via typedef based on compile-time define
#if FASTLED_RMT5_USE_TIMER_ISR
// Timer mode: High-frequency GPTimer interrupts with nibble-level filling
using ActiveIsrMgrImpl = rmt5_timer::RmtWorkerIsrMgrImpl;
#else
// Threshold mode: RMT peripheral threshold interrupts with byte-level filling
using ActiveIsrMgrImpl = rmt5_threshold::RmtWorkerIsrMgrImpl;
#endif

// API binding functions forward to the active implementation
Result<RmtIsrHandle, RmtRegisterError> RmtWorkerIsrMgr::startTransmission(
    uint8_t channel_id,
    volatile bool* completed,
    fl::span<volatile rmt_item32_t> rmt_mem,
    fl::span<const uint8_t> pixel_data,
    const ChipsetTiming& timing,
    rmt_channel_handle_t channel,
    rmt_encoder_handle_t copy_encoder
) {
    return ActiveIsrMgrImpl::getInstance().startTransmission(
        channel_id, completed, rmt_mem, pixel_data, timing, channel, copy_encoder
    );
}

void RmtWorkerIsrMgr::stopTransmission(const RmtIsrHandle& handle) {
    ActiveIsrMgrImpl::getInstance().stopTransmission(handle);
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
