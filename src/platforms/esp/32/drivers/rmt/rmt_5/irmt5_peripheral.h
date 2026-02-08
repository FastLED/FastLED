/// @file irmt5_peripheral.h
/// @brief Virtual interface for RMT5 peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the ChannelEngineRMT.
/// It abstracts all ESP-IDF RMT5 API calls into a clean interface that can be:
/// - Implemented by Rmt5PeripheralESP (real hardware delegate)
/// - Implemented by Rmt5PeripheralMock (unit test simulation)
///
/// ## Design Rationale
///
/// The ChannelEngineRMT contains complex logic for channel time-multiplexing,
/// buffer management, and ISR coordination. This logic should be unit testable
/// without requiring real ESP32 hardware. By extracting a virtual peripheral
/// interface, we achieve:
///
/// 1. **Testability**: Mock implementation enables host-based unit tests
/// 2. **Separation of Concerns**: Hardware delegation vs. business logic
/// 3. **Performance**: Virtual dispatch adds only ~2-3 CPU cycles overhead
/// 4. **Maintainability**: Clear contract between engine and hardware
///
/// ## Interface Contract
///
/// - All methods return `bool` (true = success, false = error)
/// - Methods mirror ESP-IDF RMT5 API semantics exactly
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
/// - `registerTxCallback()` callback runs in ISR context
/// - Callback MUST be ISR-safe (no logging, blocking, or heap allocation)
/// - See ChannelEngineRMT::transmitDoneCallback for ISR safety rules

#pragma once

