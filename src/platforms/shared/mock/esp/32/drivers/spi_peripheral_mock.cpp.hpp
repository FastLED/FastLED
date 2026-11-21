// IWYU pragma: private

/// @file spi_peripheral_mock.cpp
/// @brief Mock SPI peripheral implementation for unit testing
///
/// This is a synchronous mock that requires manual event pumping via
/// simulateTransactionComplete(). No background threads are used.

// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "platforms/shared/mock/esp/32/drivers/spi_peripheral_mock.h"
#include "fl/log/log.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/malloc.h"
#include "fl/stl/singleton.h"

#ifdef FL_IS_WIN
// IWYU pragma: begin_keep
#include <malloc.h>  // ok include - For _aligned_malloc/_aligned_free on Windows
// IWYU pragma: end_keep
#else
// IWYU pragma: begin_keep
#include <stdlib.h>  // For aligned_alloc on POSIX
// IWYU pragma: end_keep
#endif

#ifdef ARDUINO
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep
#else
#include "platforms/stub/time_stub.h"  // For fl::micros() and delay() on host tests
#include "fl/stl/noexcept.h"
#endif

namespace fl {
namespace detail {

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of SpiPeripheralMock
///
/// This class contains all the actual implementation details.
/// It simulates SPI hardware behavior for unit testing.
/// Fully synchronous - no background threads.
class SpiPeripheralMockImpl : public SpiPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    SpiPeripheralMockImpl() FL_NO_EXCEPT;
    ~SpiPeripheralMockImpl() override = default;

    //=========================================================================
    // ISpiPeripheral Interface Implementation
    //=========================================================================

    bool initializeBus(const SpiBusConfig& config) FL_NO_EXCEPT override;
    bool addDevice(const SpiDeviceConfig& config) FL_NO_EXCEPT override;
    bool removeDevice() FL_NO_EXCEPT override;
    bool freeBus() FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;
    bool queueTransaction(const SpiTransaction& trans) FL_NO_EXCEPT override;
    bool pollTransaction(u32 timeout_ms, void** out_user) FL_NO_EXCEPT override;
    bool pollingStart(const SpiTransaction& trans) FL_NO_EXCEPT override;
    bool pollingEnd(u32 timeout_ms) FL_NO_EXCEPT override;
    int getActualFrequencyKhz() const FL_NO_EXCEPT override;
    bool registerCallback(void* callback, void* user_ctx) FL_NO_EXCEPT override;
    u8* allocateDma(size_t size) FL_NO_EXCEPT override;
    void freeDma(u8* buffer) FL_NO_EXCEPT override;
    u8* allocateInternal(size_t size) FL_NO_EXCEPT override;
    void freeInternal(u8* buffer) FL_NO_EXCEPT override;
    void resetPin(int pin) FL_NO_EXCEPT override;
    u32 getPinRouting(int pin) FL_NO_EXCEPT override;
    void delay(u32 ms) FL_NO_EXCEPT override;
    u64 getMicroseconds() FL_NO_EXCEPT override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void setTransactionDelay(u32 microseconds) FL_NO_EXCEPT override;
    void simulateTransactionComplete() FL_NO_EXCEPT override;
    void setTransactionFailure(bool should_fail) FL_NO_EXCEPT override;
    const fl::vector<TransactionRecord>& getTransactionHistory() const FL_NO_EXCEPT override;
    void clearTransactionHistory() FL_NO_EXCEPT override;
    fl::span<const u8> getLastTransactionData() const FL_NO_EXCEPT override;
    bool hasDevice() const FL_NO_EXCEPT override;
    bool canQueueTransaction() const FL_NO_EXCEPT override;
    size_t getQueuedTransactionCount() const FL_NO_EXCEPT override;
    size_t getTransactionCount() const FL_NO_EXCEPT override;
    const SpiBusConfig& getBusConfig() const FL_NO_EXCEPT override;
    const SpiDeviceConfig& getDeviceConfig() const FL_NO_EXCEPT override;
    size_t getBusInitializeCount() const FL_NO_EXCEPT override;
    size_t getBusFreeCount() const FL_NO_EXCEPT override;
    size_t getDeviceRemoveCount() const FL_NO_EXCEPT override;
    void simulatePinReroute(int pin) FL_NO_EXCEPT override;
    void reset() FL_NO_EXCEPT override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mDeviceAdded;
    SpiBusConfig mBusConfig;
    SpiDeviceConfig mDeviceConfig;

