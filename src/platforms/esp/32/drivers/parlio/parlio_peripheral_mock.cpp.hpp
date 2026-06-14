// IWYU pragma: private

/// @file parlio_peripheral_mock.cpp
/// @brief Mock PARLIO peripheral implementation for unit testing
///
/// Fully synchronous mock - no threads, no real-time delays.
/// transmit() immediately fires the ISR callback.
/// delay()/delayMicroseconds() are instant (advance simulated time).

#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#include "fl/log/log.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/stl/singleton.h"
#include "fl/stl/map.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: begin_keep
#include "fl/stl/cstdlib.h" // ok include for aligned_alloc on POSIX
// IWYU pragma: end_keep


namespace {
/// @brief Untranspose interleaved bit-parallel data to per-pin waveforms (internal helper)
fl::vector<fl::vector<fl::u8>> untransposeParlioBitstreamInternal(
    const fl::u8* transposed_data,
    size_t bit_count,
    size_t num_pins,
    fl::detail::ParlioBitPackOrder packing) FL_NOEXCEPT {

    fl::vector<fl::vector<fl::u8>> per_pin_data(num_pins);

    size_t bits_per_pin = bit_count / num_pins;
    size_t bytes_per_pin = (bits_per_pin + 7) / 8;

    for (size_t pin = 0; pin < num_pins; pin++) {
        per_pin_data[pin].resize(bytes_per_pin, 0);
    }

    for (size_t bit_idx = 0; bit_idx < bit_count; bit_idx++) {
        size_t byte_idx = bit_idx / 8;
        size_t bit_pos;
        if (packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB) {
            bit_pos = bit_idx % 8;
        } else {
            bit_pos = 7 - (bit_idx % 8);
        }
        bool bit_value = (transposed_data[byte_idx] >> bit_pos) & 1;

        size_t hardware_pin_idx = bit_idx % num_pins;
        size_t lane_idx = hardware_pin_idx;
        size_t lane_bit_idx = bit_idx / num_pins;
        size_t lane_byte_idx = lane_bit_idx / 8;
        size_t lane_bit_pos = lane_bit_idx % 8;

        if (bit_value) {
            per_pin_data[lane_idx][lane_byte_idx] |= (1 << lane_bit_pos);
        }
    }
    return per_pin_data;
}
} // anonymous namespace


namespace fl {
namespace detail {

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Synchronous mock PARLIO peripheral for unit testing.
/// No threads, no real delays. transmit() fires ISR callback immediately.
class ParlioPeripheralMockImpl : public ParlioPeripheralMock {
public:
    ParlioPeripheralMockImpl() FL_NOEXCEPT;
    ~ParlioPeripheralMockImpl() override;

    // IParlioPeripheral Interface
    bool initialize(const ParlioPeripheralConfig& config) FL_NOEXCEPT override;
    bool deinitialize() FL_NOEXCEPT override;
    bool enable() FL_NOEXCEPT override;
    bool disable() FL_NOEXCEPT override;
    bool transmit(const u8* buffer, size_t bit_count, u16 idle_value) FL_NOEXCEPT override;
    bool waitAllDone(u32 timeout_ms) FL_NOEXCEPT override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) FL_NOEXCEPT override;
    u8* allocateDmaBuffer(size_t size) FL_NOEXCEPT override;
    void freeDmaBuffer(u8* buffer) FL_NOEXCEPT override;
    void delay(u32 ms) FL_NOEXCEPT override;
    void delayMicroseconds(u32 us) FL_NOEXCEPT override;
    u32 millis() FL_NOEXCEPT override;
    u64 getMicroseconds() FL_NOEXCEPT override;
    void freeDmaBuffer(void* ptr) FL_NOEXCEPT override;

    // Mock-Specific API
    void setTransmitDelay(u32 microseconds) FL_NOEXCEPT override;
    void simulateTransmitComplete() FL_NOEXCEPT override;
    void setTransmitFailure(bool should_fail) FL_NOEXCEPT override;
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const FL_NOEXCEPT override;
    void clearTransmissionHistory() FL_NOEXCEPT override;
    fl::span<const u8> getTransmissionDataForPin(int gpio_pin) const FL_NOEXCEPT override;
    bool isInitialized() const FL_NOEXCEPT override;
    bool isEnabled() const FL_NOEXCEPT override;
    bool isTransmitting() const FL_NOEXCEPT override;
    size_t getTransmitCount() const FL_NOEXCEPT override;
    const ParlioPeripheralConfig& getConfig() const FL_NOEXCEPT override;
    void reset() FL_NOEXCEPT override;

private:
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
    u32 mTransmitDelayUs;
    bool mShouldFailTransmit;

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;
    fl::fl_map<int, fl::vector<u8>> mPerPinData;

