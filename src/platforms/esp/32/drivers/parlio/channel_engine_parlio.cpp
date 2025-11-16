/// @file channel_engine_parlio.cpp
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/S3
///
/// This implementation uses ESP32's Parallel IO (PARLIO) peripheral to drive
/// multiple LED strips simultaneously on parallel GPIO pins. It supports
/// ESP32-P4 and ESP32-S3 variants that have PARLIO hardware.

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
// Bit Transposition Algorithm Design
//-----------------------------------------------------------------------------
//
// PARLIO hardware transmits data in parallel across multiple GPIO pins.
// For WS2812 LEDs, we need to transpose data from per-strip format to
// bit-parallel format.
//
// INPUT FORMAT (per-strip):
//   Strip 0: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   Strip 1: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//   Strip N: [LED0_R, LED0_G, LED0_B, LED1_R, LED1_G, LED1_B, ...]
//
// OUTPUT FORMAT (bit-parallel):
//   For each LED byte position across all strips:
//     - Encode byte using encodeLedByte() → 32-bit waveform
//     - Transpose bits across strips at each clock tick
//     - Pack into width-specific format
//
// ALGORITHM STEPS:
//
// 1. Iterate through each LED position (0 to numLeds-1)
// 2. For each color channel (R, G, B):
//    a. For each strip (0 to numStrips-1):
//       - Get LED byte: channelData[strip]->getData()[ledIndex*3 + colorChannel]
//       - Encode: waveform[strip] = encodeLedByte(byte)
//    b. Transpose waveform bits:
//       - Each waveform is 32 bits (8 LED bits × 4 clock ticks)
//       - For each clock tick (0 to 31):
//         - Extract bit [tick] from each strip's waveform
//         - Pack into width-specific byte(s)
//         - Append to transposed_buffer
//
// MEMORY LAYOUT EXAMPLES:
//
// For 1-bit width (1 strip):
//   - Each clock tick = 1 bit → packed into bytes (8 ticks per byte)
//   - Each LED byte = 32 bits = 4 bytes
//   - Each LED (RGB) = 12 bytes
//
// For 8-bit width (8 strips):
//   - Each clock tick = 8 bits = 1 byte
//   - Each LED byte = 32 ticks = 32 bytes
//   - Each LED (RGB) = 96 bytes
//
// For 16-bit width (16 strips):
//   - Each clock tick = 16 bits = 2 bytes
//   - Each LED byte = 32 ticks = 64 bytes
//   - Each LED (RGB) = 192 bytes
//
// BUFFER SIZE CALCULATION:
//   For N LEDs, M strips, width W:
//   - Each LED = 3 bytes (RGB)
//   - Each byte = 32 clock ticks
//   - Each tick uses ceil(W / 8) bytes
//   - Total: N × 3 × 32 × ceil(W / 8) bytes
//
// EXAMPLE (3 LEDs, 2 strips, 8-bit width):
//   Input sizes: 9 bytes per strip (3 LEDs × 3 bytes)
//   Output size: 3 LEDs × 3 bytes × 32 ticks × 1 byte = 288 bytes
//
// CHUNKING STRATEGY:
//   PARLIO has 65535 byte max transfer size. For large LED counts:
//   - Break transmission at LED boundaries (not in middle of RGB)
//   - Calculate chunk size: min(numLeds, 65535 / (3 × 32 × ceil(W/8)))
//   - Transmit chunks sequentially
//
//-----------------------------------------------------------------------------

/// @brief Encode a single LED byte (0x00-0xFF) into 32-bit PARLIO waveform
/// @param byte LED color byte to encode (R, G, or B value)
/// @return 32-bit waveform for PARLIO transmission
///
/// WS2812 timing with PARLIO 4-tick encoding:
/// - Bit 0: 0b1000 (312.5ns high, 937.5ns low)
/// - Bit 1: 0b1110 (937.5ns high, 312.5ns low)
///
/// Each byte produces 8 × 4 = 32 bits of output.
/// MSB is transmitted first (standard WS2812 protocol).
///
/// Examples:
/// - encodeLedByte(0xFF) = 0xEEEEEEEE (all bits 1)
/// - encodeLedByte(0x00) = 0x88888888 (all bits 0)
/// - encodeLedByte(0xAA) = 0xEE88EE88EE88EE88 (10101010)
static uint32_t IRAM_ATTR encodeLedByte(uint8_t byte) {
    uint32_t result = 0;

    // Process each bit (MSB first)
    for (int i = 7; i >= 0; i--) {
        // Shift result to make room for 4 new bits
        result <<= 4;

        // Check bit value
        if (byte & (1 << i)) {
            // Bit is 1: encode as 0b1110 (hex: 0xE)
            result |= 0xE;
        } else {
            // Bit is 0: encode as 0b1000 (hex: 0x8)
            result |= 0x8;
        }
    }

    return result;
}

