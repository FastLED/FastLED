/// @file lcd_rgb_peripheral_mock.cpp
/// @brief Mock LCD RGB peripheral implementation for unit testing

// This mock is only for host testing (uses std::thread which is not available on embedded platforms)
// Compile for stub platform testing OR non-Arduino host platforms
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "lcd_rgb_peripheral_mock.h"
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

/// @brief Internal implementation of LcdRgbPeripheralMock
class LcdRgbPeripheralMockImpl : public LcdRgbPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    LcdRgbPeripheralMockImpl();
    ~LcdRgbPeripheralMockImpl() override;

    //=========================================================================
    // ILcdRgbPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const LcdRgbPeripheralConfig& config) override;
    void deinitialize() override;
    bool isInitialized() const override;

    uint16_t* allocateFrameBuffer(size_t size_bytes) override;
    void freeFrameBuffer(uint16_t* buffer) override;

    bool drawFrame(const uint16_t* buffer, size_t size_bytes) override;
    bool waitFrameDone(uint32_t timeout_ms) override;
    bool isBusy() const override;

    bool registerDrawCallback(void* callback, void* user_ctx) override;
    const LcdRgbPeripheralConfig& getConfig() const override;

    uint64_t getMicroseconds() override;
    void delay(uint32_t ms) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateDrawComplete() override;
    void setDrawFailure(bool should_fail) override;
    void setDrawDelay(uint32_t microseconds) override;
    const fl::vector<FrameRecord>& getFrameHistory() const override;
    void clearFrameHistory() override;
    fl::span<const uint16_t> getLastFrameData() const override;
    bool isEnabled() const override;
    size_t getDrawCount() const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mEnabled;
    bool mBusy;
    size_t mDrawCount;
    LcdRgbPeripheralConfig mConfig;

    // ISR callback
    void* mCallback;
    void* mUserCtx;

    // Simulation settings
    uint32_t mDrawDelayUs;
    bool mDrawDelayForced;  // If true, use mDrawDelayUs instead of calculating from PCLK
    bool mShouldFailDraw;

    // Frame capture
    fl::vector<FrameRecord> mHistory;

    // Pending draw state
    size_t mPendingDraws;

    // Per-draw tracking for simulation thread
    struct PendingDraw {
        uint64_t completion_time_us;
    };
    fl::vector<PendingDraw> mPendingQueue;

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

