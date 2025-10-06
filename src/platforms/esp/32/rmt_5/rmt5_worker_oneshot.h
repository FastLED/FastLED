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

#ifdef __cplusplus
}
#endif

namespace fl {

/**
 * RmtWorkerOneShot - One-shot encoding RMT worker (zero-flicker alternative)
 *
 * Strategy:
 * - Pre-encodes ENTIRE LED strip to RMT symbols before transmission
 * - Fire-and-forget transmission (no ISR refill needed)
 * - Zero flicker at cost of 32x memory overhead
 *
 * Memory Cost:
 * - 50 LEDs (150 bytes): 4.8KB symbols (32x overhead)
 * - 100 LEDs (300 bytes): 9.6KB symbols (32x overhead)
 * - 200 LEDs (600 bytes): 19.2KB symbols (32x overhead)
 * - 300 LEDs (900 bytes): 28.8KB symbols (32x overhead)
 *
 * Use Cases:
 * - Small to medium LED counts (<200 LEDs)
 * - Absolute zero-flicker requirement
 * - Abundant RAM available (ESP32-S3: 512KB)
 * - Simplicity preferred over memory efficiency
 *
 * Advantages:
 * - ✅ Absolute zero flicker (pre-encoded buffer)
 * - ✅ No ISR overhead (CPU available for other tasks)
 * - ✅ Simple implementation (no interrupt handling)
 * - ✅ Deterministic timing (no ISR jitter)
 * - ✅ Wi-Fi immune (cannot be interrupted)
 *
 * Disadvantages:
 * - ❌ 32x memory overhead (impractical for large strips)
 * - ❌ Scales poorly with multiple strips
 * - ❌ Pre-encoding latency (slight delay before TX)
 */
class RmtWorkerOneShot : public IRmtWorkerBase {
public:
    friend class RmtWorkerPool;

    // Worker lifecycle
    RmtWorkerOneShot();
    ~RmtWorkerOneShot() override;

    // Initialize hardware channel (called once per worker)
    bool initialize(uint8_t worker_id) override;

    // Check if worker is available for assignment
    bool isAvailable() const override { return mAvailable; }

    // Configuration (called before each transmission)
    bool configure(gpio_num_t pin, int t1, int t2, int t3, uint32_t reset_ns) override;

    // One-shot transmission (pre-encodes entire buffer)
    void transmit(const uint8_t* pixel_data, int num_bytes) override;
    void waitForCompletion() override;

    // Get worker ID
    uint8_t getWorkerId() const override { return mWorkerId; }

    // Get worker type (one-shot)
    WorkerType getWorkerType() const override { return WorkerType::ONE_SHOT; }

    // Check if channel has been created
    bool hasChannel() const override { return mChannel != nullptr; }

private:
    // Hardware resources (persistent)
    rmt_channel_handle_t mChannel;
    rmt_encoder_handle_t mEncoder;  // ESP-IDF bytes encoder
    uint8_t mChannelId;
    uint8_t mWorkerId;

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
    rmt_item32_t mReset;

    // Pre-encoded symbol buffer (OWNED by worker - key difference from double-buffer)
    rmt_item32_t* mEncodedSymbols;
    size_t mEncodedCapacity;    // Current buffer capacity (in symbols)
    size_t mEncodedSize;        // Actual encoded size for current transmission

    // Transmission state
    volatile bool mAvailable;     // Worker available for assignment
    volatile bool mTransmitting;  // Transmission in progress

    // Pre-encode pixel data to RMT symbols
    void preEncode(const uint8_t* pixel_data, int num_bytes);

    // Helper: create RMT channel (called from configure on first use)
    bool createChannel(gpio_num_t pin);

    // Convert byte to 8 RMT items (one per bit)
    void convertByteToRmt(uint8_t byte, rmt_item32_t* out);

    // Completion callback (ISR context)
    static bool IRAM_ATTR onTransDoneCallback(
        rmt_channel_handle_t channel,
        const rmt_tx_done_event_data_t *edata,
        void *user_data
    );

    // Helper: extract channel ID from handle
    static uint32_t getChannelIdFromHandle(rmt_channel_handle_t handle);
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
