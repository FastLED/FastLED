#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_encoder.h"
#include "fl/chipsets/led_timing.h"
#include "fl/log.h"
#include "ftl/assert.h"
#include "ftl/memory.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_encoder.h"
#include "driver/rmt_common.h"
#include "esp_check.h"
FL_EXTERN_C_END

#define RMT5_ENCODER_TAG "rmt5_encoder"

namespace fl {

// Structure that extends rmt_encoder_t to hold our FastLEDRmtEncoder instance
typedef struct {
    rmt_encoder_t base;           // Must be first member for casting
    FastLEDRmtEncoder* instance;  // Pointer to C++ instance
} fastled_encoder_wrapper_t;

//=============================================================================
// FastLEDRmtEncoder Implementation
//=============================================================================

FastLEDRmtEncoder::FastLEDRmtEncoder()
    : mEncoder(nullptr)
    , mBytesEncoder(nullptr)
    , mCopyEncoder(nullptr)
    , mState(0)
    , mBit0HighTicks(0)
    , mBit0LowTicks(0)
    , mBit1HighTicks(0)
    , mBit1LowTicks(0)
    , mResetTicks(0)
{
    mResetCode.duration0 = 0;
    mResetCode.level0 = 0;
    mResetCode.duration1 = 0;
    mResetCode.level1 = 0;
}

FastLEDRmtEncoder::~FastLEDRmtEncoder() {
    if (mBytesEncoder) {
        rmt_del_encoder(mBytesEncoder);
        mBytesEncoder = nullptr;
    }
    if (mCopyEncoder) {
        rmt_del_encoder(mCopyEncoder);
        mCopyEncoder = nullptr;
    }
    // Don't delete mEncoder here - it's deleted via delCallback
}

esp_err_t FastLEDRmtEncoder::initialize(const ChipsetTiming& timing, uint32_t resolution_hz) {
    // Convert timing from nanoseconds to RMT ticks
    // RMT resolution is typically 40MHz (25ns per tick)
    const uint64_t ns_per_tick = 1000000000ULL / resolution_hz;

    // Bit 0: High for T1, Low for (T2 + T3)
    mBit0HighTicks = (timing.T1 + ns_per_tick / 2) / ns_per_tick;
    mBit0LowTicks = ((timing.T2 + timing.T3) + ns_per_tick / 2) / ns_per_tick;
    // Bit 1: High for (T1 + T2), Low for T3
    mBit1HighTicks = ((timing.T1 + timing.T2) + ns_per_tick / 2) / ns_per_tick;
    mBit1LowTicks = (timing.T3 + ns_per_tick / 2) / ns_per_tick;
    mResetTicks = (timing.RESET * 1000ULL + ns_per_tick / 2) / ns_per_tick;  // RESET is in microseconds

    FL_LOG_RMT("FastLEDRmtEncoder: Timing (ticks @ " << resolution_hz << " Hz):");
    FL_LOG_RMT("  Bit0: " << mBit0HighTicks << "H + " << mBit0LowTicks << "L = " << (mBit0HighTicks + mBit0LowTicks) << " ticks");
    FL_LOG_RMT("  Bit1: " << mBit1HighTicks << "H + " << mBit1LowTicks << "L = " << (mBit1HighTicks + mBit1LowTicks) << " ticks");
    FL_LOG_RMT("  Reset: " << mResetTicks << " ticks (" << timing.RESET << " us)");

    // Create bytes encoder for pixel data
    // WS2812/similar protocols send MSB first: each byte becomes 8 RMT pulses
    rmt_bytes_encoder_config_t bytes_config = {};
    bytes_config.bit0.duration0 = mBit0HighTicks;
    bytes_config.bit0.level0 = 1;
    bytes_config.bit0.duration1 = mBit0LowTicks;
    bytes_config.bit0.level1 = 0;
    bytes_config.bit1.duration0 = mBit1HighTicks;
    bytes_config.bit1.level0 = 1;
    bytes_config.bit1.duration1 = mBit1LowTicks;
    bytes_config.bit1.level1 = 0;
    bytes_config.flags.msb_first = 1;  // WS2812 sends MSB first

    esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &mBytesEncoder);
    if (ret != ESP_OK) {
        FL_WARN("FastLEDRmtEncoder: Failed to create bytes encoder: " << esp_err_to_name(ret));
        return ret;
    }

    // Create copy encoder for reset pulse
    // Reset pulse is a long low signal to latch colors into LEDs
    rmt_copy_encoder_config_t copy_config = {};
    ret = rmt_new_copy_encoder(&copy_config, &mCopyEncoder);
    if (ret != ESP_OK) {
        FL_WARN("FastLEDRmtEncoder: Failed to create copy encoder: " << esp_err_to_name(ret));
        rmt_del_encoder(mBytesEncoder);
        mBytesEncoder = nullptr;
        return ret;
    }

    // Configure reset code (low signal for RESET microseconds)
    mResetCode.duration0 = mResetTicks;
    mResetCode.level0 = 0;
    mResetCode.duration1 = 0;
    mResetCode.level1 = 0;

