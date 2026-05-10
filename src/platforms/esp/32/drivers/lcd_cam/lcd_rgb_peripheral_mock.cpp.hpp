// IWYU pragma: private

/// @file lcd_rgb_peripheral_mock.cpp
/// @brief Mock LCD RGB peripheral implementation for unit testing

// Synchronous mock for host testing — no background thread, no wall-clock timing.
// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.h"
#include "fl/log/log.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: begin_keep
#include "fl/stl/cstdlib.h" // ok include for aligned_alloc on POSIX
// IWYU pragma: end_keep

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

    LcdRgbPeripheralMockImpl() FL_NOEXCEPT;
    ~LcdRgbPeripheralMockImpl() override;

    //=========================================================================
    // ILcdRgbPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const LcdRgbPeripheralConfig& config) FL_NOEXCEPT override;
    void deinitialize() FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;

    u16* allocateFrameBuffer(size_t size_bytes) FL_NOEXCEPT override;
    void freeFrameBuffer(u16* buffer) FL_NOEXCEPT override;

    bool drawFrame(const u16* buffer, size_t size_bytes) FL_NOEXCEPT override;
    bool waitFrameDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool isBusy() const FL_NOEXCEPT override;

    bool registerDrawCallback(void* callback, void* user_ctx) FL_NOEXCEPT override;
    const LcdRgbPeripheralConfig& getConfig() const FL_NOEXCEPT override;

    u64 getMicroseconds() FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void simulateDrawComplete() FL_NOEXCEPT override;
    void setDrawFailure(bool should_fail) FL_NOEXCEPT override;
    void setDrawDelay(u32 microseconds) FL_NOEXCEPT override;
    const fl::vector<FrameRecord>& getFrameHistory() const FL_NOEXCEPT override;
    void clearFrameHistory() FL_NOEXCEPT override;
    fl::span<const u16> getLastFrameData() const FL_NOEXCEPT override;
    bool isEnabled() const FL_NOEXCEPT override;
    size_t getDrawCount() const FL_NOEXCEPT override;
    void reset() FL_NOEXCEPT override;

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
    u32 mDrawDelayUs;
    bool mDrawDelayForced;  // If true, use mDrawDelayUs instead of calculating from PCLK
    bool mShouldFailDraw;

    // Frame capture
    fl::vector<FrameRecord> mHistory;

    // Pending draw state
    size_t mPendingDraws;

    // Simulated time (advances only via delay calls — deterministic, no wall-clock jitter)
    u64 mSimulatedTimeUs;
    bool mFiringCallbacks;  // Re-entrancy guard
    size_t mDeferredCallbackCount;  // Count of pending callbacks to fire

    // Synchronous callback pumping (matches I2S mock pattern)
    void pumpDeferredCallbacks() FL_NOEXCEPT;
    void fireCallback() FL_NOEXCEPT;
};

//=============================================================================
// Singleton Instance
//=============================================================================