/// @brief Determine optimal data width based on channel count
/// @param numChannels Number of active channels/strips
/// @return Power-of-2 data width (1, 2, 4, 8, or 16)
static uint8_t determineDataWidth(size_t numChannels) {
    if (numChannels <= 1) return 1;
    if (numChannels <= 2) return 2;
    if (numChannels <= 4) return 4;
    if (numChannels <= 8) return 8;
    return 16;
}

/// @brief Transpose and pack LED data for PARLIO transmission
/// @param channelData Input channel data (per-strip RGB data)
/// @param numLeds Number of LEDs per strip
/// @param dataWidth PARLIO data width (1, 2, 4, 8, or 16)
/// @param outputBuffer Output buffer for transposed data
/// @return true if successful, false on error
///
/// This function performs the complex bit transposition algorithm:
/// 1. For each LED position and color channel (R, G, B):
///    - Encode each strip's byte using encodeLedByte() → 32-bit waveform
///    - Transpose waveform bits across strips at each clock tick
///    - Pack into width-specific format
///
/// Memory layout depends on data width:
/// - 1-bit: 8 ticks per byte (each tick = 1 bit)
/// - 2-bit: 4 ticks per byte (each tick = 2 bits)
/// - 4-bit: 2 ticks per byte (each tick = 4 bits)
/// - 8-bit: 1 tick per byte (each tick = 8 bits = 1 byte)
/// - 16-bit: 1 tick per 2 bytes (each tick = 16 bits = 2 bytes)
static bool transposeAndPack(fl::span<const ChannelDataPtr> channelData,
                              size_t numLeds,
                              uint8_t dataWidth,
                              fl::vector<uint8_t>& outputBuffer) {
    // Validate inputs
    if (channelData.size() == 0 || numLeds == 0) {
        return false;
    }

    // Calculate buffer size needed:
    // numLeds × 3 colors × 32 ticks × (dataWidth / 8) bytes per tick
    size_t bytesPerTick = (dataWidth + 7) / 8;  // Round up to nearest byte
    size_t totalBytes = numLeds * 3 * 32 * bytesPerTick;

    // Resize output buffer
    outputBuffer.clear();
    outputBuffer.reserve(totalBytes);

    // Temporary storage for encoded waveforms (one per strip)
    uint32_t waveforms[16] = {0};  // Max 16 strips

    // Process each LED
    for (size_t ledIdx = 0; ledIdx < numLeds; ledIdx++) {
        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            // Step 1: Encode each strip's byte into 32-bit waveform
            for (size_t stripIdx = 0; stripIdx < channelData.size(); stripIdx++) {
                const uint8_t* data = channelData[stripIdx]->getData();
                size_t byteOffset = ledIdx * 3 + colorIdx;
                uint8_t byte = data[byteOffset];
                waveforms[stripIdx] = encodeLedByte(byte);
            }

            // Step 2: Transpose bits across strips for each clock tick
            // Each waveform has 32 bits (8 LED bits × 4 clock ticks)
            for (size_t tick = 0; tick < 32; tick++) {
                // Extract bit at position 'tick' from each strip's waveform
                // Process MSB first (bit 31 down to bit 0)
                size_t bitPos = 31 - tick;

                // Pack bits based on data width
                if (dataWidth == 1) {
                    // 1-bit width: 8 ticks per byte
                    if (tick % 8 == 0) {
                        outputBuffer.push_back(0);  // Start new byte
                    }
                    // Extract bit from strip 0 and shift into byte
                    uint8_t bit = (waveforms[0] >> bitPos) & 0x01;
                    outputBuffer.back() |= (bit << (7 - (tick % 8)));

                } else if (dataWidth == 2) {
                    // 2-bit width: 4 ticks per byte
                    if (tick % 4 == 0) {
                        outputBuffer.push_back(0);  // Start new byte
                    }
                    // Extract bits from strips 0-1
                    for (size_t stripIdx = 0; stripIdx < 2; stripIdx++) {
                        uint8_t bit = (waveforms[stripIdx] >> bitPos) & 0x01;
                        outputBuffer.back() |= (bit << (7 - (tick % 4) * 2 - stripIdx));
                    }

                } else if (dataWidth == 4) {
                    // 4-bit width: 2 ticks per byte
                    if (tick % 2 == 0) {
                        outputBuffer.push_back(0);  // Start new byte
                    }
                    // Extract bits from strips 0-3
                    for (size_t stripIdx = 0; stripIdx < 4; stripIdx++) {
                        uint8_t bit = (waveforms[stripIdx] >> bitPos) & 0x01;
                        outputBuffer.back() |= (bit << (7 - (tick % 2) * 4 - stripIdx));
                    }

                } else if (dataWidth == 8) {
                    // 8-bit width: 1 tick per byte
                    uint8_t byte = 0;
                    // Extract bits from strips 0-7
                    for (size_t stripIdx = 0; stripIdx < 8 && stripIdx < channelData.size(); stripIdx++) {
                        uint8_t bit = (waveforms[stripIdx] >> bitPos) & 0x01;
                        byte |= (bit << (7 - stripIdx));
                    }
                    outputBuffer.push_back(byte);

                } else if (dataWidth == 16) {
                    // 16-bit width: 1 tick per 2 bytes
                    uint16_t word = 0;
                    // Extract bits from strips 0-15
                    for (size_t stripIdx = 0; stripIdx < 16 && stripIdx < channelData.size(); stripIdx++) {
                        uint8_t bit = (waveforms[stripIdx] >> bitPos) & 0x01;
                        word |= (bit << (15 - stripIdx));
                    }
                    // Write as big-endian (MSB first)
                    outputBuffer.push_back((word >> 8) & 0xFF);
                    outputBuffer.push_back(word & 0xFF);
                }
            }
        }
    }

    return true;
}

