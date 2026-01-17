/// @file i2s_lcd_cam_peripheral_mock.cpp
/// @brief Mock I2S LCD_CAM peripheral implementation for unit testing

// This mock is only for host testing (uses std::thread which is not available on embedded platforms)
// Compile for stub platform testing OR non-Arduino host platforms
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "i2s_lcd_cam_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"

#include <thread>  // ok include
#include <chrono>  // ok include
#include <mutex>   // ok include
#include <condition_variable>  // ok include
#include <stdlib.h> // ok include for aligned_alloc on POSIX

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

/// @brief Internal implementation of I2sLcdCamPeripheralMock
class I2sLcdCamPeripheralMockImpl : public I2sLcdCamPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    I2sLcdCamPeripheralMockImpl();
    ~I2sLcdCamPeripheralMockImpl() override;

    //=========================================================================
    // II2sLcdCamPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const I2sLcdCamConfig& config) override;
    void deinitialize() override;
    bool isInitialized() const override;

    uint16_t* allocateBuffer(size_t size_bytes) override;
    void freeBuffer(uint16_t* buffer) override;

    bool transmit(const uint16_t* buffer, size_t size_bytes) override;
    bool waitTransmitDone(uint32_t timeout_ms) override;
    bool isBusy() const override;

    bool registerTransmitCallback(void* callback, void* user_ctx) override;
    const I2sLcdCamConfig& getConfig() const override;

    uint64_t getMicroseconds() override;
    void delay(uint32_t ms) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateTransmitComplete() override;
    void setTransmitFailure(bool should_fail) override;
    void setTransmitDelay(uint32_t microseconds) override;
    const fl::vector<TransmitRecord>& getTransmitHistory() const override;
    void clearTransmitHistory() override;
    fl::span<const uint16_t> getLastTransmitData() const override;
    bool isEnabled() const override;
    size_t getTransmitCount() const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mEnabled;
    bool mBusy;
    size_t mTransmitCount;
    I2sLcdCamConfig mConfig;

    // ISR callback
    void* mCallback;
    void* mUserCtx;

    // Simulation settings
    uint32_t mTransmitDelayUs;
    bool mTransmitDelayForced;  // If true, use mTransmitDelayUs instead of calculating
    bool mShouldFailTransmit;

    // Transmit capture
    fl::vector<TransmitRecord> mHistory;

    // Pending transmit state
    size_t mPendingTransmits;

    // Per-transmit tracking for simulation thread
    struct PendingTransmit {
        uint64_t completion_time_us;
    };
    fl::vector<PendingTransmit> mPendingQueue;

    // Thread synchronization
    std::mutex mMutex;  // okay std namespace
    std::condition_variable mCondVar;  // okay std namespace
    fl::atomic<bool> mCallbackExecuting{false};

    // Simulation thread
    void simulationThreadFunc();
    fl::unique_ptr<std::thread> mSimulationThread;  // okay std namespace
    fl::atomic<bool> mSimulationThreadShouldStop;
};

//=============================================================================
// Singleton Instance
//=============================================================================

