/*
  FastLED — Stub ISR Implementation
  ----------------------------------
  Stub/simulation implementation of the cross-platform ISR API.
  Used for testing, host simulation, and platforms without hardware timers.

  License: MIT (FastLED)
*/

// Only compile this implementation for stub/host platform builds
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)

#include "fl/isr.h"
#include "fl/namespace.h"
#include "fl/unique_ptr.h"
#include "fl/atomic.h"

#include <chrono>
#include <thread>

// Simple logging for stub platform (avoid FL_WARN/FL_DBG due to exception issues)
#include <iostream>
#define STUB_LOG(msg) std::cerr << "[fl::isr::stub] " << msg << std::endl

namespace fl {
namespace isr {

// =============================================================================
// Stub Handle Storage
// =============================================================================

struct stub_isr_handle_data {
    fl::unique_ptr<std::thread> timer_thread;  // Thread for timer simulation
    fl::atomic<bool> should_stop;              // Stop flag for thread
    fl::atomic<bool> is_enabled;               // Current enable state
    isr_handler_t user_handler;                // User handler function
    void* user_data;                           // User context
    uint32_t frequency_hz;                     // Timer frequency
    bool is_one_shot;                          // One-shot vs auto-reload
    bool is_timer;                             // true = timer, false = external

    stub_isr_handle_data()
        : should_stop(false)
        , is_enabled(true)
        , user_handler(nullptr)
        , user_data(nullptr)
        , frequency_hz(0)
        , is_one_shot(false)
        , is_timer(false)
    {}

    ~stub_isr_handle_data() {
        if (timer_thread && timer_thread->joinable()) {
            should_stop = true;
            timer_thread->join();
        }
    }
};

// Platform ID for stub
constexpr uint8_t STUB_PLATFORM_ID = 0;

// =============================================================================
// Timer Thread Function
// =============================================================================

static void timer_thread_func(stub_isr_handle_data* handle_data)
{
    if (!handle_data || !handle_data->user_handler) {
        return;
    }

    const uint64_t period_us = 1000000ULL / handle_data->frequency_hz;

    // Use high-resolution clock for accurate timing
    using clock = std::chrono::high_resolution_clock;
    auto next_tick = clock::now() + std::chrono::microseconds(period_us);

    while (!handle_data->should_stop) {
        // Execute handler first (if enabled)
        if (handle_data->is_enabled) {
            // Call user handler
            handle_data->user_handler(handle_data->user_data);

            if (handle_data->is_one_shot) {
                handle_data->is_enabled = false;
                break;
            }
        }

        if (handle_data->should_stop) {
            break;
        }

        // Sleep until next tick using precise timing
        std::this_thread::sleep_until(next_tick);
        next_tick += std::chrono::microseconds(period_us);
    }
}

// =============================================================================
// Stub ISR Implementation Class
// =============================================================================

class StubIsrImpl : public IsrImpl {
public:
    StubIsrImpl() = default;
    ~StubIsrImpl() override = default;

    int attachTimerHandler(const isr_config_t& config, isr_handle_t* out_handle) override {
        if (!config.handler) {
            STUB_LOG("attachTimerHandler: handler is null");
            return -1;  // Invalid parameter
        }

        if (config.frequency_hz == 0) {
            STUB_LOG("attachTimerHandler: frequency_hz is 0");
            return -2;  // Invalid frequency
        }

        // Allocate handle data
        auto handle_data = new stub_isr_handle_data();
        if (!handle_data) {
            STUB_LOG("attachTimerHandler: failed to allocate handle data");
            return -3;  // Out of memory
        }

        handle_data->is_timer = true;
        handle_data->user_handler = config.handler;
        handle_data->user_data = config.user_data;
        handle_data->frequency_hz = config.frequency_hz;
        handle_data->is_one_shot = (config.flags & ISR_FLAG_ONE_SHOT) != 0;

        // Start timer thread (unless manual tick mode)
        if (!(config.flags & ISR_FLAG_MANUAL_TICK)) {
            handle_data->timer_thread = fl::make_unique<std::thread>(timer_thread_func, handle_data);
            if (!handle_data->timer_thread) {
                STUB_LOG("attachTimerHandler: failed to create timer thread");
                delete handle_data;
                return -4;  // Thread creation failed
            }
        }

        STUB_LOG("Stub timer started at " << config.frequency_hz << " Hz");

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = STUB_PLATFORM_ID;
        }

        return 0;  // Success
    }

    int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) override {
        if (!config.handler) {
            STUB_LOG("attachExternalHandler: handler is null");
            return -1;  // Invalid parameter
        }

        // Allocate handle data
        auto handle_data = new stub_isr_handle_data();
        if (!handle_data) {
            STUB_LOG("attachExternalHandler: failed to allocate handle data");
            return -3;  // Out of memory
        }

        handle_data->is_timer = false;
        handle_data->user_handler = config.handler;
        handle_data->user_data = config.user_data;

        STUB_LOG("Stub GPIO interrupt attached on pin " << static_cast<int>(pin));

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = STUB_PLATFORM_ID;
        }

        return 0;  // Success
    }

    int detachHandler(isr_handle_t& handle) override {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("detachHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("detachHandler: null handle data");
            return -1;  // Invalid handle
        }

        // Stop timer thread if running
        if (handle_data->timer_thread) {
            handle_data->should_stop = true;
            if (handle_data->timer_thread->joinable()) {
                handle_data->timer_thread->join();
            }
        }

        delete handle_data;
        handle.platform_handle = nullptr;
        handle.platform_id = 0;

        STUB_LOG("Stub handler detached");
        return 0;  // Success
    }

    int enableHandler(const isr_handle_t& handle) override {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("enableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("enableHandler: null handle data");
            return -1;  // Invalid handle
        }

        handle_data->is_enabled = true;
        STUB_LOG("Stub handler enabled");
        return 0;  // Success
    }

    int disableHandler(const isr_handle_t& handle) override {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("disableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("disableHandler: null handle data");
            return -1;  // Invalid handle
        }

        handle_data->is_enabled = false;
        STUB_LOG("Stub handler disabled");
        return 0;  // Success
    }

    bool isHandlerEnabled(const isr_handle_t& handle) override {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            return false;
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            return false;
        }

        return handle_data->is_enabled;
    }

    const char* getErrorString(int error_code) override {
        switch (error_code) {
            case 0: return "Success";
            case -1: return "Invalid parameter";
            case -2: return "Invalid frequency";
            case -3: return "Out of memory";
            case -4: return "Thread creation failed";
            default: return "Unknown error";
        }
    }

    const char* getPlatformName() override {
        return "Stub";
    }

    uint32_t getMaxTimerFrequency() override {
        return 0;  // Unlimited in simulation
    }

    uint32_t getMinTimerFrequency() override {
        return 1;  // 1 Hz
    }

    uint8_t getMaxPriority() override {
        return 1;  // No priority in stub
    }

    bool requiresAssemblyHandler(uint8_t priority) override {
        (void)priority;
        return false;  // Stub never requires assembly
    }
};

// =============================================================================
// Strong Symbol Factory Function (overrides weak default)
// =============================================================================

/**
 * Strong symbol that overrides the weak default in isr_null.cpp.
 * Returns the stub-specific implementation.
 */
IsrImpl& IsrImpl::get_instance() {
    static StubIsrImpl stub_impl;
    return stub_impl;
}

} // namespace isr
} // namespace fl

#endif // STUB_PLATFORM || FASTLED_STUB_IMPL