    // Pending transmission count (decremented by ISR callback or simulateTransmitComplete)
    size_t mPendingTransmissions;

    // Simulated time (microseconds) — advances with delay/delayMicroseconds calls
    u64 mSimulatedTimeUs;

    // Deferred callback queue: transmit() queues callbacks, they fire after transmit returns.
    // This prevents recursive transmit→callback→transmit chains.
    size_t mDeferredCallbackCount;
    bool mFiringCallbacks;  // Re-entrancy guard

    // Re-entrancy guard for settleEngineState(), which calls
    // ParlioEngine::poll() — and poll() itself calls waitAllDone(0), which
    // can re-enter settleEngineState().
    bool mSettlingEngine;

    /// Fire the ISR callback synchronously
    void fireCallback() FL_NOEXCEPT;

    /// Pump all deferred callbacks (called from delay/poll points)
    void pumpDeferredCallbacks() FL_NOEXCEPT;

    /// Drive ParlioEngine::poll() once to settle the engine's async state
    /// (mTransmitting / mStreamComplete) at a transmit boundary. See the
    /// comment in waitAllDone() for why this is needed in the
    /// post-#2992 async engine.
    void settleEngineState() FL_NOEXCEPT;
};

//=============================================================================
// Singleton Instance
//=============================================================================

ParlioPeripheralMock& ParlioPeripheralMock::instance() FL_NOEXCEPT {
    return Singleton<ParlioPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

ParlioPeripheralMockImpl::ParlioPeripheralMockImpl() FL_NOEXCEPT
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
      mPerPinData(),
      mPendingTransmissions(0),
      mSimulatedTimeUs(0),
      mDeferredCallbackCount(0),
      mFiringCallbacks(false),
      mSettlingEngine(false) {
}

ParlioPeripheralMockImpl::~ParlioPeripheralMockImpl() {
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool ParlioPeripheralMockImpl::initialize(const ParlioPeripheralConfig& config) FL_NOEXCEPT {
    if (config.data_width == 0 || config.data_width > 16) {
        FL_WARN("ParlioPeripheralMock: Invalid data width: " << config.data_width);
        return false;
    }
    mConfig = config;
    mInitialized = true;
    return true;
}

bool ParlioPeripheralMockImpl::deinitialize() FL_NOEXCEPT {
    // Settle engine state before tearing the peripheral down. The engine's
    // own initialize() re-init path (parlio_engine.cpp.hpp:1411) hits
    // deinitialize() between transmissions without first calling poll(), so
    // this is the natural place to clear any leftover async state. See
    // waitAllDone() for the full rationale.
    settleEngineState();
    mInitialized = false;
    mEnabled = false;
    return true;
}

bool ParlioPeripheralMockImpl::enable() FL_NOEXCEPT {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot enable - not initialized");
        return false;
    }
    mEnabled = true;
    return true;
}

bool ParlioPeripheralMockImpl::disable() FL_NOEXCEPT {
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

bool ParlioPeripheralMockImpl::transmit(const u8* buffer, size_t bit_count, u16 idle_value) FL_NOEXCEPT {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not initialized");
        return false;
    }

    if (!mEnabled) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not enabled");
        return false;
    }

    if (mShouldFailTransmit) {
        return false;
    }

    size_t byte_count = (bit_count + 7) / 8;

    // Capture transmission data
    TransmissionRecord record;
    record.buffer_copy.resize(byte_count);
    fl::memcpy(record.buffer_copy.data(), buffer, byte_count);
    record.bit_count = bit_count;
    record.idle_value = idle_value;
    record.timestamp_us = mSimulatedTimeUs;
    mHistory.push_back(fl::move(record));

    // Untranspose to extract per-pin waveforms
    fl::vector<fl::vector<u8>> per_pin_waveforms = untransposeParlioBitstreamInternal(
        buffer, bit_count, mConfig.data_width, mConfig.packing);

    mPerPinData.clear();
    for (size_t i = 0; i < per_pin_waveforms.size() && i < static_cast<size_t>(mConfig.data_width); i++) {
        int gpio_pin = mConfig.gpio_pins[i];
        mPerPinData[gpio_pin] = fl::move(per_pin_waveforms[i]);
    }

    mTransmitCount++;
    mTransmitting = true;
    mPendingTransmissions++;

    // Queue a deferred callback instead of firing recursively
    mDeferredCallbackCount++;

    // If we're not already inside a callback chain, pump now
    pumpDeferredCallbacks();

    return true;
}

bool ParlioPeripheralMockImpl::waitAllDone(u32 timeout_ms) FL_NOEXCEPT {
    (void)timeout_ms;
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot wait - not initialized");
        return false;
    }
    // GAP CLOSURE (#2992 follow-up):
    //
    // Real ESP-IDF parlio_tx_unit_wait_all_done() blocks until the DMA queue
    // drains and the per-buffer txDone ISRs fire. After #2992 the engine
    // moved mTransmitting clearing out of the txDone ISR and into poll() —
    // so on real hardware too, returning from waitAllDone() does NOT by
    // itself clear mTransmitting; a poll() tick is also required.
    //
    // The mock has always fired txDone callbacks synchronously inside
    // transmit(), so by the time we get here every callback has run and
    // mStreamComplete is set. The only piece missing is the poll() tick.
    // Drive it here so consecutive beginTransmission() calls don't observe
    // a stuck mTransmitting=true from the previous frame.
    settleEngineState();
    mTransmitting = false;
    return mPendingTransmissions == 0;
}

