/// @file channel_engine_parlio.cpp
/// @brief Parallel IO implementation of ChannelEngine for ESP32-P4/C6/H2/C5
///
/// This implementation uses ESP32's Parallel IO (PARLIO) peripheral to drive
/// multiple LED strips simultaneously on parallel GPIO pins. It supports
/// ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 variants that have PARLIO
/// hardware. Note: ESP32-S3 does NOT have PARLIO (it has LCD peripheral
/// instead).
///
/// This is a runtime-configurable implementation supporting 1-16 channels with
/// power-of-2 data widths (1, 2, 4, 8, 16) determined at construction time.
///
/// This file now contains only channel management logic. All hardware-specific
/// operations have been moved to parlio_engine.{h,cpp}.

#ifdef ESP32

#include "fl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "channel_engine_parlio.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/delay.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/stl/algorithm.h"
#include "fl/trace.h"

    // The test may have 3000 LEDs, but we use streaming buffers for large strips
#ifndef FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL
#define FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL 300
#endif // defined(FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL)

namespace fl {

//=============================================================================
// Constructors / Destructors - Implementation Class
//=============================================================================

ChannelEnginePARLIOImpl::ChannelEnginePARLIOImpl(size_t data_width)
    : mEngine(detail::ParlioEngine::getInstance()),
      mInitialized(false),
      mDataWidth(data_width),
      mCurrentGroupIndex(0) {

    // Validate data width
    if (data_width != 1 && data_width != 2 && data_width != 4 &&
        data_width != 8 && data_width != 16) {
        FL_WARN("PARLIO: Invalid data_width="
                << data_width << " (must be 1, 2, 4, 8, or 16)");
        // Constructor will still complete, but initialization will fail
        return;
    }
}

ChannelEnginePARLIOImpl::~ChannelEnginePARLIOImpl() {
    // Wait for any active transmissions to complete
    while (poll() == EngineState::BUSY) {
        fl::delayMicroseconds(100);
    }

    // Clear state
    mScratchBuffer.clear();
    mEnqueuedChannels.clear();
    mTransmittingChannels.clear();
    mChipsetGroups.clear();
    mCurrentGroupIndex = 0;
}

// NOTE: Old ISR callback functions (txDoneCallback, transposeAndQueueNextChunk, generateRingBuffer)
// have been removed. These belonged to the old architecture where the channel engine implemented
// its own ISR handling. The new architecture delegates all hardware operations to ParlioEngine HAL.

//=============================================================================
// Private Methods - ISR Streaming Support
//=============================================================================

// Phase 0: ISR submits next buffer when current buffer completes
// Per LOOP.md line 36: "parlio_tx_unit_transmit() must be called from the main thread (CPU)
// only once, after this it must be called from the on done callback ISR until the last buffer"
//
// ISR responsibilities:
//   1. Increment buffers_completed counter (signals CPU one buffer finished)
//   2. Check if more buffers need to be submitted
//   3. If yes: Submit next buffer from ring to hardware via parlio_tx_unit_transmit()
//   4. Update ring_read_ptr to indicate buffer consumed
//   5. Check for completion (all buffers transmitted)
//
// CPU responsibilities (in poll()):
//   - Pre-compute next DMA buffer when ring has space
//   - Update ring_write_ptr after pre-computation
//   - Detect stream_complete flag for transmission end
__attribute__((optimize("O3")))
bool FL_IRAM ChannelEnginePARLIOImpl::txDoneCallback(
    parlio_tx_unit_handle_t tx_unit, const void *edata, void *user_ctx) {
    (void)edata;

    auto *self = static_cast<ChannelEnginePARLIOImpl *>(user_ctx);
    if (!self || !self->mState.mIsrContext) {
        return false;
    }

    // Phase 0: Access ISR state via cache-aligned ParlioIsrContext struct
    ParlioIsrContext* ctx = self->mState.mIsrContext;

    // Step 1: Increment completion counter (signals CPU that one buffer finished)
    ctx->buffers_completed = ctx->buffers_completed + 1;
    ctx->isr_count = ctx->isr_count + 1;

    // Step 2: Check if we need to submit another buffer
    // We submit if: buffers_completed < buffers_total AND ring has data (read_ptr != write_ptr)
    if (ctx->buffers_completed < ctx->buffers_total) {
        // Check if next buffer is ready in ring
        // Ring is not empty when read_ptr != write_ptr
        volatile size_t read_ptr = ctx->ring_read_ptr;
        volatile size_t write_ptr = ctx->ring_write_ptr;

        if (read_ptr != write_ptr) {
            // Next buffer is ready - submit it to hardware
            size_t buffer_idx = read_ptr % ctx->ring_size;

            // Get buffer pointer and size from ring
            uint8_t* buffer_ptr = self->mState.ring_buffers[buffer_idx].get();
            size_t buffer_size = self->mState.ring_buffer_sizes[buffer_idx];

            if (buffer_ptr && buffer_size > 0) {
                // ITERATION 1: Increment attempt counter for debugging
                ctx->isr_submit_attempted = ctx->isr_submit_attempted + 1;

                // Submit buffer to hardware
                // Phase 1 - Iteration 2: Use consistent idle_value = 0x0000 (LOW) for all buffers
                // WS2812B protocol requires LOW state between data frames
                // Alternating idle_value creates spurious transitions between chunks
                parlio_transmit_config_t tx_config = {};
                tx_config.idle_value = 0x0000; // Keep pins LOW between chunks

                esp_err_t err = parlio_tx_unit_transmit(tx_unit, buffer_ptr,
                                                         buffer_size * 8, &tx_config);

                if (err == ESP_OK) {
                    // ITERATION 1: Increment success counter
                    ctx->isr_submit_success = ctx->isr_submit_success + 1;
                    // Successfully submitted - increment read pointer
                    ctx->ring_read_ptr = ctx->ring_read_ptr + 1;
                } else {
                    // ITERATION 1: Increment failure counter
                    ctx->isr_submit_failed = ctx->isr_submit_failed + 1;
                    // Submission failed - set error flag for CPU to detect
                    ctx->ring_error = true;
                }
            } else {
                // Invalid buffer - set error flag
                ctx->ring_error = true;
            }
        } else {
            // ITERATION 1: Increment ring empty counter
            ctx->isr_ring_empty = ctx->isr_ring_empty + 1;
            // Ring underflow - CPU didn't generate buffer fast enough
            // This is an error condition (ring should always be ahead)
            ctx->ring_error = true;
        }
    } else {
        // ITERATION 1: Increment all-done counter
        ctx->isr_all_buffers_done = ctx->isr_all_buffers_done + 1;
        // All buffers have been submitted - mark transmission complete
        ctx->stream_complete = true;
        ctx->transmitting = false;
    }

    return false; // No high-priority task woken
}

bool FL_IRAM ChannelEnginePARLIOImpl::transposeAndQueueNextChunk() {
    // ISR-OPTIMIZED: Generic waveform expansion + bit-pack transposition
    // Uses fl::expandByteToWaveforms() with precomputed bit0/bit1 patterns
    // Transposes byte-based waveforms to PARLIO's bit-packed format

    // Calculate chunk parameters
    size_t ledsRemaining = mState.total_leds - mState.current_led;
    size_t ledsThisChunk = (ledsRemaining < mState.leds_per_chunk)
                               ? ledsRemaining
                               : mState.leds_per_chunk;

    if (ledsThisChunk == 0) {
        return true; // Nothing to do
    }

    // Get the next buffer to fill
    uint8_t *nextBuffer = const_cast<uint8_t *>(mState.fill_buffer);

    // Zero output buffer to prevent garbage data from previous use
    fl::memset(nextBuffer, 0x00, mState.buffer_size);

    if (nextBuffer == nullptr) {
        return false;
    }

    // Use pre-allocated waveform expansion buffer
    const size_t max_pulses_per_bit = 16; // Conservative max
    const size_t bytes_per_lane = 8 * max_pulses_per_bit; // 128 bytes per lane
    const size_t waveform_buffer_size = mState.data_width * bytes_per_lane;

    if (!mState.waveform_expansion_buffer ||
        mState.waveform_expansion_buffer_size < waveform_buffer_size) {
        mState.error_occurred = true;
        return false;
    }

    uint8_t *laneWaveforms = mState.waveform_expansion_buffer.get();
    const uint8_t *bit0_wave = mState.bit0_waveform.data();
    const uint8_t *bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    // Zero waveform buffer to eliminate leftover data
    fl::memset(laneWaveforms, 0x00, waveform_buffer_size);

    size_t outputIdx = 0;

    // Process each LED in this chunk
    for (size_t ledIdx = 0; ledIdx < ledsThisChunk; ledIdx++) {
        size_t absoluteLedIdx = mState.current_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = absoluteLedIdx * 3 + colorIdx;

            // Step 1: Expand each lane's byte to waveform using generic
            // function
            for (size_t lane = 0; lane < mState.data_width; lane++) {
                uint8_t *laneWaveform =
                    laneWaveforms +
                    (lane *
                     bytes_per_lane); // Pointer to this lane's waveform buffer

                if (lane < mState.actual_channels) {
                    // Real channel - expand waveform from scratch buffer
                    const uint8_t *laneData =
                        mState.scratch_padded_buffer.data() +
                        (lane * mState.lane_stride);
                    uint8_t byte = laneData[byteOffset];

                    // PHASE 9: Use optimized nibble lookup table for waveform expansion
                    size_t expanded = fl::expandByteToWaveformsOptimized(
                        byte, mState.nibble_lut, laneWaveform, bytes_per_lane);
                    if (expanded == 0) {
                        // Waveform expansion failed - abort
                        mState.error_occurred = true;
                        // CRITICAL: laneWaveforms points to
                        // mState.waveform_expansion_buffer (pre-allocated heap
                        // buffer managed by this class). DO NOT call
                        // heap_caps_free(laneWaveforms)! The buffer is owned by
                        // mState and will be freed in the destructor.
                        return false;
                    }
                } else {
                    // Dummy lane - zero waveform (keeps GPIO LOW)
                    fl::memset(laneWaveform, 0x00, bytes_per_lane);
                }
            }

            // Step 2: Bit-pack transpose waveform bytes to PARLIO format
            // Convert byte-based waveforms (0xFF/0x00) to bit-packed output
            //
            // PARLIO Data Format:
            // - For data_width=N, each N bits represent one clock cycle
            // - Bits are packed: bit 0 = lane 0, bit 1 = lane 1, etc.
            // - Each byte contains 8/N clock cycles worth of data
            //
            // Example: data_width=1, 80 pulses
            //   - Need 80 clock cycles = 80 bits = 10 bytes
            //   - Each byte: [tick7, tick6, ..., tick1, tick0]
            const size_t pulsesPerByte =
                8 * mState.pulses_per_bit; // 80 pulses for 8 bits

            if (mState.data_width <= 8) {
                // Pack into single bytes
                // Each byte contains (8 / data_width) clock cycles
                size_t ticksPerByte = 8 / mState.data_width;
                size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;

                for (size_t byteIdx = 0; byteIdx < numOutputBytes; byteIdx++) {
                    uint8_t outputByte = 0;

                    // Pack ticksPerByte time ticks into this byte
                    for (size_t t = 0; t < ticksPerByte; t++) {
                        size_t tick = byteIdx * ticksPerByte + t;
                        if (tick >= pulsesPerByte) break; // Avoid overflow

                        // Extract pulses from all lanes at this time tick
                        for (size_t lane = 0; lane < mState.data_width; lane++) {
                            uint8_t *laneWaveform =
                                laneWaveforms + (lane * bytes_per_lane);
                            uint8_t pulse = laneWaveform[tick];
                            uint8_t bit =
                                (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0

                            // Bit position: temporal order (tick 0 → bit 0, tick 1 → bit 1, ...)
                            // PARLIO transmits LSB-first (bit 0 of byte sent first)
                            // This gives correct temporal sequence: tick 0, tick 1, ..., tick 7
                            size_t bitPos = t * mState.data_width + lane;
                            outputByte |= (bit << bitPos);
                        }
                    }

                    nextBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words (two bytes per tick)
                // For 16-bit width: 1 clock cycle = 16 bits = 2 bytes
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    // Extract one pulse from each lane and pack into bits
                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t *laneWaveform =
                            laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit =
                            (pulse != 0) ? 1 : 0; // Convert 0xFF→1, 0x00→0
                        outputWord |= (bit << lane); // Lane 0 = bit 0, etc.
                    }

                    // FIX C3-2: Check for buffer overflow BEFORE writing (prevent corruption)
                    if (outputIdx + 2 > mState.buffer_size) {
                        mState.error_occurred = true;
                        return false;
                    }

                    // Write as two bytes (low byte first for little-endian)
                    nextBuffer[outputIdx++] = outputWord & 0xFF;          // Low byte
                    nextBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;  // High byte
                }
            }
        }
    }

