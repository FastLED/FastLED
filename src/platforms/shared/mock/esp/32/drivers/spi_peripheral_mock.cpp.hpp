/// @file spi_peripheral_mock.cpp
/// @brief Mock SPI peripheral implementation for unit testing

// This mock is only for host testing (uses std::thread which is not available on embedded platforms)
// Compile for stub platform testing OR non-Arduino host platforms
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "spi_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"

#include <thread>  // ok include
#include <chrono>  // ok include

#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>  // ok include - For _aligned_malloc/_aligned_free on Windows
#else
#include <stdlib.h>  // For aligned_alloc on POSIX
#endif

#ifdef ARDUINO
#include <Arduino.h>  // ok include - For micros() and delay() on Arduino platforms
#else
#include "platforms/stub/time_stub.h"  // For micros() and delay() on host tests
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
class SpiPeripheralMockImpl : public SpiPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    SpiPeripheralMockImpl();
    ~SpiPeripheralMockImpl() override;

    //=========================================================================
    // ISpiPeripheral Interface Implementation
    //=========================================================================

    bool initializeBus(const SpiBusConfig& config) override;
    bool addDevice(const SpiDeviceConfig& config) override;
    bool removeDevice() override;
    bool freeBus() override;
    bool isInitialized() const override;
    bool queueTransaction(const SpiTransaction& trans) override;
    bool pollTransaction(uint32_t timeout_ms) override;
    bool registerCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDma(size_t size) override;
    void freeDma(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    uint64_t getMicroseconds() override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void setTransactionDelay(uint32_t microseconds) override;
    void simulateTransactionComplete() override;
    void setTransactionFailure(bool should_fail) override;
    const fl::vector<TransactionRecord>& getTransactionHistory() const override;
    void clearTransactionHistory() override;
    fl::span<const uint8_t> getLastTransactionData() const override;
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
    uint32_t mTransactionDelayUs;
    bool mShouldFailTransaction;

    // Waveform capture
    fl::vector<TransactionRecord> mHistory;

    // Pending transaction state
    size_t mPendingTransactions;

    // Simulation thread for automatic callback triggering
    void simulationThreadFunc();
    fl::unique_ptr<std::thread> mSimulationThread;  // okay std namespace
    fl::atomic<bool> mSimulationThreadShouldStop;
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
      mPendingTransactions(0),
      mSimulationThread(),
      mSimulationThreadShouldStop(false) {
    // Start simulation thread
    mSimulationThread = fl::make_unique<std::thread>([this]() { simulationThreadFunc(); });  // okay std namespace
}

SpiPeripheralMockImpl::~SpiPeripheralMockImpl() {
    // Stop simulation thread
    mSimulationThreadShouldStop = true;
    if (mSimulationThread && mSimulationThread->joinable()) {
        mSimulationThread->join();
    }
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

    // Calculate realistic transaction delay based on clock frequency and bit count
    // clock_speed_hz is stored in mDeviceConfig.clock_speed_hz (e.g., 2500000 Hz for WS2812)
    // Transmission time = (length_bits / clock_speed_hz) seconds = (length_bits * 1000000 / clock_speed_hz) microseconds
    if (mDeviceConfig.clock_speed_hz > 0 && trans.length_bits > 0) {
        // Calculate transmission time in microseconds
        uint64_t transmission_time_us = (static_cast<uint64_t>(trans.length_bits) * 1000000ULL) / mDeviceConfig.clock_speed_hz;
        // Add small overhead for buffer switching (10 microseconds)
        mTransactionDelayUs = static_cast<uint32_t>(transmission_time_us) + 10;
    } else {
        // Fallback: Use a small default delay if clock not configured
        mTransactionDelayUs = 100;  // 100 microseconds default
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
    record.timestamp_us = micros();

    // Store in history
    mHistory.push_back(fl::move(record));

    // Queue transaction for processing
    mQueuedTransactions.push_back(trans);

    // Update state
    mTransactionCount++;
    mPendingTransactions++;

    // Simulation thread will automatically trigger callback after mTransactionDelayUs

    return true;
}

bool SpiPeripheralMockImpl::pollTransaction(uint32_t timeout_ms) {
    if (!mInitialized) {
        FL_WARN("SpiPeripheralMock: Cannot poll - not initialized");
        return false;
    }

    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralMock: Cannot poll - no device added");
        return false;
    }

    // Check if there are completed transactions
    if (mQueuedTransactions.empty()) {
        // No queued transactions - nothing to poll
        return false;
    }

    // Simulate instant completion if no delay set
    if (mTransactionDelayUs == 0 || mPendingTransactions == 0) {
        // Remove oldest transaction from queue (FIFO)
        mQueuedTransactions.erase(mQueuedTransactions.begin());
        return true;
    }

    // For non-zero timeout, wait for transaction to complete
    if (timeout_ms == 0) {
        // Non-blocking poll - return false if still pending
        return false;
    }

    // Wait for transaction completion with timeout
    uint32_t start_us = micros();
    uint32_t timeout_us = timeout_ms * 1000;

    while (mPendingTransactions > 0 && (micros() - start_us) < timeout_us) {
        // Busy wait (simulation)
        std::this_thread::sleep_for(std::chrono::microseconds(100));  // okay std namespace
    }

    if (mPendingTransactions > 0) {
        return false;  // Timeout
    }

    // Remove completed transaction from queue
    if (!mQueuedTransactions.empty()) {
        mQueuedTransactions.erase(mQueuedTransactions.begin());
    }

    return true;
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

uint8_t* SpiPeripheralMockImpl::allocateDma(size_t size) {
    // Round up to 4-byte multiple (SPI DMA alignment requirement)
    size_t aligned_size = ((size + 3) / 4) * 4;

    // Allocate regular heap memory (no DMA requirement on host)
    // Use aligned_alloc for 4-byte alignment (matches real hardware)
    void* buffer = nullptr;

    // Alignment must be at least sizeof(void*) for posix_memalign
    // Use 8-byte alignment which satisfies both 4-byte SPI requirement
    // and POSIX alignment constraints on 64-bit systems
    constexpr size_t kAlignment = sizeof(void*) >= 4 ? sizeof(void*) : 4;

#if defined(_WIN32) || defined(_WIN64)
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

    return static_cast<uint8_t*>(buffer);
}

void SpiPeripheralMockImpl::freeDma(uint8_t* buffer) {
    if (buffer != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

//=============================================================================
// Platform Utilities
//=============================================================================

void SpiPeripheralMockImpl::delay(uint32_t ms) {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    ::delay(static_cast<int>(ms));
}

uint64_t SpiPeripheralMockImpl::getMicroseconds() {
    // Use the same timestamp source as queueTransaction() for consistency
    return micros();
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void SpiPeripheralMockImpl::setTransactionDelay(uint32_t microseconds) {
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

fl::span<const uint8_t> SpiPeripheralMockImpl::getLastTransactionData() const {
    if (mHistory.empty()) {
        return fl::span<const uint8_t>();
    }

    const auto& last_record = mHistory.back();
    return fl::span<const uint8_t>(last_record.buffer_copy);
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

//=============================================================================
// Simulation Thread
//=============================================================================

void SpiPeripheralMockImpl::simulationThreadFunc() {
    while (!mSimulationThreadShouldStop) {
        // Check if there are pending transactions
        if (mPendingTransactions > 0 && mTransactionDelayUs > 0) {
            // Sleep for the transaction delay
            std::this_thread::sleep_for(std::chrono::microseconds(mTransactionDelayUs));  // okay std namespace

            // After delay, simulate transaction complete by calling callback
            if (mCallback && mPendingTransactions > 0) {
                // Decrement pending count
                mPendingTransactions--;

                // Remove oldest transaction from queue
                if (!mQueuedTransactions.empty()) {
                    mQueuedTransactions.erase(mQueuedTransactions.begin());
                }

                // Call post-transaction callback
                using CallbackType = void (*)(void*);
                auto callback_fn = reinterpret_cast<CallbackType>(mCallback);  // ok reinterpret cast
                callback_fn(nullptr);
            }
        } else {
            // No pending transactions - sleep briefly to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::microseconds(100));  // okay std namespace
        }
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
