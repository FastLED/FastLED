/// @file channel_engine_parlio.cpp
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/C6/H2/C5
///
/// This implementation uses ESP32's Parallel IO (PARLIO) peripheral to drive
/// multiple LED strips simultaneously on parallel GPIO pins. It supports
/// ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 variants that have PARLIO hardware.
/// Note: ESP32-S3 does NOT have PARLIO (it has LCD peripheral instead).

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "channel_engine_parlio.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "ftl/algorithm.h"
#include "ftl/time.h"
#include "ftl/assert.h"
#include "fl/delay.h"

FL_EXTERN_C_BEGIN
#include "driver/parlio_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"  // For DMA-capable memory allocation
FL_EXTERN_C_END

namespace fl {

//=============================================================================
// Constants
//=============================================================================

// WS2812 timing requirements with PARLIO
// Clock: 3.2 MHz (312.5ns per tick)
// Each LED bit is encoded as 4 clock ticks
static constexpr uint32_t PARLIO_CLOCK_FREQ_HZ = 3200000;  // 3.2 MHz

// Default GPIO pins for PARLIO output (can be customized later)
// These are just placeholders - real pins should be configured per platform
static constexpr int DEFAULT_PARLIO_PINS[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

//=============================================================================
// Helper Functions
//=============================================================================

//-----------------------------------------------------------------------------
// ISR Transposition Algorithm Design
//-----------------------------------------------------------------------------
//
// PARLIO hardware transmits data in parallel across multiple GPIO pins.
// For WS2812 LEDs, we use the generic waveform generator (fl::channels::waveform_generator)
// with custom bit-packing for PARLIO's parallel format.
//
// INPUT FORMAT (per-strip layout):
//   Strip 0: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   Strip 1: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   ...
//   Strip 7: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//
// ISR PROCESSING (per LED byte):
//   For each color byte position:
//     1. Read byte from all 8 lanes (per-strip layout with stride)
//     2. Expand each byte using fl::expandByteToWaveforms() with precomputed bit0/bit1 patterns
//        - Each byte → 32 pulse bytes (0xFF=HIGH, 0x00=LOW)
//     3. Bit-pack transpose: convert pulse bytes to PARLIO's bit-packed format
//        - Each pulse byte → 1 bit, packed across lanes
//
// OUTPUT FORMAT (bit-parallel, 8 lanes):
//   Byte 0:  [S7_p0, S6_p0, ..., S1_p0, S0_p0]  // Pulse 0 from all strips
//   Byte 1:  [S7_p1, S6_p1, ..., S1_p1, S0_p1]  // Pulse 1 from all strips
//   ...
//   Byte 31: [S7_p31, S6_p31, ..., S1_p31, S0_p31]  // Pulse 31 from all strips
//   (Pattern repeats for each color of each LED)
//
// MEMORY LAYOUT:
//   For 8 lanes:
//     - Each LED byte → 32 ticks × 1 byte = 32 bytes (after waveform encoding)
//     - Each LED (RGB) → 96 bytes (after waveform encoding)
//
// BUFFER SIZE CALCULATION:
//   Scratch buffer: N LEDs × 3 colors × 8 strips bytes (per-strip layout)
//   DMA chunks: 100 LEDs × 3 × 32 ticks × 1 byte = 9600 bytes (per buffer)
//
// EXAMPLE (1000 LEDs, 8 strips):
//   Scratch buffer: 1000 × 3 × 8 = 24,000 bytes (per-strip layout)
//   DMA buffers: 2 × (100 × 3 × 32 × 1) = 19,200 bytes (ping-pong expanded waveforms)
//   Total: ~43 KB (waveform expansion happens in ISR, not stored)
//
// CHUNKING STRATEGY:
//   PARLIO has 65535 byte max transfer size. For large LED counts:
//   - Break transmission at LED boundaries (not in middle of RGB)
//   - Fixed chunk size: 100 LEDs (9600 bytes per chunk)
//   - Max ~682 LEDs per chunk (65535 / 96 bytes per LED)
//   - Transmit chunks sequentially via ping-pong DMA buffering
//
// WAVEFORM GENERATION:
//   Uses generic waveform generator with WS2812 timing parameters:
//   - T1: 312ns (HIGH time for bit 0)
//   - T2: 625ns (additional HIGH time for bit 1)
//   - T3: 312ns (LOW tail duration)
//   - Clock: 3.2 MHz (312.5ns per pulse)
//   - Result: 4 pulses per bit (bit0=[0xFF,0x00,0x00,0x00], bit1=[0xFF,0xFF,0xFF,0x00])
//
//-----------------------------------------------------------------------------

/// @brief Calculate optimal chunk size for 8-lane streaming
/// @return LEDs per chunk for ping-pong buffering
///
/// For 8-lane mode: 3 colors × 32 ticks × 1 byte = 96 bytes per LED
/// PARLIO max transfer size: 65535 bytes → max 682 LEDs
/// Target: 100 LEDs per chunk (9.6KB) for balanced memory/performance
static size_t calculateStreamingChunkSize() {
    return 100;  // Optimal for 8-lane configuration
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEnginePARLIO::ChannelEnginePARLIO()
    : mInitialized(false)
{
    // Register as listener for end frame events
    EngineEvents::addListener(this);

    FL_DBG("PARLIO Channel Engine initialized");
}

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    // Unregister from events
    EngineEvents::removeListener(this);

    // Wait for any active transmissions to complete
    while (pollDerived() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Clean up PARLIO TX unit resources
    if (mState.tx_unit != nullptr) {
        FL_DBG("PARLIO: Cleaning up TX unit");

        // Wait for any pending transmissions (with timeout)
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.tx_unit, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Wait for transmission timeout during cleanup: " << err);
        }

        // Disable TX unit
        err = parlio_tx_unit_disable(mState.tx_unit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to disable TX unit: " << err);
        }

        // Delete TX unit
        err = parlio_del_tx_unit(mState.tx_unit);
        if (err != ESP_OK) {
            FL_WARN("PARLIO: Failed to delete TX unit: " << err);
        }

        mState.tx_unit = nullptr;
        FL_DBG("PARLIO: TX unit cleanup complete");
    }

    // Free DMA buffers (waveform output buffers)
    if (mState.buffer_a != nullptr) {
        heap_caps_free(mState.buffer_a);
        mState.buffer_a = nullptr;
    }
    if (mState.buffer_b != nullptr) {
        heap_caps_free(mState.buffer_b);
        mState.buffer_b = nullptr;
    }

    // Clear state
    mState.pins.clear();
    mState.scratch_padded_buffer.clear();
    mState.buffer_size = 0;
    mState.num_lanes = 0;
    mState.lane_stride = 0;
    mState.transmitting = false;
    mState.stream_complete = false;
    mState.error_occurred = false;

    FL_DBG("PARLIO: Channel Engine destroyed");
}

//=============================================================================
// EngineEvents::Listener Interface
//=============================================================================

void ChannelEnginePARLIO::onEndFrame() {
    // Frame has ended - this is a good time to check state
    // Currently a no-op, may be used for cleanup or diagnostics later
}


//=============================================================================
// Private Methods - ISR Streaming Support
//=============================================================================

bool IRAM_ATTR ChannelEnginePARLIO::txDoneCallback(parlio_tx_unit_handle_t tx_unit,
                                                     const void* edata,
                                                     void* user_ctx) {
    // ISR context - must be fast and IRAM-safe
    // Note: edata is actually const parlio_tx_done_event_data_t* but we don't use it
    (void)edata;  // Unused parameter
    (void)tx_unit;  // Unused parameter
    ChannelEnginePARLIO* self = static_cast<ChannelEnginePARLIO*>(user_ctx);
    if (!self) {
        return false;
    }

    // Check if there are more chunks to transmit
    if (self->mState.current_led >= self->mState.total_leds) {
        // Transmission complete
        self->mState.stream_complete = true;
        self->mState.transmitting = false;
        return false;
    }

    // Transpose and queue next chunk
    if (!self->transposeAndQueueNextChunk()) {
        self->mState.error_occurred = true;
        self->mState.transmitting = false;
        return false;
    }

    return false;  // No high-priority task woken
}

bool IRAM_ATTR ChannelEnginePARLIO::transposeAndQueueNextChunk() {
    // ISR-OPTIMIZED: Generic waveform expansion + bit-pack transposition
    // Uses fl::expandByteToWaveforms() with precomputed bit0/bit1 patterns
    // Transposes byte-based waveforms to PARLIO's bit-packed format

    // Calculate chunk parameters
    size_t ledsRemaining = mState.total_leds - mState.current_led;
    size_t ledsThisChunk = (ledsRemaining < mState.leds_per_chunk) ? ledsRemaining : mState.leds_per_chunk;

    if (ledsThisChunk == 0) {
        return true;  // Nothing to do
    }

    // Get the next buffer to fill
    uint8_t* nextBuffer = const_cast<uint8_t*>(mState.fill_buffer);

    // Temporary waveform buffers for each lane (stack-allocated for ISR safety)
    // Each byte expands to 8 bits × pulses_per_bit (typically 4) = 32 bytes
    uint8_t laneWaveforms[8][32];

    // Precomputed waveform spans
    fl::span<const uint8_t> bit0_wave(mState.bit0_waveform.data(), mState.pulses_per_bit);
    fl::span<const uint8_t> bit1_wave(mState.bit1_waveform.data(), mState.pulses_per_bit);

    size_t outputIdx = 0;

    // Process each LED in this chunk
    for (size_t ledIdx = 0; ledIdx < ledsThisChunk; ledIdx++) {
        size_t absoluteLedIdx = mState.current_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = absoluteLedIdx * 3 + colorIdx;

            // Step 1: Expand each lane's byte to waveform using generic function
            for (size_t lane = 0; lane < 8; lane++) {
                const uint8_t* laneData = mState.scratch_padded_buffer.data() + (lane * mState.lane_stride);
                uint8_t byte = laneData[byteOffset];

                // Use generic waveform expansion
                fl::span<uint8_t> waveformOutput(laneWaveforms[lane], 32);
                fl::expandByteToWaveforms(byte, bit0_wave, bit1_wave, waveformOutput);
            }

            // Step 2: Bit-pack transpose waveform bytes to PARLIO format
            // Convert byte-based waveforms (0xFF/0x00) to bit-packed output
            const size_t pulsesPerByte = 8 * mState.pulses_per_bit;  // Should be 32
            for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                uint8_t outputByte = 0;

                // Extract one pulse from each lane and pack into bits
                for (size_t lane = 0; lane < 8; lane++) {
                    uint8_t pulse = laneWaveforms[lane][tick];
                    uint8_t bit = (pulse != 0) ? 1 : 0;  // Convert 0xFF→1, 0x00→0
                    outputByte |= (bit << (7 - lane));
                }

                nextBuffer[outputIdx++] = outputByte;
            }
        }
    }