    // FIX C3-2: Removed redundant buffer overflow check (now checked BEFORE each write)
    // Previously checked AFTER all writes completed, which was too late to prevent corruption

    // Queue transmission of this buffer
    parlio_transmit_config_t tx_config = {};
    tx_config.idle_value = 0x0000;

    size_t chunkBits =
        outputIdx * 8; // PARLIO API requires payload size in bits

    esp_err_t err = parlio_tx_unit_transmit(mState.tx_unit, nextBuffer,
                                            chunkBits, &tx_config);

    if (err != ESP_OK) {
        mState.error_occurred = true;
        // Phase 0: Record error code
        // CRITICAL: laneWaveforms points to mState.waveform_expansion_buffer
        // (pre-allocated heap buffer managed by this class). DO NOT call
        // heap_caps_free(laneWaveforms)! The buffer is owned by mState and will
        // be freed in the destructor.
        return false;
    }

    // Update state
    mState.current_led += ledsThisChunk;

    // Phase 0: Track chunks queued and bytes transmitted
    size_t bytes_this_chunk = ledsThisChunk * 3 * mState.num_lanes;

    // Swap buffer pointers for ping-pong buffering
    // After first transmission: active=buffer_a, fill=buffer_b
    // After second transmission: active=buffer_b, fill=buffer_a
    mState.active_buffer = mState.fill_buffer;
    if (mState.fill_buffer == mState.buffer_a.get()) {
        mState.fill_buffer = mState.buffer_b.get();
    } else {
        mState.fill_buffer = mState.buffer_a.get();
    }

