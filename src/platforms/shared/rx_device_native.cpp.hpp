/// @file platforms/shared/rx_device_native.cpp.hpp
/// @brief NativeRxDevice implementation for host/desktop testing

// IWYU pragma: private

#include "platforms/shared/rx_device_native.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_STUB

#include "platforms/stub/stub_gpio.h"
#include "fl/rx_device.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/stl/vector.h"

namespace fl {

// ============================================================================
// Factory
// ============================================================================

fl::shared_ptr<NativeRxDevice> NativeRxDevice::create(int pin) {
    return fl::make_shared<NativeRxDevice>(pin);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

NativeRxDevice::NativeRxDevice(int pin)
    : mPin(pin)
    , mFinished(false)
    , mArmed(false) {
}

NativeRxDevice::~NativeRxDevice() {
    // Clear callback if still armed
    if (mArmed) {
        fl::stub::clearPinEdgeCallback(mPin);
        mArmed = false;
    }
}

// ============================================================================
// RxDevice interface
// ============================================================================

bool NativeRxDevice::begin(const RxConfig& config) {
    (void)config;
    // Clear any previous callback before re-arming
    if (mArmed) {
        fl::stub::clearPinEdgeCallback(mPin);
    }
    mEdges.clear();
    mFinished = false;
    // Register callback on the pin: simulateWS2812Output() will fire it
    fl::stub::setPinEdgeCallback(mPin, [this](bool high, u32 duration_ns) {
        this->onEdge(high, duration_ns);
    });
    mArmed = true;
    return true;
}

bool NativeRxDevice::finished() const {
    return mFinished;
}

RxWaitResult NativeRxDevice::wait(u32 timeout_ms) {
    (void)timeout_ms;
    // Clear callback — edges are already in mEdges from synchronous simulation
    if (mArmed) {
        fl::stub::clearPinEdgeCallback(mPin);
        mArmed = false;
    }
    mFinished = true;
    if (mEdges.empty()) {
        FL_WARN("NativeRxDevice: No edges captured for pin " << mPin
                << " - stub channel engine may not have called simulateWS2812Output()");
        return RxWaitResult::TIMEOUT;
    }
    return RxWaitResult::SUCCESS;
}

// ============================================================================
// Edge callback — called by simulateWS2812Output() via registered callback
// ============================================================================

void NativeRxDevice::onEdge(bool high, u32 duration_ns) {
    EdgeTime edge;
    edge.ns   = duration_ns;
    edge.high = high ? 1u : 0u;
    mEdges.push_back(edge);
}

// ============================================================================
// Decode
// ============================================================================

/// @brief Decode WS2812 edges to bytes using 4-phase timing thresholds
///
/// Each WS2812 bit produces two edge entries:
///   EdgeTime(high=true,  ns=T0H or T1H) — HIGH duration
///   EdgeTime(high=false, ns=T0L or T1L) — LOW duration
///
/// Bit 0: T0H ∈ [t0h_min, t0h_max], T0L ∈ [t0l_min, t0l_max]
/// Bit 1: T1H ∈ [t1h_min, t1h_max], T1L ∈ [t1l_min, t1l_max]
fl::Result<u32, DecodeError> NativeRxDevice::decode(const ChipsetTiming4Phase& timing,
                                                     fl::span<u8> out) {
    size_t edge_count = mEdges.size();
    if (edge_count == 0) {
        FL_WARN("NativeRxDevice::decode: No edges recorded for pin " << mPin);
        return fl::Result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t bytes_written = 0;
    size_t bit_index = 0;      // 0–7 within current byte (MSB first)
    u8 current_byte = 0;
    u32 error_count = 0;

    size_t i = 0;
    while (i + 1 < edge_count) {
        EdgeTime high_edge = mEdges[i];
        EdgeTime low_edge  = mEdges[i + 1];

        // Edges must alternate HIGH then LOW
        if (!high_edge.high || low_edge.high) {
            error_count++;
            i++;
            continue;
        }

        u32 high_ns = high_edge.ns;
        u32 low_ns  = low_edge.ns;

        // Determine bit value from HIGH duration
        bool is_bit1 = (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
                        low_ns  >= timing.t1l_min_ns && low_ns  <= timing.t1l_max_ns);
        bool is_bit0 = (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
                        low_ns  >= timing.t0l_min_ns && low_ns  <= timing.t0l_max_ns);

        if (!is_bit0 && !is_bit1) {
            // Check gap tolerance: if low duration exceeds reset threshold, stop decoding
            if (low_ns >= (u32)(timing.reset_min_us * 1000u)) {
                // Reset pulse — end of frame
                if (bit_index != 0) {
                    FL_WARN("NativeRxDevice::decode: Partial byte at reset (bit_index=" << bit_index << ")");
                }
                break;
            }
            // Try gap tolerance
            if (timing.gap_tolerance_ns > 0 && low_ns <= timing.gap_tolerance_ns) {
                // Tolerable gap - try to decode bit from HIGH duration alone
                if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                    is_bit1 = true;
                } else if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                    is_bit0 = true;
                }
            }

            if (!is_bit0 && !is_bit1) {
                error_count++;
                i += 2;
                continue;
            }
        }

        int bit = is_bit1 ? 1 : 0;

        // Accumulate MSB-first into current_byte
        current_byte = (current_byte << 1) | (u8)bit;
        bit_index++;

        if (bit_index == 8) {
            // Byte complete
            if (bytes_written >= out.size()) {
                // Buffer overflow
                return fl::Result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out[bytes_written++] = current_byte;
            current_byte = 0;
            bit_index = 0;
        }

        i += 2;
    }

    // Check error rate
    if (bytes_written > 0 && error_count * 10 > bytes_written * 8) {
        // More than 10% errors
        return fl::Result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::Result<u32, DecodeError>::success(static_cast<u32>(bytes_written));
}

size_t NativeRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) {
    size_t total = mEdges.size();
    if (offset >= total) return 0;

    size_t count = total - offset;
    if (count > out.size()) count = out.size();

    for (size_t i = 0; i < count; i++) {
        out[i] = mEdges[offset + i];
    }
    return count;
}

const char* NativeRxDevice::name() const {
    return "native";
}

int NativeRxDevice::getPin() const {
    return mPin;
}

bool NativeRxDevice::injectEdges(fl::span<const EdgeTime> edges) {
    for (size_t i = 0; i < edges.size(); i++) {
        mEdges.push_back(edges[i]);
    }
    return true;
}

}  // namespace fl

#endif  // FL_IS_STUB