    // Transaction queue (in flight) and completed transactions not yet
    // returned by pollTransaction()
    fl::vector<SpiTransaction> mQueuedTransactions;
    fl::vector<SpiTransaction> mCompletedTransactions;
    size_t mMaxQueueSize;
    size_t mTransactionCount;

    // Post-transaction callback
    void* mCallback;
    void* mUserCtx;

    // Simulation settings
    u32 mTransactionDelayUs;
    bool mShouldFailTransaction;

    // Waveform capture
    fl::vector<TransactionRecord> mHistory;

    // Pending transaction state
    size_t mPendingTransactions;

    // Bus lifecycle counters
    size_t mBusInitializeCount;
    size_t mBusFreeCount;
    size_t mDeviceRemoveCount;

    // Pin routing (pin -> routing snapshot)
    fl::flat_map<int, u32> mPinRouting;  // fl-lint: int-template-ok mirrors ESP-IDF int GPIO pin numbers
    static constexpr u32 kSpiRouting = 0x100u;
    static constexpr u32 kOtherPeripheral = 0x8000u;
};

//=============================================================================
// Singleton Instance
//=============================================================================

SpiPeripheralMock& SpiPeripheralMock::instance() FL_NO_EXCEPT {
    return Singleton<SpiPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor
//=============================================================================

SpiPeripheralMockImpl::SpiPeripheralMockImpl()
 FL_NO_EXCEPT : mInitialized(false),
      mDeviceAdded(false),
      mBusConfig(),
      mDeviceConfig(),
      mQueuedTransactions(),
      mCompletedTransactions(),
      mMaxQueueSize(0),
      mTransactionCount(0),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mTransactionDelayUs(0),
      mShouldFailTransaction(false),
      mHistory(),
      mPendingTransactions(0),
      mBusInitializeCount(0),
      mBusFreeCount(0),
      mDeviceRemoveCount(0),
      mPinRouting() {
    // No background thread - fully synchronous mock
}

//=============================================================================
// Bus Lifecycle Methods
//=============================================================================

bool SpiPeripheralMockImpl::initializeBus(const SpiBusConfig& config) FL_NO_EXCEPT {
    if (mInitialized) {
        FL_WARN("SpiPeripheralMock: Already initialized");
        return false;
    }

    // Validate config: a bus needs a data line. SCLK may be -1 (the clockless
    // driver keeps the clock internal, like Espressif's led_strip).
    if (config.mosi_pin < 0 && config.miso_pin < 0) {
        FL_WARN("SpiPeripheralMock: No data pin (MOSI and MISO both unset)");
        return false;
    }

    // Store configuration
    mBusConfig = config;
    mInitialized = true;
    mBusInitializeCount++;
    if (config.mosi_pin >= 0) {
        mPinRouting[config.mosi_pin] = kSpiRouting;
    }

    return true;
}

bool SpiPeripheralMockImpl::addDevice(const SpiDeviceConfig& config) FL_NO_EXCEPT {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot add device - bus not initialized");
        return false;
    }

    if (mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Device already added");
        return false;
    }

    // Validate config
    if (config.clock_speed_hz <= 0) {
        FL_WARN("SpiPeripheralMock: Invalid clock speed: " << config.clock_speed_hz);
        return false;
    }

    if (config.queue_size <= 0) {
        FL_WARN("SpiPeripheralMock: Invalid queue size: " << config.queue_size);
        return false;
    }

    // Store configuration
    mDeviceConfig = config;
    mMaxQueueSize = static_cast<size_t>(config.queue_size);
    mDeviceAdded = true;

    return true;
}

bool SpiPeripheralMockImpl::removeDevice() FL_NO_EXCEPT {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: No device to remove");
        return false;
    }

    mDeviceAdded = false;
    mQueuedTransactions.clear();
    mCompletedTransactions.clear();
    mDeviceRemoveCount++;
    return true;
}

bool SpiPeripheralMockImpl::freeBus() FL_NO_EXCEPT {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Bus not initialized");
        return false;
    }

    if (mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Must remove device before freeing bus");
        return false;
    }

    mInitialized = false;
    mBusFreeCount++;
    return true;
}

bool SpiPeripheralMockImpl::isInitialized() const FL_NO_EXCEPT {
    return mInitialized;
}

//=============================================================================
// Transaction Methods
//=============================================================================

