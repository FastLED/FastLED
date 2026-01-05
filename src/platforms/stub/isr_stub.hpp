/*
  FastLED — Stub/WASM ISR Implementation
  ---------------------------------------
  Host-based implementation of the cross-platform ISR API using POSIX threads.
  Used for testing, host simulation, WASM, and platforms without hardware timers.

  License: MIT (FastLED)
*/

#pragma once

// Only compile this implementation for stub/host/WASM platform builds
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)

#include "fl/isr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/atomic.h"
#include "fl/stl/vector.h"
#include "fl/stl/mutex.h"
#include "fl/stl/condition_variable.h"
#include "fl/numeric_limits.h"

#include <chrono>
#include <thread>

// Simple logging for stub/WASM platforms (avoid FL_WARN/FL_DBG due to exception issues)
#include <iostream>
#if defined(FL_IS_WASM)
    #define STUB_LOG(msg) std::cerr << "[fl::isr::wasm] " << msg << std::endl  // okay std namespace
#else
    #define STUB_LOG(msg) std::cerr << "[fl::isr::stub] " << msg << std::endl  // okay std namespace
#endif

namespace fl {
namespace isr {

// =============================================================================
// Stub Handle Storage
// =============================================================================

struct stub_isr_handle_data {
    fl::atomic<bool> mIsEnabled;               // Current enable state
    isr_handler_t mUserHandler;                // User handler function
    void* mUserData;                           // User context
    uint32_t mFrequencyHz;                     // Timer frequency
    uint64_t mNextTickUs;                      // Next scheduled tick (microseconds since epoch)
    bool mIsOneShot;                           // One-shot vs auto-reload
    bool mIsTimer;                             // true = timer, false = external
    uint32_t mHandleId;                        // Unique ID for this handler

    stub_isr_handle_data()
        : mIsEnabled(true)
        , mUserHandler(nullptr)
        , mUserData(nullptr)
        , mFrequencyHz(0)
        , mNextTickUs(0)
        , mIsOneShot(false)
        , mIsTimer(false)
        , mHandleId(0)
    {}
};

// Platform ID for stub/WASM
#if defined(FL_IS_WASM)
    constexpr uint8_t STUB_PLATFORM_ID = 200;  // WASM platform
#else
    constexpr uint8_t STUB_PLATFORM_ID = 0;    // Stub platform
#endif

// =============================================================================
// Global Timer Thread Manager
// =============================================================================

class TimerThreadManager {
public:
    static TimerThreadManager& instance() {
        static TimerThreadManager inst;
        return inst;
    }

    void add_handler(stub_isr_handle_data* handler) {
        fl::unique_lock<fl::mutex> lock(mMutex);

        // Assign unique ID
        handler->mHandleId = mNextHandleId++;

        // Calculate initial tick time
        auto now = get_time_us();
        uint64_t period_us = handler->mFrequencyHz > 0 ? (1000000ULL / handler->mFrequencyHz) : 0;
        handler->mNextTickUs = now + period_us;

        mHandlers.push_back(handler);

        // Start timer thread if not already running
        if (!mTimerThread) {
            mShouldStop = false;
            mTimerThread = fl::make_unique<std::thread>(&TimerThreadManager::timer_thread_func, this);  // okay std namespace
        } else {
            // Wake up the timer thread to process the new handler
            mCondVar.notify_one();
        }
    }

    void reschedule_handler(stub_isr_handle_data* handler) {
        fl::unique_lock<fl::mutex> lock(mMutex);
        uint64_t now = get_time_us();
        uint64_t period_us = handler->mFrequencyHz > 0 ? (1000000ULL / handler->mFrequencyHz) : 0;
        handler->mNextTickUs = now + period_us;
        // Wake up the timer thread to process the rescheduled handler
        mCondVar.notify_one();
    }

