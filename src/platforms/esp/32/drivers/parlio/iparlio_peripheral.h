/// @file iparlio_peripheral.h
/// @brief Virtual interface for PARLIO peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the ParlioEngine.
/// It abstracts all ESP-IDF PARLIO API calls into a clean interface that can be:
/// - Implemented by ParlioPeripheralESP (real hardware delegate)
/// - Implemented by ParlioPeripheralMock (unit test simulation)
///
/// ## Design Rationale
///
/// The ParlioEngine contains complex logic for waveform generation, ring buffer
/// management, and ISR coordination. This logic should be unit testable without
/// requiring real ESP32 hardware. By extracting a virtual peripheral interface,
/// we achieve:
///
/// 1. **Testability**: Mock implementation enables host-based unit tests
/// 2. **Separation of Concerns**: Hardware delegation vs. business logic
/// 3. **Performance**: Virtual dispatch adds only ~2-3 CPU cycles overhead
/// 4. **Maintainability**: Clear contract between engine and hardware
///
/// ## Interface Contract
///
/// - All methods return `bool` (true = success, false = error)
/// - Methods mirror ESP-IDF PARLIO API semantics exactly
/// - No ESP-IDF types leak into interface (opaque handles via void*)
/// - Memory alignment: All DMA buffers MUST be 64-byte aligned
/// - Thread safety: Caller responsible for synchronization
///
/// ## Memory Management
///
/// DMA buffers allocated via `allocateDmaBuffer()` MUST be:
/// - 64-byte aligned (cache line alignment)
/// - DMA-capable memory (ESP-IDF: MALLOC_CAP_DMA)
/// - Freed via `freeDmaBuffer()` when no longer needed
///
/// ## ISR Safety
///
/// - `registerTxDoneCallback()` callback runs in ISR context
/// - Callback MUST be ISR-safe (no logging, blocking, or heap allocation)
/// - See ParlioIsrContext documentation for ISR safety rules

#pragma once

// This interface is platform-agnostic (no ESP32 guard)
// - ParlioPeripheralESP requires ESP32 (real hardware)
// - ParlioPeripheralMock works on all platforms (testing)

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/task.h"  // For OS-level task management (fl::os_task namespace)
#include "fl/stl/vector.h"  // For fl::vector_fixed

namespace fl {
namespace detail {

//=============================================================================
// Enums
//=============================================================================

/// @brief PARLIO bit packing order
///
/// Controls how bits are mapped to GPIO pins during parallel transmission.
/// See ESP32-C6 Technical Reference Manual for detailed bit mapping.
enum class ParlioBitPackOrder {
    FL_PARLIO_LSB = 0,  ///< LSB packing: bits sent in order [0,1,2,3,4,5,6,7]
    FL_PARLIO_MSB = 1   ///< MSB packing: bits sent in order [7,6,5,4,3,2,1,0] (time-reversed)
};

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief PARLIO peripheral configuration
///
/// Encapsulates all parameters needed to initialize the PARLIO hardware.
/// Maps directly to ESP-IDF's parlio_tx_unit_config_t structure.
struct ParlioPeripheralConfig {
    size_t data_width;                  ///< PARLIO data width (1, 2, 4, 8, or 16)
    fl::vector_fixed<int, 16> gpio_pins;///< GPIO pin assignments (-1 for unused)
    uint32_t clock_freq_hz;             ///< Clock frequency (Hz)
    size_t queue_depth;                 ///< Hardware queue depth
    size_t max_transfer_size;           ///< Max DMA transfer size (bytes)
    ParlioBitPackOrder packing;         ///< Bit packing order (ParlioBitPackOrder::FL_PARLIO_LSB or ParlioBitPackOrder::FL_PARLIO_MSB)

    /// @brief Default constructor (for mock testing)
    ParlioPeripheralConfig()
        : data_width(0),
          gpio_pins(),
          clock_freq_hz(0),
          queue_depth(0),
          max_transfer_size(0),
          packing(ParlioBitPackOrder::FL_PARLIO_MSB) {}  ///< Default to MSB packing (Wave8 format)

