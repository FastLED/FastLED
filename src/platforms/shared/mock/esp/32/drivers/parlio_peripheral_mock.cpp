/// @file parlio_peripheral_mock.cpp
/// @brief Mock PARLIO peripheral implementation for unit testing

#include "parlio_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"

#ifdef ARDUINO
#include <Arduino.h>  // For micros() and delay() on Arduino platforms
#else
#include "platforms/stub/time_stub.h"  // For micros() and delay() on host tests
#endif

namespace fl {
namespace detail {

//=============================================================================
// Singleton Instance (for test access)
//=============================================================================

// Global pointer to the last created mock instance (for getParlioMockInstance())
static ParlioPeripheralMock* g_mock_instance = nullptr;

ParlioPeripheralMock* getParlioMockInstance() {
    return g_mock_instance;
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

ParlioPeripheralMock::ParlioPeripheralMock()
    : mInitialized(false),
      mEnabled(false),
      mTransmitting(false),
      mTransmitCount(0),
      mConfig(),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mTransmitDelayUs(0),
      mShouldFailTransmit(false),
      mHistory(),
      mPendingTransmissions(0) {
    // Register this instance as the global singleton (for test access)
    g_mock_instance = this;
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool ParlioPeripheralMock::initialize(const ParlioPeripheralConfig& config) {
    if (mInitialized) {
        FL_WARN("ParlioPeripheralMock: Already initialized");
        return false;
    }

    // Validate config
    if (config.data_width == 0 || config.data_width > 16) {
        FL_WARN("ParlioPeripheralMock: Invalid data width: " << config.data_width);
        return false;
    }

    // Store configuration
    mConfig = config;
    mInitialized = true;

    return true;
}

bool ParlioPeripheralMock::enable() {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot enable - not initialized");
        return false;
    }

    mEnabled = true;
    return true;
}

bool ParlioPeripheralMock::disable() {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot disable - not initialized");
        return false;
    }

    mEnabled = false;
    return true;
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool ParlioPeripheralMock::transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not initialized");
        return false;
    }

    if (!mEnabled) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not enabled");
        return false;
    }

    // Check for injected failure
    if (mShouldFailTransmit) {
        return false;
    }

    // Calculate buffer size in bytes
    size_t byte_count = (bit_count + 7) / 8;

    // Capture transmission data (copy buffer for later inspection)
    TransmissionRecord record;
    record.buffer_copy.resize(byte_count);
    fl::memcpy(record.buffer_copy.data(), buffer, byte_count);
    record.bit_count = bit_count;
    record.idle_value = idle_value;
    record.timestamp_us = micros();

    // Store in history
    mHistory.push_back(fl::move(record));

    // Update state
    mTransmitCount++;
    mTransmitting = true;
    mPendingTransmissions++;

    return true;
}

bool ParlioPeripheralMock::waitAllDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot wait - not initialized");
        return false;
    }

    // Simulate instant completion (no pending transmissions)
    if (mPendingTransmissions == 0) {
        mTransmitting = false;
        return true;
    }

    // Simulate timeout (for testing timeout paths)
    if (timeout_ms == 0) {
        // Non-blocking poll - return false if still pending
        return false;
    }

    // For non-zero timeout, simulate completion after delay
    // (In real tests, simulateTransmitComplete() is called explicitly)
    // This is a fallback for simple tests that don't need precise timing control
    if (mTransmitDelayUs > 0) {
        uint32_t start_us = micros();
        uint32_t timeout_us = timeout_ms * 1000;

        while (mPendingTransmissions > 0 && (micros() - start_us) < timeout_us) {
            // Busy wait (simulation)
        }

        if (mPendingTransmissions > 0) {
            return false;  // Timeout
        }
    }

    mTransmitting = false;
    return true;
}

//=============================================================================
// ISR Callback Registration
//=============================================================================

bool ParlioPeripheralMock::registerTxDoneCallback(void* callback, void* user_ctx) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot register callback - not initialized");
        return false;
    }

    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

uint8_t* ParlioPeripheralMock::allocateDmaBuffer(size_t size) {
    // Round up to 64-byte multiple (same as real implementation)
    size_t aligned_size = ((size + 63) / 64) * 64;

    // Allocate regular heap memory (no DMA requirement on host)
    // Use aligned_alloc for 64-byte alignment (matches real hardware)
    void* buffer = nullptr;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use _aligned_malloc
    buffer = _aligned_malloc(aligned_size, 64);
#else
    // POSIX: Use aligned_alloc
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<uint8_t*>(buffer);
}