    void remove_handler(stub_isr_handle_data* handler) {
        fl::unique_lock<fl::mutex> lock(mMutex);

        // Remove from vector
        for (size_t i = 0; i < mHandlers.size(); ++i) {
            if (mHandlers[i] == handler) {
                mHandlers.erase(mHandlers.begin() + i);
                break;
            }
        }

        // Stop thread if no more handlers
        if (mHandlers.empty() && mTimerThread) {
            mShouldStop = true;
            mCondVar.notify_one();  // Wake the thread so it can exit
            lock.unlock();  // Unlock before joining
            if (mTimerThread->joinable()) {
                mTimerThread->join();
            }
            mTimerThread.reset();
        }
    }

    ~TimerThreadManager() {
        if (mTimerThread) {
            {
                fl::unique_lock<fl::mutex> lock(mMutex);
                mShouldStop = true;
                mCondVar.notify_one();  // Wake the thread so it can exit
            }
            if (mTimerThread->joinable()) {
                mTimerThread->join();
            }
        }
    }

private:
    TimerThreadManager() : mShouldStop(false), mNextHandleId(1) {}
    TimerThreadManager(const TimerThreadManager&) = delete;
    TimerThreadManager& operator=(const TimerThreadManager&) = delete;

    static uint64_t get_time_us() {
        using clock = std::chrono::high_resolution_clock;  // okay std namespace
        auto now = clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();  // okay std namespace
    }

    void timer_thread_func() {
        fl::unique_lock<fl::mutex> lock(mMutex);

        while (!mShouldStop) {
            uint64_t now = get_time_us();
            uint64_t next_wake = fl::numeric_limits<uint64_t>::max();  // Initialize to max value
            bool has_enabled_handlers = false;

            // Execute all handlers that are due
            for (auto* handler : mHandlers) {
                if (!handler->mIsEnabled || !handler->mIsTimer) {
                    continue;
                }

                has_enabled_handlers = true;

                // Check if this handler should fire
                if (now >= handler->mNextTickUs) {
                    // Execute handler
                    if (handler->mUserHandler) {
                        handler->mUserHandler(handler->mUserData);
                    }

                    // Schedule next tick or disable if one-shot
                    if (handler->mIsOneShot) {
                        handler->mIsEnabled = false;
                    } else {
                        uint64_t period_us = 1000000ULL / handler->mFrequencyHz;
                        handler->mNextTickUs = now + period_us;
                    }
                }

                // Track earliest next tick for sleep duration
                if (handler->mIsEnabled && handler->mNextTickUs < next_wake) {
                    next_wake = handler->mNextTickUs;
                }
            }

            // Sleep until next handler should fire (or until notified)
            if (has_enabled_handlers && next_wake > now) {
                // Wait for exact duration until next handler fires
                // Will wake immediately if notify_one() is called (handler added/removed/rescheduled)
                uint64_t sleep_us = next_wake - now;
                mCondVar.wait_for(lock, std::chrono::microseconds(sleep_us));  // okay std namespace
            } else if (!has_enabled_handlers) {
                // No enabled handlers - wait indefinitely until notified (handler added/rescheduled)
                mCondVar.wait(lock);  // okay std namespace
            }
        }
    }

    fl::mutex mMutex;
    fl::condition_variable mCondVar;
    fl::vector<stub_isr_handle_data*> mHandlers;
    fl::unique_ptr<std::thread> mTimerThread;  // okay std namespace
    fl::atomic<bool> mShouldStop;
    uint32_t mNextHandleId;
};

// =============================================================================
// Stub ISR Implementation (Free Functions)
// =============================================================================

inline int stub_attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
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

        // Add to global timer thread manager (unless manual tick mode)
        if (!(config.flags & ISR_FLAG_MANUAL_TICK)) {
            TimerThreadManager::instance().add_handler(handle_data);
        }

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = STUB_PLATFORM_ID;
        }

        return 0;  // Success
}

inline int stub_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
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

        //STUB_LOG("GPIO interrupt attached on pin " << static_cast<int>(pin));

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = STUB_PLATFORM_ID;
        }

        return 0;  // Success
}