    // Queue transmission of this buffer
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

    size_t chunkBits = outputIdx * 8;  // PARLIO API requires payload size in bits

    esp_err_t err = parlio_tx_unit_transmit(
        mState.tx_unit,
        nextBuffer,
        chunkBits,
        &tx_config
    );

    if (err != ESP_OK) {
        mState.error_occurred = true;
        return false;
    }

    // Update state
    mState.current_led += ledsThisChunk;

    // Swap buffer pointers for next iteration
    uint8_t* temp = const_cast<uint8_t*>(mState.active_buffer);
    mState.active_buffer = mState.fill_buffer;
    mState.fill_buffer = temp;

    return true;
}

//=============================================================================
// Protected Interface - ChannelEngine Implementation
//=============================================================================

ChannelEngine::EngineState ChannelEnginePARLIO::pollDerived() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized || mState.tx_unit == nullptr) {
        return EngineState::READY;
    }

    // Check for ISR-reported errors
    if (mState.error_occurred) {
        FL_WARN("PARLIO: Error occurred during streaming transmission");
        mState.transmitting = false;
        mState.error_occurred = false;
        return EngineState::ERROR;
    }

    // Check if streaming is complete
    if (mState.stream_complete) {
        // Wait for final chunk to complete
        esp_err_t err = parlio_tx_unit_wait_all_done(mState.tx_unit, 0);

        if (err == ESP_OK) {
            mState.transmitting = false;
            mState.stream_complete = false;
            FL_DBG("PARLIO: Streaming transmission complete");
            return EngineState::READY;
        } else if (err == ESP_ERR_TIMEOUT) {
            // Final chunk still transmitting
            return EngineState::BUSY;
        } else {
            FL_WARN("PARLIO: Error waiting for final chunk: " << err);
            mState.transmitting = false;
            mState.stream_complete = false;
            return EngineState::ERROR;
        }
    }

    // If we're not transmitting, we're ready
    if (!mState.transmitting) {
        return EngineState::READY;
    }

    // Streaming in progress
    return EngineState::BUSY;
}

