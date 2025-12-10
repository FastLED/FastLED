/// @file rx_device.cpp
/// @brief Implementation of RxDevice factory

#include "rx_device.h"
#include "platforms/shared/rx_device_dummy.h"
#include "fl/chipsets/led_timing.h"
#include "fl/str.h"

#ifdef ESP32
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx.h"
#endif

namespace fl {

// Implementation of make4PhaseTiming function
ChipsetTiming4Phase make4PhaseTiming(const ChipsetTiming& timing_3phase,
                                      uint32_t tolerance_ns) {
    // Calculate derived values from 3-phase timing
    // The encoder uses:
    //   Bit 0: T1 high + (T2+T3) low
    //   Bit 1: (T1+T2) high + T3 low
    uint32_t t0h = timing_3phase.T1;                    // Bit 0 high time
    uint32_t t0l = timing_3phase.T2 + timing_3phase.T3; // Bit 0 low time
    uint32_t t1h = timing_3phase.T1 + timing_3phase.T2; // Bit 1 high time
    uint32_t t1l = timing_3phase.T3;                    // Bit 1 low time

    return ChipsetTiming4Phase{
        // Bit 0 timing thresholds
        .t0h_min_ns = (t0h > tolerance_ns) ? (t0h - tolerance_ns) : 0,
        .t0h_max_ns = t0h + tolerance_ns,
        .t0l_min_ns = (t0l > tolerance_ns) ? (t0l - tolerance_ns) : 0,
        .t0l_max_ns = t0l + tolerance_ns,

        // Bit 1 timing thresholds
        .t1h_min_ns = (t1h > tolerance_ns) ? (t1h - tolerance_ns) : 0,
        .t1h_max_ns = t1h + tolerance_ns,
        .t1l_min_ns = (t1l > tolerance_ns) ? (t1l - tolerance_ns) : 0,
        .t1l_max_ns = t1l + tolerance_ns,

        // Reset pulse threshold
        .reset_min_us = timing_3phase.RESET
    };
}

} // namespace fl

#ifdef ESP32

// ESP32-specific implementation
namespace fl {

// Factory implementation for ESP32
// Both RmtRxChannel and GpioIsrRx inherit from RxDevice, so no adapters needed
fl::shared_ptr<RxDevice> RxDevice::create(const char* type,
                                           int pin,
                                           size_t buffer_size,
                                           fl::optional<uint32_t> hz) {
    if (fl::strcmp(type, "RMT") == 0) {
        // Use platform default (40MHz) if not specified
        uint32_t resolution_hz = hz.has_value() ? hz.value() : 40000000;
        auto device = RmtRxChannel::create(pin, resolution_hz, buffer_size);
        if (!device) {
            return fl::make_shared<DummyRxDevice>("RMT RX channel creation failed");
        }
        return device;
    }
    else if (fl::strcmp(type, "ISR") == 0) {
        auto device = GpioIsrRx::create(pin, buffer_size);
        if (!device) {
            return fl::make_shared<DummyRxDevice>("GPIO ISR RX creation failed");
        }
        return device;
    }
    else {
        return fl::make_shared<DummyRxDevice>("Unknown RX device type (valid: \"RMT\", \"ISR\")");
    }
}

} // namespace fl

#else // !ESP32

// Stub implementation for non-ESP32 platforms
namespace fl {

fl::shared_ptr<RxDevice> RxDevice::create(const char* type,
                                           int pin,
                                           size_t buffer_size,
                                           fl::optional<uint32_t> hz) {
    (void)type;
    (void)pin;
    (void)buffer_size;
    (void)hz;
    return fl::make_shared<DummyRxDevice>("RX devices not supported on this platform");
}

} // namespace fl

#endif // ESP32

// ============================================================================
// RxDecoder Implementation (shared across all platforms)
// ============================================================================

#include "fl/dbg.h"
#include "fl/warn.h"