/// @brief Calculate maximum LEDs per chunk based on PARLIO transfer size limit
/// @param dataWidth PARLIO data width (1, 2, 4, 8, or 16)
/// @return Maximum number of LEDs that fit in one chunk
///
/// PARLIO has a maximum transfer size of 65535 bytes per transmission.
/// Each LED requires: 3 colors × 32 ticks × (dataWidth / 8) bytes per tick
///
/// Examples:
/// - 8-bit width: 3 × 32 × 1 = 96 bytes per LED → max 682 LEDs
/// - 16-bit width: 3 × 32 × 2 = 192 bytes per LED → max 341 LEDs
static size_t calculateMaxLedsPerChunk(uint8_t dataWidth) {
    size_t bytesPerTick = (dataWidth + 7) / 8;
    size_t bytesPerLed = 3 * 32 * bytesPerTick;
    size_t maxBytes = 65535;  // PARLIO max transfer size
    return maxBytes / bytesPerLed;
}

/// @brief Calculate optimal chunk size for streaming (balances memory vs. refill frequency)
/// @param dataWidth PARLIO data width (1, 2, 4, 8, or 16)
/// @return Optimal LEDs per chunk for ping-pong buffering
///
/// For streaming mode, we use smaller chunks to reduce memory usage while
/// ensuring the ISR has enough time to refill the next buffer.
///
/// Target: ~50-100 LEDs per chunk (4.8-9.6KB for 8-bit width)
/// This provides ~1.5-3ms transmission time per chunk at WS2812 speeds,
/// giving plenty of time for ISR to transpose the next chunk.
static size_t calculateStreamingChunkSize(uint8_t dataWidth) {
    // Start with a reasonable default
    size_t targetLedsPerChunk = 100;

    // Don't exceed hardware limits
    size_t maxLedsPerChunk = calculateMaxLedsPerChunk(dataWidth);
    return ftl::min(targetLedsPerChunk, maxLedsPerChunk);
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

    // Free DMA buffers
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
    mState.buffer_size = 0;
    mState.data_width = 0;
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
    // Calculate chunk parameters
    size_t ledsRemaining = mState.total_leds - mState.current_led;
    size_t ledsThisChunk = ftl::min(ledsRemaining, mState.leds_per_chunk);

    if (ledsThisChunk == 0) {
        return true;  // Nothing to do
    }

    // Swap buffers: what was being filled is now active
    uint8_t* nextBuffer = const_cast<uint8_t*>(mState.fill_buffer);

    // Create offset channel spans for this chunk
    // Note: We need to offset into each channel's data by current_led position
    size_t byteOffset = mState.current_led * 3;  // 3 bytes per LED (RGB)

    // Create temporary offset channel data
    // WARNING: This creates temporary ChannelDataPtr objects in ISR
    // For now, we'll use a simpler approach - store offset pointers

    // Transpose this chunk into the fill buffer
    // We'll use a simplified inline version for ISR context
    size_t bytesPerTick = (mState.data_width + 7) / 8;
    size_t outputIdx = 0;

    uint32_t waveforms[16] = {0};  // Max 16 strips

    // Process each LED in this chunk
    for (size_t ledIdx = 0; ledIdx < ledsThisChunk; ledIdx++) {
        size_t absoluteLedIdx = mState.current_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            // Encode each strip's byte into waveform
            for (size_t stripIdx = 0; stripIdx < mState.source_channels.size(); stripIdx++) {
                const uint8_t* data = mState.source_channels[stripIdx]->getData();
                size_t byteOffset = absoluteLedIdx * 3 + colorIdx;
                uint8_t byte = data[byteOffset];
                waveforms[stripIdx] = encodeLedByte(byte);
            }

            // Transpose bits for 8-bit width (most common case)
            if (mState.data_width == 8) {
                for (size_t tick = 0; tick < 32; tick++) {
                    size_t bitPos = 31 - tick;
                    uint8_t outputByte = 0;

                    for (size_t stripIdx = 0; stripIdx < 8 && stripIdx < mState.source_channels.size(); stripIdx++) {
                        uint8_t bit = (waveforms[stripIdx] >> bitPos) & 0x01;
                        outputByte |= (bit << (7 - stripIdx));
                    }

                    nextBuffer[outputIdx++] = outputByte;
                }
            } else {
                // Other widths not yet optimized for ISR - would need similar treatment
                // For now, mark error
                mState.error_occurred = true;
                return false;
            }
        }
    }

    // Queue transmission of this buffer
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

    size_t chunkBytes = outputIdx;
    size_t chunkBits = chunkBytes * 8;  // PARLIO API requires payload size in bits

    esp_err_t err = parlio_tx_unit_transmit(
        mState.tx_unit,
        nextBuffer,
        chunkBits,  // Payload size in bits, not bytes
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
    // Ensure PARLIO peripheral is initialized
    initializeIfNeeded();

    // If initialization failed, abort
    if (!mInitialized || mState.tx_unit == nullptr) {
        FL_WARN("PARLIO: Cannot transmit - initialization failed");
        return;
    }

    // Validate channel data
    if (channelData.size() == 0) {
        FL_DBG("PARLIO: No channels to transmit");
        return;
    }

    // Check if we're already transmitting
    if (mState.transmitting) {
        FL_WARN("PARLIO: Transmission already in progress");
        return;
    }

    // Validate channel count matches configured data width
    if (channelData.size() > mState.data_width) {
        FL_WARN("PARLIO: Channel count (" << channelData.size()
                << ") exceeds data_width (" << static_cast<int>(mState.data_width) << ")");
        return;
    }

    // Validate LED data consistency across all channels
    size_t firstChannelSize = 0;
    bool hasData = false;

    for (size_t i = 0; i < channelData.size(); i++) {
        if (!channelData[i]) {
            FL_WARN("PARLIO: Null channel data at index " << i);
            return;
        }

        size_t dataSize = channelData[i]->getSize();

        if (dataSize > 0) {
            if (!hasData) {
                firstChannelSize = dataSize;
                hasData = true;
            } else if (dataSize != firstChannelSize) {
                FL_WARN("PARLIO: Channel " << i << " has " << dataSize
                        << " bytes, expected " << firstChannelSize);
                return;
            }
        }

        if (dataSize > 0 && dataSize % 3 != 0) {
            FL_WARN("PARLIO: Channel " << i << " has " << dataSize
                    << " bytes (not a multiple of 3)");
            return;
        }
    }

    if (!hasData || firstChannelSize == 0) {
        FL_DBG("PARLIO: No LED data to transmit");
        return;
    }

    size_t numLeds = firstChannelSize / 3;
    FL_DBG("PARLIO: beginTransmission - " << channelData.size()
            << " channels, " << numLeds << " LEDs (streaming mode)");

    // Initialize streaming state
    mState.source_channels = channelData;
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

    FL_DBG("PARLIO: Starting initialization (streaming mode)");

    // Step 1: Determine data width
    size_t numChannels = 8;
    mState.data_width = determineDataWidth(numChannels);

    FL_DBG("PARLIO: Using data_width=" << static_cast<int>(mState.data_width));

    // Step 2: Calculate streaming chunk size
    mState.leds_per_chunk = calculateStreamingChunkSize(mState.data_width);
    FL_DBG("PARLIO: Chunk size=" << mState.leds_per_chunk << " LEDs");

    // Step 3: Configure GPIO pins
    mState.pins.clear();
    for (size_t i = 0; i < mState.data_width; i++) {
        mState.pins.push_back(static_cast<gpio_num_t>(DEFAULT_PARLIO_PINS[i]));
    }

    // Step 4: Configure PARLIO TX unit
    parlio_tx_unit_config_t config = {};
    config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    config.output_clk_freq_hz = PARLIO_CLOCK_FREQ_HZ;
    config.data_width = mState.data_width;
    config.trans_queue_depth = 2;  // Support ping-pong buffering
    config.max_transfer_size = 65535;

    // Assign GPIO pins
    for (size_t i = 0; i < mState.data_width; i++) {
        config.data_gpio_nums[i] = mState.pins[i];
    }
    for (size_t i = mState.data_width; i < 16; i++) {
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
    callbacks.on_trans_done = txDoneCallback;

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
    size_t bytesPerTick = (mState.data_width + 7) / 8;
    size_t bufferSize = mState.leds_per_chunk * 3 * 32 * bytesPerTick;

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