LcdRgbPeripheralMock& LcdRgbPeripheralMock::instance() FL_NOEXCEPT {
    return Singleton<LcdRgbPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

LcdRgbPeripheralMockImpl::LcdRgbPeripheralMockImpl() FL_NOEXCEPT
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
      mSimulatedTimeUs(0),
      mFiringCallbacks(false),
      mDeferredCallbackCount(0) {
}

LcdRgbPeripheralMockImpl::~LcdRgbPeripheralMockImpl() {
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool LcdRgbPeripheralMockImpl::initialize(const LcdRgbPeripheralConfig& config) FL_NOEXCEPT {
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

void LcdRgbPeripheralMockImpl::deinitialize() FL_NOEXCEPT {
    mInitialized = false;
    mEnabled = false;
    mBusy = false;
    mPendingDraws = 0;
    mDeferredCallbackCount = 0;
}

bool LcdRgbPeripheralMockImpl::isInitialized() const FL_NOEXCEPT {
    return mInitialized;
}

//=============================================================================
// Buffer Management
//=============================================================================

u16* LcdRgbPeripheralMockImpl::allocateFrameBuffer(size_t size_bytes) FL_NOEXCEPT {
    // Round up to 64-byte alignment
    size_t aligned_size = ((size_bytes + 63) / 64) * 64;

    void* buffer = nullptr;
#ifdef FL_IS_WIN
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("LcdRgbPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<u16*>(buffer);
}

void LcdRgbPeripheralMockImpl::freeFrameBuffer(u16* buffer) FL_NOEXCEPT {
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

bool LcdRgbPeripheralMockImpl::drawFrame(const u16* buffer, size_t size_bytes) FL_NOEXCEPT {
    if (!mInitialized) {
        FL_WARN("LcdRgbPeripheralMock: Cannot draw - not initialized");
        return false;
    }

    if (mShouldFailDraw) {
        return false;
    }

    // Calculate draw delay - use forced value if set, otherwise calculate from PCLK
    u32 draw_delay_us;
    if (mDrawDelayForced) {
        draw_delay_us = mDrawDelayUs;
    } else if (mConfig.pclk_hz > 0) {
        // Pixels = size_bytes / 2 (16-bit pixels)
        size_t pixels = size_bytes / 2;
        // Time = pixels / pclk_hz (seconds) * 1000000 (microseconds)
        u64 draw_time_us = (static_cast<u64>(pixels) * 1000000ULL) / mConfig.pclk_hz;
        draw_delay_us = static_cast<u32>(draw_time_us) + 10;
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
    record.timestamp_us = mSimulatedTimeUs;

    mHistory.push_back(fl::move(record));

    // Queue deferred callback — will be fired synchronously via pumpDeferredCallbacks
    mDrawCount++;
    mBusy = true;
    mPendingDraws++;
    mDeferredCallbackCount++;

    // If we're not already inside a callback chain, pump now
    pumpDeferredCallbacks();

    return true;
}

bool LcdRgbPeripheralMockImpl::waitFrameDone(u32 timeout_ms) FL_NOEXCEPT {
    if (!mInitialized) {
        return false;
    }

    // In the synchronous mock, everything completes immediately when drawFrame() is called.
    // waitFrameDone just returns the completion status.
    (void)timeout_ms;  // timeout not used in synchronous mock
    if (mPendingDraws == 0) {
        mBusy = false;
        return true;
    }

    return false;
}

bool LcdRgbPeripheralMockImpl::isBusy() const FL_NOEXCEPT {
    return mBusy;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool LcdRgbPeripheralMockImpl::registerDrawCallback(void* callback, void* user_ctx) FL_NOEXCEPT {
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

const LcdRgbPeripheralConfig& LcdRgbPeripheralMockImpl::getConfig() const FL_NOEXCEPT {
    return mConfig;
}

u64 LcdRgbPeripheralMockImpl::getMicroseconds() FL_NOEXCEPT {
    return mSimulatedTimeUs;
}

void LcdRgbPeripheralMockImpl::delay(u32 ms) FL_NOEXCEPT {
    mSimulatedTimeUs += static_cast<u64>(ms) * 1000;
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void LcdRgbPeripheralMockImpl::simulateDrawComplete() FL_NOEXCEPT {
    if (mPendingDraws == 0) {
        return;
    }

    // Manually complete one pending draw and fire its callback
    fireCallback();
}

void LcdRgbPeripheralMockImpl::setDrawFailure(bool should_fail) FL_NOEXCEPT {
    mShouldFailDraw = should_fail;
}

void LcdRgbPeripheralMockImpl::setDrawDelay(u32 microseconds) FL_NOEXCEPT {
    mDrawDelayUs = microseconds;
    mDrawDelayForced = true;  // Mark as explicitly set - don't recalculate in drawFrame()
}

const fl::vector<LcdRgbPeripheralMock::FrameRecord>& LcdRgbPeripheralMockImpl::getFrameHistory() const FL_NOEXCEPT {
    return mHistory;
}

void LcdRgbPeripheralMockImpl::clearFrameHistory() FL_NOEXCEPT {
    mHistory.clear();
    mPendingDraws = 0;
    mDeferredCallbackCount = 0;
    mBusy = false;
}

fl::span<const u16> LcdRgbPeripheralMockImpl::getLastFrameData() const FL_NOEXCEPT {
    if (mHistory.empty()) {
        return fl::span<const u16>();
    }
    return fl::span<const u16>(mHistory.back().buffer_copy);
}

bool LcdRgbPeripheralMockImpl::isEnabled() const FL_NOEXCEPT {
    return mEnabled;
}

size_t LcdRgbPeripheralMockImpl::getDrawCount() const FL_NOEXCEPT {
    return mDrawCount;
}

void LcdRgbPeripheralMockImpl::reset() FL_NOEXCEPT {
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
    mSimulatedTimeUs = 0;
    mFiringCallbacks = false;
    mDeferredCallbackCount = 0;
}

//=============================================================================
// Synchronous Callback Pumping
//=============================================================================

void LcdRgbPeripheralMockImpl::pumpDeferredCallbacks() FL_NOEXCEPT {
    // Re-entrancy guard: if we're already firing callbacks (from within
    // a callback that called drawFrame()), just let the outer loop handle it.
    if (mFiringCallbacks) {
        return;
    }
    mFiringCallbacks = true;

    // Process all deferred callbacks. Each callback may trigger more
    // drawFrame() calls which queue more callbacks, so loop until empty.
    while (mDeferredCallbackCount > 0) {
        mDeferredCallbackCount--;
        fireCallback();
    }

    mFiringCallbacks = false;
}

void LcdRgbPeripheralMockImpl::fireCallback() FL_NOEXCEPT {
    if (mPendingDraws > 0) {
        mPendingDraws--;
    }

    if (mPendingDraws == 0) {
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