    // Note: laneWaveforms points to mState.waveform_expansion_buffer
    // (pre-allocated heap buffer). DO NOT free it here - the buffer is managed
    // by mState and freed in the destructor.

    return true;
}

//=============================================================================
// Phase 0: Ring Buffer Generation (CPU Thread)
//=============================================================================

bool ChannelEnginePARLIOImpl::generateRingBuffer(size_t ring_index, size_t start_led, size_t led_count) {
    // Validate parameters
    if (ring_index >= PARLIO_RING_BUFFER_COUNT) {
        FL_WARN("PARLIO: Invalid ring_index=" << ring_index << " (max=" << PARLIO_RING_BUFFER_COUNT << ")");
        return false;
    }

    if (start_led + led_count > mState.mIsrContext->total_leds) {
        FL_WARN("PARLIO: Ring buffer LED range overflow: start=" << start_led
                << " count=" << led_count << " total=" << mState.mIsrContext->total_leds);
        return false;
    }

    // Get ring buffer pointer
    uint8_t* outputBuffer = mState.ring_buffers[ring_index].get();
    if (!outputBuffer) {
        FL_WARN("PARLIO: Ring buffer " << ring_index << " not allocated");
        return false;
    }

    // Zero output buffer to prevent garbage data
    fl::memset(outputBuffer, 0x00, mState.ring_buffer_capacity);

    // Get waveform generation resources
    uint8_t* laneWaveforms = mState.waveform_expansion_buffer.get();
    const uint8_t* bit0_wave = mState.bit0_waveform.data();
    const uint8_t* bit1_wave = mState.bit1_waveform.data();
    size_t waveform_size = mState.pulses_per_bit;

    const size_t max_pulses_per_bit = 16;
    const size_t bytes_per_lane = 8 * max_pulses_per_bit;

    size_t outputIdx = 0;

    // Process each LED in this buffer
    for (size_t ledIdx = 0; ledIdx < led_count; ledIdx++) {
        size_t absoluteLedIdx = start_led + ledIdx;

        // Process each color channel (R, G, B)
        for (size_t colorIdx = 0; colorIdx < 3; colorIdx++) {
            size_t byteOffset = absoluteLedIdx * 3 + colorIdx;

            // Step 1: Expand each lane's byte to waveform
            for (size_t lane = 0; lane < mState.data_width; lane++) {
                uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);

                if (lane < mState.actual_channels) {
                    // Real channel - expand waveform from scratch buffer
                    const uint8_t* laneData = mState.scratch_padded_buffer.data() + (lane * mState.lane_stride);
                    uint8_t byte = laneData[byteOffset];

                    size_t expanded = fl::expandByteToWaveforms(
                        byte, bit0_wave, waveform_size, bit1_wave, waveform_size,
                        laneWaveform, bytes_per_lane);

                    if (expanded == 0) {
                        FL_WARN("PARLIO: Waveform expansion failed at LED " << absoluteLedIdx);
                        return false;
                    }
                } else {
                    // Dummy lane - zero waveform (keeps GPIO LOW)
                    fl::memset(laneWaveform, 0x00, bytes_per_lane);
                }
            }

            // Step 2: Bit-pack transpose waveform bytes to PARLIO format
            const size_t pulsesPerByte = 8 * mState.pulses_per_bit;

            if (mState.data_width <= 8) {
                // Pack into single bytes
                size_t ticksPerByte = 8 / mState.data_width;
                size_t numOutputBytes = (pulsesPerByte + ticksPerByte - 1) / ticksPerByte;

                for (size_t byteIdx = 0; byteIdx < numOutputBytes; byteIdx++) {
                    uint8_t outputByte = 0;

                    for (size_t t = 0; t < ticksPerByte; t++) {
                        size_t tick = byteIdx * ticksPerByte + t;
                        if (tick >= pulsesPerByte) break;

                        for (size_t lane = 0; lane < mState.data_width; lane++) {
                            uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                            uint8_t pulse = laneWaveform[tick];
                            uint8_t bit = (pulse != 0) ? 1 : 0;

                            size_t bitPos = t * mState.data_width + lane;
                            outputByte |= (bit << bitPos);
                        }
                    }

                    // Check for buffer overflow
                    if (outputIdx + 1 > mState.ring_buffer_capacity) {
                        FL_WARN("PARLIO: Ring buffer overflow at outputIdx=" << outputIdx);
                        return false;
                    }

                    outputBuffer[outputIdx++] = outputByte;
                }
            } else if (mState.data_width == 16) {
                // Pack into 16-bit words
                for (size_t tick = 0; tick < pulsesPerByte; tick++) {
                    uint16_t outputWord = 0;

                    for (size_t lane = 0; lane < 16; lane++) {
                        uint8_t* laneWaveform = laneWaveforms + (lane * bytes_per_lane);
                        uint8_t pulse = laneWaveform[tick];
                        uint8_t bit = (pulse != 0) ? 1 : 0;
                        outputWord |= (bit << lane);
                    }

                    // Check for buffer overflow
                    if (outputIdx + 2 > mState.ring_buffer_capacity) {
                        FL_WARN("PARLIO: Ring buffer overflow at outputIdx=" << outputIdx);
                        return false;
                    }

                    outputBuffer[outputIdx++] = outputWord & 0xFF;
                    outputBuffer[outputIdx++] = (outputWord >> 8) & 0xFF;
                }
            }
        }
    }

    // Add padding to ensure byte boundary alignment between chunks
    // WS2812B reset time is 50μs minimum, but we use minimal padding here
    // The ISR adds inter-chunk delay via idle_value
    size_t padding_bytes = 8;  // 8 bytes = 64 bits = 8μs at 8MHz per bit
    for (size_t i = 0; i < padding_bytes && outputIdx < mState.ring_buffer_capacity; i++) {
        outputBuffer[outputIdx++] = 0x00;
    }

    // Store actual size of this buffer
    mState.ring_buffer_sizes[ring_index] = outputIdx;

    // DEBUG: Copy DMA buffer data to debug deque for validation
    if (mState.mIsrContext) {
        // Append this buffer's data to the debug deque (push_back each byte)
        for (size_t i = 0; i < outputIdx; i++) {
            mState.mIsrContext->mDebugDmaOutput.push_back(outputBuffer[i]);
        }
    }

    return true;
}

