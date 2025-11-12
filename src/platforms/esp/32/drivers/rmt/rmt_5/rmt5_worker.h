#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "ftl/atomic.h"
#include "rmt5_worker_base.h"

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
 * - Manual buffer refill via fillNextHalf() in interrupt context
 * - Direct RMT memory access like RMT4
 * - Two interrupt handling modes (selectable via FASTLED_RMT5_USE_DIRECT_ISR):
 *   1. Direct ISR (default): Low-level ISR with direct register access (faster)
 *   2. Callback API: High-level RMT5 driver callbacks (simpler, higher overhead)
 */
// Forward declare extern "C" function for friend declaration
extern "C" void IRAM_ATTR rmt5_nmi_buffer_refill(void);

class RmtWorker : public IRmtWorkerBase {
public:
    friend class ChannelEngineRMT;
    friend void rmt5_nmi_buffer_refill(void);

    // Memory configuration (matching RMT4)
    #ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
    #define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
    #endif

    #ifndef FASTLED_RMT_MEM_BLOCKS
    #define FASTLED_RMT_MEM_BLOCKS 2
    #endif

    static constexpr int MAX_PULSES = FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS;
    static constexpr int PULSES_PER_FILL = MAX_PULSES / 2;  // Half buffer

    // Worker lifecycle
    RmtWorker();
    ~RmtWorker() override;

    // Initialize hardware channel (called once per worker)
    bool initialize(uint8_t worker_id) override;

    // Check if worker is available for assignment
    // Availability is determined by semaphore count (1 = available, 0 = busy)
    bool isAvailable() const override {
        return uxSemaphoreGetCount(mIsrData.mCompletionSemaphore) > 0;
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

    // Mark worker as unavailable (called by pool under spinlock)
    void markAsUnavailable() override;

    // Buffer refill (interrupt context) - public for NMI access
    void IRAM_ATTR fillNextHalf();

    // ISR-optimized data structure (minimal pointer chasing for interrupt handlers)
    // This struct groups the minimal data needed by ISR for completion signaling
    // NOTE: Worker availability is tracked by semaphore count (1 = available, 0 = busy)
    struct IsrData {
        SemaphoreHandle_t mCompletionSemaphore;  // Completion synchronization + availability flag
    };

private:
    // Allow engine to access mAvailable for synchronized state changes
    friend class ChannelEngineRMT;
    // Hardware resources (persistent)
    rmt_channel_handle_t mChannel;
    uint8_t mChannelId;
    uint8_t mWorkerId;
    intr_handle_t mIntrHandle;
    bool mInterruptAllocated;  // Track if interrupt has been allocated (lazy initialization)

    // Current configuration
    gpio_num_t mCurrentPin;
    int mT1, mT2, mT3;
    uint32_t mResetNs;

    // Pre-calculated RMT symbols
    union rmt_item32_t {
        struct {
            uint32_t duration0 : 15;
            uint32_t level0 : 1;
            uint32_t duration1 : 15;
            uint32_t level1 : 1;
        };
        uint32_t val;
    };

    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // Ping-pong buffer state (like RMT4)
    volatile int mCur;               // Current byte position in pixel data
    volatile uint8_t mWhichHalf;     // Which half of buffer (0 or 1)
    volatile rmt_item32_t* mRMT_mem_start;  // Start of RMT channel memory
    volatile rmt_item32_t* mRMT_mem_ptr;    // Current write pointer in RMT memory

    // ISR-optimized data (grouped for minimal pointer chasing in interrupt handlers)
    IsrData mIsrData;

    // Transmission state
    const uint8_t* mPixelData;        // POINTER ONLY - not owned by worker
    int mNumBytes;                    // Total bytes to transmit

    // ISR handlers (selected via FASTLED_RMT5_USE_DIRECT_ISR)
    static void IRAM_ATTR globalISR(void* arg);  // Direct ISR (used when FASTLED_RMT5_USE_DIRECT_ISR=1)
    static bool IRAM_ATTR onTransDoneCallback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data);  // RMT5 callback (used when FASTLED_RMT5_USE_DIRECT_ISR=0)
    void IRAM_ATTR handleThresholdInterrupt();
    void IRAM_ATTR handleDoneInterrupt();

    // Helper: allocate interrupt (lazy, called from first transmit())
    bool allocateInterrupt();

    // Helper: create RMT channel (called from configure on first use)
    bool createRMTChannel(gpio_num_t pin);

    // Helper: tear down RMT channel and cleanup GPIO (called when switching pins)
    void tearDownRMTChannel(gpio_num_t old_pin);

    // Helper: convert byte to RMT pulses
    void IRAM_ATTR convertByteToRmt(uint8_t byte, volatile rmt_item32_t* pItem);

    // Helper: start RMT transmission
    void IRAM_ATTR tx_start();

    // Helper: extract channel ID from handle
    static uint32_t getChannelIdFromHandle(rmt_channel_handle_t handle);
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