    /// @brief Constructor with mandatory parameters
    /// @tparam PinContainer Any container with .size() and operator[] (e.g., fl::vector, fl::vector_fixed)
    /// @param pins GPIO pin assignments (unused slots automatically filled with -1)
    /// @param clock_freq Clock frequency in Hz
    /// @param queue_depth Hardware queue depth
    /// @param max_transfer Maximum DMA transfer size in bytes
    /// @param pack_order Bit packing order (default: ParlioBitPackOrder::FL_PARLIO_MSB)
    template<typename PinContainer>
    ParlioPeripheralConfig(const PinContainer& pins,
                           uint32_t clock_freq,
                           size_t queue_depth_val,
                           size_t max_transfer,
                           ParlioBitPackOrder pack_order = ParlioBitPackOrder::FL_PARLIO_MSB)
        : data_width(pins.size()),
          gpio_pins(),
          clock_freq_hz(clock_freq),
          queue_depth(queue_depth_val),
          max_transfer_size(max_transfer),
          packing(pack_order) {
        // Resize to full 16 slots
        gpio_pins.resize(16);

        // Copy provided pins
        for (size_t i = 0; i < pins.size() && i < 16; i++) {
            gpio_pins[i] = pins[i];
        }

        // Fill remaining slots with -1 (unused)
        for (size_t i = pins.size(); i < 16; i++) {
            gpio_pins[i] = -1;
        }
    }
};

// Task management removed from peripheral interface
// Use fl::TaskCoroutine directly instead of peripheral->createTask()
// Timer types removed - use fl::isr::isr_handle_t and fl::isr::isr_config_t instead

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for PARLIO peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF PARLIO operations.
/// Implementations:
/// - ParlioPeripheralESP: Thin wrapper around ESP-IDF APIs (real hardware)
/// - ParlioPeripheralMock: Simulation for host-based unit tests
///
/// ## Usage Pattern
/// ```cpp
/// // Create peripheral (real or mock)
/// IParlioPeripheral* peripheral = new ParlioPeripheralESP();
///
/// // Configure
/// ParlioPeripheralConfig config = {...};
/// if (!peripheral->initialize(config)) { /* error */ }
///
/// // Register ISR callback
/// peripheral->registerTxDoneCallback(callback, user_ctx);
///
/// // Start transmission
/// peripheral->enable();
/// peripheral->transmit(buffer, bit_count, idle_value);
///
/// // Wait for completion
/// peripheral->waitAllDone(timeout_ms);
/// peripheral->disable();
///
/// // Cleanup
/// delete peripheral;
/// ```
class IParlioPeripheral {
public:
    virtual ~IParlioPeripheral() = default;

    //=========================================================================
    // Lifecycle Methods
    //=========================================================================

    /// @brief Initialize PARLIO peripheral with configuration
    /// @param config Hardware configuration (pins, clock, queue depth)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: parlio_new_tx_unit()
    ///
    /// This method:
    /// - Creates the PARLIO TX unit
    /// - Configures GPIO pins
    /// - Sets clock frequency
    /// - Allocates hardware resources
    ///
    /// Call once during engine initialization. Must succeed before any
    /// other methods can be used.
    virtual bool initialize(const ParlioPeripheralConfig& config) = 0;

    /// @brief Enable PARLIO TX unit for transmission
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: parlio_tx_unit_enable()
    ///
    /// Must be called before transmit(). The peripheral remains enabled
    /// until disable() is called. Multiple transmit() calls can occur
    /// while enabled.
    virtual bool enable() = 0;

    /// @brief Disable PARLIO TX unit after transmission
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: parlio_tx_unit_disable()
    ///
    /// Call after waitAllDone() completes. Disabling while transmission
    /// is active may cause data corruption or hardware errors.
    virtual bool disable() = 0;