bool SpiPeripheralMockImpl::queueTransaction(const SpiTransaction& trans) FL_NO_EXCEPT {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot queue transaction - not initialized");
        return false;
    }

    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Cannot queue transaction - no device added");
        return false;
    }

    // Check for injected failure
    if (mShouldFailTransaction) {
        return false;
    }

    // Check queue capacity
    if (mQueuedTransactions.size() >= mMaxQueueSize) {
        FL_WARN("SpiPeripheralMock: Transaction queue full (" << mMaxQueueSize << ")");
        return false;
    }

    // Calculate buffer size in bytes
    size_t byte_count = (trans.length_bits + 7) / 8;

    // Capture transaction data (copy buffer for later inspection)
    TransactionRecord record;
    if (trans.tx_buffer != nullptr && byte_count > 0) {
        record.buffer_copy.resize(byte_count);
        fl::memcpy(record.buffer_copy.data(), trans.tx_buffer, byte_count);
    }
    record.length_bits = trans.length_bits;
    record.flags = trans.flags;
    record.user = trans.user;
    record.timestamp_us = fl::micros();

    // Store in history
    mHistory.push_back(fl::move(record));

    // Queue transaction for processing
    mQueuedTransactions.push_back(trans);

    // Update state
    mTransactionCount++;
    mPendingTransactions++;

    // Tests must call simulateTransactionComplete() to trigger callbacks

    return true;
}

bool SpiPeripheralMockImpl::pollTransaction(u32 timeout_ms, void** out_user) FL_NO_EXCEPT {
    (void)timeout_ms;  // Unused in synchronous mock

    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot poll - not initialized");
        return false;
    }

    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Cannot poll - no device added");
        return false;
    }

    // In synchronous mock, poll returns the oldest transaction completed via
    // simulateTransactionComplete()
    if (mCompletedTransactions.empty()) {
        return false;
    }

    if (out_user != nullptr) {
        *out_user = mCompletedTransactions.front().user;
    }
    mCompletedTransactions.erase(mCompletedTransactions.begin());
    return true;
}

bool SpiPeripheralMockImpl::pollingStart(const SpiTransaction& trans) FL_NO_EXCEPT {
    return queueTransaction(trans);
}

bool SpiPeripheralMockImpl::pollingEnd(u32 timeout_ms) FL_NO_EXCEPT {
    return pollTransaction(timeout_ms, nullptr);
}