void ChannelEnginePARLIO::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    // Validate channel data first (before initialization)
    if (channelData.size() == 0) {
        FL_DBG("PARLIO: No channels to transmit");
        return;
    }

    // Ensure PARLIO peripheral is initialized
    initializeIfNeeded();

    // If initialization failed, abort
    if (!mInitialized || mState.tx_unit == nullptr) {
        FL_WARN("PARLIO: Cannot transmit - initialization failed");
        return;
    }

    // Check if we're already transmitting
    if (mState.transmitting) {
        FL_WARN("PARLIO: Transmission already in progress");
        return;
    }

    // Validate channel count is exactly 8
    if (channelData.size() != 8) {
        FL_WARN("PARLIO: Only 8 channels supported (got " << channelData.size() << " channels)");
        return;
    }

    // Validate LED data and find maximum channel size
    size_t maxChannelSize = 0;
    bool hasData = false;

    for (size_t i = 0; i < channelData.size(); i++) {
        if (!channelData[i]) {
            FL_WARN("PARLIO: Null channel data at index " << i);
            return;
        }

        size_t dataSize = channelData[i]->getSize();

        if (dataSize > 0) {
            hasData = true;
            if (dataSize > maxChannelSize) {
                maxChannelSize = dataSize;
            }
        }

        if (dataSize > 0 && dataSize % 3 != 0) {
            FL_WARN("PARLIO: Channel " << i << " has " << dataSize
                    << " bytes (not a multiple of 3)");
            return;
        }
    }

    if (!hasData || maxChannelSize == 0) {
        FL_DBG("PARLIO: No LED data to transmit");
        return;
    }

    size_t numLeds = maxChannelSize / 3;
    FL_DBG("PARLIO: beginTransmission - " << channelData.size()
            << " channels, " << numLeds << " LEDs (streaming mode)");

    // Prepare single segmented scratch buffer for all N lanes
    // Layout: [lane0_data][lane1_data]...[laneN_data]
    // Each lane is maxChannelSize bytes (padded to same size)
    // This is regular heap (non-DMA) - only output waveform buffers need DMA capability

    size_t totalBufferSize = channelData.size() * maxChannelSize;
    mState.scratch_padded_buffer.resize(totalBufferSize);
    mState.lane_stride = maxChannelSize;
    mState.num_lanes = channelData.size();

    FL_DBG("PARLIO: Prepared scratch buffer: " << totalBufferSize
            << " bytes (" << channelData.size() << " lanes × " << maxChannelSize << " bytes)");

    // Write all channels to segmented scratch buffer with stride-based layout
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        uint8_t* laneDst = mState.scratch_padded_buffer.data() + (i * maxChannelSize);
        fl::span<uint8_t> dst(laneDst, maxChannelSize);

        if (dataSize < maxChannelSize) {
            // Lane needs padding
            FL_DBG("PARLIO: Padding lane " << i << " from " << dataSize
                    << " to " << maxChannelSize << " bytes");
            channelData[i]->writeWithPadding(dst);
        } else {
            // No padding needed - direct copy
            const auto& srcData = channelData[i]->getData();
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }

    // Initialize streaming state
    mState.total_leds = numLeds;
    mState.current_led = 0;
    mState.stream_complete = false;
    mState.error_occurred = false;
    mState.transmitting = true;

    // Transpose and queue first chunk (into buffer_a)
    mState.fill_buffer = mState.buffer_a;
    mState.active_buffer = nullptr;  // No active transmission yet

    if (!transposeAndQueueNextChunk()) {
        FL_WARN("PARLIO: Failed to queue initial chunk");
        mState.transmitting = false;
        return;
    }

    FL_DBG("PARLIO: Initial chunk queued, streaming started");

    // If there are more chunks, queue second chunk (into buffer_b)
    if (mState.current_led < mState.total_leds) {
        if (!transposeAndQueueNextChunk()) {
            FL_WARN("PARLIO: Failed to queue second chunk");
            mState.transmitting = false;
            return;
        }
        FL_DBG("PARLIO: Second chunk queued");
    }

    // ISR will handle remaining chunks automatically
}