//=============================================================================
// Public Interface - IChannelEngine Implementation
//=============================================================================

void ChannelEnginePARLIOImpl::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEnginePARLIOImpl::show() {
    FL_SCOPED_TRACE;
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Group channels by chipset timing configuration
        mChipsetGroups.clear();
        mCurrentGroupIndex = 0;

        for (size_t i = 0; i < mTransmittingChannels.size(); i++) {
            const ChannelDataPtr& channel = mTransmittingChannels[i];
            const ChipsetTimingConfig& timing = channel->getTiming();

            // Find existing group with matching timing
            bool found = false;
            for (size_t j = 0; j < mChipsetGroups.size(); j++) {
                // Compare timing configurations
                if (mChipsetGroups[j].mTiming == timing) {
                    mChipsetGroups[j].mChannels.push_back(channel);
                    found = true;
                    break;
                }
            }

            // Create new group if no match found
            if (!found) {
                ChipsetGroup newGroup;
                newGroup.mTiming = timing;
                newGroup.mChannels.push_back(channel);
                mChipsetGroups.push_back(fl::move(newGroup));
            }
        }

        // Sort groups by transmission time (fastest first)
        // Transmission time = LED count * (T1 + T2 + T3)
        // This allows faster transmissions to complete first, reducing latency
        // for shorter/faster strips while longer/slower strips are still being prepared
        fl::sort(mChipsetGroups.begin(), mChipsetGroups.end(),
                 [](const ChipsetGroup& a, const ChipsetGroup& b) {
                     // Find max channel size in group a
                     size_t maxSizeA = 0;
                     for (const auto& channel : a.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeA) {
                             maxSizeA = size;
                         }
                     }

                     // Find max channel size in group b
                     size_t maxSizeB = 0;
                     for (const auto& channel : b.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeB) {
                             maxSizeB = size;
                         }
                     }

                     // Calculate transmission time: LED count * bit period
                     // Note: Using 64-bit to avoid overflow (max ~4 billion ns)
                     uint64_t transmissionTimeA = static_cast<uint64_t>(maxSizeA) * a.mTiming.total_period_ns();
                     uint64_t transmissionTimeB = static_cast<uint64_t>(maxSizeB) * b.mTiming.total_period_ns();

                     // Sort by transmission time ascending (faster groups first)
                     return transmissionTimeA < transmissionTimeB;
                 });

        // Begin transmission of first chipset group
        // Note: show() always starts first group synchronously, then returns
        // The async behavior happens when first group is also the last group
        // (common case: single chipset) - poll() will return DRAINING immediately
        if (!mChipsetGroups.empty()) {
            beginTransmission(fl::span<const ChannelDataPtr>(
                mChipsetGroups[0].mChannels.data(),
                mChipsetGroups[0].mChannels.size()));
        }
    }
}

