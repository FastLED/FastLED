// IWYU pragma: private

/// @file i2s_spi_peripheral_mock.cpp
/// @brief Mock I2S parallel SPI peripheral implementation for unit testing

#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) &&          \
     (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.h"
#include "fl/log/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: begin_keep
#include "fl/stl/cstdlib.h"
// IWYU pragma: end_keep

namespace fl {
namespace detail {

//=============================================================================
// Implementation Class
//=============================================================================

class I2sSpiPeripheralMockImpl : public I2sSpiPeripheralMock {
  public:
    I2sSpiPeripheralMockImpl() FL_NO_EXCEPT;
    ~I2sSpiPeripheralMockImpl() override;

    // II2sSpiPeripheral interface
    bool initialize(const I2sSpiConfig &config) FL_NO_EXCEPT override;
    void deinitialize() FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;

    u8 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override;
    void freeBuffer(u8 *buffer) FL_NO_EXCEPT override;

    bool transmit(const u8 *buffer, size_t size_bytes) FL_NO_EXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override;
    bool isBusy() const FL_NO_EXCEPT override;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NO_EXCEPT override;
    const I2sSpiConfig &getConfig() const FL_NO_EXCEPT override;

    u64 getMicroseconds() FL_NO_EXCEPT override;
    void delay(u32 ms) FL_NO_EXCEPT override;

    // Mock-specific API
    void simulateTransmitComplete() FL_NO_EXCEPT override;
    void setTransmitFailure(bool should_fail) FL_NO_EXCEPT override;
    void setTransmitDelay(u32 microseconds) FL_NO_EXCEPT override;
    void setAutoComplete(bool auto_complete) FL_NO_EXCEPT override;
    const fl::vector<TransmitRecord> &
    getTransmitHistory() const FL_NO_EXCEPT override;
    void clearTransmitHistory() FL_NO_EXCEPT override;
    fl::span<const u8> getLastTransmitData() const FL_NO_EXCEPT override;
    bool isEnabled() const FL_NO_EXCEPT override;
    size_t getTransmitCount() const FL_NO_EXCEPT override;
    void reset() FL_NO_EXCEPT override;

  private:
    void pumpDeferredCallbacks() FL_NO_EXCEPT;
    void fireCallback() FL_NO_EXCEPT;

    bool mInitialized;
    bool mEnabled;
    bool mBusy;
    size_t mTransmitCount;
    I2sSpiConfig mConfig;

    void *mCallback;
    void *mUserCtx;

    u32 mTransmitDelayUs;
    bool mShouldFailTransmit;

    fl::vector<TransmitRecord> mHistory;
    size_t mPendingTransmits;
    u64 mSimulatedTimeUs;
    bool mFiringCallbacks;
    size_t mDeferredCallbackCount;
    bool mAutoComplete;
};

//=============================================================================
// Singleton
//=============================================================================

I2sSpiPeripheralMock &I2sSpiPeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<I2sSpiPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

I2sSpiPeripheralMockImpl::I2sSpiPeripheralMockImpl() FL_NO_EXCEPT
    : mInitialized(false), mEnabled(false), mBusy(false), mTransmitCount(0),
      mConfig(), mCallback(nullptr), mUserCtx(nullptr), mTransmitDelayUs(0),
      mShouldFailTransmit(false), mHistory(), mPendingTransmits(0),
      mSimulatedTimeUs(0), mFiringCallbacks(false), mDeferredCallbackCount(0),
      mAutoComplete(false) {
}

I2sSpiPeripheralMockImpl::~I2sSpiPeripheralMockImpl() {}

//=============================================================================
// Lifecycle
//=============================================================================

bool I2sSpiPeripheralMockImpl::initialize(
    const I2sSpiConfig &config) FL_NO_EXCEPT {
    if (config.num_lanes == 0 || config.num_lanes > 16) {
        FL_WARN_F("I2sSpiPeripheralMock: Invalid num_lanes: %s", config.num_lanes);
        return false;
    }
    mConfig = config;
    mInitialized = true;
    mEnabled = true;
    return true;
}

void I2sSpiPeripheralMockImpl::deinitialize() FL_NO_EXCEPT {
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingTransmits = 0;
}

bool I2sSpiPeripheralMockImpl::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

u8 *I2sSpiPeripheralMockImpl::allocateBuffer(size_t size_bytes) FL_NO_EXCEPT {
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;
    void *buffer = nullptr;
#ifdef FL_IS_WIN
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif
    if (buffer == nullptr) {
        FL_WARN_F("I2sSpiPeripheralMock: Failed to allocate buffer (%s bytes)", aligned_size);
    }
    return static_cast<u8 *>(buffer);
}

void I2sSpiPeripheralMockImpl::freeBuffer(u8 *buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        fl::free(buffer);
#endif
    }
}

//=============================================================================
// Transmission
//=============================================================================

bool I2sSpiPeripheralMockImpl::transmit(const u8 *buffer,
                                        size_t size_bytes) FL_NO_EXCEPT {
    if (!mInitialized) {
        FL_WARN_F("I2sSpiPeripheralMock: Cannot transmit - not initialized");
        return false;
    }
    if (mShouldFailTransmit) {
        return false;
    }

    TransmitRecord record;
    record.buffer_copy.resize(size_bytes);
    fl::memcpy(record.buffer_copy.data(), buffer, size_bytes);
    record.size_bytes = size_bytes;
    record.timestamp_us = mSimulatedTimeUs;
    mHistory.push_back(fl::move(record));

    mTransmitCount++;
    mBusy = true;
    mPendingTransmits++;
    if (mAutoComplete) {
        mDeferredCallbackCount++;
        pumpDeferredCallbacks();
    }
    return true;
}

bool I2sSpiPeripheralMockImpl::waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT {
    if (!mInitialized) {
        return false;
    }
    (void)timeout_ms;
    if (mPendingTransmits == 0) {
        mBusy = false;
        return true;
    }
    return false;
}

bool I2sSpiPeripheralMockImpl::isBusy() const FL_NO_EXCEPT { return mBusy; }

//=============================================================================
// Callback
//=============================================================================

bool I2sSpiPeripheralMockImpl::registerTransmitCallback(
    void *callback, void *user_ctx) FL_NO_EXCEPT {
    if (!mInitialized) {
        return false;
    }
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

const I2sSpiConfig &I2sSpiPeripheralMockImpl::getConfig() const FL_NO_EXCEPT {
    return mConfig;
}

u64 I2sSpiPeripheralMockImpl::getMicroseconds() FL_NO_EXCEPT {
    return mSimulatedTimeUs;
}

void I2sSpiPeripheralMockImpl::delay(u32 ms) FL_NO_EXCEPT {
    mSimulatedTimeUs += static_cast<u64>(ms) * 1000;
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void I2sSpiPeripheralMockImpl::simulateTransmitComplete() FL_NO_EXCEPT {
    if (mPendingTransmits == 0) {
        return;
    }
    mPendingTransmits--;
    if (mPendingTransmits == 0) {
        mBusy = false;
    }
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void *, const void *, void *);
        auto callback_fn =
            reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void I2sSpiPeripheralMockImpl::setTransmitFailure(
    bool should_fail) FL_NO_EXCEPT {
    mShouldFailTransmit = should_fail;
}

void I2sSpiPeripheralMockImpl::setTransmitDelay(u32 microseconds) FL_NO_EXCEPT {
    mTransmitDelayUs = microseconds;
}

void I2sSpiPeripheralMockImpl::setAutoComplete(
    bool auto_complete) FL_NO_EXCEPT {
    mAutoComplete = auto_complete;
}

const fl::vector<I2sSpiPeripheralMock::TransmitRecord> &
I2sSpiPeripheralMockImpl::getTransmitHistory() const FL_NO_EXCEPT {
    return mHistory;
}

void I2sSpiPeripheralMockImpl::clearTransmitHistory() FL_NO_EXCEPT {
    mHistory.clear();
    mPendingTransmits = 0;
    mBusy = false;
}

fl::span<const u8>
I2sSpiPeripheralMockImpl::getLastTransmitData() const FL_NO_EXCEPT {
    if (mHistory.empty()) {
        return fl::span<const u8>();
    }
    return fl::span<const u8>(mHistory.back().buffer_copy);
}

bool I2sSpiPeripheralMockImpl::isEnabled() const FL_NO_EXCEPT {
    return mEnabled;
}

size_t I2sSpiPeripheralMockImpl::getTransmitCount() const FL_NO_EXCEPT {
    return mTransmitCount;
}

void I2sSpiPeripheralMockImpl::reset() FL_NO_EXCEPT {
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mTransmitCount = 0;
    mConfig = I2sSpiConfig();
    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransmitDelayUs = 0;
    mShouldFailTransmit = false;
    mHistory.clear();
    mPendingTransmits = 0;
    mSimulatedTimeUs = 0;
    mFiringCallbacks = false;
    mDeferredCallbackCount = 0;
    mAutoComplete = false;
}

//=============================================================================
// Callback Pumping
//=============================================================================

void I2sSpiPeripheralMockImpl::pumpDeferredCallbacks() FL_NO_EXCEPT {
    if (mFiringCallbacks) {
        return;
    }
    mFiringCallbacks = true;
    while (mDeferredCallbackCount > 0) {
        mDeferredCallbackCount--;
        fireCallback();
    }
    mFiringCallbacks = false;
}

void I2sSpiPeripheralMockImpl::fireCallback() FL_NO_EXCEPT {
    if (mPendingTransmits > 0) {
        mPendingTransmits--;
    }
    if (mPendingTransmits == 0) {
        mBusy = false;
    }
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void *, const void *, void *);
        auto callback_fn =
            reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || host platform
