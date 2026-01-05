/// @file parlio_peripheral_mock.cpp
/// @brief Mock PARLIO peripheral implementation for unit testing

#include "parlio_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"

#include <thread>  // ok include
#include <chrono>  // ok include

#ifdef ARDUINO
#include <Arduino.h>  // For micros() and delay() on Arduino platforms
#else
#include "platforms/stub/time_stub.h"  // For micros() and delay() on host tests
#endif

namespace fl {
namespace detail {

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of ParlioPeripheralMock
///
/// This class contains all the actual implementation details.
/// It's a simple synchronous mock without threading.
class ParlioPeripheralMockImpl : public ParlioPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    ParlioPeripheralMockImpl();
    ~ParlioPeripheralMockImpl() override;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override;
    bool enable() override;
    bool disable() override;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override;
    bool waitAllDone(uint32_t timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    uint64_t getMicroseconds() override;
    void freeDmaBuffer(void* ptr) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void setTransmitDelay(uint32_t microseconds) override;
    void simulateTransmitComplete() override;
    void setTransmitFailure(bool should_fail) override;
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const override;
    void clearTransmissionHistory() override;
    bool isInitialized() const override;
    bool isEnabled() const override;
    bool isTransmitting() const override;
    size_t getTransmitCount() const override;
    const ParlioPeripheralConfig& getConfig() const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mEnabled;
    bool mTransmitting;
    size_t mTransmitCount;
    ParlioPeripheralConfig mConfig;

    // ISR callback
    void* mCallback;
    void* mUserCtx;

    // Simulation settings
    uint32_t mTransmitDelayUs;
    bool mShouldFailTransmit;

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;

    // Pending transmission state (for waitAllDone simulation)
    size_t mPendingTransmissions;

    // Simulation thread for automatic ISR callback
    void simulationThreadFunc();
    fl::unique_ptr<std::thread> mSimulationThread;  // okay std namespace
    fl::atomic<bool> mSimulationThreadShouldStop;
};

//=============================================================================
// Singleton Instance
//=============================================================================

ParlioPeripheralMock& ParlioPeripheralMock::instance() {
    return Singleton<ParlioPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor
//=============================================================================

ParlioPeripheralMockImpl::ParlioPeripheralMockImpl()
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
      mPendingTransmissions(0),
      mSimulationThread(),
      mSimulationThreadShouldStop(false) {
    // Start simulation thread
    mSimulationThread = fl::make_unique<std::thread>([this]() { simulationThreadFunc(); });  // okay std namespace
}

ParlioPeripheralMockImpl::~ParlioPeripheralMockImpl() {
    // Stop simulation thread
    mSimulationThreadShouldStop = true;
    if (mSimulationThread && mSimulationThread->joinable()) {
        mSimulationThread->join();
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool ParlioPeripheralMockImpl::initialize(const ParlioPeripheralConfig& config) {
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

bool ParlioPeripheralMockImpl::enable() {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot enable - not initialized");
        return false;
    }

    mEnabled = true;
    return true;
}

bool ParlioPeripheralMockImpl::disable() {
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

bool ParlioPeripheralMockImpl::transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) {
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

    // Calculate realistic transmission delay based on clock frequency and bit count
    // clock_freq_hz is stored in mConfig.clock_freq_hz (e.g., 8 MHz = 8000000 Hz for WS2812)
    // Transmission time = (bit_count / clock_freq_hz) seconds = (bit_count * 1000000 / clock_freq_hz) microseconds
    if (mConfig.clock_freq_hz > 0) {
        // Calculate transmission time in microseconds
        uint64_t transmission_time_us = (static_cast<uint64_t>(bit_count) * 1000000ULL) / mConfig.clock_freq_hz;
        // Add small overhead for buffer switching (10 microseconds)
        mTransmitDelayUs = static_cast<uint32_t>(transmission_time_us) + 10;
    } else {
        // Fallback: Use a small default delay if clock not configured
        mTransmitDelayUs = 100;  // 100 microseconds default
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

    // Simulation thread will automatically call ISR callback after mTransmitDelayUs

    return true;
}

bool ParlioPeripheralMockImpl::waitAllDone(uint32_t timeout_ms) {
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

bool ParlioPeripheralMockImpl::registerTxDoneCallback(void* callback, void* user_ctx) {
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

uint8_t* ParlioPeripheralMockImpl::allocateDmaBuffer(size_t size) {
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

void ParlioPeripheralMockImpl::freeDmaBuffer(uint8_t* buffer) {
    if (buffer != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

void ParlioPeripheralMockImpl::delay(uint32_t ms) {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    ::delay(static_cast<int>(ms));
}

uint64_t ParlioPeripheralMockImpl::getMicroseconds() {
    // Use the same timestamp source as transmit() for consistency
    return micros();
}

void ParlioPeripheralMockImpl::freeDmaBuffer(void* ptr) {
    // Mock uses standard heap allocation, so use standard free()
    if (ptr) {
        free(ptr);
    }
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void ParlioPeripheralMockImpl::setTransmitDelay(uint32_t microseconds) {
    mTransmitDelayUs = microseconds;
}

void ParlioPeripheralMockImpl::simulateTransmitComplete() {
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

void ParlioPeripheralMockImpl::setTransmitFailure(bool should_fail) {
    mShouldFailTransmit = should_fail;
}

const fl::vector<ParlioPeripheralMock::TransmissionRecord>&
ParlioPeripheralMockImpl::getTransmissionHistory() const {
    return mHistory;
}

bool ParlioPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

bool ParlioPeripheralMockImpl::isEnabled() const {
    return mEnabled;
}

bool ParlioPeripheralMockImpl::isTransmitting() const {
    return mTransmitting;
}

size_t ParlioPeripheralMockImpl::getTransmitCount() const {
    return mTransmitCount;
}

const ParlioPeripheralConfig& ParlioPeripheralMockImpl::getConfig() const {
    return mConfig;
}

void ParlioPeripheralMockImpl::clearTransmissionHistory() {
    mHistory.clear();
    mTransmitCount = 0;
    mPendingTransmissions = 0;
    mTransmitting = false;
}

void ParlioPeripheralMockImpl::reset() {
    mInitialized = false;
    mEnabled = false;
    mTransmitting = false;
    mTransmitCount = 0;

    fl::vector<int> empty_pins;  // Empty vector (size = 0)
    mConfig = ParlioPeripheralConfig(empty_pins, 0, 0, 0);

    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransmitDelayUs = 0;
    mShouldFailTransmit = false;
    mHistory.clear();
    mPendingTransmissions = 0;
}

//=============================================================================
// Simulation Thread
//=============================================================================

void ParlioPeripheralMockImpl::simulationThreadFunc() {
    using clock = std::chrono::steady_clock;  // okay std namespace

    while (!mSimulationThreadShouldStop) {
        // Check if there are pending transmissions
        if (mPendingTransmissions > 0 && mTransmitDelayUs > 0) {
            // Sleep for the transmission delay
            std::this_thread::sleep_for(std::chrono::microseconds(mTransmitDelayUs));  // okay std namespace

            // After delay, simulate transmission complete by calling ISR callback
            if (mCallback && mPendingTransmissions > 0) {
                // Decrement pending count
                mPendingTransmissions--;

                // Update state
                if (mPendingTransmissions == 0) {
                    mTransmitting = false;
                }

                // Call ISR callback
                using CallbackType = bool (*)(void*, const void*, void*);
                auto callback_fn = reinterpret_cast<CallbackType>(mCallback);
                callback_fn(nullptr, nullptr, mUserCtx);
            }
        } else {
            // No pending transmissions - sleep briefly to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::microseconds(100));  // okay std namespace
        }
    }
}

} // namespace detail
} // namespace fl
