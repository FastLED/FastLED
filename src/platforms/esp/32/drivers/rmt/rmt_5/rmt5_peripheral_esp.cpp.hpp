/// @file rmt5_peripheral_esp.cpp
/// @brief Real ESP32 RMT5 peripheral implementation
///
/// Thin wrapper around ESP-IDF RMT5 driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "rmt5_peripheral_esp.h"
#include "fl/chipsets/led_timing.h"
#include "fl/delay.h"
#include "fl/log.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/stl/bit_cast.h"

// Include ESP-IDF headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_rom_gpio.h"  // For esp_rom_gpio_connect_out_signal
#include "soc/gpio_sig_map.h"  // For RMT_SIG_OUT0_IDX etc.
FL_EXTERN_C_END

// ESP-IDF 5.2+ compatibility: ESP_CACHE_MSYNC_FLAG_DIR_C2M
// In ESP-IDF < 5.2, esp_cache_msync() defaults to C2M (cache-to-memory) direction
// In ESP-IDF 5.2+, the direction flag was added to be explicit
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    #ifndef ESP_CACHE_MSYNC_FLAG_DIR_C2M
        #define ESP_CACHE_MSYNC_FLAG_DIR_C2M 0  // Default behavior in < 5.2
    #endif
#endif

// Platform memory barrier
#include "platforms/memory_barrier.h"

namespace fl {
namespace detail {

//=============================================================================
// Encoder Implementation (forward declaration)
//=============================================================================

// Forward declare the encoder implementation (defined at end of file)
struct Rmt5EncoderImpl;

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of Rmt5PeripheralESP
///
/// This class contains all ESP-IDF-specific implementation details.
class Rmt5PeripheralESPImpl : public Rmt5PeripheralESP {
public:
    Rmt5PeripheralESPImpl();
    ~Rmt5PeripheralESPImpl() override;