LcdRgbPeripheralMock& LcdRgbPeripheralMock::instance() {
    return Singleton<LcdRgbPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

LcdRgbPeripheralMockImpl::LcdRgbPeripheralMockImpl()
    : mInitialized(false),
      mEnabled(false),
      mBusy(false),
      mDrawCount(0),
      mConfig(),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mDrawDelayUs(0),
      mDrawDelayForced(false),
      mShouldFailDraw(false),
      mHistory(),
      mPendingDraws(0),
      mPendingQueue(),
      mMutex(),
      mCondVar(),
      mCallbackExecuting(false),
      mSimulationThread(),
      mSimulationThreadShouldStop(false) {
    // Start simulation thread after all members initialized
    mSimulationThread = fl::make_unique<std::thread>([this]() { simulationThreadFunc(); });  // okay std namespace
}

LcdRgbPeripheralMockImpl::~LcdRgbPeripheralMockImpl() {
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

bool LcdRgbPeripheralMockImpl::initialize(const LcdRgbPeripheralConfig& config) {
    // Validate config
    if (config.num_lanes == 0 || config.num_lanes > 16) {
        FL_WARN("LcdRgbPeripheralMock: Invalid num_lanes: " << config.num_lanes);
        return false;
    }

    mConfig = config;
    mInitialized = true;
    mEnabled = true;
    return true;
}

void LcdRgbPeripheralMockImpl::deinitialize() {
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingDraws = 0;
    mPendingQueue.clear();
}

bool LcdRgbPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

uint16_t* LcdRgbPeripheralMockImpl::allocateFrameBuffer(size_t size_bytes) {
    // Round up to 64-byte alignment
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;

    void* buffer = nullptr;
#if defined(_WIN32) || defined(_WIN64)
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("LcdRgbPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<uint16_t*>(buffer);
}

void LcdRgbPeripheralMockImpl::freeFrameBuffer(uint16_t* buffer) {
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

bool LcdRgbPeripheralMockImpl::drawFrame(const uint16_t* buffer, size_t size_bytes) {
    if (!mInitialized) {
        FL_WARN("LcdRgbPeripheralMock: Cannot draw - not initialized");
        return false;
    }

    if (mShouldFailDraw) {
        return false;
    }

    // Calculate draw delay - use forced value if set, otherwise calculate from PCLK
    uint32_t draw_delay_us;
    if (mDrawDelayForced) {
        draw_delay_us = mDrawDelayUs;
    } else if (mConfig.pclk_hz > 0) {
        // Pixels = size_bytes / 2 (16-bit pixels)
        size_t pixels = size_bytes / 2;
        // Time = pixels / pclk_hz (seconds) * 1000000 (microseconds)
        uint64_t draw_time_us = (static_cast<uint64_t>(pixels) * 1000000ULL) / mConfig.pclk_hz;
        draw_delay_us = static_cast<uint32_t>(draw_time_us) + 10;
        mDrawDelayUs = draw_delay_us;
    } else {
        draw_delay_us = 100;  // Default fallback
        mDrawDelayUs = draw_delay_us;
    }

    // Capture frame data
    FrameRecord record;
    size_t word_count = size_bytes / 2;
    record.buffer_copy.resize(word_count);
    fl::memcpy(record.buffer_copy.data(), buffer, size_bytes);
    record.size_bytes = size_bytes;
    record.timestamp_us = micros();

    mHistory.push_back(fl::move(record));

    // Update state with mutex protection
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mDrawCount++;
        mBusy = true;
        mPendingDraws++;

        // Enqueue for simulation thread
        PendingDraw pending;
        pending.completion_time_us = micros() + draw_delay_us;
        mPendingQueue.push_back(pending);
    }

    // Wake simulation thread
    mCondVar.notify_one();

    return true;
}

bool LcdRgbPeripheralMockImpl::waitFrameDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        return false;
    }

    // Check if already complete
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        if (mPendingDraws == 0) {
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
            if (mPendingDraws == 0) {
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

bool LcdRgbPeripheralMockImpl::isBusy() const {
    return mBusy;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool LcdRgbPeripheralMockImpl::registerDrawCallback(void* callback, void* user_ctx) {
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

const LcdRgbPeripheralConfig& LcdRgbPeripheralMockImpl::getConfig() const {
    return mConfig;
}

uint64_t LcdRgbPeripheralMockImpl::getMicroseconds() {
    return micros();
}

void LcdRgbPeripheralMockImpl::delay(uint32_t ms) {
    ::delay(static_cast<int>(ms));
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void LcdRgbPeripheralMockImpl::simulateDrawComplete() {
    if (mPendingDraws == 0) {
        return;
    }

    mPendingDraws--;

    if (mPendingDraws == 0) {
        mBusy = false;
    }

    // Fire callback
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void LcdRgbPeripheralMockImpl::setDrawFailure(bool should_fail) {
    mShouldFailDraw = should_fail;
}

void LcdRgbPeripheralMockImpl::setDrawDelay(uint32_t microseconds) {
    mDrawDelayUs = microseconds;
    mDrawDelayForced = true;  // Mark as explicitly set - don't recalculate in drawFrame()
}

const fl::vector<LcdRgbPeripheralMock::FrameRecord>& LcdRgbPeripheralMockImpl::getFrameHistory() const {
    return mHistory;
}

void LcdRgbPeripheralMockImpl::clearFrameHistory() {
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mHistory.clear();
    mPendingDraws = 0;
    mBusy = false;
}

fl::span<const uint16_t> LcdRgbPeripheralMockImpl::getLastFrameData() const {
    if (mHistory.empty()) {
        return fl::span<const uint16_t>();
    }
    return fl::span<const uint16_t>(mHistory.back().buffer_copy);
}

bool LcdRgbPeripheralMockImpl::isEnabled() const {
    return mEnabled;
}

size_t LcdRgbPeripheralMockImpl::getDrawCount() const {
    return mDrawCount;
}

void LcdRgbPeripheralMockImpl::reset() {
    // Clear queue first
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mPendingQueue.clear();
        mPendingDraws = 0;
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
    mDrawCount = 0;
    mConfig = LcdRgbPeripheralConfig();
    mCallback = nullptr;
    mUserCtx = nullptr;
    mDrawDelayUs = 0;
    mDrawDelayForced = false;
    mShouldFailDraw = false;
    mHistory.clear();
    mPendingDraws = 0;
    mPendingQueue.clear();
}

//=============================================================================
// Simulation Thread
//=============================================================================

void LcdRgbPeripheralMockImpl::simulationThreadFunc() {
    while (!mSimulationThreadShouldStop) {
        std::unique_lock<std::mutex> lock(mMutex);  // okay std namespace

        // Wait when queue is empty
        if (mPendingQueue.empty()) {
            mCondVar.wait_for(lock, std::chrono::milliseconds(10));  // okay std namespace
            continue;
        }

        // Check if first draw has completed
        uint64_t now_us = micros();

        if (now_us >= mPendingQueue[0].completion_time_us) {
            // Remove from queue
            mPendingQueue.erase(mPendingQueue.begin());

            if (mPendingDraws > 0) {
                mPendingDraws--;
            }

            if (mPendingDraws == 0) {
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
