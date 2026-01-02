/*
  FastLED — Stub ISR Implementation
  ----------------------------------
  Stub/simulation implementation of the cross-platform ISR API.
  Used for testing, host simulation, and platforms without hardware timers.

  License: MIT (FastLED)
*/

#pragma once

// Only compile this implementation for stub/host platform builds
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)

#include "fl/isr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/atomic.h"

#include <chrono>
#include <thread>

// Simple logging for stub platform (avoid FL_WARN/FL_DBG due to exception issues)
#include <iostream>
#define STUB_LOG(msg) std::cerr << "[fl::isr::stub] " << msg << std::endl  // okay std namespace

namespace fl {
namespace isr {

// =============================================================================
// Stub Handle Storage
// =============================================================================

struct stub_isr_handle_data {
    fl::unique_ptr<std::thread> mTimerThread;  // Thread for timer simulation  // okay std namespace
    fl::atomic<bool> mShouldStop;              // Stop flag for thread
    fl::atomic<bool> mIsEnabled;               // Current enable state
    isr_handler_t mUserHandler;                // User handler function
    void* mUserData;                           // User context
    uint32_t mFrequencyHz;                     // Timer frequency
    bool mIsOneShot;                           // One-shot vs auto-reload
    bool mIsTimer;                             // true = timer, false = external

    stub_isr_handle_data()
        : mShouldStop(false)
        , mIsEnabled(true)
        , mUserHandler(nullptr)
        , mUserData(nullptr)
        , mFrequencyHz(0)
        , mIsOneShot(false)
        , mIsTimer(false)
    {}

    ~stub_isr_handle_data() {
        if (mTimerThread && mTimerThread->joinable()) {
            mShouldStop = true;
            mTimerThread->join();
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
    if (!handle_data || !handle_data->mUserHandler) {
        return;
    }

    const uint64_t period_us = 1000000ULL / handle_data->mFrequencyHz;

    // Use high-resolution clock for accurate timing
    using clock = std::chrono::high_resolution_clock;  // okay std namespace
    auto next_tick = clock::now() + std::chrono::microseconds(period_us);  // okay std namespace

    while (!handle_data->mShouldStop) {
        // Execute handler first (if enabled)
        if (handle_data->mIsEnabled) {
            // Call user handler
            handle_data->mUserHandler(handle_data->mUserData);

            if (handle_data->mIsOneShot) {
                handle_data->mIsEnabled = false;
                break;
            }
        }

        if (handle_data->mShouldStop) {
            break;
        }

        // Sleep until next tick using precise timing
        std::this_thread::sleep_until(next_tick);  // okay std namespace
        next_tick += std::chrono::microseconds(period_us);  // okay std namespace
    }
}

// =============================================================================
// Stub ISR Implementation (Free Functions)
// =============================================================================

int stub_attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
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

        handle_data->mIsTimer = true;
        handle_data->mUserHandler = config.handler;
        handle_data->mUserData = config.user_data;
        handle_data->mFrequencyHz = config.frequency_hz;
        handle_data->mIsOneShot = (config.flags & ISR_FLAG_ONE_SHOT) != 0;

        // Start timer thread (unless manual tick mode)
        if (!(config.flags & ISR_FLAG_MANUAL_TICK)) {
            handle_data->mTimerThread = fl::make_unique<std::thread>(timer_thread_func, handle_data);  // okay std namespace
            if (!handle_data->mTimerThread) {
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

int stub_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
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

        handle_data->mIsTimer = false;
        handle_data->mUserHandler = config.handler;
        handle_data->mUserData = config.user_data;

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

int stub_detach_handler(isr_handle_t& handle) {
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
        if (handle_data->mTimerThread) {
            handle_data->mShouldStop = true;
            if (handle_data->mTimerThread->joinable()) {
                handle_data->mTimerThread->join();
            }
        }

        delete handle_data;
        handle.platform_handle = nullptr;
        handle.platform_id = 0;

        STUB_LOG("Stub handler detached");
        return 0;  // Success
}

int stub_enable_handler(isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("enableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("enableHandler: null handle data");
            return -1;  // Invalid handle
        }

        handle_data->mIsEnabled = true;
        STUB_LOG("Stub handler enabled");
        return 0;  // Success
}

int stub_disable_handler(isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("disableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("disableHandler: null handle data");
            return -1;  // Invalid handle
        }

        handle_data->mIsEnabled = false;
        STUB_LOG("Stub handler disabled");
        return 0;  // Success
}

bool stub_is_handler_enabled(const isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            return false;
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            return false;
        }

        return handle_data->mIsEnabled;
}

const char* stub_get_error_string(int error_code) {
        switch (error_code) {
            case 0: return "Success";
            case -1: return "Invalid parameter";
            case -2: return "Invalid frequency";
            case -3: return "Out of memory";
            case -4: return "Thread creation failed";
            default: return "Unknown error";
        }
}

const char* stub_get_platform_name() {
    return "Stub";
}

uint32_t stub_get_max_timer_frequency() {
    return 0;  // Unlimited in simulation
}

uint32_t stub_get_min_timer_frequency() {
    return 1;  // 1 Hz
}

uint8_t stub_get_max_priority() {
    return 1;  // No priority in stub
}

bool stub_requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;  // Stub never requires assembly
}

} // namespace isr

// fl::isr::platform namespace wrappers (call fl::isr:: functions)
namespace isr {
namespace platform {

int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) {
    return stub_attach_timer_handler(config, handle);
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return stub_attach_external_handler(pin, config, handle);
}

int detach_handler(isr_handle_t& handle) {
    return stub_detach_handler(handle);
}

int enable_handler(isr_handle_t& handle) {
    return stub_enable_handler(handle);
}

int disable_handler(isr_handle_t& handle) {
    return stub_disable_handler(handle);
}

bool is_handler_enabled(const isr_handle_t& handle) {
    return stub_is_handler_enabled(handle);
}

const char* get_error_string(int error_code) {
    return stub_get_error_string(error_code);
}

const char* get_platform_name() {
    return stub_get_platform_name();
}

uint32_t get_max_timer_frequency() {
    return stub_get_max_timer_frequency();
}

uint32_t get_min_timer_frequency() {
    return stub_get_min_timer_frequency();
}

uint8_t get_max_priority() {
    return stub_get_max_priority();
}

bool requires_assembly_handler(uint8_t priority) {
    return stub_requires_assembly_handler(priority);
}

} // namespace platform
} // namespace isr
} // namespace fl

#endif // STUB_PLATFORM || FASTLED_STUB_IMPL