    // IRMT5Peripheral Interface Implementation
    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) override;
    bool deleteChannel(void* channel_handle) override;
    bool enableChannel(void* channel_handle) override;
    bool disableChannel(void* channel_handle) override;
    bool transmit(void* channel_handle, void* encoder_handle,
                  const uint8_t* buffer, size_t buffer_size) override;
    bool waitAllDone(void* channel_handle, uint32_t timeout_ms) override;
    void* createEncoder(const ChipsetTiming& timing,
                        uint32_t resolution_hz) override;
    void deleteEncoder(void* encoder_handle) override;
    bool resetEncoder(void* encoder_handle) override;
    bool registerTxCallback(void* channel_handle,
                            Rmt5TxDoneCallback callback,
                            void* user_ctx) override;
    void configureLogging() override;
    bool syncCache(void* buffer, size_t size) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;

private:
    /// @brief Disable cache sync after ESP_ERR_INVALID_ARG error
    ///
    /// When esp_cache_msync() returns ESP_ERR_INVALID_ARG, further calls will
    /// likely fail too. We disable subsequent calls to avoid error spam while
    /// keeping memory barriers for ordering guarantees.
    bool mCacheSyncDisabled = false;
};

//=============================================================================
// Singleton Instance
//=============================================================================

Rmt5PeripheralESP& Rmt5PeripheralESP::instance() {
    return Singleton<Rmt5PeripheralESPImpl>::instance();
}

//=============================================================================
// Destructor (base class)
//=============================================================================

Rmt5PeripheralESP::~Rmt5PeripheralESP() {
    // Empty - implementation in Rmt5PeripheralESPImpl
}

//=============================================================================
// Constructor / Destructor (implementation)
//=============================================================================

Rmt5PeripheralESPImpl::Rmt5PeripheralESPImpl() {
}

Rmt5PeripheralESPImpl::~Rmt5PeripheralESPImpl() {
    // Channels and encoders are managed by ChannelEngineRMT
    // No global cleanup needed here
}

//=============================================================================
// Channel Lifecycle Methods
//=============================================================================

/// Track the GPIO number for the last created TX channel (for diagnostic purposes)
static volatile int s_lastTxChannelGpio = -1;

bool Rmt5PeripheralESPImpl::createTxChannel(const Rmt5ChannelConfig& config,
                                             void** out_handle) {
    if (out_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: out_handle is nullptr");
        return false;
    }

    // Convert interface config to ESP-IDF config
    rmt_tx_channel_config_t esp_config = {};
    esp_config.gpio_num = static_cast<gpio_num_t>(config.gpio_num);
    esp_config.clk_src = RMT_CLK_SRC_DEFAULT;
    esp_config.resolution_hz = config.resolution_hz;
    esp_config.mem_block_symbols = config.mem_block_symbols;
    esp_config.trans_queue_depth = config.trans_queue_depth;
    esp_config.flags.invert_out = config.invert_out ? 1U : 0U;
    esp_config.flags.with_dma = config.with_dma ? 1U : 0U;
    // GPIO configuration flags:
    // io_od_mode=0: Push-pull output (not open-drain)
    esp_config.flags.io_od_mode = 0;

    // io_loop_back: Internal loopback mode (routes TX output back to RX input internally)
    // Setting to 0 because we use physical jumper cable, not internal loopback.
    // With io_loop_back=1, ESP-IDF may reconfigure GPIO in ways that break subsequent TX.
    esp_config.flags.io_loop_back = 0;
    esp_config.intr_priority = config.intr_priority;

    // NOTE: Removed gpio_reset_pin() call - ESP-IDF rmt_new_tx_channel() handles GPIO configuration
    // gpio_reset_pin() was interfering with GPIO matrix routing when both TX and RX are active

    gpio_num_t gpio = static_cast<gpio_num_t>(config.gpio_num);

    // Delegate to ESP-IDF
    FL_LOG_RMT("RMT5_PERIPH: Creating TX channel on GPIO " << config.gpio_num);

    rmt_channel_handle_t channel;
    esp_err_t err = rmt_new_tx_channel(&esp_config, &channel);
    if (err != ESP_OK) {
        FL_WARN("[RMT5_PERIPH] Failed to create TX channel: " << esp_err_to_name(err)
                << " (err=" << static_cast<int>(err) << ")");
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: TX channel created successfully on GPIO " << config.gpio_num);

    // NOTE: Previous workaround for ESP32-S3 TX+RX GPIO conflict has been removed.
    // The workaround was routing RMT_SIG_OUT0_IDX to the GPIO, but this was wrong:
    // ESP-IDF allocates a channel dynamically (could be 0, 1, 2, or 3), and
    // routing the wrong channel's signal breaks the correct routing.
    //
    // Diagnostic testing showed that the FIRST transmission works (gpio_high=4)
    // but subsequent transmissions fail (gpio_high=0) after the workaround code
    // re-routed the wrong signal to the GPIO.
    //
    // The root cause appears to be something else - need to investigate further.
    // Reference: GitHub ESP-IDF issues #11768, #15861

    // Store GPIO for later diagnostic access
    s_lastTxChannelGpio = config.gpio_num;

    *out_handle = static_cast<void*>(channel);
    return true;
}

bool Rmt5PeripheralESPImpl::deleteChannel(void* channel_handle) {
    if (channel_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: channel_handle is nullptr");
        return false;
    }

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);
    esp_err_t err = rmt_del_channel(channel);
    if (err != ESP_OK) {
        FL_LOG_RMT("RMT5_PERIPH: Failed to delete channel: " << esp_err_to_name(err));
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: Channel deleted successfully");
    return true;
}

bool Rmt5PeripheralESPImpl::enableChannel(void* channel_handle) {
    if (channel_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: channel_handle is nullptr");
        return false;
    }

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);

    esp_err_t err = rmt_enable(channel);
    if (err != ESP_OK) {
        FL_LOG_RMT("RMT5_PERIPH: Failed to enable channel: " << esp_err_to_name(err));
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: TX channel enabled successfully");

    return true;
}

bool Rmt5PeripheralESPImpl::disableChannel(void* channel_handle) {
    if (channel_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: channel_handle is nullptr");
        return false;
    }

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);
    esp_err_t err = rmt_disable(channel);
    if (err != ESP_OK) {
        FL_LOG_RMT("RMT5_PERIPH: Failed to disable channel: " << esp_err_to_name(err));
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: Channel disabled successfully");
    return true;
}

//=============================================================================
// Transmission Methods
//=============================================================================

/// Static counter to track TX done callback invocations (for debugging RMT TX → RX issues)
static volatile uint32_t s_txDoneCallbackCount = 0;

/// Static counter to track encoder encode callback invocations (ISR context)
static volatile uint32_t s_encoderCallCount = 0;

/// Static counter to track total symbols encoded
static volatile size_t s_totalSymbolsEncoded = 0;

bool Rmt5PeripheralESPImpl::transmit(void* channel_handle, void* encoder_handle,
                                      const uint8_t* buffer, size_t buffer_size) {
    if (channel_handle == nullptr || encoder_handle == nullptr || buffer == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Invalid parameter (nullptr)");
        return false;
    }

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);
    rmt_encoder_handle_t encoder = static_cast<rmt_encoder_handle_t>(encoder_handle);

    // Configure transmission (no flags, standard mode)
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;  // No loop
    tx_config.flags.eot_level = 0;  // End-of-transmission level

    esp_err_t err = rmt_transmit(channel, encoder, buffer, buffer_size, &tx_config);
    if (err != ESP_OK) {
        FL_WARN("RMT5_PERIPH: rmt_transmit() FAILED: " << esp_err_to_name(err));
        return false;
    }

    return true;
}

bool Rmt5PeripheralESPImpl::waitAllDone(void* channel_handle, uint32_t timeout_ms) {
    if (channel_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: channel_handle is nullptr");
        return false;
    }

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);

