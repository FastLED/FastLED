#pragma once

#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/stdint.h"
#include "fl/namespace.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#ifdef __cplusplus
}
#endif

namespace fl {

/**
 * IRmtWorkerBase - Abstract interface for RMT workers
 *
 * Purpose:
 * - Allows worker pool to manage both double-buffer and one-shot workers
 * - Enables hybrid mode (automatic selection based on strip size)
 * - Common interface for all worker types
 */
class IRmtWorkerBase {
public:
    virtual ~IRmtWorkerBase() = default;

    // Worker lifecycle
    virtual bool initialize(uint8_t worker_id) = 0;
    virtual bool isAvailable() const = 0;

    // Configuration
    virtual bool configure(gpio_num_t pin, int t1, int t2, int t3, uint32_t reset_ns) = 0;

    // Transmission
    virtual void transmit(const uint8_t* pixel_data, int num_bytes) = 0;
    virtual void waitForCompletion() = 0;

    // Worker info
    virtual uint8_t getWorkerId() const = 0;

    // Worker type identification (for debugging/telemetry)
    enum class WorkerType {
        DOUBLE_BUFFER,  // RmtWorker - interrupt-driven double-buffer
        ONE_SHOT        // RmtWorkerOneShot - pre-encoded fire-and-forget
    };

    virtual WorkerType getWorkerType() const = 0;

    // Channel status
    virtual bool hasChannel() const = 0;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