    FL_LOG_RMT("FastLEDRmtEncoder: Initialized successfully");
    return ESP_OK;
}

size_t IRAM_ATTR FastLEDRmtEncoder::encode(rmt_channel_handle_t channel,
                                           const void *primary_data, size_t data_size,
                                           rmt_encode_state_t *ret_state) {
    // CRITICAL: Use separate variables for session state and accumulated state
    // This matches ESP-IDF led_strip encoder pattern exactly
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    // State machine: 0 = encode pixel data, 1 = encode reset pulse
    switch (mState) {
    case 0: // Encode pixel data using bytes encoder
        encoded_symbols += mBytesEncoder->encode(mBytesEncoder, channel,
                                                 primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            mState = 1;  // Move to reset pulse state
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
            goto out;  // Yield - buffer full
        }
        // Fallthrough to reset pulse if data complete
        [[fallthrough]];

    case 1: // Encode reset pulse using copy encoder
        encoded_symbols += mCopyEncoder->encode(mCopyEncoder, channel,
                                                &mResetCode, sizeof(mResetCode), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            mState = 0;  // Reset state machine for next transmission
            state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_COMPLETE);
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
            goto out;  // Yield - buffer full
        }
        break;

    default:
        // Invalid state - reset to known state
        state = RMT_ENCODING_RESET;
        break;
    }

out:
    *ret_state = state;
    return encoded_symbols;
}

esp_err_t IRAM_ATTR FastLEDRmtEncoder::reset() {
    mState = 0;
    if (mBytesEncoder) {
        mBytesEncoder->reset(mBytesEncoder);
    }
    if (mCopyEncoder) {
        mCopyEncoder->reset(mCopyEncoder);
    }
    return ESP_OK;
}

esp_err_t FastLEDRmtEncoder::del() {
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

//=============================================================================
// Static Callbacks (C linkage for rmt_encoder_t interface)
//=============================================================================

size_t IRAM_ATTR FastLEDRmtEncoder::encodeCallback(rmt_encoder_t *encoder,
                                                    rmt_channel_handle_t channel,
                                                    const void *primary_data, size_t data_size,
                                                    rmt_encode_state_t *ret_state) {
    fastled_encoder_wrapper_t *wrapper = reinterpret_cast<fastled_encoder_wrapper_t*>(encoder);
    return wrapper->instance->encode(channel, primary_data, data_size, ret_state);
}

esp_err_t IRAM_ATTR FastLEDRmtEncoder::resetCallback(rmt_encoder_t *encoder) {
    fastled_encoder_wrapper_t *wrapper = reinterpret_cast<fastled_encoder_wrapper_t*>(encoder);
    return wrapper->instance->reset();
}

esp_err_t FastLEDRmtEncoder::delCallback(rmt_encoder_t *encoder) {
    fastled_encoder_wrapper_t *wrapper = reinterpret_cast<fastled_encoder_wrapper_t*>(encoder);
    FastLEDRmtEncoder *instance = wrapper->instance;
    instance->del();
    delete instance;  // Delete C++ instance
    fl::free(wrapper);  // Free C wrapper
    return ESP_OK;
}

//=============================================================================
// Factory Function
//=============================================================================

esp_err_t fastled_rmt_new_encoder(const ChipsetTiming& timing,
                                   uint32_t resolution_hz,
                                   rmt_encoder_handle_t *ret_encoder) {
    FL_ASSERT(ret_encoder != nullptr, "ret_encoder cannot be null");

    // Allocate wrapper that holds both rmt_encoder_t base and C++ instance pointer
    fastled_encoder_wrapper_t *wrapper =
        static_cast<fastled_encoder_wrapper_t*>(fl::malloc(sizeof(fastled_encoder_wrapper_t)));
    if (wrapper == nullptr) {
        FL_WARN("fastled_rmt_new_encoder: Failed to allocate wrapper");
        return ESP_ERR_NO_MEM;
    }

    // Create C++ instance
    FastLEDRmtEncoder *instance = new FastLEDRmtEncoder();
    if (instance == nullptr) {
        FL_WARN("fastled_rmt_new_encoder: Failed to create encoder instance");
        fl::free(wrapper);
        return ESP_ERR_NO_MEM;
    }

    // Initialize encoder with timing
    esp_err_t ret = instance->initialize(timing, resolution_hz);
    if (ret != ESP_OK) {
        delete instance;
        fl::free(wrapper);
        return ret;
    }

    // Wire up the wrapper
    wrapper->base.encode = FastLEDRmtEncoder::encodeCallback;
    wrapper->base.reset = FastLEDRmtEncoder::resetCallback;
    wrapper->base.del = FastLEDRmtEncoder::delCallback;
    wrapper->instance = instance;

    // Store wrapper handle in instance for transmission
    instance->mEncoder = &wrapper->base;

    *ret_encoder = &wrapper->base;
    return ESP_OK;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
