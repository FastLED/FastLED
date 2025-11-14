#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "ftl/atomic.h"
#include "rmt5_worker_base.h"
#include "rmt5_worker_isr_mgr.h"

FL_EXTERN_C_BEGIN

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "soc/rmt_struct.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "esp_intr_alloc.h"

FL_EXTERN_C_END

namespace fl {

// Forward declarations
class ChannelEngineRMT;

/**
 * RmtWorker - Low-level RMT channel worker with ping-pong buffers
 *
 * Architecture:
 * - Owns persistent RMT hardware channel and buffer state
 * - Does NOT own pixel data - uses pointers to controller-owned buffers
 * - Supports reconfiguration for different pins/timings (worker pooling)
 * - Implements RMT4-style interrupt-driven buffer refill
 *
 * Implementation:
 * - Ping-pong buffer transmission with interrupt-driven refill
 * - Buffer refill handled by ISR manager in interrupt context
 * - Direct RMT memory access like RMT4
 * - Two interrupt handling modes (selectable via FASTLED_RMT5_USE_DIRECT_ISR):
 *   1. Direct ISR (default): Low-level ISR with direct register access (faster)
 *   2. Callback API: High-level RMT5 driver callbacks (simpler, higher overhead)
 */


class RmtWorker : public IRmtWorkerBase {
public:
    friend class ChannelEngineRMT;
    friend void rmt5_nmi_buffer_refill(void);

    // Memory configuration (matching RMT4)
    // Memory configuration now in common.h
    static constexpr int MAX_PULSES = FASTLED_RMT5_MAX_PULSES;
    static constexpr int PULSES_PER_FILL = FASTLED_RMT5_PULSES_PER_FILL;

    // Worker lifecycle
    RmtWorker();
    ~RmtWorker() override;

    // Initialize hardware channel (called once per worker)
    bool initialize(uint8_t worker_id) override;

    // Check if worker is available for assignment
    // Availability is tracked via volatile bool (set by ISR on completion)
    bool isAvailable() const override {
        return mAvailable;
    }

    // Configuration (called before each transmission)
    bool configure(gpio_num_t pin, const ChipsetTiming& timing) override;

    // Transmission control
    void transmit(const uint8_t* pixel_data, int num_bytes) override;
    void waitForCompletion() override;

    // Get worker ID
    uint8_t getWorkerId() const override { return mWorkerId; }

    // Get worker type
    WorkerType getWorkerType() const override { return WorkerType::STANDARD; }

    // Check if channel has been created
    bool hasChannel() const override { return mChannel != nullptr; }

    // Mark worker as available (called by pool under spinlock)
    void markAsAvailable() override;

private:
    // Allow engine to access ISR data for synchronized state changes
    friend class ChannelEngineRMT;
    // Hardware resources (persistent)
    rmt_channel_handle_t mChannel;
    rmt_encoder_handle_t mCopyEncoder;  // Copy encoder for hybrid API + timer ISR approach
    uint8_t mWorkerId;
    uint8_t mChannelId;  // Hardware channel ID (stored separately from ISR data)

    // Current configuration
    gpio_num_t mCurrentPin;
    ChipsetTiming mTiming;

private:
    // Availability flag (volatile for ISR/main thread communication)
    // Set to true by ISR when transmission completes
    // Set to false by main thread when worker is assigned
    volatile bool mAvailable;

    // ISR handle result (acquired from ISR manager during transmission)
    // - Contains opaque handle to channel registration on success
    // - Checked via .ok() method - type system forces error handling
    // - Managed by startTransmission() / stopTransmission()
    Result<RmtIsrHandle, RmtRegisterError> mHandleResult;

    // Helper: create RMT channel (called from configure on first use)
    bool createRMTChannel(gpio_num_t pin);

    // Helper: tear down RMT channel and cleanup GPIO (called when switching pins)
    void tearDownRMTChannel(gpio_num_t old_pin);

    // Helper: extract channel ID from handle
    static uint32_t getChannelIdFromHandle(rmt_channel_handle_t handle);
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