    /// @brief Check if peripheral is initialized
    /// @return true if initialized, false otherwise
    ///
    /// Used by ParlioEngine to detect if peripheral was reset (for testing).
    /// Production hardware: Always returns true after initialize() succeeds.
    /// Mock implementation: Returns false after reset() is called.
    virtual bool isInitialized() const = 0;

    //=========================================================================
    // Transmission Methods
    //=========================================================================

    /// @brief Submit a DMA buffer for transmission
    /// @param buffer DMA buffer pointer (must be 64-byte aligned)
    /// @param bit_count Number of bits to transmit (buffer size * 8)
    /// @param idle_value Value to output when idle (typically 0x0000)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: parlio_tx_unit_transmit()
    ///
    /// This method queues a DMA buffer for transmission. The buffer must:
    /// - Be allocated via allocateDmaBuffer() (DMA-capable, 64-byte aligned)
    /// - Remain valid until TX done callback fires
    /// - Contain bit-parallel waveform data (PARLIO format)
    ///
    /// The peripheral will trigger the TX done callback when transmission
    /// completes. Multiple buffers can be queued (up to queue_depth).
    virtual bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) = 0;

    /// @brief Wait for all queued transmissions to complete
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if all transmissions complete, false on timeout or error
    ///
    /// Maps to ESP-IDF: parlio_tx_unit_wait_all_done()
    ///
    /// Blocks until all queued DMA buffers have been transmitted, or timeout
    /// occurs. Use timeout_ms = 0 for non-blocking status check.
    ///
    /// Returns true if:
    /// - All transmissions complete within timeout
    /// - No transmissions are active (immediate return)
    ///
    /// Returns false if:
    /// - Timeout occurs before completion
    /// - Hardware error occurs during transmission
    virtual bool waitAllDone(uint32_t timeout_ms) = 0;

    //=========================================================================
    // ISR Callback Registration
    //=========================================================================

    /// @brief Register ISR callback for transmission completion events
    /// @param callback Function pointer to ISR callback (cast to void*)
    /// @param user_ctx User context pointer (passed to callback)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: parlio_tx_unit_register_event_callbacks()
    ///
    /// Callback signature (cast from void*):
    /// ```cpp
    /// bool callback(void* tx_unit, const void* edata, void* user_ctx);
    /// ```
    ///
    /// The callback:
    /// - Runs in ISR context (MUST be ISR-safe)
    /// - Receives opaque tx_unit handle (implementation-specific)
    /// - Receives event data (implementation-specific, typically nullptr)
    /// - Receives user context pointer (set via this method)
    /// - Returns true if high-priority task woken, false otherwise
    ///
    /// ⚠️  ISR SAFETY RULES:
    /// - NO logging (FL_LOG_PARLIO, FL_WARN, FL_DBG, printf, etc.)
    /// - NO blocking operations (mutex, delay, heap allocation)
    /// - MINIMIZE execution time (<10µs ideal)
    /// - Use atomic operations and memory barriers for shared state
    ///
    /// See ParlioIsrContext documentation for detailed ISR safety guidelines.
    virtual bool registerTxDoneCallback(void* callback, void* user_ctx) = 0;

    //=========================================================================
    // DMA Memory Management
    //=========================================================================

    /// @brief Allocate DMA-capable buffer with 64-byte alignment
    /// @param size Size in bytes (will be rounded up to 64-byte multiple)
    /// @return Pointer to allocated buffer, or nullptr on error
    ///
    /// Maps to ESP-IDF: heap_caps_aligned_alloc(64, size, MALLOC_CAP_DMA)
    ///
    /// The returned buffer:
    /// - Is 64-byte aligned (cache line alignment)
    /// - Is DMA-capable (can be used by PARLIO hardware)
    /// - Must be freed via freeDmaBuffer() when done
    ///
    /// Size is automatically rounded up to 64-byte multiple to ensure
    /// cache sync operations work correctly (address AND size must be aligned).
    virtual uint8_t* allocateDmaBuffer(size_t size) = 0;