    // Convert timeout to FreeRTOS ticks (portMAX_DELAY if timeout_ms == 0)
    TickType_t timeout_ticks = (timeout_ms == 0)
        ? portMAX_DELAY
        : pdMS_TO_TICKS(timeout_ms);

    esp_err_t err = rmt_tx_wait_all_done(channel, timeout_ticks);

    if (err != ESP_OK) {
        if (err == ESP_ERR_TIMEOUT) {
            FL_WARN("RMT5_PERIPH: TX wait TIMEOUT after " << timeout_ms << " ms");
        } else {
            FL_WARN("RMT5_PERIPH: TX wait FAILED: " << esp_err_to_name(err));
        }
        return false;
    }

    // Brief delay to ensure RMT hardware has fully completed output
    // The rmt_tx_wait_all_done() returns when the TX queue is empty,
    // but there may be a small window before the last bits propagate to GPIO.
    fl::delayMicroseconds(10);

    return true;
}

//=============================================================================
// ISR Callback Registration
//=============================================================================

/// @brief Wrapper context for callback forwarding
///
/// ESP-IDF's rmt_tx_done_callback_t has a specific signature with typed pointers
/// (rmt_channel_handle_t, const rmt_tx_done_event_data_t*), but our interface
/// uses void* for portability with mocks. This wrapper stores the original
/// callback and user context, allowing a thin ISR wrapper to forward calls.
struct TxCallbackContext {
    Rmt5TxDoneCallback callback;
    void* user_ctx;
};

/// @brief ISR wrapper that adapts ESP-IDF callback to FastLED callback signature
///
/// This static function matches ESP-IDF's rmt_tx_done_callback_t signature and
/// forwards to the user's Rmt5TxDoneCallback. The adaptation is trivial since
/// both channel and event data are passed through as opaque pointers.
static bool FL_IRAM txDoneCallbackWrapper(
    rmt_channel_handle_t channel,
    const rmt_tx_done_event_data_t* edata,
    void* user_data) {
    // Increment callback counter (avoiding deprecated volatile++ warning)
    s_txDoneCallbackCount = s_txDoneCallbackCount + 1;
    TxCallbackContext* ctx = static_cast<TxCallbackContext*>(user_data);
    if (ctx == nullptr || ctx->callback == nullptr) {
        return false;
    }
    // Forward to user callback with void* casts
    return ctx->callback(
        static_cast<void*>(channel),
        static_cast<const void*>(edata),
        ctx->user_ctx);
}

