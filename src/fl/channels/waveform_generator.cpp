/// @file waveform_generator.cpp
/// @brief Implementation of generic waveform generator for clockless LED protocols

#include "waveform_generator.h"
#include "fl/math_macros.h"
#include "ftl/cstring.h"

namespace fl {

// ============================================================================
// Waveform Generator Functions
// ============================================================================

size_t generateBit0Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b0_waveform
) {
    // Validate inputs
    if (hz == 0 || t1_ns == 0 || t2_ns == 0 || t3_ns == 0) {
        return 0;
    }

    // Calculate resolution: nanoseconds per pulse
    // resolution_ns = 1,000,000,000 / hz
    uint32_t resolution_ns = 1000000000UL / hz;
    if (resolution_ns == 0) {
        return 0;  // Frequency too high
    }

    // Calculate pulses using ceiling division
    uint32_t pulses_t1 = (t1_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t2 = (t2_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t3 = (t3_ns + resolution_ns - 1) / resolution_ns;

    uint32_t total_pulses = pulses_t1 + pulses_t2 + pulses_t3;

    if (b0_waveform.size() < total_pulses) {
        return 0;  // Buffer too small
    }

    // Bit 0: HIGH for t1, LOW for t2+t3
    size_t idx = 0;
    for (uint32_t i = 0; i < pulses_t1; i++) {
        b0_waveform[idx++] = 0xFF;  // HIGH
    }
    for (uint32_t i = 0; i < pulses_t2 + pulses_t3; i++) {
        b0_waveform[idx++] = 0x00;  // LOW
    }

    return total_pulses;
}

size_t generateBit1Waveform(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b1_waveform
) {
    // Validate inputs
    if (hz == 0 || t1_ns == 0 || t2_ns == 0 || t3_ns == 0) {
        return 0;
    }

    // Calculate resolution: nanoseconds per pulse
    uint32_t resolution_ns = 1000000000UL / hz;
    if (resolution_ns == 0) {
        return 0;  // Frequency too high
    }

    // Calculate pulses using ceiling division
    uint32_t pulses_t1 = (t1_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t2 = (t2_ns + resolution_ns - 1) / resolution_ns;
    uint32_t pulses_t3 = (t3_ns + resolution_ns - 1) / resolution_ns;

    uint32_t total_pulses = pulses_t1 + pulses_t2 + pulses_t3;

    if (b1_waveform.size() < total_pulses) {
        return 0;  // Buffer too small
    }

    // Bit 1: HIGH for t1+t2, LOW for t3
    size_t idx = 0;
    for (uint32_t i = 0; i < pulses_t1 + pulses_t2; i++) {
        b1_waveform[idx++] = 0xFF;  // HIGH
    }
    for (uint32_t i = 0; i < pulses_t3; i++) {
        b1_waveform[idx++] = 0x00;  // LOW
    }

    return total_pulses;
}

bool generateWaveforms(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    fl::span<uint8_t> b0_waveform,
    size_t& b0_size,
    fl::span<uint8_t> b1_waveform,
    size_t& b1_size
) {
    b0_size = generateBit0Waveform(hz, t1_ns, t2_ns, t3_ns, b0_waveform);
    b1_size = generateBit1Waveform(hz, t1_ns, t2_ns, t3_ns, b1_waveform);

    // Return true only if both succeeded (non-zero sizes)
    return (b0_size > 0 && b1_size > 0);
}

size_t expandByteToWaveforms(
    uint8_t dataByte,
    fl::span<const uint8_t> b0_waveform,
    fl::span<const uint8_t> b1_waveform,
    fl::span<uint8_t> output
) {
    // Validate inputs
    if (b0_waveform.size() != b1_waveform.size() || b0_waveform.size() == 0) {
        return 0;
    }

    const size_t pulsesPerBit = b0_waveform.size();
    const size_t totalSize = 8 * pulsesPerBit;

    if (output.size() < totalSize) {
        return 0;
    }

    // Expand each bit (MSB first) to its waveform
    size_t outIdx = 0;
    for (int bitPos = 7; bitPos >= 0; bitPos--) {
        bool bitValue = (dataByte >> bitPos) & 0x01;
        const fl::span<const uint8_t>& waveform = bitValue ? b1_waveform : b0_waveform;

        // Copy waveform for this bit
        for (size_t i = 0; i < pulsesPerBit; i++) {
            output[outIdx++] = waveform[i];
        }
    }

    return totalSize;
}

// ============================================================================
// Multi-Lane Transposer Functions
// ============================================================================

size_t transposeLanes(
    const fl::span<const uint8_t>* laneData,
    uint8_t numLanes,
    size_t segmentLength,
    fl::span<uint8_t> output
) {
    if (numLanes == 0 || numLanes > MAX_LANES || segmentLength == 0) {
        return 0;
    }

    const size_t outputSize = segmentLength * numLanes;
    if (output.size() < outputSize) {
        return 0;
    }

    // Interleave bytes from each lane
    // Output format: [L0_byte0, L1_byte0, ..., LN_byte0, L0_byte1, L1_byte1, ...]
    size_t outIdx = 0;
    for (size_t bytePos = 0; bytePos < segmentLength; bytePos++) {
        for (uint8_t lane = 0; lane < numLanes; lane++) {
            if (laneData[lane].size() > bytePos) {
                output[outIdx++] = laneData[lane][bytePos];
            } else {
                output[outIdx++] = 0x00;  // Pad with zeros if lane is shorter
            }
        }
    }

    return outputSize;
}

void transpose8Lanes(
    const fl::span<const uint8_t> laneData[8],
    size_t byteOffset,
    fl::span<uint8_t> output
) {
    if (output.size() < 8) {
        return;
    }

    // Gather 8 bytes (one from each lane)
    uint8_t input[8];
    for (int i = 0; i < 8; i++) {
        if (laneData[i].size() > byteOffset) {
            input[i] = laneData[i][byteOffset];
        } else {
            input[i] = 0x00;
        }
    }

    // Optimized 8x8 bit matrix transpose (from Hacker's Delight)
    // This transposes 8 bytes so that bit N from each byte becomes byte N
    uint32_t x, y, t;

    // Pack into two 32-bit words
    x = (input[0] << 24) | (input[1] << 16) | (input[2] << 8) | input[3];
    y = (input[4] << 24) | (input[5] << 16) | (input[6] << 8) | input[7];

    // Perform transpose magic
    t = (x ^ (x >> 7)) & 0x00AA00AA;
    x = x ^ t ^ (t << 7);
    t = (y ^ (y >> 7)) & 0x00AA00AA;
    y = y ^ t ^ (t << 7);

    t = (x ^ (x >> 14)) & 0x0000CCCC;
    x = x ^ t ^ (t << 14);
    t = (y ^ (y >> 14)) & 0x0000CCCC;
    y = y ^ t ^ (t << 14);

    t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
    y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
    x = t;

    // Unpack results
    output[0] = static_cast<uint8_t>(x >> 24);
    output[1] = static_cast<uint8_t>(x >> 16);
    output[2] = static_cast<uint8_t>(x >> 8);
    output[3] = static_cast<uint8_t>(x);
    output[4] = static_cast<uint8_t>(y >> 24);
    output[5] = static_cast<uint8_t>(y >> 16);
    output[6] = static_cast<uint8_t>(y >> 8);
    output[7] = static_cast<uint8_t>(y);
}

// ============================================================================
// ISR Processing Implementation
// ============================================================================

void ISRState::init(
    uint32_t hz,
    uint32_t t1_ns,
    uint32_t t2_ns,
    uint32_t t3_ns,
    uint8_t lanes,
    size_t bytesPerLaneTotal,
    size_t segmentSizeBytes
) {
    numLanes = lanes;
    bytesPerLane = bytesPerLaneTotal;
    segmentSize = segmentSizeBytes;

    // Calculate total segments
    totalSegments = (bytesPerLane + segmentSize - 1) / segmentSize;  // Round up

    // Generate precomputed waveforms
    fl::span<uint8_t> zeroSpan(zeroBitWave.data(), zeroBitWave.size());
    fl::span<uint8_t> oneSpan(oneBitWave.data(), oneBitWave.size());

    size_t b0_size = generateBit0Waveform(hz, t1_ns, t2_ns, t3_ns, zeroSpan);
    size_t b1_size = generateBit1Waveform(hz, t1_ns, t2_ns, t3_ns, oneSpan);

    // Both should have the same size
    pulsesPerBit = (b0_size == b1_size) ? b0_size : 0;

    reset();
}

void ISRState::reset() {
    currentSegment = 0;
    isComplete = false;
}

bool processNextSegment(ISRState& state) {
    if (state.isComplete || state.currentSegment >= state.totalSegments) {
        state.isComplete = true;
        return false;
    }

    // Calculate segment boundaries
    const size_t segmentStart = state.currentSegment * state.segmentSize;
    const size_t segmentEnd = fl_min(segmentStart + state.segmentSize, state.bytesPerLane);
    const size_t segmentBytes = segmentEnd - segmentStart;

    if (segmentBytes == 0) {
        state.isComplete = true;
        return false;
    }

    // Process each lane: convert LED bytes to waveforms
    fl::span<const uint8_t> zeroWave(state.zeroBitWave.data(), state.pulsesPerBit);
    fl::span<const uint8_t> oneWave(state.oneBitWave.data(), state.pulsesPerBit);

    for (uint8_t lane = 0; lane < state.numLanes; lane++) {
        const auto& sourceData = state.laneDataSources[lane];
        auto& waveformBuffer = state.laneWaveformBuffers[lane];

        size_t waveIdx = 0;
        for (size_t byteIdx = segmentStart; byteIdx < segmentEnd; byteIdx++) {
            uint8_t dataByte = (byteIdx < sourceData.size()) ? sourceData[byteIdx] : 0x00;

            // Expand this byte to waveforms
            fl::span<uint8_t> waveformOutput(
                waveformBuffer.data() + waveIdx,
                waveformBuffer.size() - waveIdx
            );

            size_t written = expandByteToWaveforms(
                dataByte,
                zeroWave,
                oneWave,
                waveformOutput
            );

            waveIdx += written;
        }
    }

    // Transpose waveforms from all lanes into DMA buffer
    fl::span<const uint8_t> laneWaveforms[8];
    for (uint8_t lane = 0; lane < state.numLanes; lane++) {
        laneWaveforms[lane] = state.laneWaveformBuffers[lane];
    }

    const size_t waveformBytesPerSegment = segmentBytes * 8 * state.pulsesPerBit;

    transposeLanes(
        laneWaveforms,
        state.numLanes,
        waveformBytesPerSegment,
        state.dmaBuffer
    );

    // Advance to next segment
    state.currentSegment++;

    if (state.currentSegment >= state.totalSegments) {
        state.isComplete = true;
    }

    return true;
}

}  // namespace fl
