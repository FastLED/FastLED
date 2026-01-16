/// @file parlio_peripheral_mock.cpp
/// @brief Mock PARLIO peripheral implementation for unit testing

// This mock is only for host testing (uses std::thread which is not available on embedded platforms)
// Compile for stub platform testing OR non-Arduino host platforms
#if defined(FASTLED_STUB_IMPL) || (!defined(ARDUINO) && (defined(__linux__) || defined(__APPLE__) || defined(_WIN32)))

#include "parlio_peripheral_mock.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"
#include "fl/singleton.h"
#include "fl/stl/atomic.h"
#include "fl/stl/map.h"

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


namespace {
/// @brief Untranspose interleaved bit-parallel data to per-pin waveforms (internal helper)
/// @param transposed_data Interleaved bit data (output from wave8Transpose_2/4/8/16)
/// @param bit_count Total number of bits transmitted
/// @param num_pins Number of parallel pins (data_width)
/// @return Vector of per-pin waveforms (one vector per pin)
///
/// The transposed data is in bit-parallel format where bits from multiple pins
/// are interleaved. This function reverses the transposition to extract the
/// original waveform for each pin.
///
/// **CRITICAL: 2-lane transposition quirk**
///
/// The wave8Transpose functions create different bit orderings:
/// - **2-lane**: FL_WAVE8_SPREAD_TO_16 macro maps:
///               - Lane 0 → even bit positions (0,2,4,6,...)
///               - Lane 1 → odd bit positions (1,3,5,7,...)
///               When PARLIO sends bits sequentially (bit N → Pin N%2):
///               - Pin 0 (receives even bits) gets Lane 0 data
///               - Pin 1 (receives odd bits) gets Lane 1 data
///               However, due to bit ordering within bytes (LSB-first transmission),
///               we need a swap during untransposition to present the logical API
///               where Pin 0 → Lane 0, Pin 1 → Lane 1
///
/// - **4/8/16-lane**: Bit-to-pin mapping works correctly with no swap needed:
///                    - Pin i receives Lane i data (direct mapping)
///
/// For 2-lane example:
/// - Input: Lane 0 = 0xFF, Lane 1 = 0x00
/// - After transposition: [0xAA, 0xAA, ...] (Lane 0 at even bits, Lane 1 at odd bits)
/// - After untransposition with swap: Pin 0 → Lane 0 (0xFF), Pin 1 → Lane 1 (0x00)
fl::vector<fl::vector<uint8_t>> untransposeParlioBitstreamInternal(
    const uint8_t* transposed_data,
    size_t bit_count,
    size_t num_pins,
    fl::detail::ParlioBitPackOrder packing) {

    // Initialize per-pin storage
    fl::vector<fl::vector<uint8_t>> per_pin_data(num_pins);

    // Calculate number of bytes per pin's waveform
    size_t bits_per_pin = bit_count / num_pins;
    size_t bytes_per_pin = (bits_per_pin + 7) / 8;

    for (size_t pin = 0; pin < num_pins; pin++) {
        per_pin_data[pin].resize(bytes_per_pin, 0);
    }

    // Untranspose bit-by-bit
    // PARLIO hardware sends bits based on packing order
    for (size_t bit_idx = 0; bit_idx < bit_count; bit_idx++) {
        // Get bit from transposed buffer
        size_t byte_idx = bit_idx / 8;
        size_t bit_pos;
        if (packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB) {
            // LSB packing: bits sent LSB-first within each byte
            bit_pos = bit_idx % 8;
        } else {
            // MSB packing: bits sent MSB-first within each byte (reversed)
            bit_pos = 7 - (bit_idx % 8);
        }
        bool bit_value = (transposed_data[byte_idx] >> bit_pos) & 1;

        // Determine which pin this bit belongs to (cycles through pins)
        size_t hardware_pin_idx = bit_idx % num_pins;

        // Direct mapping: hardware pin index maps directly to lane index
        size_t lane_idx = hardware_pin_idx;

        // Determine position in the lane's waveform
        size_t lane_bit_idx = bit_idx / num_pins;
        size_t lane_byte_idx = lane_bit_idx / 8;
        size_t lane_bit_pos = lane_bit_idx % 8;  // LSB first

        // Set the bit in the lane's waveform
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

/// @brief Internal implementation of ParlioPeripheralMock
///
/// This class contains all the actual implementation details.
/// It's a simple synchronous mock without threading.
class ParlioPeripheralMockImpl : public ParlioPeripheralMock {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    ParlioPeripheralMockImpl();
    ~ParlioPeripheralMockImpl() override;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override;
    bool enable() override;
    bool disable() override;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override;
    bool waitAllDone(uint32_t timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    uint64_t getMicroseconds() override;
    void freeDmaBuffer(void* ptr) override;

    //=========================================================================
    // Mock-Specific API
    //=========================================================================

    void setTransmitDelay(uint32_t microseconds) override;
    void simulateTransmitComplete() override;
    void setTransmitFailure(bool should_fail) override;
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const override;
    void clearTransmissionHistory() override;
    fl::span<const uint8_t> getTransmissionDataForPin(int gpio_pin) const override;
    bool isInitialized() const override;
    bool isEnabled() const override;
    bool isTransmitting() const override;
    size_t getTransmitCount() const override;
    const ParlioPeripheralConfig& getConfig() const override;
    void reset() override;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

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
    uint32_t mTransmitDelayUs;
    bool mShouldFailTransmit;

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;

    // Untransposed per-pin waveform data (stored separately from transmission records)
    // Maps actual GPIO pin numbers to their waveform data for the most recent transmission
    fl::fl_map<int, fl::vector<uint8_t>> mPerPinData;

    // Pending transmission state (for waitAllDone simulation)
    size_t mPendingTransmissions;

    // Per-transmission tracking for simulation thread
    struct PendingTransmission {
        uint64_t completion_time_us;  // When this transmission should complete
    };
    fl::vector<PendingTransmission> mPendingQueue;

    // CRITICAL: Thread synchronization primitives MUST be declared BEFORE mSimulationThread
    // because the constructor body starts the thread, and the thread immediately accesses
    // these members. Member initialization happens in declaration order, so declaring these
    // first ensures they are fully initialized before the thread starts.

    // Mutex for thread-safe access to shared state
    std::mutex mMutex;  // okay std namespace

    // Condition variable for efficient thread synchronization
    std::condition_variable mCondVar;  // okay std namespace

    // Flag to indicate callback is currently executing (outside mutex)
    fl::atomic<bool> mCallbackExecuting{false};

    // Simulation thread for automatic ISR callback
    void simulationThreadFunc();
    fl::unique_ptr<std::thread> mSimulationThread;  // okay std namespace
    fl::atomic<bool> mSimulationThreadShouldStop;
};

//=============================================================================
// Singleton Instance
//=============================================================================

ParlioPeripheralMock& ParlioPeripheralMock::instance() {
    return Singleton<ParlioPeripheralMockImpl>::instance();
}

//=============================================================================
// Constructor
//=============================================================================

ParlioPeripheralMockImpl::ParlioPeripheralMockImpl()
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
      mPendingQueue(),
      // Thread synchronization primitives initialized here (in declaration order)
      mMutex(),
      mCondVar(),
      mCallbackExecuting(false),
      mSimulationThread(),
      mSimulationThreadShouldStop(false) {
    // CRITICAL: Start simulation thread AFTER all members are initialized
    // Member initialization happens in DECLARATION ORDER (not initializer list order):
    // 1. All primitive members (mInitialized, mEnabled, etc.) are initialized
    // 2. Collections (mHistory, mPerPinData, mPendingQueue) are initialized
    // 3. Thread synchronization (mMutex, mCondVar, mCallbackExecuting) are initialized
    // 4. Thread members (mSimulationThread, mSimulationThreadShouldStop) are initialized
    // 5. Constructor body executes (we are here)
    // 6. Thread starts and can safely access all members
    //
    // By declaring mMutex/mCondVar/mCallbackExecuting BEFORE mSimulationThread in the class,
    // we ensure they are fully constructed before the thread starts accessing them.
    mSimulationThread = fl::make_unique<std::thread>([this]() { simulationThreadFunc(); });  // okay std namespace
}

ParlioPeripheralMockImpl::~ParlioPeripheralMockImpl() {
    // Stop simulation thread
    mSimulationThreadShouldStop = true;
    // Wake up simulation thread so it can exit cleanly
    mCondVar.notify_one();
    if (mSimulationThread && mSimulationThread->joinable()) {
        mSimulationThread->join();
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool ParlioPeripheralMockImpl::initialize(const ParlioPeripheralConfig& config) {
    // Allow re-initialization (tests may need to reconfigure the mock)
    // This is safe because we're just updating configuration, not tearing down
    // the simulation thread or other infrastructure

    // Validate config
    if (config.data_width == 0 || config.data_width > 16) {
        FL_WARN("ParlioPeripheralMock: Invalid data width: " << config.data_width);
        return false;
    }

    // Store configuration (thread-safe - no lock needed as config is only read during transmit)
    mConfig = config;
    mInitialized = true;

    return true;
}

bool ParlioPeripheralMockImpl::enable() {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot enable - not initialized");
        return false;
    }

    mEnabled = true;
    return true;
}

bool ParlioPeripheralMockImpl::disable() {
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

bool ParlioPeripheralMockImpl::transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not initialized");
        return false;
    }

    if (!mEnabled) {
        FL_WARN("ParlioPeripheralMock: Cannot transmit - not enabled");
        return false;
    }

    // Check for injected failure
    if (mShouldFailTransmit) {
        return false;
    }

    // Calculate realistic transmission delay based on clock frequency and bit count
    // clock_freq_hz is stored in mConfig.clock_freq_hz (e.g., 8 MHz = 8000000 Hz for WS2812)
    // Transmission time = (bit_count / clock_freq_hz) seconds = (bit_count * 1000000 / clock_freq_hz) microseconds
    uint32_t transmission_delay_us;
    if (mConfig.clock_freq_hz > 0) {
        // Calculate transmission time in microseconds
        uint64_t transmission_time_us = (static_cast<uint64_t>(bit_count) * 1000000ULL) / mConfig.clock_freq_hz;
        // Add small overhead for buffer switching (10 microseconds)
        transmission_delay_us = static_cast<uint32_t>(transmission_time_us) + 10;
    } else {
        // Fallback: Use a small default delay if clock not configured
        transmission_delay_us = 100;  // 100 microseconds default
    }

    // Keep mTransmitDelayUs updated for backward compatibility (waitAllDone uses it)
    mTransmitDelayUs = transmission_delay_us;

    // Calculate buffer size in bytes
    size_t byte_count = (bit_count + 7) / 8;

    // Capture transmission data (copy buffer for later inspection)
    TransmissionRecord record;
    record.buffer_copy.resize(byte_count);
    fl::memcpy(record.buffer_copy.data(), buffer, byte_count);
    record.bit_count = bit_count;
    record.idle_value = idle_value;
    record.timestamp_us = micros();

    // Store in history
    mHistory.push_back(fl::move(record));

    // Untranspose the data to extract per-pin waveforms
    // Store separately to avoid bloating transmission records
    fl::vector<fl::vector<uint8_t>> per_pin_waveforms = untransposeParlioBitstreamInternal(
        buffer,
        bit_count,
        mConfig.data_width,
        mConfig.packing
    );

    // Map pin indices to actual GPIO pin numbers
    // per_pin_waveforms is indexed by lane_idx (after swap in untranspose)
    // For 2-lane: lane 0 data is from odd bits, lane 1 data is from even bits
    // GPIO pins[0] should get data from lane 0, pins[1] should get data from lane 1
    // But untranspose swapped: even bit positions → lane 1, odd bit positions → lane 0
    // So we need to reverse: gpio_pins[0] → per_pin_waveforms[0] (lane 0 from odd bits)
    mPerPinData.clear();
    for (size_t i = 0; i < per_pin_waveforms.size() && i < static_cast<size_t>(mConfig.data_width); i++) {
        int gpio_pin = mConfig.gpio_pins[i];
        mPerPinData[gpio_pin] = fl::move(per_pin_waveforms[i]);
    }

    // Update state with mutex protection
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mTransmitCount++;
        mTransmitting = true;
        mPendingTransmissions++;

        // Enqueue this transmission for the simulation thread
        PendingTransmission pending;
        pending.completion_time_us = micros() + transmission_delay_us;
        mPendingQueue.push_back(pending);
    }

    // Wake up simulation thread to process the new transmission
    mCondVar.notify_one();

    // Simulation thread will automatically call ISR callback after transmission_delay_us

    return true;
}

bool ParlioPeripheralMockImpl::waitAllDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot wait - not initialized");
        return false;
    }

    // Check if instantly complete (no pending transmissions)
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        if (mPendingTransmissions == 0) {
            mTransmitting = false;
            return true;
        }
    }

