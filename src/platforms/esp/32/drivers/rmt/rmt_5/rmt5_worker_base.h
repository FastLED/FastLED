#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "ftl/stdint.h"
#include "fl/chipsets/timing_traits.h"

FL_EXTERN_C_BEGIN

#include "driver/gpio.h"

FL_EXTERN_C_END

namespace fl {

/**
 * IRmtWorkerBase - Abstract interface for RMT workers
 *
 * Purpose:
 * - Common interface for all worker types
 * - Enables worker pool management
 */
class IRmtWorkerBase {
public:
    virtual ~IRmtWorkerBase() = default;

    // Worker lifecycle
    virtual bool initialize(uint8_t worker_id) = 0;
    virtual bool isAvailable() const = 0;

    // Configuration
    virtual bool configure(gpio_num_t pin, const ChipsetTiming& timing) = 0;

    // Transmission
    virtual void transmit(const uint8_t* pixel_data, int num_bytes) = 0;
    virtual void waitForCompletion() = 0;

    // Worker lifecycle management
    // Called by pool to mark worker as available after transmission completes
    // This separates "transmission done" (ISR) from "worker available" (pool)
    virtual void markAsAvailable() = 0;

    // Called by pool to mark worker as unavailable when acquired
    // Must be called under pool's spinlock for atomic visibility
    virtual void markAsUnavailable() = 0;

    // Worker info
    virtual uint8_t getWorkerId() const = 0;

    // Worker type identification (for debugging/telemetry)
    enum class WorkerType {
        STANDARD    // RmtWorker - interrupt-driven with ping-pong buffers
    };

    virtual WorkerType getWorkerType() const = 0;

    // Channel status
    virtual bool hasChannel() const = 0;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
