// IWYU pragma: private

/// @file lcd_spi_peripheral_mock.cpp
/// @brief Mock LCD_CAM SPI peripheral implementation for unit testing

#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || \
    (!defined(ARDUINO) &&          \
     (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.h"
#include "fl/system/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: begin_keep
#include "fl/stl/cstdlib.h"
// IWYU pragma: end_keep

namespace fl {
namespace detail {

class LcdSpiPeripheralMockImpl : public LcdSpiPeripheralMock {
  public:
    LcdSpiPeripheralMockImpl() FL_NOEXCEPT;
    ~LcdSpiPeripheralMockImpl() override;

    bool initialize(const LcdSpiConfig &config) FL_NOEXCEPT override;
    void deinitialize() FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;

    u16 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override;
    void freeBuffer(u16 *buffer) FL_NOEXCEPT override;

    bool transmit(const u16 *buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerTransmitCallback(void *callback,
                                  void *user_ctx) FL_NOEXCEPT override;
    const LcdSpiConfig &getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

    void simulateTransmitComplete() FL_NOEXCEPT override;
    void setTransmitFailure(bool should_fail) FL_NOEXCEPT override;
    void setTransmitDelay(u32 microseconds) FL_NOEXCEPT override;
    void setAutoComplete(bool auto_complete) FL_NOEXCEPT override;
    const fl::vector<TransmitRecord> &
    getTransmitHistory() const FL_NOEXCEPT override;
    void clearTransmitHistory() FL_NOEXCEPT override;
    fl::span<const u16> getLastTransmitData() const FL_NOEXCEPT override;
    bool isEnabled() const FL_NOEXCEPT override;
    size_t getTransmitCount() const FL_NOEXCEPT override;
    size_t getDeinitCount() const FL_NOEXCEPT override;
    void reset() FL_NOEXCEPT override;

  private:
    void pumpDeferredCallbacks() FL_NOEXCEPT;
    void fireCallback() FL_NOEXCEPT;

    bool mInitialized;
    bool mEnabled;
    bool mBusy;
    size_t mTransmitCount;
    LcdSpiConfig mConfig;
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
    size_t mDeinitCount;
};

LcdSpiPeripheralMock &LcdSpiPeripheralMock::instance() FL_NOEXCEPT {
    return Singleton<LcdSpiPeripheralMockImpl>::instance();
}

LcdSpiPeripheralMockImpl::LcdSpiPeripheralMockImpl() FL_NOEXCEPT
    : mInitialized(false), mEnabled(false), mBusy(false), mTransmitCount(0),
      mConfig(), mCallback(nullptr), mUserCtx(nullptr), mTransmitDelayUs(0),
      mShouldFailTransmit(false), mHistory(), mPendingTransmits(0),
      mSimulatedTimeUs(0), mFiringCallbacks(false), mDeferredCallbackCount(0),
      mAutoComplete(false), mDeinitCount(0) {
}

LcdSpiPeripheralMockImpl::~LcdSpiPeripheralMockImpl() {}

bool LcdSpiPeripheralMockImpl::initialize(
    const LcdSpiConfig &config) FL_NOEXCEPT {
    if (config.num_lanes == 0 || config.num_lanes > 16) {
        FL_WARN("LcdSpiPeripheralMock: Invalid num_lanes: "
                << config.num_lanes);
        return false;
    }
    mConfig = config;
    mInitialized = true;
    mEnabled = true;
    return true;
}

void LcdSpiPeripheralMockImpl::deinitialize() FL_NOEXCEPT {
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingTransmits = 0;
    mDeinitCount++;
}

bool LcdSpiPeripheralMockImpl::isInitialized() const FL_NOEXCEPT {
    return mInitialized;
}

u16 *LcdSpiPeripheralMockImpl::allocateBuffer(size_t size_bytes) FL_NOEXCEPT {
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;
    void *buffer = nullptr;
#ifdef FL_IS_WIN
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif
    if (buffer == nullptr) {
        FL_WARN("LcdSpiPeripheralMock: Failed to allocate buffer ("
                << aligned_size << " bytes)");
    }
    return static_cast<u16 *>(buffer);
}

void LcdSpiPeripheralMockImpl::freeBuffer(u16 *buffer) FL_NOEXCEPT {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        fl::free(buffer);
#endif
    }
}

bool LcdSpiPeripheralMockImpl::transmit(const u16 *buffer,
                                        size_t size_bytes) FL_NOEXCEPT {
    if (!mInitialized) {
        FL_WARN("LcdSpiPeripheralMock: Cannot transmit - not initialized");
        return false;
    }
    if (mShouldFailTransmit) {
        return false;
    }

    TransmitRecord record;
    size_t word_count = size_bytes / 2;
    record.buffer_copy.resize(word_count);
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

bool LcdSpiPeripheralMockImpl::waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT {
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

bool LcdSpiPeripheralMockImpl::isBusy() const FL_NOEXCEPT { return mBusy; }

bool LcdSpiPeripheralMockImpl::registerTransmitCallback(
    void *callback, void *user_ctx) FL_NOEXCEPT {
    if (!mInitialized) {
        return false;
    }
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

const LcdSpiConfig &LcdSpiPeripheralMockImpl::getConfig() const FL_NOEXCEPT {
    return mConfig;
}

u64 LcdSpiPeripheralMockImpl::getMicroseconds() FL_NOEXCEPT {
    return mSimulatedTimeUs;
}

void LcdSpiPeripheralMockImpl::delay(u32 ms) FL_NOEXCEPT {
    mSimulatedTimeUs += static_cast<u64>(ms) * 1000;
}

void LcdSpiPeripheralMockImpl::simulateTransmitComplete() FL_NOEXCEPT {
    if (mPendingTransmits == 0) {
        return;
    }

    // Drain the entire ISR callback chain. When a callback calls transmit()
    // (to submit the next DMA chunk), mPendingTransmits increments again.
    // The loop continues until no more chunks are queued.
    // This simulates instant DMA completion for all chunks in the stream.
    //
    // For single-shot transmissions (no callback registered), the loop
    // fires once and exits — behavior is identical to the old code.
    while (mPendingTransmits > 0) {
        mPendingTransmits--;
        if (mPendingTransmits == 0) {
            mBusy = false;
        }
        if (mCallback != nullptr) {
            using CallbackType = bool (*)(void *, const void *, void *);
            auto cb =
                reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
            cb(nullptr, nullptr, mUserCtx);
        }
    }
}

void LcdSpiPeripheralMockImpl::setTransmitFailure(
    bool should_fail) FL_NOEXCEPT {
    mShouldFailTransmit = should_fail;
}

void LcdSpiPeripheralMockImpl::setTransmitDelay(
    u32 microseconds) FL_NOEXCEPT {
    mTransmitDelayUs = microseconds;
}

void LcdSpiPeripheralMockImpl::setAutoComplete(
    bool auto_complete) FL_NOEXCEPT {
    mAutoComplete = auto_complete;
}

const fl::vector<LcdSpiPeripheralMock::TransmitRecord> &
LcdSpiPeripheralMockImpl::getTransmitHistory() const FL_NOEXCEPT {
    return mHistory;
}

void LcdSpiPeripheralMockImpl::clearTransmitHistory() FL_NOEXCEPT {
    mHistory.clear();
    mPendingTransmits = 0;
    mBusy = false;
}

fl::span<const u16>
LcdSpiPeripheralMockImpl::getLastTransmitData() const FL_NOEXCEPT {
    if (mHistory.empty()) {
        return fl::span<const u16>();
    }
    return fl::span<const u16>(mHistory.back().buffer_copy);
}

bool LcdSpiPeripheralMockImpl::isEnabled() const FL_NOEXCEPT {
    return mEnabled;
}

size_t LcdSpiPeripheralMockImpl::getTransmitCount() const FL_NOEXCEPT {
    return mTransmitCount;
}

size_t LcdSpiPeripheralMockImpl::getDeinitCount() const FL_NOEXCEPT {
    return mDeinitCount;
}

void LcdSpiPeripheralMockImpl::reset() FL_NOEXCEPT {
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mTransmitCount = 0;
    mConfig = LcdSpiConfig();
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
    mDeinitCount = 0;
}

void LcdSpiPeripheralMockImpl::pumpDeferredCallbacks() FL_NOEXCEPT {
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

void LcdSpiPeripheralMockImpl::fireCallback() FL_NOEXCEPT {
    if (mPendingTransmits > 0) {
        mPendingTransmits--;
    }
    if (mPendingTransmits == 0) {
        mBusy = false;
    }
    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void *, const void *, void *);
        auto cb = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        cb(nullptr, nullptr, mUserCtx);
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || host platform
