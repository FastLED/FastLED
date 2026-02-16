// IWYU pragma: private

/// @file spi_peripheral_mock.cpp
/// @brief Mock SPI peripheral implementation for unit testing
///
/// This is a synchronous mock that requires manual event pumping via
/// simulateTransactionComplete(). No background threads are used.

// Compile for stub platform testing OR non-Arduino host platforms
#include "platforms/is_platform.h"
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_WIN)))

#include "spi_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"

#ifdef FL_IS_WIN
#include <malloc.h>  // ok include - For _aligned_malloc/_aligned_free on Windows
#else
#include <stdlib.h>  // For aligned_alloc on POSIX
#endif

#ifdef ARDUINO
#include <Arduino.h>  // ok include - For fl::micros() and delay() on Arduino platforms
#else
#include "platforms/stub/time_stub.h"  // For fl::micros() and delay() on host tests
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

    SpiPeripheralMockImpl();
    ~SpiPeripheralMockImpl() override = default;

    //=========================================================================
    // ISpiPeripheral Interface Implementation
    //=========================================================================

    bool initializeBus(const SpiBusConfig& config) override;
    bool addDevice(const SpiDeviceConfig& config) override;
    bool removeDevice() override;
    bool freeBus() override;
    bool isInitialized() const override;
    bool queueTransaction(const SpiTransaction& trans) override;
    bool pollTransaction(u32 timeout_ms) override;
    bool registerCallback(void* callback, void* user_ctx) override;
    u8* allocateDma(size_t size) override;
    void freeDma(u8* buffer) override;
    void delay(u32 ms) override;
    u64 getMicroseconds() override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void setTransactionDelay(u32 microseconds) override;
    void simulateTransactionComplete() override;
    void setTransactionFailure(bool should_fail) override;
    const fl::vector<TransactionRecord>& getTransactionHistory() const override;
    void clearTransactionHistory() override;
    fl::span<const u8> getLastTransactionData() const override;
    bool hasDevice() const override;
    bool canQueueTransaction() const override;
    size_t getQueuedTransactionCount() const override;
    size_t getTransactionCount() const override;
    const SpiBusConfig& getBusConfig() const override;
    const SpiDeviceConfig& getDeviceConfig() const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mDeviceAdded;
    SpiBusConfig mBusConfig;
    SpiDeviceConfig mDeviceConfig;

    // Transaction queue
    fl::vector<SpiTransaction> mQueuedTransactions;
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
};

//=============================================================================
// Singleton Instance
//=============================================================================

SpiPeripheralMock& SpiPeripheralMock::instance() {
    return Singleton<SpiPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor
//=============================================================================

SpiPeripheralMockImpl::SpiPeripheralMockImpl()
    : mInitialized(false),
      mDeviceAdded(false),
      mBusConfig(),
      mDeviceConfig(),
      mQueuedTransactions(),
      mMaxQueueSize(0),
      mTransactionCount(0),
      mCallback(nullptr),
      mUserCtx(nullptr),
      mTransactionDelayUs(0),
      mShouldFailTransaction(false),
      mHistory(),
      mPendingTransactions(0) {
    // No background thread - fully synchronous mock
}

//=============================================================================
// Bus Lifecycle Methods
//=============================================================================

bool SpiPeripheralMockImpl::initializeBus(const SpiBusConfig& config) {
    if (mInitialized) {
        FL_WARN("SpiPeripheralMock: Already initialized");
        return false;
    }

    // Validate config
    if (config.sclk_pin < 0) {
        FL_WARN("SpiPeripheralMock: Invalid SCLK pin: " << config.sclk_pin);
        return false;
    }

    // Store configuration
    mBusConfig = config;
    mInitialized = true;

    return true;
}

bool SpiPeripheralMockImpl::addDevice(const SpiDeviceConfig& config) {
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

bool SpiPeripheralMockImpl::removeDevice() {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: No device to remove");
        return false;
    }

    mDeviceAdded = false;
    mQueuedTransactions.clear();
    return true;
}

bool SpiPeripheralMockImpl::freeBus() {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Bus not initialized");
        return false;
    }

    if (mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Must remove device before freeing bus");
        return false;
    }

    mInitialized = false;
    return true;
}