int SpiPeripheralMockImpl::getActualFrequencyKhz() const FL_NO_EXCEPT {
    return mDeviceAdded ? mDeviceConfig.clock_speed_hz / 1000 : 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool SpiPeripheralMockImpl::registerCallback(void* callback, void* user_ctx) FL_NO_EXCEPT {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot register callback - not initialized");
        return false;
    }

    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

u8* SpiPeripheralMockImpl::allocateDma(size_t size) FL_NO_EXCEPT {
    // Round up to 4-byte multiple (SPI DMA alignment requirement)
    size_t aligned_size = ((size + 3) / 4) * 4;

    // Allocate regular heap memory (no DMA requirement on host)
    // Use aligned_alloc for 4-byte alignment (matches real hardware)
    void* buffer = nullptr;

    // Alignment must be at least sizeof(void*) for posix_memalign
    // Use 8-byte alignment which satisfies both 4-byte SPI requirement
    // and POSIX alignment constraints on 64-bit systems
    constexpr size_t kAlignment = sizeof(void*) >= 4 ? sizeof(void*) : 4;

#ifdef FL_IS_WIN
    // Windows: Use _aligned_malloc
    buffer = _aligned_malloc(aligned_size, kAlignment);
#else
    // POSIX: Use posix_memalign (alignment must be power of 2 and >= sizeof(void*))
    if (posix_memalign(&buffer, kAlignment, aligned_size) != 0) {
        buffer = nullptr; // Ensure buffer is null on failure
    }
#endif

    if (buffer == nullptr) {
        FL_WARN("SpiPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<u8*>(buffer);
}

void SpiPeripheralMockImpl::freeDma(u8* buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        fl::free(buffer);
#endif
    }
}

u8* SpiPeripheralMockImpl::allocateInternal(size_t size) FL_NO_EXCEPT {
    return static_cast<u8*>(fl::malloc(size));
}

void SpiPeripheralMockImpl::freeInternal(u8* buffer) FL_NO_EXCEPT {
    fl::free(buffer);
}

//=============================================================================
// Pin Routing
//=============================================================================

void SpiPeripheralMockImpl::resetPin(int pin) FL_NO_EXCEPT {
    mPinRouting.erase(pin);
}

u32 SpiPeripheralMockImpl::getPinRouting(int pin) FL_NO_EXCEPT {
    auto it = mPinRouting.find(pin);
    return it == mPinRouting.end() ? kPinRoutingUnknown : it->second;
}

//=============================================================================
// Platform Utilities
//=============================================================================

void SpiPeripheralMockImpl::delay(u32 ms) FL_NO_EXCEPT {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    fl::delay(ms);
}

u64 SpiPeripheralMockImpl::getMicroseconds() FL_NO_EXCEPT {
    // Use the same timestamp source as queueTransaction() for consistency
    return fl::micros();
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void SpiPeripheralMockImpl::setTransactionDelay(u32 microseconds) FL_NO_EXCEPT {
    mTransactionDelayUs = microseconds;
}

void SpiPeripheralMockImpl::simulateTransactionComplete() FL_NO_EXCEPT {
    if (mPendingTransactions == 0) {
        // No pending transactions - nothing to complete
        return;
    }

    // Decrement pending count
    mPendingTransactions--;

    // Move oldest transaction from the queue to the completed list (FIFO)
    if (!mQueuedTransactions.empty()) {
        mCompletedTransactions.push_back(mQueuedTransactions.front());
        mQueuedTransactions.erase(mQueuedTransactions.begin());
    }

    // Fire post-transaction callback (if registered)
    if (mCallback != nullptr) {
        // Cast callback to correct signature
        // void (*callback)(spi_transaction_t* trans)
        // For mock purposes, we pass nullptr since we don't have real ESP-IDF transaction structure
        using CallbackType = void (*)(void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback);  // ok reinterpret cast

        // Call with nullptr transaction pointer (mock doesn't have ESP-IDF transaction)
        callback_fn(nullptr);
    }
}

void SpiPeripheralMockImpl::setTransactionFailure(bool should_fail) FL_NO_EXCEPT {
    mShouldFailTransaction = should_fail;
}

const fl::vector<SpiPeripheralMock::TransactionRecord>&
SpiPeripheralMockImpl::getTransactionHistory() const FL_NO_EXCEPT {
    return mHistory;
}

void SpiPeripheralMockImpl::clearTransactionHistory() FL_NO_EXCEPT {
    mHistory.clear();
    mTransactionCount = 0;
    mPendingTransactions = 0;
    mQueuedTransactions.clear();
    mCompletedTransactions.clear();
}

fl::span<const u8> SpiPeripheralMockImpl::getLastTransactionData() const FL_NO_EXCEPT {
    if (mHistory.empty()) {
        return fl::span<const u8>();
    }

    const auto& last_record = mHistory.back();
    return fl::span<const u8>(last_record.buffer_copy);
}

//-------------------------------------------------------------------------
// State Inspection
//-------------------------------------------------------------------------

bool SpiPeripheralMockImpl::hasDevice() const FL_NO_EXCEPT {
    return mDeviceAdded;
}

bool SpiPeripheralMockImpl::canQueueTransaction() const FL_NO_EXCEPT {
    return mDeviceAdded && mQueuedTransactions.size() < mMaxQueueSize;
}

size_t SpiPeripheralMockImpl::getQueuedTransactionCount() const FL_NO_EXCEPT {
    return mQueuedTransactions.size();
}

size_t SpiPeripheralMockImpl::getTransactionCount() const FL_NO_EXCEPT {
    return mTransactionCount;
}

const SpiBusConfig& SpiPeripheralMockImpl::getBusConfig() const FL_NO_EXCEPT {
    return mBusConfig;
}

const SpiDeviceConfig& SpiPeripheralMockImpl::getDeviceConfig() const FL_NO_EXCEPT {
    return mDeviceConfig;
}

size_t SpiPeripheralMockImpl::getBusInitializeCount() const FL_NO_EXCEPT {
    return mBusInitializeCount;
}

size_t SpiPeripheralMockImpl::getBusFreeCount() const FL_NO_EXCEPT {
    return mBusFreeCount;
}

size_t SpiPeripheralMockImpl::getDeviceRemoveCount() const FL_NO_EXCEPT {
    return mDeviceRemoveCount;
}

void SpiPeripheralMockImpl::simulatePinReroute(int pin) FL_NO_EXCEPT {
    mPinRouting[pin] = kOtherPeripheral;
}

void SpiPeripheralMockImpl::reset() FL_NO_EXCEPT {
    mInitialized = false;
    mDeviceAdded = false;
    mBusConfig = SpiBusConfig();
    mDeviceConfig = SpiDeviceConfig();
    mQueuedTransactions.clear();
    mMaxQueueSize = 0;
    mTransactionCount = 0;
    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransactionDelayUs = 0;
    mShouldFailTransaction = false;
    mHistory.clear();
    mPendingTransactions = 0;
    mCompletedTransactions.clear();
    mBusInitializeCount = 0;
    mBusFreeCount = 0;
    mDeviceRemoveCount = 0;
    mPinRouting.clear();
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
