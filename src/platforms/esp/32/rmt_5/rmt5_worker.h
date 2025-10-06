#pragma once

#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/namespace.h"
#include "rmt5_worker_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "soc/rmt_struct.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_intr_alloc.h"

#ifdef __cplusplus
}
#endif

namespace fl {

// Forward declarations
class RmtWorkerPool;

/**
 * RmtWorker - Low-level RMT channel worker with double-buffered ping-pong
 *
 * Architecture:
 * - Owns persistent RMT hardware channel and double-buffer state
 * - Does NOT own pixel data - uses pointers to controller-owned buffers
 * - Supports reconfiguration for different pins/timings (worker pooling)
 * - Implements RMT4-style interrupt-driven buffer refill
 *
 * Phase 1 Implementation:
 * - Double-buffered transmission with Level 3 ISR
 * - Manual buffer refill via fillNextHalf() in interrupt context
 * - Direct RMT memory access like RMT4
 */
class RmtWorker : public IRmtWorkerBase {
public:
    friend class RmtWorkerPool;

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
    bool isAvailable() const override { return mAvailable; }

    // Configuration (called before each transmission)
    bool configure(gpio_num_t pin, int t1, int t2, int t3, uint32_t reset_ns) override;

    // Transmission control
    void transmit(const uint8_t* pixel_data, int num_bytes) override;
    void waitForCompletion() override;

    // Get worker ID
    uint8_t getWorkerId() const override { return mWorkerId; }

    // Get worker type (double-buffer)
    WorkerType getWorkerType() const override { return WorkerType::DOUBLE_BUFFER; }

    // Check if channel has been created
    bool hasChannel() const override { return mChannel != nullptr; }

private:
    // Hardware resources (persistent)
    rmt_channel_handle_t mChannel;
    uint8_t mChannelId;
    uint8_t mWorkerId;
    intr_handle_t mIntrHandle;

    // Current configuration
    gpio_num_t mCurrentPin;
    int mT1, mT2, mT3;
    uint32_t mResetNs;

    // Pre-calculated RMT symbols
    union rmt_item32_t {
        struct {
            uint16_t duration0 : 15;
            uint16_t level0 : 1;
            uint16_t duration1 : 15;
            uint16_t level1 : 1;
        };
        uint32_t val;
    };

    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // Double buffer state (like RMT4)
    volatile int mCur;               // Current byte position in pixel data
    volatile uint8_t mWhichHalf;     // Which half of buffer (0 or 1)
    volatile rmt_item32_t* mRMT_mem_start;  // Start of RMT channel memory
    volatile rmt_item32_t* mRMT_mem_ptr;    // Current write pointer in RMT memory

    // Transmission state
    volatile bool mAvailable;        // Worker available for assignment
    volatile bool mTransmitting;     // Transmission in progress
    const uint8_t* mPixelData;       // POINTER ONLY - not owned by worker
    int mNumBytes;                   // Total bytes to transmit

    // Spinlock for ISR synchronization
    static portMUX_TYPE sRmtSpinlock;

    // Double buffer refill (interrupt context)
    void IRAM_ATTR fillNextHalf();

    // ISR handlers
    static void IRAM_ATTR globalISR(void* arg);  // Direct ISR (currently unused)
    static bool IRAM_ATTR onTransDoneCallback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data);  // RMT5 callback
    void IRAM_ATTR handleThresholdInterrupt();
    void IRAM_ATTR handleDoneInterrupt();

    // Helper: create RMT channel (called from configure on first use)
    bool createChannel(gpio_num_t pin);

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
