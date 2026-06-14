/// @file platforms/shared/rx_device_native.cpp.hpp
/// @brief NativeRxDevice implementation for host/desktop testing

// IWYU pragma: private

#include "platforms/shared/rx_device_native.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_STUB

#include "platforms/stub/stub_gpio.h"
#include "fl/channels/rx.h"
#include "fl/channels/rx/decode_ws2812.h"
#include "fl/log/log.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// Factory
// ============================================================================

fl::shared_ptr<NativeRxDevice> NativeRxDevice::create(int pin) FL_NOEXCEPT {
    return fl::make_shared<NativeRxDevice>(pin);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

NativeRxDevice::NativeRxDevice(int pin)
 FL_NOEXCEPT : mPin(pin)
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

bool NativeRxDevice::begin(const RxConfig& config) FL_NOEXCEPT {
    (void)config;
    // Clear any previous callback before re-arming
    if (mArmed) {
        fl::stub::clearPinEdgeCallback(mPin);
    }
    mEdges.clear();
    mFinished = false;
    // Register callback on the pin: simulateWS2812Output() will fire it
    fl::stub::setPinEdgeCallback(mPin, [this](bool high, u32 duration_ns) FL_NOEXCEPT {
        this->onEdge(high, duration_ns);
    });
    mArmed = true;
    return true;
}

bool NativeRxDevice::finished() const FL_NOEXCEPT {
    return mFinished;
}

RxWaitResult NativeRxDevice::wait(u32 timeout_ms) FL_NOEXCEPT {
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

void NativeRxDevice::onEdge(bool high, u32 duration_ns) FL_NOEXCEPT {
    EdgeTime edge;
    edge.ns   = duration_ns;
    edge.high = high ? 1u : 0u;
    mEdges.push_back(edge);
}

// ============================================================================
// Decode
// ============================================================================

fl::result<u32, DecodeError> NativeRxDevice::decode(const ChipsetTiming4Phase& timing,
                                                     fl::span<u8> out) FL_NOEXCEPT {
    if (mEdges.empty()) {
        FL_WARN("NativeRxDevice::decode: No edges recorded for pin " << mPin);
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return fl::channels::rx::decodeWs2812Edges(
        timing, fl::span<const EdgeTime>(mEdges), out);
}

size_t NativeRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NOEXCEPT {
    size_t total = mEdges.size();
    if (offset >= total) return 0;

    size_t count = total - offset;
    if (count > out.size()) count = out.size();

    for (size_t i = 0; i < count; i++) {
        out[i] = mEdges[offset + i];
    }
    return count;
}

const char* NativeRxDevice::name() const FL_NOEXCEPT {
    return "native";
}

int NativeRxDevice::getPin() const FL_NOEXCEPT {
    return mPin;
}

bool NativeRxDevice::injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT {
    for (size_t i = 0; i < edges.size(); i++) {
        mEdges.push_back(edges[i]);
    }
    return true;
}

}  // namespace fl

#endif  // FL_IS_STUB