//=============================================================================
// Private Methods - Initialization
//=============================================================================

void ChannelEnginePARLIO::initializeIfNeeded() {
    if (mInitialized) {
        return;
    }

    FL_DBG("PARLIO: Starting initialization (8-lane mode)");

    // Step 1: Calculate streaming chunk size (hardcoded for 8 lanes)
    mState.leds_per_chunk = calculateStreamingChunkSize();
    FL_DBG("PARLIO: Chunk size=" << mState.leds_per_chunk << " LEDs");

    // Step 2: Generate precomputed waveforms using generic waveform generator
    // WS2812 timing parameters (nanoseconds):
    // - T1: 312ns (HIGH time for bit 0)
    // - T2: 625ns (additional HIGH time for bit 1)
    // - T3: 312ns (LOW tail duration)
    fl::span<uint8_t> bit0_span(mState.bit0_waveform.data(), mState.bit0_waveform.size());
    fl::span<uint8_t> bit1_span(mState.bit1_waveform.data(), mState.bit1_waveform.size());

    size_t bit0_size = fl::generateBit0Waveform(PARLIO_CLOCK_FREQ_HZ, 312, 625, 312, bit0_span);
    size_t bit1_size = fl::generateBit1Waveform(PARLIO_CLOCK_FREQ_HZ, 312, 625, 312, bit1_span);

    if (bit0_size == 0 || bit1_size == 0 || bit0_size != bit1_size) {
        FL_WARN("PARLIO: Failed to generate waveforms (bit0=" << bit0_size << ", bit1=" << bit1_size << ")");
        mInitialized = false;
        return;
    }

    mState.pulses_per_bit = bit0_size;
    FL_DBG("PARLIO: Generated waveforms: " << mState.pulses_per_bit << " pulses per bit");

    // Step 3: Configure GPIO pins (always 8 pins)
    mState.pins.clear();
    for (size_t i = 0; i < 8; i++) {
        mState.pins.push_back(DEFAULT_PARLIO_PINS[i]);
    }

    // Step 4: Configure PARLIO TX unit
    parlio_tx_unit_config_t config = {};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.output_clk_freq_hz = PARLIO_CLOCK_FREQ_HZ;
    config.data_width = 8;  // Always 8 lanes
    config.trans_queue_depth = 2;  // Support ping-pong buffering
    config.max_transfer_size = 65535;

    // Assign GPIO pins (always 8 pins)
    for (size_t i = 0; i < 8; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(mState.pins[i]);
    }
    for (size_t i = 8; i < 16; i++) {
        config.data_gpio_nums[i] = static_cast<gpio_num_t>(-1);
    }

    config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Step 5: Create TX unit
    esp_err_t err = parlio_new_tx_unit(&config, &mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to create TX unit: " << err);
        mInitialized = false;
        return;
    }

    FL_DBG("PARLIO: TX unit created");

    // Step 6: Register ISR callback for streaming
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = reinterpret_cast<parlio_tx_done_callback_t>(txDoneCallback);

    err = parlio_tx_unit_register_event_callbacks(mState.tx_unit, &callbacks, this);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to register callbacks: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    FL_DBG("PARLIO: ISR callbacks registered");

    // Step 7: Enable TX unit
    err = parlio_tx_unit_enable(mState.tx_unit);
    if (err != ESP_OK) {
        FL_WARN("PARLIO: Failed to enable TX unit: " << err);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    FL_DBG("PARLIO: TX unit enabled");

    // Step 8: Allocate double buffers for ping-pong streaming
    // Use DMA-capable memory (required for PARLIO/GDMA)
    // For 8 lanes: 1 byte per tick
    size_t bufferSize = mState.leds_per_chunk * 3 * 32 * 1;  // 3 colors × 32 ticks × 1 byte

    mState.buffer_a = static_cast<uint8_t*>(
        heap_caps_malloc(bufferSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    );
    mState.buffer_b = static_cast<uint8_t*>(
        heap_caps_malloc(bufferSize, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    );

    if (mState.buffer_a == nullptr || mState.buffer_b == nullptr) {
        FL_WARN("PARLIO: Failed to allocate DMA-capable buffers (" << bufferSize << " bytes each)");

        // Clean up partial allocation
        if (mState.buffer_a != nullptr) {
            heap_caps_free(mState.buffer_a);
            mState.buffer_a = nullptr;
        }
        if (mState.buffer_b != nullptr) {
            heap_caps_free(mState.buffer_b);
            mState.buffer_b = nullptr;
        }

        // Clean up TX unit
        parlio_tx_unit_disable(mState.tx_unit);
        parlio_del_tx_unit(mState.tx_unit);
        mState.tx_unit = nullptr;
        mInitialized = false;
        return;
    }

    mState.buffer_size = bufferSize;

    FL_DBG("PARLIO: DMA-capable double buffers allocated (2 × " << bufferSize << " = "
            << (bufferSize * 2) << " bytes total)");

    // Initialize streaming state
    mState.transmitting = false;
    mState.stream_complete = false;
    mState.error_occurred = false;
    mState.current_led = 0;
    mState.total_leds = 0;

    mInitialized = true;
    FL_DBG("PARLIO: Initialization complete (streaming mode active)");
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