I2sLcdCamPeripheralMock& I2sLcdCamPeripheralMock::instance() {
    return Singleton<I2sLcdCamPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

I2sLcdCamPeripheralMockImpl::I2sLcdCamPeripheralMockImpl()
    : mInitialized(false),
      mEnabled(false),
      mBusy(false),
      mTransmitCount(0),
      mConfig(),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mTransmitDelayUs(0),
      mTransmitDelayForced(false),
      mShouldFailTransmit(false),
      mHistory(),
      mPendingTransmits(0),
      mPendingQueue(),
      mMutex(),
      mCondVar(),
      mCallbackExecuting(false),
      mSimulationThread(),
      mSimulationThreadShouldStop(false) {
    // Start simulation thread after all members initialized
    mSimulationThread = fl::make_unique<std::thread>([this]() { simulationThreadFunc(); });  // okay std namespace
}

I2sLcdCamPeripheralMockImpl::~I2sLcdCamPeripheralMockImpl() {
    // Stop simulation thread
    mSimulationThreadShouldStop = true;
    mCondVar.notify_one();
    if (mSimulationThread && mSimulationThread->joinable()) {
        mSimulationThread->join();
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool I2sLcdCamPeripheralMockImpl::initialize(const I2sLcdCamConfig& config) {
    // Validate config
    if (config.num_lanes == 0 || config.num_lanes > 16) {
        FL_WARN("I2sLcdCamPeripheralMock: Invalid num_lanes: " << config.num_lanes);
        return false;
    }

    mConfig = config;
    mInitialized = true;
    mEnabled = true;
    return true;
}

void I2sLcdCamPeripheralMockImpl::deinitialize() {
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingTransmits = 0;
    mPendingQueue.clear();
}

bool I2sLcdCamPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

uint16_t* I2sLcdCamPeripheralMockImpl::allocateBuffer(size_t size_bytes) {
    // Round up to 64-byte alignment (PSRAM requirement)
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;

    void* buffer = nullptr;
#if defined(_WIN32) || defined(_WIN64)
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("I2sLcdCamPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<uint16_t*>(buffer);
}

void I2sLcdCamPeripheralMockImpl::freeBuffer(uint16_t* buffer) {
    if (buffer != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool I2sLcdCamPeripheralMockImpl::transmit(const uint16_t* buffer, size_t size_bytes) {
    if (!mInitialized) {
        FL_WARN("I2sLcdCamPeripheralMock: Cannot transmit - not initialized");
        return false;
    }

    if (mShouldFailTransmit) {
        return false;
    }

    // Calculate transmit delay - use forced value if set, otherwise calculate from PCLK
    uint32_t transmit_delay_us;
    if (mTransmitDelayForced) {
        transmit_delay_us = mTransmitDelayUs;
    } else if (mConfig.pclk_hz > 0) {
        // Pixels = size_bytes / 2 (16-bit pixels)
        size_t pixels = size_bytes / 2;
        // Time = pixels / pclk_hz (seconds) * 1000000 (microseconds)
        uint64_t transmit_time_us = (static_cast<uint64_t>(pixels) * 1000000ULL) / mConfig.pclk_hz;
        transmit_delay_us = static_cast<uint32_t>(transmit_time_us) + 10;
        mTransmitDelayUs = transmit_delay_us;
    } else {
        transmit_delay_us = 100;  // Default fallback
        mTransmitDelayUs = transmit_delay_us;
    }

    // Capture transmit data
    TransmitRecord record;
    size_t word_count = size_bytes / 2;
    record.buffer_copy.resize(word_count);
    fl::memcpy(record.buffer_copy.data(), buffer, size_bytes);
    record.size_bytes = size_bytes;
    record.timestamp_us = micros();

    mHistory.push_back(fl::move(record));

    // Update state with mutex protection
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mTransmitCount++;
        mBusy = true;
        mPendingTransmits++;

        // Enqueue for simulation thread
        PendingTransmit pending;
        pending.completion_time_us = micros() + transmit_delay_us;
        mPendingQueue.push_back(pending);
    }

    // Wake simulation thread
    mCondVar.notify_one();

    return true;
}

bool I2sLcdCamPeripheralMockImpl::waitTransmitDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        return false;
    }

    // Check if already complete
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        if (mPendingTransmits == 0) {
            mBusy = false;
            return true;
        }
    }

    if (timeout_ms == 0) {
        return false;  // Non-blocking poll, still pending
    }

    // Wait for completion
    uint32_t start_us = micros();
    uint32_t timeout_us = timeout_ms * 1000;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
            if (mPendingTransmits == 0) {
                mBusy = false;
                return true;
            }
        }

        if ((micros() - start_us) >= timeout_us) {
            return false;  // Timeout
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));  // okay std namespace
    }
}