bool SpiPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Transaction Methods
//=============================================================================

bool SpiPeripheralMockImpl::queueTransaction(const SpiTransaction& trans) {
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

bool SpiPeripheralMockImpl::pollTransaction(u32 timeout_ms) {
    (void)timeout_ms;  // Unused in synchronous mock

    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot poll - not initialized");
        return false;
    }

    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Cannot poll - no device added");
        return false;
    }

    // In synchronous mock, poll just checks if there are completed transactions
    // (i.e., transactions that have been processed via simulateTransactionComplete)
    if (mQueuedTransactions.empty()) {
        return false;
    }

    // If there are no pending transactions, the queued ones are "complete"
    if (mPendingTransactions == 0) {
        mQueuedTransactions.erase(mQueuedTransactions.begin());
        return true;
    }

    return false;
}

//=============================================================================
// Callback Registration
//=============================================================================

bool SpiPeripheralMockImpl::registerCallback(void* callback, void* user_ctx) {
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

u8* SpiPeripheralMockImpl::allocateDma(size_t size) {
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

void SpiPeripheralMockImpl::freeDma(u8* buffer) {
    if (buffer != nullptr) {
#ifdef FL_IS_WIN
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

//=============================================================================
// Platform Utilities
//=============================================================================

void SpiPeripheralMockImpl::delay(u32 ms) {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    fl::delay(ms);
}

u64 SpiPeripheralMockImpl::getMicroseconds() {
    // Use the same timestamp source as queueTransaction() for consistency
    return fl::micros();
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void SpiPeripheralMockImpl::setTransactionDelay(u32 microseconds) {
    mTransactionDelayUs = microseconds;
}

void SpiPeripheralMockImpl::simulateTransactionComplete() {
    if (mPendingTransactions == 0) {
        // No pending transactions - nothing to complete
        return;
    }

    // Decrement pending count
    mPendingTransactions--;

    // Remove oldest transaction from queue (FIFO)
    if (!mQueuedTransactions.empty()) {
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

void SpiPeripheralMockImpl::setTransactionFailure(bool should_fail) {
    mShouldFailTransaction = should_fail;
}

const fl::vector<SpiPeripheralMock::TransactionRecord>&
SpiPeripheralMockImpl::getTransactionHistory() const {
    return mHistory;
}

void SpiPeripheralMockImpl::clearTransactionHistory() {
    mHistory.clear();
    mTransactionCount = 0;
    mPendingTransactions = 0;
    mQueuedTransactions.clear();
}

fl::span<const u8> SpiPeripheralMockImpl::getLastTransactionData() const {
    if (mHistory.empty()) {
        return fl::span<const u8>();
    }

    const auto& last_record = mHistory.back();
    return fl::span<const u8>(last_record.buffer_copy);
}

//-------------------------------------------------------------------------
// State Inspection
//-------------------------------------------------------------------------

bool SpiPeripheralMockImpl::hasDevice() const {
    return mDeviceAdded;
}

bool SpiPeripheralMockImpl::canQueueTransaction() const {
    return mDeviceAdded && mQueuedTransactions.size() < mMaxQueueSize;
}

size_t SpiPeripheralMockImpl::getQueuedTransactionCount() const {
    return mQueuedTransactions.size();
}

size_t SpiPeripheralMockImpl::getTransactionCount() const {
    return mTransactionCount;
}

const SpiBusConfig& SpiPeripheralMockImpl::getBusConfig() const {
    return mBusConfig;
}

const SpiDeviceConfig& SpiPeripheralMockImpl::getDeviceConfig() const {
    return mDeviceConfig;
}

void SpiPeripheralMockImpl::reset() {
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
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