IChannelEngine::EngineState ChannelEnginePARLIOImpl::poll() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized) {
        return EngineState::READY;
    }

    // Poll HAL state
    detail::ParlioEngineState halState = mEngine.poll();

    switch (halState) {
        case detail::ParlioEngineState::READY:
            // Current group completed - check if more groups need transmission
            if (!mChipsetGroups.empty() && mCurrentGroupIndex < mChipsetGroups.size() - 1) {
                // More groups remaining - start next group
                mCurrentGroupIndex++;

                // Check if this is the last group
                bool is_last_group = (mCurrentGroupIndex == mChipsetGroups.size() - 1);

                beginTransmission(fl::span<const ChannelDataPtr>(
                    mChipsetGroups[mCurrentGroupIndex].mChannels.data(),
                    mChipsetGroups[mCurrentGroupIndex].mChannels.size()));

                // If this is the last group, return DRAINING immediately (async)
                // Otherwise return READY and wait for next poll
                return is_last_group ? EngineState::DRAINING : EngineState::READY;
            }

            // All groups completed - clear state and return READY
            mTransmittingChannels.clear();
            mChipsetGroups.clear();
            mCurrentGroupIndex = 0;
            return EngineState::READY;

        case detail::ParlioEngineState::DRAINING:
            // Current group still transmitting - return DRAINING
            return EngineState::DRAINING;

        case detail::ParlioEngineState::BUSY:
            return EngineState::BUSY;

        case detail::ParlioEngineState::ERROR:
            mTransmittingChannels.clear();
            mChipsetGroups.clear();
            mCurrentGroupIndex = 0;
            return EngineState::ERROR;

        default:
            return EngineState::ERROR;
    }
}