bool I2sLcdCamPeripheralMockImpl::isBusy() const {
    return mBusy;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool I2sLcdCamPeripheralMockImpl::registerTransmitCallback(void* callback, void* user_ctx) {
    if (!mInitialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

//=============================================================================
// State Inspection
//=============================================================================

const I2sLcdCamConfig& I2sLcdCamPeripheralMockImpl::getConfig() const {
    return mConfig;
}

uint64_t I2sLcdCamPeripheralMockImpl::getMicroseconds() {
    return micros();
}

void I2sLcdCamPeripheralMockImpl::delay(uint32_t ms) {
    ::delay(static_cast<int>(ms));
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void I2sLcdCamPeripheralMockImpl::simulateTransmitComplete() {
    if (mPendingTransmits == 0) {
        return;
    }

    mPendingTransmits--;

    if (mPendingTransmits == 0) {
        mBusy = false;
    }

    // Fire callback
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void I2sLcdCamPeripheralMockImpl::setTransmitFailure(bool should_fail) {
    mShouldFailTransmit = should_fail;
}

void I2sLcdCamPeripheralMockImpl::setTransmitDelay(uint32_t microseconds) {
    mTransmitDelayUs = microseconds;
    mTransmitDelayForced = true;  // Mark as explicitly set - don't recalculate in transmit()
}

const fl::vector<I2sLcdCamPeripheralMock::TransmitRecord>& I2sLcdCamPeripheralMockImpl::getTransmitHistory() const {
    return mHistory;
}

void I2sLcdCamPeripheralMockImpl::clearTransmitHistory() {
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mHistory.clear();
    mPendingTransmits = 0;
    mBusy = false;
}

fl::span<const uint16_t> I2sLcdCamPeripheralMockImpl::getLastTransmitData() const {
    if (mHistory.empty()) {
        return fl::span<const uint16_t>();
    }
    return fl::span<const uint16_t>(mHistory.back().buffer_copy);
}

bool I2sLcdCamPeripheralMockImpl::isEnabled() const {
    return mEnabled;
}

size_t I2sLcdCamPeripheralMockImpl::getTransmitCount() const {
    return mTransmitCount;
}

void I2sLcdCamPeripheralMockImpl::reset() {
    // Clear queue first
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mPendingQueue.clear();
        mPendingTransmits = 0;
        mBusy = false;
    }

    mCondVar.notify_one();

    // Wait for callback to finish
    while (mCallbackExecuting.load(fl::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));  // okay std namespace
    }

    std::this_thread::sleep_for(std::chrono::microseconds(100));  // okay std namespace

    // Reset all state
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace

    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mTransmitCount = 0;
    mConfig = I2sLcdCamConfig();
    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransmitDelayUs = 0;
    mTransmitDelayForced = false;
    mShouldFailTransmit = false;
    mHistory.clear();
    mPendingTransmits = 0;
    mPendingQueue.clear();
}

//=============================================================================
// Simulation Thread
//=============================================================================

void I2sLcdCamPeripheralMockImpl::simulationThreadFunc() {
    while (!mSimulationThreadShouldStop) {
        std::unique_lock<std::mutex> lock(mMutex);  // okay std namespace

        // Wait when queue is empty
        if (mPendingQueue.empty()) {
            mCondVar.wait_for(lock, std::chrono::milliseconds(10));  // okay std namespace
            continue;
        }

        // Check if first transmit has completed
        uint64_t now_us = micros();

        if (now_us >= mPendingQueue[0].completion_time_us) {
            // Remove from queue
            mPendingQueue.erase(mPendingQueue.begin());

            if (mPendingTransmits > 0) {
                mPendingTransmits--;
            }

            if (mPendingTransmits == 0) {
                mBusy = false;
            }

            // Get callback info
            auto callback = mCallback;
            auto user_ctx = mUserCtx;

            mCallbackExecuting.store(true, fl::memory_order_release);

            lock.unlock();

            // Call callback
            if (callback) {
                using CallbackType = bool (*)(void*, const void*, void*);
                auto callback_fn = reinterpret_cast<CallbackType>(callback); // ok reinterpret cast
                callback_fn(nullptr, nullptr, user_ctx);
            }

            lock.lock();
            mCallbackExecuting.store(false, fl::memory_order_release);
        } else {
            // Wait until next completion time
            uint64_t wait_us = mPendingQueue[0].completion_time_us - now_us;
            mCondVar.wait_for(lock, std::chrono::microseconds(wait_us));  // okay std namespace
        }
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