void ParlioPeripheralMockImpl::settleEngineState() FL_NOEXCEPT {
    // Single-threaded re-entrancy guard: ParlioEngine::poll() calls
    // waitAllDone(0), which would call us again.
    if (mSettlingEngine) {
        return;
    }
    mSettlingEngine = true;
    // One poll() tick is sufficient when mStreamComplete is already set
    // (the common path here); when nothing is pending it's a cheap no-op.
    ParlioEngine::getInstance().poll();
    mSettlingEngine = false;
}

//=============================================================================
// ISR Callback
//=============================================================================

bool ParlioPeripheralMockImpl::registerTxDoneCallback(void* callback, void* user_ctx) FL_NOEXCEPT {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot register callback - not initialized");
        return false;
    }
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

void ParlioPeripheralMockImpl::fireCallback() FL_NOEXCEPT {
    if (mPendingTransmissions > 0) {
        mPendingTransmissions--;
    }
    if (mPendingTransmissions == 0) {
        mTransmitting = false;
    }

    if (mCallback != nullptr) {
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void ParlioPeripheralMockImpl::pumpDeferredCallbacks() FL_NOEXCEPT {
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

//=============================================================================
// DMA Memory Management
//=============================================================================

u8* ParlioPeripheralMockImpl::allocateDmaBuffer(size_t size) FL_NOEXCEPT {
    size_t aligned_size = ((size + 63) / 64) * 64;
    void* buffer = nullptr;

#ifdef FL_IS_WIN
    buffer = _aligned_malloc(aligned_size, 64);
#else
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }
    return static_cast<u8*>(buffer);
}

void ParlioPeripheralMockImpl::freeDmaBuffer(u8* buffer) FL_NOEXCEPT {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        fl::free(buffer);
#endif
    }
}

//=============================================================================
// Time and Delay — Simulated (instant, no real sleeping)
//=============================================================================

void ParlioPeripheralMockImpl::delay(u32 ms) FL_NOEXCEPT {
    mSimulatedTimeUs += static_cast<u64>(ms) * 1000;
}

void ParlioPeripheralMockImpl::delayMicroseconds(u32 us) FL_NOEXCEPT {
    mSimulatedTimeUs += us;
}

u32 ParlioPeripheralMockImpl::millis() FL_NOEXCEPT {
    return static_cast<u32>(mSimulatedTimeUs / 1000);
}

u64 ParlioPeripheralMockImpl::getMicroseconds() FL_NOEXCEPT {
    return mSimulatedTimeUs;
}

void ParlioPeripheralMockImpl::freeDmaBuffer(void* ptr) FL_NOEXCEPT {
    if (ptr) {
        fl::free(ptr);
    }
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void ParlioPeripheralMockImpl::setTransmitDelay(u32 microseconds) FL_NOEXCEPT {
    mTransmitDelayUs = microseconds;
}

void ParlioPeripheralMockImpl::simulateTransmitComplete() FL_NOEXCEPT {
    fireCallback();
}

void ParlioPeripheralMockImpl::setTransmitFailure(bool should_fail) FL_NOEXCEPT {
    mShouldFailTransmit = should_fail;
}

const fl::vector<ParlioPeripheralMock::TransmissionRecord>&
ParlioPeripheralMockImpl::getTransmissionHistory() const FL_NOEXCEPT {
    return mHistory;
}

bool ParlioPeripheralMockImpl::isInitialized() const FL_NOEXCEPT {
    return mInitialized;
}

bool ParlioPeripheralMockImpl::isEnabled() const FL_NOEXCEPT {
    return mEnabled;
}

bool ParlioPeripheralMockImpl::isTransmitting() const FL_NOEXCEPT {
    return mTransmitting;
}

size_t ParlioPeripheralMockImpl::getTransmitCount() const FL_NOEXCEPT {
    return mTransmitCount;
}

const ParlioPeripheralConfig& ParlioPeripheralMockImpl::getConfig() const FL_NOEXCEPT {
    return mConfig;
}

void ParlioPeripheralMockImpl::clearTransmissionHistory() FL_NOEXCEPT {
    mHistory.clear();
    mPerPinData.clear();
    mPendingTransmissions = 0;
    mTransmitting = false;
}

fl::span<const u8> ParlioPeripheralMockImpl::getTransmissionDataForPin(int gpio_pin) const FL_NOEXCEPT {
    if (mPerPinData.empty()) {
        return fl::span<const u8>();
    }
    auto it = mPerPinData.find(gpio_pin);
    if (it == mPerPinData.end()) {
        FL_WARN("ParlioPeripheralMock: GPIO pin " << gpio_pin << " not found in transmission data");
        return fl::span<const u8>();
    }
    return fl::span<const u8>(it->second);
}

void ParlioPeripheralMockImpl::reset() FL_NOEXCEPT {
    mInitialized = false;
    mEnabled = false;
    mTransmitting = false;
    mTransmitCount = 0;

    fl::vector<int> empty_pins;
    mConfig = ParlioPeripheralConfig(empty_pins, 0, 0, 0, ParlioBitPackOrder::FL_PARLIO_MSB);

    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransmitDelayUs = 0;
    mShouldFailTransmit = false;
    mHistory.clear();
    mPerPinData.clear();
    mPendingTransmissions = 0;
    mSimulatedTimeUs = 0;
    mDeferredCallbackCount = 0;
    mFiringCallbacks = false;
    mSettlingEngine = false;
}

//=============================================================================
// Public Untranspose Function
//=============================================================================

fl::vector<fl::pair<int, fl::vector<u8>>> ParlioPeripheralMock::untransposeParlioBitstream(
    fl::span<const u8> transposed_data,
    fl::span<const int> pins,
    ParlioBitPackOrder packing) FL_NOEXCEPT {

    fl::vector<fl::pair<int, fl::vector<u8>>> result;

    if (transposed_data.empty() || pins.empty()) {
        return result;
    }

    size_t num_pins = pins.size();
    size_t bit_count = transposed_data.size() * 8;

    fl::vector<fl::vector<u8>> per_pin_waveforms = untransposeParlioBitstreamInternal(
        transposed_data.data(), bit_count, num_pins, packing);

    for (size_t i = 0; i < per_pin_waveforms.size() && i < pins.size(); i++) {
        int gpio_pin = pins[i];
        fl::pair<int, fl::vector<u8>> pair;
        pair.first = gpio_pin;
        pair.second = fl::move(per_pin_waveforms[i]);
        result.emplace_back(pair);
    }

    return result;
}

//=============================================================================
// Global Cleanup Function
//=============================================================================

void cleanup_parlio_mock() FL_NOEXCEPT {
    ParlioEngine::getInstance().cleanup();
    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();
}

} // namespace detail
} // namespace fl


#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