void ParlioPeripheralMock::freeDmaBuffer(uint8_t* buffer) {
    if (buffer != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

void ParlioPeripheralMock::delay(uint32_t ms) {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    ::delay(static_cast<int>(ms));
}

//=============================================================================
// Task Management
//=============================================================================

task_handle_t ParlioPeripheralMock::createTask(const TaskConfig& config) {
    // Mock implementation - no actual task creation
    // Return a non-null synthetic handle to indicate success
    // Use a unique value by encoding the task count in the pointer

    // Suppress unused parameter warning - config is intentionally ignored in mock
    (void)config;

    task_handle_t handle = reinterpret_cast<task_handle_t>(
        static_cast<uintptr_t>(0xDEAD0000 + mMockTasks.size())
    );

    mMockTasks.push_back(handle);
    return handle;
}

void ParlioPeripheralMock::deleteTask(task_handle_t task_handle) {
    if (task_handle == nullptr) {
        return;
    }

    // Remove from tracking vector
    auto it = fl::find(mMockTasks.begin(), mMockTasks.end(), task_handle);
    if (it != mMockTasks.end()) {
        mMockTasks.erase(it);
    }
}

void ParlioPeripheralMock::deleteCurrentTask() {
    // Mock implementation for self-deleting tasks
    // In a real implementation, this would terminate the current thread/task
    // For now, this is a no-op (tasks are expected to exit normally)
}

//=============================================================================
// Timer Management
//=============================================================================

timer_handle_t ParlioPeripheralMock::createTimer(const TimerConfig& config) {
    // Mock implementation - no actual timer creation
    // Return a non-null synthetic handle to indicate success

    // Suppress unused parameter warning - config is intentionally ignored in mock
    (void)config;

    // Return a synthetic handle (use a different base address than tasks)
    timer_handle_t handle = reinterpret_cast<timer_handle_t>(
        static_cast<uintptr_t>(0xF1FE0000)  // "FIFE" (timer) marker
    );

    return handle;
}

bool ParlioPeripheralMock::enableTimer(timer_handle_t handle) {
    // Mock implementation - always succeeds if handle is non-null
    return handle != nullptr;
}

bool ParlioPeripheralMock::startTimer(timer_handle_t handle) {
    // Mock implementation - always succeeds if handle is non-null
    return handle != nullptr;
}

bool ParlioPeripheralMock::stopTimer(timer_handle_t handle) {
    // Mock implementation - always succeeds (even for null handles, matching ESP implementation)
    (void)handle;
    return true;
}

bool ParlioPeripheralMock::disableTimer(timer_handle_t handle) {
    // Mock implementation - always succeeds if handle is non-null
    return handle != nullptr;
}

void ParlioPeripheralMock::deleteTimer(timer_handle_t handle) {
    // Mock implementation - no-op (nothing to clean up)
    (void)handle;
}

uint64_t ParlioPeripheralMock::getMicroseconds() {
    // Use the same timestamp source as transmit() for consistency
    return micros();
}

void ParlioPeripheralMock::freeDmaBuffer(void* ptr) {
    // Mock uses standard heap allocation, so use standard free()
    if (ptr) {
        free(ptr);
    }
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void ParlioPeripheralMock::setTransmitDelay(uint32_t microseconds) {
    mTransmitDelayUs = microseconds;
}

void ParlioPeripheralMock::simulateTransmitComplete() {
    if (mPendingTransmissions == 0) {
        // No pending transmissions - nothing to complete
        return;
    }

    // Decrement pending count
    mPendingTransmissions--;

    // If all transmissions complete, clear transmitting flag
    if (mPendingTransmissions == 0) {
        mTransmitting = false;
    }

    // Fire ISR callback (if registered)
    if (mCallback != nullptr) {
        // Cast callback to correct signature
        // bool (*callback)(void* tx_unit, const void* edata, void* user_ctx)
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback);

        // Call with null tx_unit handle (mock doesn't have real handle)
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void ParlioPeripheralMock::setTransmitFailure(bool should_fail) {
    mShouldFailTransmit = should_fail;
}

const fl::vector<ParlioPeripheralMock::TransmissionRecord>&
ParlioPeripheralMock::getTransmissionHistory() const {
    return mHistory;
}

void ParlioPeripheralMock::clearTransmissionHistory() {
    mHistory.clear();
    mTransmitCount = 0;
    mPendingTransmissions = 0;
    mTransmitting = false;
}

} // namespace detail
} // namespace fl