inline int stub_detach_handler(isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("detachHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("detachHandler: null handle data");
            return -1;  // Invalid handle
        }

        // Remove from global timer thread manager if it's a timer
        if (handle_data->mIsTimer) {
            TimerThreadManager::instance().remove_handler(handle_data);
        }

        delete handle_data;
        handle.platform_handle = nullptr;
        handle.platform_id = 0;

        //STUB_LOG("Handler detached");
        return 0;  // Success
}

inline int stub_enable_handler(isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            STUB_LOG("enableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            STUB_LOG("enableHandler: null handle data");
            return -1;  // Invalid handle
        }

        // Enable first, then reschedule
        handle_data->mIsEnabled = true;

        // Reschedule the handler to start from current time
        if (handle_data->mIsTimer) {
            TimerThreadManager::instance().reschedule_handler(handle_data);
        }

        STUB_LOG("Handler enabled");
        return 0;  // Success
}

inline int stub_disable_handler(isr_handle_t& handle) {
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
        //STUB_LOG("Handler disabled");
        return 0;  // Success
}

inline bool stub_is_handler_enabled(const isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != STUB_PLATFORM_ID) {
            return false;
        }

        stub_isr_handle_data* handle_data = static_cast<stub_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            return false;
        }

        return handle_data->mIsEnabled;
}

inline const char* stub_get_error_string(int error_code) {
        switch (error_code) {
            case 0: return "Success";
            case -1: return "Invalid parameter";
            case -2: return "Invalid frequency";
            case -3: return "Out of memory";
            case -4: return "Thread creation failed";
            default: return "Unknown error";
        }
}

inline const char* stub_get_platform_name() {
#if defined(FL_IS_WASM)
    return "WASM";
#else
    return "Stub";
#endif
}

inline uint32_t stub_get_max_timer_frequency() {
    return 0;  // Unlimited in host-based simulation
}

inline uint32_t stub_get_min_timer_frequency() {
    return 1;  // 1 Hz
}

inline uint8_t stub_get_max_priority() {
    return 1;  // No priority in host-based platforms
}

inline bool stub_requires_assembly_handler(uint8_t priority) {
    (void)priority;
    return false;  // Host-based platforms never require assembly
}

} // namespace isr

// fl::isr::platform namespace wrappers (call fl::isr:: functions)
namespace isr {
namespace platform {

inline int attach_timer_handler(const isr_config_t& config, isr_handle_t* handle) {
    return stub_attach_timer_handler(config, handle);
}

inline int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
    return stub_attach_external_handler(pin, config, handle);
}

inline int detach_handler(isr_handle_t& handle) {
    return stub_detach_handler(handle);
}

inline int enable_handler(isr_handle_t& handle) {
    return stub_enable_handler(handle);
}

inline int disable_handler(isr_handle_t& handle) {
    return stub_disable_handler(handle);
}

inline bool is_handler_enabled(const isr_handle_t& handle) {
    return stub_is_handler_enabled(handle);
}

inline const char* get_error_string(int error_code) {
    return stub_get_error_string(error_code);
}

inline const char* get_platform_name() {
    return stub_get_platform_name();
}

inline uint32_t get_max_timer_frequency() {
    return stub_get_max_timer_frequency();
}

inline uint32_t get_min_timer_frequency() {
    return stub_get_min_timer_frequency();
}

inline uint8_t get_max_priority() {
    return stub_get_max_priority();
}

inline bool requires_assembly_handler(uint8_t priority) {
    return stub_requires_assembly_handler(priority);
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// No-op for stub/host platform
inline void interruptsDisable() {
    // No-op: stub platform doesn't have hardware interrupts
}

/// No-op for stub/host platform
inline void interruptsEnable() {
    // No-op: stub platform doesn't have hardware interrupts
}

} // namespace fl

#endif // STUB_PLATFORM || FASTLED_STUB_IMPL || FL_IS_WASM