// This interface is platform-agnostic (no ESP32 guard)
// - Rmt5PeripheralESP requires ESP32 (real hardware)
// - Rmt5PeripheralMock works on all platforms (testing)

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {

// Forward declaration
struct ChipsetTiming;

namespace detail {

//=============================================================================
// Callback Types
//=============================================================================

/// @brief TX done callback type for RMT transmission completion
///
/// This callback is invoked when RMT transmission completes.
/// It runs in ISR context on ESP32, so it must be ISR-safe.
///
/// @param channel_handle Opaque channel handle (rmt_channel_handle_t on ESP32)
/// @param event_data Opaque event data pointer (rmt_tx_done_event_data_t* on ESP32, may be nullptr)
/// @param user_ctx User context pointer from registerTxCallback
/// @return true if high-priority task was woken, false otherwise
///
/// Note: Uses void* for event_data to match ESP-IDF's callback signature exactly,
/// avoiding any function pointer casting which would trigger UBSan.
using Rmt5TxDoneCallback = bool (*)(void* channel_handle,
                                     const void* event_data,
                                     void* user_ctx);

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief RMT5 TX channel configuration
///
/// Encapsulates all parameters needed to initialize an RMT TX channel.
/// Maps directly to ESP-IDF's rmt_tx_channel_config_t structure.
struct Rmt5ChannelConfig {
    int gpio_num;                   ///< GPIO pin number for RMT output
    u32 resolution_hz;         ///< Channel clock resolution (Hz)
    size_t mem_block_symbols;       ///< Memory block size in RMT symbols (48-bit symbols)
    size_t trans_queue_depth;       ///< Transaction queue depth (typically 1)
    bool invert_out;                ///< Invert output signal
    bool with_dma;                  ///< Enable DMA for this channel
    int intr_priority;              ///< Interrupt priority (0 = default, 1-7 = custom)

    /// @brief Default constructor (for mock testing)
    Rmt5ChannelConfig()
        : gpio_num(-1),
          resolution_hz(0),
          mem_block_symbols(0),
          trans_queue_depth(1),
          invert_out(false),
          with_dma(false),
          intr_priority(0) {}

    /// @brief Constructor with mandatory parameters
    /// @param pin GPIO pin number
    /// @param res_hz Clock resolution in Hz
    /// @param mem_blocks Memory block size in symbols
    /// @param queue_depth Transaction queue depth
    /// @param use_dma Enable DMA
    /// @param intr_pri Interrupt priority
    Rmt5ChannelConfig(int pin, u32 res_hz, size_t mem_blocks,
                      size_t queue_depth, bool use_dma, int intr_pri = 0)
        : gpio_num(pin),
          resolution_hz(res_hz),
          mem_block_symbols(mem_blocks),
          trans_queue_depth(queue_depth),
          invert_out(false),
          with_dma(use_dma),
          intr_priority(intr_pri) {}
};

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for RMT5 peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF RMT5 operations.
/// Implementations:
/// - Rmt5PeripheralESP: Thin wrapper around ESP-IDF APIs (real hardware)
/// - Rmt5PeripheralMock: Simulation for host-based unit tests
///
/// ## Usage Pattern
/// ```cpp
/// // Create peripheral (real or mock)
/// IRMT5Peripheral& peripheral = Rmt5PeripheralESP::instance();
///
/// // Create channel
/// void* channel_handle;
/// Rmt5ChannelConfig config(18, 40000000, 64, 1, true);
/// if (!peripheral.createTxChannel(config, &channel_handle)) { /* error */ }
///
/// // Create encoder
/// void* encoder = peripheral.createEncoder(WS2812_TIMING, 40000000);
///
/// // Register ISR callback
/// peripheral.registerTxCallback(channel_handle, callback, user_ctx);
///
/// // Start transmission
/// peripheral.enableChannel(channel_handle);
/// peripheral.transmit(channel_handle, encoder, buffer, size);
///
/// // Wait for completion
/// peripheral.waitAllDone(channel_handle, timeout_ms);
/// peripheral.disableChannel(channel_handle);
///
/// // Cleanup
/// peripheral.deleteEncoder(encoder);
/// peripheral.deleteChannel(channel_handle);
/// ```
class IRMT5Peripheral {
public:
    virtual ~IRMT5Peripheral() = default;

    //=========================================================================
    // Channel Lifecycle Methods
    //=========================================================================

    /// @brief Create RMT TX channel with configuration
    /// @param config Hardware configuration (pin, clock, memory, DMA)
    /// @param out_handle Output parameter for channel handle (opaque void*)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_new_tx_channel()
    ///
    /// This method:
    /// - Creates the RMT TX channel
    /// - Configures GPIO pin
    /// - Sets clock resolution
    /// - Allocates hardware memory blocks
    /// - Optionally enables DMA
    ///
    /// The returned handle is opaque (void*) to avoid ESP-IDF type leakage.
    /// Real implementation: rmt_channel_handle_t
    /// Mock implementation: integer channel ID or dummy pointer
    virtual bool createTxChannel(const Rmt5ChannelConfig& config,
                                 void** out_handle) = 0;

    /// @brief Delete RMT channel and free resources
    /// @param channel_handle Channel handle from createTxChannel()
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_del_channel()
    ///
    /// Frees all resources associated with the channel. The channel must
    /// be disabled before deletion.
    virtual bool deleteChannel(void* channel_handle) = 0;

    /// @brief Enable RMT TX channel for transmission
    /// @param channel_handle Channel handle from createTxChannel()
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_enable()
    ///
    /// Must be called before transmit(). The channel remains enabled
    /// until disableChannel() is called. Multiple transmit() calls can
    /// occur while enabled.
    virtual bool enableChannel(void* channel_handle) = 0;

    /// @brief Disable RMT TX channel after transmission
    /// @param channel_handle Channel handle from createTxChannel()
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_disable()
    ///
    /// Call after waitAllDone() completes. Disabling while transmission
    /// is active may cause data corruption or hardware errors.
    virtual bool disableChannel(void* channel_handle) = 0;

    //=========================================================================
    // Transmission Methods
    //=========================================================================

    /// @brief Submit pixel data for RMT transmission
    /// @param channel_handle Channel handle from createTxChannel()
    /// @param encoder_handle Encoder handle from createEncoder()
    /// @param buffer Pixel data buffer (RGB/GRB bytes)
    /// @param buffer_size Size of buffer in bytes
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_transmit()
    ///
    /// This method queues pixel data for transmission. The buffer must:
    /// - Remain valid until TX done callback fires
    /// - Contain RGB/GRB pixel data (encoder converts to RMT symbols)
    ///
    /// The encoder performs the pixel-to-waveform conversion. The peripheral
    /// will trigger the TX done callback when transmission completes.
    virtual bool transmit(void* channel_handle, void* encoder_handle,
                          const u8* buffer, size_t buffer_size) = 0;

    /// @brief Wait for all queued transmissions to complete
    /// @param channel_handle Channel handle from createTxChannel()
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if transmission complete, false on timeout or error
    ///
    /// Maps to ESP-IDF: rmt_tx_wait_all_done()
    ///
    /// Blocks until the channel completes transmission, or timeout occurs.
    /// Use timeout_ms = 0 for non-blocking status check.
    ///
    /// Returns true if:
    /// - Transmission completes within timeout
    /// - No transmission is active (immediate return)
    ///
    /// Returns false if:
    /// - Timeout occurs before completion
    /// - Hardware error occurs during transmission
    virtual bool waitAllDone(void* channel_handle, u32 timeout_ms) = 0;

    //=========================================================================
    // Encoder Management
    //=========================================================================

    /// @brief Create RMT encoder for LED chipset timing
    /// @param timing Chipset timing parameters (T1, T2, T3, RESET)
    /// @param resolution_hz Channel clock resolution (must match channel config)
    /// @return Encoder handle (opaque void*), or nullptr on error
    ///
    /// Maps to: Rmt5EncoderImpl::create()
    ///
    /// Creates an encoder that converts RGB/GRB pixel data into RMT symbols
    /// according to the chipset timing. The encoder:
    /// - Converts bit 0 → T0H high + T0L low pulses
    /// - Converts bit 1 → T1H high + T1L low pulses
    /// - Appends RESET pulse after pixel data
    ///
    /// The returned handle is opaque (void*) to avoid ESP-IDF type leakage.
    /// Real implementation: Rmt5EncoderImpl*
    /// Mock implementation: integer encoder ID or dummy pointer
    ///
    /// The encoder can be reused across multiple transmit() calls and
    /// multiple channels (if they share the same timing and resolution).
    virtual void* createEncoder(const ChipsetTiming& timing,
                                 u32 resolution_hz) = 0;

    /// @brief Delete encoder and free resources
    /// @param encoder_handle Encoder handle from createEncoder()
    ///
    /// Maps to: delete Rmt5EncoderImpl (which calls rmt_del_encoder)
    ///
    /// Frees all resources associated with the encoder. Safe to call with
    /// nullptr (no-op).
    virtual void deleteEncoder(void* encoder_handle) = 0;

    /// @brief Reset encoder state machine to initial state
    /// @param encoder_handle Encoder handle from createEncoder()
    /// @return true on success, false on error
    ///
    /// Maps to: rmt_encoder_t->reset()
    ///
    /// Resets the encoder's internal state machine back to its initial state.
    /// This should be called before each transmission to ensure:
    /// - State counter is reset to 0 (data phase)
    /// - Sub-encoders (bytes_encoder, copy_encoder) are reset
    /// - No leftover state from previous transmissions
    ///
    /// Called by ChannelEngineRMT before each transmit() operation.
    virtual bool resetEncoder(void* encoder_handle) = 0;

    //=========================================================================
    // ISR Callback Registration
    //=========================================================================

    /// @brief Register ISR callback for transmission completion events
    /// @param channel_handle Channel handle from createTxChannel()
    /// @param callback Function pointer to ISR callback
    /// @param user_ctx User context pointer (passed to callback)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: rmt_tx_register_event_callbacks()
    ///
    /// The callback:
    /// - Runs in ISR context (MUST be ISR-safe)
    /// - Receives opaque channel handle (implementation-specific)
    /// - Receives event data (may be nullptr)
    /// - Receives user context pointer (set via this method)
    /// - Returns true if high-priority task woken, false otherwise
    ///
    /// ⚠️  ISR SAFETY RULES:
    /// - NO logging (FL_LOG_RMT, FL_WARN, FL_DBG, printf, etc.)
    /// - NO blocking operations (mutex, delay, heap allocation)
    /// - MINIMIZE execution time (<10µs ideal)
    /// - Use atomic operations and memory barriers for shared state
    virtual bool registerTxCallback(void* channel_handle,
                                    Rmt5TxDoneCallback callback,
                                    void* user_ctx) = 0;

    //=========================================================================
    // Platform Configuration
    //=========================================================================

    /// @brief Configure platform-specific logging levels
    ///
    /// Maps to ESP-IDF: esp_log_level_set()
    ///
    /// This method configures logging verbosity for RMT and cache subsystems:
    /// - ESP32: Suppresses ESP-IDF "no free channels" warnings (expected during time-multiplexing)
    /// - ESP32: Disables cache coherency warnings (non-fatal, handled by memory barriers)
    /// - Mock: No-op (host platforms don't have ESP-IDF logging)
    ///
    /// Called once during ChannelEngineRMT construction.
    virtual void configureLogging() = 0;

    /// @brief Synchronize CPU cache to memory for DMA buffer
    /// @param buffer Pointer to buffer to sync
    /// @param size Size of buffer in bytes
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: esp_cache_msync() with memory barriers
    ///
    /// CRITICAL: This ensures CPU writes to LED buffers are flushed to SRAM
    /// before RMT DMA reads the data. Without this, DMA may read stale cache
    /// data, causing LED color corruption.
    ///
    /// Platform behavior:
    /// - ESP32-S3/C3/C6/H2: Have data cache, require explicit synchronization
    /// - ESP32/S2: No data cache, operation is a no-op
    /// - Mock: No-op (host platforms don't have DMA hardware)
    ///
    /// Even if cache sync fails, memory barriers ensure write ordering.
    /// Errors are logged but non-fatal.
    virtual bool syncCache(void* buffer, size_t size) = 0;

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
    /// - Is DMA-capable (can be used by RMT hardware)
    /// - Must be freed via freeDmaBuffer() when done
    ///
    /// Size is automatically rounded up to 64-byte multiple to ensure
    /// cache sync operations work correctly (address AND size must be aligned).
    virtual u8* allocateDmaBuffer(size_t size) = 0;

    /// @brief Free DMA buffer allocated via allocateDmaBuffer()
    /// @param buffer Buffer pointer (must be from allocateDmaBuffer())
    ///
    /// Maps to ESP-IDF: heap_caps_free()
    ///
    /// Safe to call with nullptr (no-op).
    virtual void freeDmaBuffer(u8* buffer) = 0;
};

} // namespace detail
} // namespace fl