//=============================================================================
// Private Methods - Transmission
//=============================================================================

void ChannelEnginePARLIOImpl::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {

    // Validate channel data first (before initialization)
    if (channelData.size() == 0) {
        return;
    }

    // Validate channel count is within bounds
    size_t channel_count = channelData.size();
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many channels (got " << channel_count
                                                  << ", max 16)");
        return;
    }

    // Validate channel count matches data_width constraints
    size_t required_width = selectDataWidth(channel_count);
    if (required_width != mDataWidth) {
        FL_WARN("PARLIO: Channel count "
                << channel_count << " requires data_width=" << required_width
                << " but this instance is data_width=" << mDataWidth);
        return;
    }

    // Extract and validate GPIO pins from channel data
    fl::vector<int> pins;
    for (size_t i = 0; i < channel_count; i++) {
        int pin = channelData[i]->getPin();
        pins.push_back(pin);
    }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mDataWidth; i++) {
        pins.push_back(-1);
    }

    // Extract timing configuration from first channel
    const ChipsetTimingConfig &timing = channelData[0]->getTiming();

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
    }

    if (!hasData || maxChannelSize == 0) {
        return;
    }

    // Calculate max LEDs for buffer sizing (cap at 300 to avoid excessive memory usage)

    size_t actualLeds = maxChannelSize / 3;
    size_t maxLeds = (actualLeds < FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL) ? actualLeds : FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL;

    // Initialize HAL if needed
    if (!mEngine.initialize(mDataWidth, pins, timing, maxLeds)) {
        FL_WARN("PARLIO: HAL initialization failed");
        return;
    }

    mInitialized = true;

    // Prepare scratch buffer (per-lane layout)
    prepareScratchBuffer(channelData, maxChannelSize);

    // Delegate to HAL (blocking call)
    if (!mEngine.beginTransmission(
            mScratchBuffer.data(),
            maxChannelSize,
            channelData.size(),
            maxChannelSize)) {
        FL_WARN("PARLIO: Transmission failed");
    }
}

