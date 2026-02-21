/// @file platforms/stub/stub_gpio.cpp.hpp
/// @brief Stub GPIO state tracking implementation

// IWYU pragma: private

#include "platforms/stub/stub_gpio.h"
#include "fl/stl/map.h"
#include "fl/stl/vector.h"

namespace fl {
namespace stub {

// ============================================================================
// Internal State
// ============================================================================

namespace {

/// Per-pin callback storage: map from pin number to callback
static fl::map<int, PinEdgeCallback>& pinCallbackMap() {
    static fl::map<int, PinEdgeCallback>* m = new fl::map<int, PinEdgeCallback>();
    return *m;
}

/// Per-pin state: current HIGH/LOW value
static fl::map<int, int>& pinStateMap() {
    // Heap-allocated and never freed: avoids static destructor ordering issues
    // on Windows where fl::allocator_slab may be destroyed before this map.
    static fl::map<int, int>* state = new fl::map<int, int>();
    return *state;
}

/// Per-pin edge buffer + armed flag
struct PinEdgeBuffer {
    fl::vector<fl::EdgeTime> edges;
    bool armed = false;
};

static fl::map<int, PinEdgeBuffer>& edgeBufferMap() {
    // Heap-allocated and never freed: avoids static destructor ordering issues.
    static fl::map<int, PinEdgeBuffer>* buffers = new fl::map<int, PinEdgeBuffer>();
    return *buffers;
}

/// Record an edge for a pin if it is armed
/// @param pin GPIO pin
/// @param high new level after this transition
/// @param duration_ns how long the pin will stay at this level
inline void recordEdge(int pin, bool high, u32 duration_ns) {
    auto& buffers = edgeBufferMap();
    auto it = buffers.find(pin);
    if (it == buffers.end()) return;
    if (!it->second.armed) return;

    fl::EdgeTime edge;
    edge.ns = duration_ns;
    edge.high = high ? 1u : 0u;
    it->second.edges.push_back(edge);
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

void setPinState(int pin, bool high) {
    pinStateMap()[pin] = high ? 1 : 0;
    // Fire callback for this pin with duration=0 (state changes via direct GPIO toggling carry no timing info)
    auto& callbacks = pinCallbackMap();
    auto it = callbacks.find(pin);
    if (it != callbacks.end() && it->second) {
        it->second(high, 0u);
    }
}

bool getPinState(int pin) {
    auto& state = pinStateMap();
    auto it = state.find(pin);
    if (it == state.end()) return false;
    return it->second != 0;
}

void armPinEdges(int pin) {
    auto& buffers = edgeBufferMap();
    auto& buf = buffers[pin];
    buf.edges.clear();
    buf.armed = true;
}

void clearPinEdges(int pin) {
    auto& buffers = edgeBufferMap();
    auto it = buffers.find(pin);
    if (it != buffers.end()) {
        it->second.edges.clear();
        it->second.armed = false;
    }
}

size_t getEdgeCount(int pin) {
    auto& buffers = edgeBufferMap();
    auto it = buffers.find(pin);
    if (it == buffers.end()) return 0;
    return it->second.edges.size();
}

fl::EdgeTime getEdge(int pin, size_t index) {
    auto& buffers = edgeBufferMap();
    auto it = buffers.find(pin);
    if (it == buffers.end()) return fl::EdgeTime();
    const auto& edges = it->second.edges;
    if (index >= edges.size()) return fl::EdgeTime();
    return edges[index];
}

void simulateWS2812Output(int pin,
                           fl::span<const u8> data,
                           const fl::ChipsetTimingConfig& timing) {
    // Make sure edge buffer is accessible (arm it if needed, don't clear if already armed)
    auto& buffers = edgeBufferMap();
    (void)buffers[pin];  // Ensure map entry exists; don't arm here
    // Don't arm here - armPinEdges() should be called before this (by NativeRxDevice::begin())
    // If not armed, edges are silently dropped (caller didn't set up capture)

    const u32 t1 = timing.t1_ns;          // Bit-0 high duration
    const u32 t2 = timing.t2_ns;          // Extra high for bit-1
    const u32 t3 = timing.t3_ns;          // Low duration for bit-1 (also T0L tail = T2+T3 for bit-0)

    // Get callback for this pin (if any)
    auto& callbacks = pinCallbackMap();
    auto it = callbacks.find(pin);
    PinEdgeCallback* cb = (it != callbacks.end()) ? &it->second : nullptr;

    // Emit each byte MSB-first as WS2812 bits
    for (size_t byteIdx = 0; byteIdx < data.size(); byteIdx++) {
        u8 byte = data[byteIdx];
        for (int bit = 7; bit >= 0; bit--) {
            bool is_one = (byte >> bit) & 1;
            u32 high_ns, low_ns;
            if (is_one) {
                // Bit 1: HIGH for T1+T2, then LOW for T3
                high_ns = t1 + t2;
                low_ns  = t3;
            } else {
                // Bit 0: HIGH for T1, then LOW for T2+T3
                high_ns = t1;
                low_ns  = t2 + t3;
            }
            // Record in the TX pin's own armed buffer (for code that still uses arm API)
            recordEdge(pin, true,  high_ns);
            recordEdge(pin, false, low_ns);
            // Fire pin callback if one is registered (simulates physical wire: TX output → RX listener)
            if (cb && *cb) {
                (*cb)(true,  high_ns);
                (*cb)(false, low_ns);
            }
        }
    }
    // Note: No explicit reset pulse is appended. The decoder will see the
    // end of data naturally when edges run out. NativeRxDevice::wait() detects
    // completion by checking that edges are present.
}

void injectEdges(int pin, fl::span<const fl::EdgeTime> edges) {
    auto& buffers = edgeBufferMap();
    auto& buf = buffers[pin];
    if (!buf.armed) {
        // Auto-arm if not yet armed
        buf.edges.clear();
        buf.armed = true;
    }
    for (size_t i = 0; i < edges.size(); i++) {
        buf.edges.push_back(edges[i]);
    }
}

void setPinEdgeCallback(int pin, PinEdgeCallback cb) {
    pinCallbackMap()[pin] = fl::move(cb);
}

void clearPinEdgeCallback(int pin) {
    auto& m = pinCallbackMap();
    auto it = m.find(pin);
    if (it != m.end()) {
        it->second = PinEdgeCallback{};  // null the function
    }
}

}  // namespace stub
}  // namespace fl