    /// @brief Free DMA buffer allocated via allocateDmaBuffer()
    /// @param buffer Buffer pointer (must be from allocateDmaBuffer())
    ///
    /// Maps to ESP-IDF: heap_caps_free()
    ///
    /// Safe to call with nullptr (no-op).
    virtual void freeDmaBuffer(uint8_t* buffer) = 0;

    //=========================================================================
    // Platform Utilities
    //=========================================================================

    /// @brief Portable task delay (platform-independent sleep)
    /// @param ms Delay duration in milliseconds
    ///
    /// Maps to platform-specific delay:
    /// - ESP32/FreeRTOS: vTaskDelay(pdMS_TO_TICKS(ms))
    /// - Host/Mock: std::this_thread::sleep_for() or usleep()
    ///
    /// This method provides a portable delay abstraction for cleanup operations,
    /// ISR stabilization delays, and debug task synchronization. The virtual
    /// dispatch overhead is minimal (1-5 CPU ticks) compared to the typical
    /// delay durations (10-600ms).
    ///
    /// Use cases:
    /// - Destructor: Wait for in-flight ISRs to complete (10ms)
    /// - Destructor: Wait for debug task to self-delete (600ms)
    /// - Debug task: Sleep between status prints (500ms)
    ///
    /// ⚠️  NOT for timing-critical operations. Use hardware timers or busy-wait
    /// for sub-millisecond precision requirements.
    virtual void delay(uint32_t ms) = 0;

    //=========================================================================
    // Task Management (REMOVED - Use fl::TaskCoroutine directly)
    //=========================================================================
    //
    // Task management has been removed from this interface.
    // Use fl::task::coroutine() instead of peripheral->createTask().
    //
    // Migration guide:
    // - OLD: task_handle_t h = peripheral->createTask(config);
    // - NEW: auto task = fl::task::coroutine({.function = func, .name = name});
    // - Self-deletion: fl::task::exitCurrent();

    //=========================================================================
    // Timer Management (REMOVED - Use fl/isr.h directly)
    //=========================================================================
    //
    // Timer management has been removed from this interface.
    // Use fl::isr::attachTimerHandler() and related functions from fl/isr.h instead.
    //
    // Migration guide:
    // - createTimer() + enableTimer() + startTimer() → fl::isr::attachTimerHandler()
    // - stopTimer() + disableTimer() + deleteTimer() → fl::isr::detachHandler()
    // - Temporary disable/enable → fl::isr::disableHandler() / fl::isr::enableHandler()

    //=========================================================================
    // Time Utilities
    //=========================================================================

    /// @brief Get current timestamp in microseconds
    /// @return Current timestamp in microseconds (monotonic clock)
    ///
    /// Maps to:
    /// - ESP32: esp_timer_get_time()
    /// - Mock: std::chrono::high_resolution_clock or simulated time
    ///
    /// Used for debug timestamps and performance measurement. Monotonic clock
    /// (does not jump backwards). Precision varies by platform (ESP32: ~1µs).
    virtual uint64_t getMicroseconds() = 0;

    //=========================================================================
    // Memory Management
    //=========================================================================

    /// @brief Free DMA-capable memory allocated by allocateDma()
    /// @param ptr Pointer to DMA memory to free (nullptr is safe, no-op)
    ///
    /// Maps to:
    /// - ESP32: heap_caps_free()
    /// - Mock: free() or delete[]
    ///
    /// MUST be used to free memory allocated by allocateDma(). Using standard
    /// free() on DMA memory may cause undefined behavior on some platforms.
    ///
    /// Thread-safe: Yes (can be called from any context)
    /// ISR-safe: No (may allocate/deallocate memory internally)
    virtual void freeDmaBuffer(void* ptr) = 0;
};

} // namespace detail
} // namespace fl