    // Simulate timeout (for testing timeout paths)
    if (timeout_ms == 0) {
        // Non-blocking poll - return false if still pending
        return false;
    }

    // For non-zero timeout, simulate completion after delay
    // (In real tests, simulateTransmitComplete() is called explicitly)
    // This is a fallback for simple tests that don't need precise timing control
    if (mTransmitDelayUs > 0) {
        uint32_t start_us = micros();
        uint32_t timeout_us = timeout_ms * 1000;

        while (true) {
            {
                std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
                if (mPendingTransmissions == 0) {
                    break;
                }
            }

            if ((micros() - start_us) >= timeout_us) {
                return false;  // Timeout
            }

            // Yield to other threads
            std::this_thread::sleep_for(std::chrono::microseconds(10));  // okay std namespace
        }
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        mTransmitting = false;
    }
    return true;
}

//=============================================================================
// ISR Callback Registration
//=============================================================================

bool ParlioPeripheralMockImpl::registerTxDoneCallback(void* callback, void* user_ctx) {
    if (!mInitialized) {
        FL_WARN("ParlioPeripheralMock: Cannot register callback - not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
    mCallback = callback;
    mUserCtx = user_ctx;
    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

uint8_t* ParlioPeripheralMockImpl::allocateDmaBuffer(size_t size) {
    // Round up to 64-byte multiple (same as real implementation)
    size_t aligned_size = ((size + 63) / 64) * 64;

    // Allocate regular heap memory (no DMA requirement on host)
    // Use aligned_alloc for 64-byte alignment (matches real hardware)
    void* buffer = nullptr;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: Use _aligned_malloc
    buffer = _aligned_malloc(aligned_size, 64);
#else
    // POSIX: Use aligned_alloc
    buffer = aligned_alloc(64, aligned_size);
#endif

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralMock: Failed to allocate buffer (" << aligned_size << " bytes)");
    }

    return static_cast<uint8_t*>(buffer);
}

void ParlioPeripheralMockImpl::freeDmaBuffer(uint8_t* buffer) {
    if (buffer != nullptr) {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(buffer);
#else
        free(buffer);
#endif
    }
}

void ParlioPeripheralMockImpl::delay(uint32_t ms) {
    // Use portable delay abstraction:
    // - Arduino: Arduino delay() function
    // - Host: time_stub.h delay() (can be fast-forwarded for testing)
    ::delay(static_cast<int>(ms));
}

uint64_t ParlioPeripheralMockImpl::getMicroseconds() {
    // Use the same timestamp source as transmit() for consistency
    return micros();
}

void ParlioPeripheralMockImpl::freeDmaBuffer(void* ptr) {
    // Mock uses standard heap allocation, so use standard free()
    if (ptr) {
        free(ptr);
    }
}

//=============================================================================
// Mock-Specific API (for unit tests)
//=============================================================================

void ParlioPeripheralMockImpl::setTransmitDelay(uint32_t microseconds) {
    mTransmitDelayUs = microseconds;
}

void ParlioPeripheralMockImpl::simulateTransmitComplete() {
    if (mPendingTransmissions == 0) {
        // No pending transmissions - nothing to complete
        return;
    }

    // Decrement pending count
    mPendingTransmissions--;

    // If all transmissions complete, clear transmitting flag
    if (mPendingTransmissions == 0) {
        mTransmitting = false;
    }

    // Fire ISR callback (if registered)
    if (mCallback != nullptr) {
        // Cast callback to correct signature
        // bool (*callback)(void* tx_unit, const void* edata, void* user_ctx)
        using CallbackType = bool (*)(void*, const void*, void*);
        auto callback_fn = reinterpret_cast<CallbackType>(mCallback); // ok reinterpret cast

        // Call with null tx_unit handle (mock doesn't have real handle)
        callback_fn(nullptr, nullptr, mUserCtx);
    }
}

void ParlioPeripheralMockImpl::setTransmitFailure(bool should_fail) {
    mShouldFailTransmit = should_fail;
}

const fl::vector<ParlioPeripheralMock::TransmissionRecord>&
ParlioPeripheralMockImpl::getTransmissionHistory() const {
    return mHistory;
}

bool ParlioPeripheralMockImpl::isInitialized() const {
    return mInitialized;
}

bool ParlioPeripheralMockImpl::isEnabled() const {
    return mEnabled;
}

bool ParlioPeripheralMockImpl::isTransmitting() const {
    return mTransmitting;
}

size_t ParlioPeripheralMockImpl::getTransmitCount() const {
    return mTransmitCount;
}

const ParlioPeripheralConfig& ParlioPeripheralMockImpl::getConfig() const {
    return mConfig;
}

void ParlioPeripheralMockImpl::clearTransmissionHistory() {
    // Lock mutex to prevent race condition with simulation thread
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace

    mHistory.clear();
    mPerPinData.clear();
    // Note: mTransmitCount is NOT reset - it's a running counter across history clears
    mPendingTransmissions = 0;
    mTransmitting = false;
}

fl::span<const uint8_t> ParlioPeripheralMockImpl::getTransmissionDataForPin(int gpio_pin) const {
    // Return empty span if no per-pin data available
    if (mPerPinData.empty()) {
        return fl::span<const uint8_t>();
    }

    // Look up the GPIO pin in the map
    auto it = mPerPinData.find(gpio_pin);
    if (it == mPerPinData.end()) {
        FL_WARN("ParlioPeripheralMock: GPIO pin " << gpio_pin << " not found in transmission data");
        return fl::span<const uint8_t>();
    }

    // Return span of the pin's data
    return fl::span<const uint8_t>(it->second);
}

void ParlioPeripheralMockImpl::reset() {
    // CRITICAL: This function must be thread-safe because the simulation thread
    // may be accessing state while reset() is called from another thread.

    // Step 1: Acquire lock and signal simulation thread to stop processing
    {
        std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace
        // Clear queue first so simulation thread has nothing to process
        mPendingQueue.clear();
        mPendingTransmissions = 0;
        mTransmitting = false;
    }

    // Step 2: Wake up simulation thread so it sees the empty queue
    // This ensures it exits any wait_for() calls and goes back to waiting
    mCondVar.notify_one();

    // Step 3: Wait for any callback currently executing to finish
    // This prevents race where simulation thread captured callback before we cleared queue
    // Use explicit atomic load with acquire ordering to prevent compiler optimization caching
    while (mCallbackExecuting.load(fl::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));  // okay std namespace
    }

    // Step 4: Small delay to ensure simulation thread has seen the empty queue
    // and is now waiting in mCondVar.wait_for()
    // This prevents a race where the thread is between checking mPendingQueue.empty()
    // and calling mCondVar.wait_for()
    std::this_thread::sleep_for(std::chrono::microseconds(100));  // okay std namespace

    // Step 5: Lock mutex again and reset all state
    // At this point, simulation thread should be safely waiting in mCondVar.wait_for()
    std::lock_guard<std::mutex> lock(mMutex);  // okay std namespace

    mInitialized = false;
    mEnabled = false;
    mTransmitting = false;
    mTransmitCount = 0;

    fl::vector<int> empty_pins;  // Empty vector (size = 0)
    mConfig = ParlioPeripheralConfig(empty_pins, 0, 0, 0, ParlioBitPackOrder::FL_PARLIO_MSB);

    mCallback = nullptr;
    mUserCtx = nullptr;
    mTransmitDelayUs = 0;
    mShouldFailTransmit = false;
    mHistory.clear();
    mPerPinData.clear();
    mPendingTransmissions = 0;
    mPendingQueue.clear();
}

//=============================================================================
// Simulation Thread
//=============================================================================

void ParlioPeripheralMockImpl::simulationThreadFunc() {
    while (!mSimulationThreadShouldStop) {
        // Lock mutex for thread-safe access to shared state
        std::unique_lock<std::mutex> lock(mMutex);  // okay std namespace

        // Wait efficiently when queue is empty, instead of busy-polling
        if (mPendingQueue.empty()) {
            // Wait for up to 10ms, or until notified by transmit()
            mCondVar.wait_for(lock, std::chrono::milliseconds(10));  // okay std namespace
            continue;  // Recheck condition after waking
        }

        // Check if there are pending transmissions in the queue
        if (!mPendingQueue.empty()) {
            // Get current time
            uint64_t now_us = micros();

            // Check if the first transmission has completed
            if (now_us >= mPendingQueue[0].completion_time_us) {
                // Remove from queue
                mPendingQueue.erase(mPendingQueue.begin());

                // Decrement pending count
                if (mPendingTransmissions > 0) {
                    mPendingTransmissions--;
                }

                // Update state
                if (mPendingTransmissions == 0) {
                    mTransmitting = false;
                }

                // Get callback info before unlocking
                auto callback = mCallback;
                auto user_ctx = mUserCtx;

                // Set flag to indicate callback is executing (use explicit store with release ordering)
                mCallbackExecuting.store(true, fl::memory_order_release);

                // Unlock before calling callback (avoid holding lock during callback)
                lock.unlock();

                // Call ISR callback if registered
                if (callback) {
                    using CallbackType = bool (*)(void*, const void*, void*);
                    auto callback_fn = reinterpret_cast<CallbackType>(callback); // ok reinterpret cast
                    callback_fn(nullptr, nullptr, user_ctx);
                } else {
                    FL_WARN("Parlio Mock: No callback registered!");
                }

                // Reacquire lock before updating shared state
                lock.lock();

                // Clear flag to indicate callback finished (use explicit store with release ordering)
                mCallbackExecuting.store(false, fl::memory_order_release);

                // Continue loop immediately to check for more completed transmissions
                continue;
            } else {
                // Wait efficiently until next transmission should complete
                uint64_t time_until_next = mPendingQueue[0].completion_time_us - now_us;

                // Use condition variable with timeout for efficient waiting
                // This avoids busy-polling while still waking up when the transmission should complete
                mCondVar.wait_for(lock, std::chrono::microseconds(time_until_next));  // okay std namespace
                // Loop will continue and recheck if transmission is ready
            }
        }
    }
}

//=============================================================================
// Public Untranspose Function
//=============================================================================

fl::vector<fl::pair<int, fl::vector<uint8_t>>> ParlioPeripheralMock::untransposeParlioBitstream(
    fl::span<const uint8_t> transposed_data,
    fl::span<const int> pins,
    ParlioBitPackOrder packing) {

    fl::vector<fl::pair<int, fl::vector<uint8_t>>> result;

    // Validate inputs
    if (transposed_data.empty() || pins.empty()) {
        return result;
    }

    size_t num_pins = pins.size();
    size_t bit_count = transposed_data.size() * 8;

    // Use internal helper to perform untransposition
    fl::vector<fl::vector<uint8_t>> per_pin_waveforms = untransposeParlioBitstreamInternal(
        transposed_data.data(),
        bit_count,
        num_pins,
        packing
    );

    // Map lane indices to GPIO pin numbers
    for (size_t i = 0; i < per_pin_waveforms.size() && i < pins.size(); i++) {
        int gpio_pin = pins[i];
        //result[gpio_pin] = fl::move(per_pin_waveforms[i]);
        fl::pair<int, fl::vector<uint8_t>> pair;
        pair.first = gpio_pin;
        pair.second = fl::move(per_pin_waveforms[i]);
        result.emplace_back(pair);
    }

    return result;
}

} // namespace detail
} // namespace fl


#endif // FASTLED_STUB_IMPL || (!ARDUINO && (linux/apple/win32))
