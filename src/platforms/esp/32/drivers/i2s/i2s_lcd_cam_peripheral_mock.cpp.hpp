// IWYU pragma: private

/// @file i2s_lcd_cam_peripheral_mock.cpp
/// @brief Mock I2S LCD_CAM peripheral implementation for unit testing

// This mock is only for host testing (uses std::thread which is not available on embedded platforms)
// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"

// IWYU pragma: begin_keep
#include "fl/stl/thread.h"  // ok include
#include "fl/stl/chrono.h"  // ok include
#include "fl/stl/mutex.h"   // ok include
#include "fl/stl/condition_variable.h"  // ok include
#include "fl/stl/cstdlib.h" // ok include for aligned_alloc on POSIX
// IWYU pragma: end_keep

#ifdef ARDUINO
// IWYU pragma: begin_keep
#include "fl/arduino.h"
// IWYU pragma: end_keep
#else
#include "platforms/stub/time_stub.h"  // For fl::micros() and delay() on host tests
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

    u16* allocateBuffer(size_t size_bytes) override;
    void freeBuffer(u16* buffer) override;

    bool transmit(const u16* buffer, size_t size_bytes) override;
    bool waitTransmitDone(u32 timeout_ms) override;
    bool isBusy() const override;

    bool registerTransmitCallback(void* callback, void* user_ctx) override;
    const I2sLcdCamConfig& getConfig() const override;

    u64 getMicroseconds() override;
    void delay(u32 ms) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateTransmitComplete() override;
    void setTransmitFailure(bool should_fail) override;
    void setTransmitDelay(u32 microseconds) override;
    const fl::vector<TransmitRecord>& getTransmitHistory() const override;
    void clearTransmitHistory() override;
    fl::span<const u16> getLastTransmitData() const override;
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
    u32 mTransmitDelayUs;
    bool mTransmitDelayForced;  // If true, use mTransmitDelayUs instead of calculating
    bool mShouldFailTransmit;

    // Transmit capture
    fl::vector<TransmitRecord> mHistory;

    // Pending transmit state
    size_t mPendingTransmits;

    // Simulated time (advances only via delay calls)
    u64 mSimulatedTimeUs;
    bool mFiringCallbacks;  // Re-entrancy guard
    size_t mDeferredCallbackCount;  // Count of pending callbacks to fire

    // Helper methods for synchronous callback firing
    void pumpDeferredCallbacks();
    void fireCallback();
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
      mSimulatedTimeUs(0),
      mFiringCallbacks(false),
      mDeferredCallbackCount(0) {
}

I2sLcdCamPeripheralMockImpl::~I2sLcdCamPeripheralMockImpl() {
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
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingTransmits = 0;
}

bool I2sLcdCamPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

u16* I2sLcdCamPeripheralMockImpl::allocateBuffer(size_t size_bytes) {
    // Round up to 64-byte alignment (PSRAM requirement)
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;

    void* buffer = nullptr;
#ifdef FL_IS_WIN
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("I2sLcdCamPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<u16*>(buffer);
}

void I2sLcdCamPeripheralMockImpl::freeBuffer(u16* buffer) {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        fl::free(buffer);
#endif
    }
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool I2sLcdCamPeripheralMockImpl::transmit(const u16* buffer, size_t size_bytes) {
    if (!mInitialized) {
        FL_WARN("I2sLcdCamPeripheralMock: Cannot transmit - not initialized");
        return false;
    }

    if (mShouldFailTransmit) {
        return false;
    }

    // Capture transmit data
    TransmitRecord record;
    size_t word_count = size_bytes / 2;
    record.buffer_copy.resize(word_count);
    fl::memcpy(record.buffer_copy.data(), buffer, size_bytes);
    record.size_bytes = size_bytes;
    record.timestamp_us = mSimulatedTimeUs;

    mHistory.push_back(fl::move(record));

    // Queue deferred callback - will be fired synchronously via pumpDeferredCallbacks
    mTransmitCount++;
    mBusy = true;
    mPendingTransmits++;
    mDeferredCallbackCount++;

    // If we're not already inside a callback chain, pump now
    pumpDeferredCallbacks();

    return true;
}

bool I2sLcdCamPeripheralMockImpl::waitTransmitDone(u32 timeout_ms) {
    if (!mInitialized) {
        return false;
    }

    // In the synchronous mock, everything completes immediately when transmit() is called.
    // waitTransmitDone just returns the completion status.
    (void)timeout_ms;  // timeout not used in synchronous mock
    if (mPendingTransmits == 0) {
        mBusy = false;
        return true;
    }

    return false;
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

u64 I2sLcdCamPeripheralMockImpl::getMicroseconds() {
    return mSimulatedTimeUs;
}

void I2sLcdCamPeripheralMockImpl::delay(u32 ms) {
    mSimulatedTimeUs += static_cast<u64>(ms) * 1000;
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

void I2sLcdCamPeripheralMockImpl::setTransmitDelay(u32 microseconds) {
    mTransmitDelayUs = microseconds;
    mTransmitDelayForced = true;  // Mark as explicitly set - don't recalculate in transmit()
}

const fl::vector<I2sLcdCamPeripheralMock::TransmitRecord>& I2sLcdCamPeripheralMockImpl::getTransmitHistory() const {
    return mHistory;
}

void I2sLcdCamPeripheralMockImpl::clearTransmitHistory() {
    mHistory.clear();
    mPendingTransmits = 0;
    mBusy = false;
}

fl::span<const u16> I2sLcdCamPeripheralMockImpl::getLastTransmitData() const {
    if (mHistory.empty()) {
        return fl::span<const u16>();
    }
    return fl::span<const u16>(mHistory.back().buffer_copy);
}

bool I2sLcdCamPeripheralMockImpl::isEnabled() const {
    return mEnabled;
}

size_t I2sLcdCamPeripheralMockImpl::getTransmitCount() const {
    return mTransmitCount;
}

void I2sLcdCamPeripheralMockImpl::reset() {
    // Reset all state (synchronous mock, no threads)
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
    mSimulatedTimeUs = 0;
    mFiringCallbacks = false;
    mDeferredCallbackCount = 0;
}

//=============================================================================
// Simulation Thread
//=============================================================================

//=============================================================================
// Synchronous Callback Pumping
//=============================================================================

void I2sLcdCamPeripheralMockImpl::pumpDeferredCallbacks() {
    // Re-entrancy guard: if we're already firing callbacks (from within
    // a callback that called transmit()), just let the outer loop handle it.
    if (mFiringCallbacks) {
        return;
    }
    mFiringCallbacks = true;

    // Process all deferred callbacks. Each callback may trigger more
    // transmit() calls which queue more callbacks, so loop until empty.
    while (mDeferredCallbackCount > 0) {
        mDeferredCallbackCount--;
        fireCallback();
    }

    mFiringCallbacks = false;
}

void I2sLcdCamPeripheralMockImpl::fireCallback() {
    if (mPendingTransmits > 0) {
        mPendingTransmits--;
    }

    if (mPendingTransmits == 0) {
        mBusy = false;
    }

    // Call the callback if registered
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