void ChannelEnginePARLIOImpl::prepareScratchBuffer(
    fl::span<const ChannelDataPtr> channelData,
    size_t maxChannelSize) {

    // Resize scratch buffer (per-lane layout)
    size_t totalSize = channelData.size() * maxChannelSize;
    mScratchBuffer.resize(totalSize);

    // Copy channel data to scratch buffer with right-padding
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        uint8_t* laneDst = mScratchBuffer.data() + (i * maxChannelSize);

        const auto& srcData = channelData[i]->getData();
        if (dataSize < maxChannelSize) {
            // Right-pad with zeros
            fl::memcpy(laneDst, srcData.data(), dataSize);
            fl::memset(laneDst + dataSize, 0, maxChannelSize - dataSize);
        } else {
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }
}

//=============================================================================
// Polymorphic Wrapper Class Implementation
//=============================================================================

ChannelEnginePARLIO::ChannelEnginePARLIO() : mEngine(), mCurrentDataWidth(0) {}

ChannelEnginePARLIO::~ChannelEnginePARLIO() {
    // fl::unique_ptr automatically deletes mEngine when it goes out of scope
    // (RAII)
    mCurrentDataWidth = 0;
}

void ChannelEnginePARLIO::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEnginePARLIO::show() {
    FL_SCOPED_TRACE;
    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Begin transmission (selects engine and delegates)
        beginTransmission(fl::span<const ChannelDataPtr>(
            mTransmittingChannels.data(), mTransmittingChannels.size()));
    }
}

IChannelEngine::EngineState ChannelEnginePARLIO::poll() {
    // Poll the engine if initialized
    if (mEngine) {
        EngineState state = mEngine->poll();

        // Clear transmitting channels when READY
        if (state == EngineState::READY && !mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }

        return state;
    }

    // No engine initialized = ready state (lazy initialization)
    return EngineState::READY;
}

void ChannelEnginePARLIO::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    // Validate channel data
    if (channelData.size() == 0) {
        return;
    }

    size_t channel_count = channelData.size();

    // Validate channel count is within bounds
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many channels (got " << channel_count
                                                  << ", max 16)");
        return;
    }

    // Determine optimal data width for this transmission
    size_t required_width = selectDataWidth(channel_count);
    if (required_width == 0) {
        FL_WARN("PARLIO: Invalid channel count " << channel_count);
        return;
    }

    // Check if we need to create or reconfigure the engine
    if (!mEngine) {
        // First initialization - create engine with optimal width
        mEngine = fl::make_unique<ChannelEnginePARLIOImpl>(required_width);
        mCurrentDataWidth = required_width;
    } else if (mCurrentDataWidth != required_width) {
        // Width changed - need to reconfigure
        // Reset unique_ptr - automatically deletes old engine, creates new one
        mEngine.reset(new ChannelEnginePARLIOImpl(required_width));
        mCurrentDataWidth = required_width;
    }

    // Delegate to internal engine
    if (mEngine) {
        // Enqueue all channels
        for (const auto& ch : channelData) {
            mEngine->enqueue(ch);
        }
        mEngine->show();
    }
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