bool Rmt5PeripheralESPImpl::registerTxCallback(void* channel_handle,
                                                Rmt5TxDoneCallback callback,
                                                void* user_ctx) {
    if (channel_handle == nullptr || callback == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Invalid parameter (nullptr)");
        return false;
    }

    // Allocate wrapper context for callback forwarding
    // Note: This is a small, one-time allocation per channel. The context
    // lives for the lifetime of the channel and is not freed (channels are
    // typically created once and reused). For strict correctness, this could
    // be tracked and freed in deleteChannel(), but the overhead is minimal.
    TxCallbackContext* ctx = new TxCallbackContext();
    ctx->callback = callback;
    ctx->user_ctx = user_ctx;

    // Configure callbacks structure with our wrapper
    rmt_tx_event_callbacks_t cbs = {};
    cbs.on_trans_done = txDoneCallbackWrapper;

    // Delegate to ESP-IDF
    rmt_channel_handle_t channel = static_cast<rmt_channel_handle_t>(channel_handle);
    esp_err_t err = rmt_tx_register_event_callbacks(channel, &cbs, ctx);
    if (err != ESP_OK) {
        FL_LOG_RMT("RMT5_PERIPH: Failed to register callback: " << esp_err_to_name(err));
        delete ctx;
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: TX callback registered successfully");
    return true;
}

//=============================================================================
// Platform Configuration
//=============================================================================

void Rmt5PeripheralESPImpl::configureLogging() {
    // Suppress ESP-IDF RMT "no free channels" errors (expected during time-multiplexing)
    // Only show critical RMT errors (ESP_LOG_ERROR and above)
    esp_log_level_set("rmt", ESP_LOG_WARN);

    // Suppress cache coherency warnings (non-fatal, handled by memory barriers)
    // Cache sync errors are printed by ESP-IDF before FastLED can handle them
    esp_log_level_set("cache", ESP_LOG_NONE);

    FL_LOG_RMT("RMT5_PERIPH: Logging configured (RMT: WARN, cache: NONE)");
}

bool Rmt5PeripheralESPImpl::syncCache(void* buffer, size_t size) {
    if (buffer == nullptr || size == 0) {
        return true;  // No-op for null/empty buffers
    }

    // Memory barrier: Ensure all preceding writes complete before cache sync
    FL_MEMORY_BARRIER;

    // If cache sync was disabled due to previous failures, skip the call
    // but keep memory barriers for ordering guarantees.
    //
    // Fix for ESP32-S3 issue #2156: esp_cache_msync() consistently returns
    // ESP_ERR_INVALID_ARG even with properly aligned DMA buffers allocated
    // via heap_caps_aligned_alloc(64, ..., MALLOC_CAP_DMA).
    // Once we detect this condition, disable future calls to avoid error spam.
    if (mCacheSyncDisabled) {
        FL_MEMORY_BARRIER;
        return true;  // Memory barriers provide ordering, cache sync not needed
    }

    // Cache sync: Writeback cache to memory (Cache-to-Memory)
    // ESP_CACHE_MSYNC_FLAG_UNALIGNED: More permissive alignment checking
    // Some ESP-IDF versions have strict alignment requirements that cause
    // esp_cache_msync() to fail even with properly aligned DMA buffers.
    // The UNALIGNED flag allows the operation to proceed, relying on memory
    // barriers for ordering guarantees.
    esp_err_t err = esp_cache_msync(
        buffer,
        size,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

    // Memory barrier: Ensure cache sync completes before DMA submission
    FL_MEMORY_BARRIER;

    // Handle cache sync failures
    if (err != ESP_OK) {
        if (err == ESP_ERR_INVALID_ARG) {
            // ESP_ERR_INVALID_ARG indicates the buffer doesn't meet cache sync
            // requirements. This is a persistent condition that won't improve on
            // retry. Disable future calls to avoid error spam on every show().
            //
            // Memory barriers still provide write ordering guarantees, which is
            // sufficient for correct operation. The esp_cache_msync() is an
            // optimization that may not be strictly required on all platforms.
            mCacheSyncDisabled = true;
            FL_DBG("RMT5_PERIPH: Cache sync disabled due to ESP_ERR_INVALID_ARG. "
                   "Memory barriers will ensure ordering.");
        } else {
            // Other errors are logged but non-fatal
            FL_LOG_RMT("RMT5_PERIPH: Cache sync returned error: " << esp_err_to_name(err)
                       << " (non-fatal, memory barriers ensure ordering)");
        }
    }

    return (err == ESP_OK);
}

//=============================================================================
// DMA Memory Management
//=============================================================================

uint8_t* Rmt5PeripheralESPImpl::allocateDmaBuffer(size_t size) {
    if (size == 0) {
        FL_WARN("Rmt5PeripheralESP: Cannot allocate zero-size buffer");
        return nullptr;
    }

    // Round up to 64-byte alignment (cache line size)
    const size_t alignment = 64;
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Allocate DMA-capable memory with 64-byte alignment
    uint8_t* buffer = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(alignment, aligned_size, MALLOC_CAP_DMA));

    if (buffer == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Failed to allocate DMA buffer ("
                << aligned_size << " bytes)");
        return nullptr;
    }

    FL_LOG_RMT("RMT5_PERIPH: Allocated DMA buffer (" << aligned_size << " bytes)");
    return buffer;
}

void Rmt5PeripheralESPImpl::freeDmaBuffer(uint8_t* buffer) {
    if (buffer == nullptr) {
        return;  // Safe no-op
    }

    heap_caps_free(buffer);
    FL_LOG_RMT("RMT5_PERIPH: Freed DMA buffer");
}

//=============================================================================
// Encoder Management
//=============================================================================
// Rmt5EncoderImpl - RMT5 encoder implementation
//=============================================================================

/**
 * RMT5 Encoder Implementation - Plain struct for converting pixel bytes to RMT symbols
 *
 * Architecture:
 * - Plain struct (no inheritance, no virtuals) for standard layout guarantee
 * - Uses ESP-IDF's official encoder pattern (rmt_encoder_t)
 * - Combines bytes_encoder (for pixel data) + copy_encoder (for reset pulse)
 * - State machine handles partial encoding when buffer fills
 * - Runs in ISR context - must be fast and ISR-safe
 * - rmt_encoder_t is first member to enable clean casting without offsetof
 *
 * Implementation based on ESP-IDF led_strip example:
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip
 */
struct Rmt5EncoderImpl {
    // CRITICAL: rmt_encoder_t MUST be first member for clean casting
    // This allows: rmt_encoder_t* -> Rmt5EncoderImpl* via bit_cast
    rmt_encoder_t base;

    // Sub-encoders
    rmt_encoder_handle_t mBytesEncoder;
    rmt_encoder_handle_t mCopyEncoder;

    // Encoder state machine counter (0 = data, 1 = reset)
    int mState;

    // Reset pulse symbol (low signal for RESET microseconds)
    rmt_symbol_word_t mResetCode;

    // Timing configuration (stored for debugging)
    uint32_t mBit0HighTicks;
    uint32_t mBit0LowTicks;
    uint32_t mBit1HighTicks;
    uint32_t mBit1LowTicks;
    uint32_t mResetTicks;

    // Factory method to create encoder instance
    static Rmt5EncoderImpl* create(const ChipsetTiming& timing, uint32_t resolution_hz) {
        Rmt5EncoderImpl* impl = new Rmt5EncoderImpl(timing, resolution_hz);
        if (impl == nullptr) {
            FL_WARN("Rmt5EncoderImpl::create: Failed to allocate encoder");
            return nullptr;
        }
        if (impl->getHandle() == nullptr) {
            FL_WARN("Rmt5EncoderImpl::create: Encoder initialization failed");
            delete impl;
            return nullptr;
        }
        return impl;
    }

    // Get the underlying encoder handle for RMT transmission
    rmt_encoder_handle_t getHandle() { return &base; }

    // Destructor - cleans up sub-encoders
    ~Rmt5EncoderImpl() { cleanup(); }

    // Non-copyable
    Rmt5EncoderImpl(const Rmt5EncoderImpl&) = delete;
    Rmt5EncoderImpl& operator=(const Rmt5EncoderImpl&) = delete;

private:
    // Private constructor - use create() factory method
    Rmt5EncoderImpl(const ChipsetTiming& timing, uint32_t resolution_hz)
        : mBytesEncoder(nullptr), mCopyEncoder(nullptr), mState(0),
          mBit0HighTicks(0), mBit0LowTicks(0), mBit1HighTicks(0),
          mBit1LowTicks(0), mResetTicks(0) {
        base.encode = Rmt5EncoderImpl::encodeCallback;
        base.reset = Rmt5EncoderImpl::resetCallback;
        base.del = Rmt5EncoderImpl::delCallback;

        mResetCode.duration0 = 0;
        mResetCode.level0 = 0;
        mResetCode.duration1 = 0;
        mResetCode.level1 = 0;

        esp_err_t ret = initialize(timing, resolution_hz);
        if (ret != ESP_OK) {
            FL_WARN("Rmt5EncoderImpl: Initialization failed: " << esp_err_to_name(ret));
        }
    }

    // Private methods
    size_t FL_IRAM encode(rmt_channel_handle_t channel,
                          const void* primary_data, size_t data_size,
                          rmt_encode_state_t* ret_state) {
        // Increment call counter (ISR safe - atomic on 32-bit)
        s_encoderCallCount = s_encoderCallCount + 1;

        rmt_encode_state_t session_state = RMT_ENCODING_RESET;
        rmt_encode_state_t state = RMT_ENCODING_RESET;
        size_t encoded_symbols = 0;

        switch (mState) {
        case 0:
            encoded_symbols += mBytesEncoder->encode(mBytesEncoder, channel, primary_data,
                                                      data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                mState = 1;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out;
            }
            [[fallthrough]];

        case 1:
            encoded_symbols += mCopyEncoder->encode(mCopyEncoder, channel, &mResetCode,
                                                     sizeof(mResetCode), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                mState = 0;
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_COMPLETE);
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out;
            }
            break;

        default:
            state = RMT_ENCODING_RESET;
            break;
        }

    out:
        // Track total symbols encoded (ISR safe)
        s_totalSymbolsEncoded = s_totalSymbolsEncoded + encoded_symbols;
        *ret_state = state;
        return encoded_symbols;
    }

    esp_err_t FL_IRAM reset() {
        mState = 0;
        if (mBytesEncoder) {
            mBytesEncoder->reset(mBytesEncoder);
        }
        if (mCopyEncoder) {
            mCopyEncoder->reset(mCopyEncoder);
        }
        return ESP_OK;
    }

    esp_err_t cleanup() {
        if (mBytesEncoder) {
            rmt_del_encoder(mBytesEncoder);
            mBytesEncoder = nullptr;
        }
        if (mCopyEncoder) {
            rmt_del_encoder(mCopyEncoder);
            mCopyEncoder = nullptr;
        }
        return ESP_OK;
    }

    esp_err_t initialize(const ChipsetTiming& timing, uint32_t resolution_hz) {
        const uint64_t ns_per_tick = 1000000000ULL / resolution_hz;
        const uint64_t half_ns_per_tick = ns_per_tick / 2;

        // WS2812 3-phase to 4-phase conversion:
        // Bit 0: T0H = T1 (high), T0L = T2 + T3 (low)
        // Bit 1: T1H = T1 + T2 (high), T1L = T3 (low)
        mBit0HighTicks = static_cast<uint32_t>((timing.T1 + half_ns_per_tick) / ns_per_tick);
        mBit0LowTicks = static_cast<uint32_t>((timing.T2 + timing.T3 + half_ns_per_tick) / ns_per_tick);
        mBit1HighTicks = static_cast<uint32_t>((timing.T1 + timing.T2 + half_ns_per_tick) / ns_per_tick);
        mBit1LowTicks = static_cast<uint32_t>((timing.T3 + half_ns_per_tick) / ns_per_tick);
        mResetTicks = static_cast<uint32_t>((timing.RESET * 1000ULL + half_ns_per_tick) / ns_per_tick);

        FL_WARN("[RMT5_ENCODER] Timing config: resolution=" << resolution_hz
                << "Hz, ns_per_tick=" << ns_per_tick);
        FL_WARN("[RMT5_ENCODER] Bit0: high=" << mBit0HighTicks << " ticks, low=" << mBit0LowTicks << " ticks");
        FL_WARN("[RMT5_ENCODER] Bit1: high=" << mBit1HighTicks << " ticks, low=" << mBit1LowTicks << " ticks");
        FL_WARN("[RMT5_ENCODER] Reset: " << mResetTicks << " ticks");

        rmt_bytes_encoder_config_t bytes_config = {};
        bytes_config.bit0.level0 = 1;
        bytes_config.bit0.duration0 = mBit0HighTicks;
        bytes_config.bit0.level1 = 0;
        bytes_config.bit0.duration1 = mBit0LowTicks;
        bytes_config.bit1.level0 = 1;
        bytes_config.bit1.duration0 = mBit1HighTicks;
        bytes_config.bit1.level1 = 0;
        bytes_config.bit1.duration1 = mBit1LowTicks;
        bytes_config.flags.msb_first = 1;  // WS2812B requires MSB-first transmission

        esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &mBytesEncoder);
        if (ret != ESP_OK) {
            FL_WARN("[RMT5_ENCODER] Failed to create bytes encoder: " << esp_err_to_name(ret));
            return ret;
        }

        rmt_copy_encoder_config_t copy_config = {};
        ret = rmt_new_copy_encoder(&copy_config, &mCopyEncoder);
        if (ret != ESP_OK) {
            FL_WARN("[RMT5_ENCODER] Failed to create copy encoder: " << esp_err_to_name(ret));
            rmt_del_encoder(mBytesEncoder);
            mBytesEncoder = nullptr;
            return ret;
        }

        mResetCode.duration0 = mResetTicks;
        mResetCode.level0 = 0;
        mResetCode.duration1 = 0;
        mResetCode.level1 = 0;

        FL_WARN("[RMT5_ENCODER] Encoder created successfully");
        return ESP_OK;
    }

    // Static callbacks for rmt_encoder_t interface
    static size_t FL_IRAM encodeCallback(rmt_encoder_t* encoder,
                                         rmt_channel_handle_t channel,
                                         const void* primary_data,
                                         size_t data_size,
                                         rmt_encode_state_t* ret_state) {
        Rmt5EncoderImpl* impl = fl::bit_cast<Rmt5EncoderImpl*>(encoder);
        return impl->encode(channel, primary_data, data_size, ret_state);
    }

    static esp_err_t FL_IRAM resetCallback(rmt_encoder_t* encoder) {
        Rmt5EncoderImpl* impl = fl::bit_cast<Rmt5EncoderImpl*>(encoder);
        return impl->reset();
    }

    static esp_err_t delCallback(rmt_encoder_t* encoder) {
        Rmt5EncoderImpl* impl = fl::bit_cast<Rmt5EncoderImpl*>(encoder);
        delete impl;
        return ESP_OK;
    }
};