namespace fl {

void RxDecoder::configure(const RxConfig& config, size_t buffer_size) {
    mConfig = config;
    mBufferSize = buffer_size;

    // Allocate EdgeTime buffer
    mEdgeBuffer.clear();
    mEdgeBuffer.reserve(buffer_size);
    for (size_t i = 0; i < buffer_size; i++) {
        mEdgeBuffer.push_back(EdgeTime());
    }

    reset();

    FL_DBG("RxDecoder configured: buffer_size=" << buffer_size
           << ", start_low=" << (mConfig.start_low ? "true" : "false")
           << ", signal_range_min=" << mConfig.signal_range_min_ns << "ns"
           << ", signal_range_max=" << mConfig.signal_range_max_ns << "ns");
}

void RxDecoder::reset() {
    mEdgeCount = 0;
    mFinished = false;
    FL_DBG("RxDecoder reset");
}

bool RxDecoder::pushEdge(bool level, uint32_t duration_ns) {
    if (mEdgeCount >= mBufferSize) {
        mFinished = true;
        return false;  // Buffer full
    }

    // Edge detection: Filter spurious edges before storing in buffer
    // This happens at ISR time (must be FL_IRAM safe, no logging)
    if (mEdgeCount == 0) {
        // First edge after reset/configure: check if it matches expected start level
        bool is_valid_start = (mConfig.start_low && level) || (!mConfig.start_low && !level);
        if (!is_valid_start) {
            // Skip spurious idle-state edge - DO NOT store in buffer
            return true;  // Return true to indicate edge was "accepted" (just filtered)
        }
    }

    // Valid edge - store in buffer
    mEdgeBuffer[mEdgeCount] = EdgeTime(level, duration_ns);
    mEdgeCount++;
    return true;
}

void RxDecoder::setFinished() {
    mFinished = true;
}

size_t RxDecoder::getRawEdgeTimes(fl::span<EdgeTime> out) const {
    if (out.empty() || mEdgeCount == 0) {
        return 0;
    }

    size_t count = (mEdgeCount < out.size()) ? mEdgeCount : out.size();
    for (size_t i = 0; i < count; i++) {
        out[i] = mEdgeBuffer[i];
    }
    return count;
}

fl::Result<uint32_t, DecodeError> RxDecoder::decode(
    const ChipsetTiming4Phase& timing,
    fl::span<uint8_t> out) {

    if (mEdgeCount == 0) {
        FL_WARN("RxDecoder::decode: no edges captured");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    if (out.empty()) {
        FL_WARN("RxDecoder::decode: output buffer is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    fl::span<const EdgeTime> edges(mEdgeBuffer.data(), mEdgeCount);

    FL_DBG("RxDecoder::decode: processing " << edges.size() << " edges");

    // Note: Edge detection already happened in pushEdge()
    // Buffer only contains valid edges (spurious idle-state edges were filtered out)

    // ========== Decode EdgeTime → Bytes ==========
    // WS2812B protocol: Each bit consists of HIGH pulse + LOW pulse
    // We process edges in pairs: edges[i] = HIGH duration, edges[i+1] = LOW duration

    uint32_t bytes_decoded = 0;
    uint8_t current_byte = 0;
    int bit_index = 0;
    size_t error_count = 0;

    // Process edge pairs from start of buffer
    for (size_t i = 0; i + 1 < edges.size(); i += 2) {
        const EdgeTime& high_edge = edges[i];
        const EdgeTime& low_edge = edges[i + 1];

        // Validate pattern: expect HIGH followed by LOW
        if (high_edge.high != 1 || low_edge.high != 0) {
            FL_DBG("Skipping invalid edge pair at index " << i
                   << " (high=" << high_edge.high << ", low=" << low_edge.high << ")");
            error_count++;
            continue;
        }

        // Decode bit based on timing
        int bit = decodeBit(high_edge.ns, low_edge.ns, timing);
        if (bit < 0) {
            FL_DBG("Invalid timing at index " << i
                   << " (high=" << high_edge.ns << "ns, low=" << low_edge.ns << "ns)");
            error_count++;
            continue;
        }

        // Accumulate bit (MSB first)
        current_byte = (current_byte << 1) | static_cast<uint8_t>(bit);
        bit_index++;

        // Byte complete?
        if (bit_index == 8) {
            if (bytes_decoded < out.size()) {
                out[bytes_decoded] = current_byte;
                bytes_decoded++;
            } else {
                FL_WARN("RxDecoder::decode: output buffer overflow at byte " << bytes_decoded);
                return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }

            // Reset for next byte
            current_byte = 0;
            bit_index = 0;
        }
    }

    // Flush partial byte if we have remaining bits
    if (bit_index != 0) {
        FL_WARN("RxDecoder::decode: partial byte at end (bit_index=" << bit_index << "), flushing");
        // Shift remaining bits to MSB position
        current_byte <<= (8 - bit_index);

        if (bytes_decoded < out.size()) {
            out[bytes_decoded] = current_byte;
            bytes_decoded++;
        }
    }

    if (error_count > 0) {
        FL_DBG("RxDecoder::decode: " << error_count << " errors during decoding");
    }

    FL_DBG("RxDecoder::decode: decoded " << bytes_decoded << " bytes from " << mEdgeCount << " edges");
    return fl::Result<uint32_t, DecodeError>::success(bytes_decoded);
}

int RxDecoder::decodeBit(uint32_t high_ns, uint32_t low_ns, const ChipsetTiming4Phase& timing) {
    // Check bit 0 pattern (short HIGH, long LOW)
    bool t0h_match = (high_ns >= timing.t0h_min_ns) && (high_ns <= timing.t0h_max_ns);
    bool t0l_match = (low_ns >= timing.t0l_min_ns) && (low_ns <= timing.t0l_max_ns);
    if (t0h_match && t0l_match) {
        return 0;
    }

    // Check bit 1 pattern (long HIGH, short LOW)
    bool t1h_match = (high_ns >= timing.t1h_min_ns) && (high_ns <= timing.t1h_max_ns);
    bool t1l_match = (low_ns >= timing.t1l_min_ns) && (low_ns <= timing.t1l_max_ns);
    if (t1h_match && t1l_match) {
        return 1;
    }

    // Timing doesn't match either pattern
    return -1;
}

} // namespace fl