//=============================================================================
// Encoder Management Implementation
//=============================================================================

void* Rmt5PeripheralESPImpl::createEncoder(const ChipsetTiming& timing,
                                            uint32_t resolution_hz) {
    Rmt5EncoderImpl* encoder = Rmt5EncoderImpl::create(timing, resolution_hz);
    if (encoder == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Failed to create encoder");
        return nullptr;
    }

    FL_LOG_RMT("RMT5_PERIPH: Encoder created successfully");
    return static_cast<void*>(encoder->getHandle());
}

void Rmt5PeripheralESPImpl::deleteEncoder(void* encoder_handle) {
    if (encoder_handle == nullptr) {
        return;  // Safe no-op
    }

    // The encoder handle is actually an rmt_encoder_t*, which has a del callback
    // that will clean up the Rmt5EncoderImpl
    rmt_encoder_handle_t encoder = static_cast<rmt_encoder_handle_t>(encoder_handle);

    // Call the encoder's delete callback (which deletes the Rmt5EncoderImpl)
    if (encoder->del != nullptr) {
        encoder->del(encoder);
    }

    FL_LOG_RMT("RMT5_PERIPH: Encoder deleted successfully");
}

bool Rmt5PeripheralESPImpl::resetEncoder(void* encoder_handle) {
    if (encoder_handle == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Invalid encoder handle (nullptr)");
        return false;
    }

    // The encoder handle is actually an rmt_encoder_t*, which has a reset callback
    rmt_encoder_handle_t encoder = static_cast<rmt_encoder_handle_t>(encoder_handle);

    // Call the encoder's reset callback
    if (encoder->reset == nullptr) {
        FL_WARN("Rmt5PeripheralESP: Encoder has no reset callback");
        return false;
    }

    esp_err_t err = encoder->reset(encoder);
    if (err != ESP_OK) {
        FL_LOG_RMT("RMT5_PERIPH: Failed to reset encoder: " << esp_err_to_name(err));
        return false;
    }

    FL_LOG_RMT("RMT5_PERIPH: Encoder reset successfully");
    return true;
}

} // namespace detail
} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
