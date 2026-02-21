/// @file platforms/shared/rx_device_native.h
/// @brief Native (stub/host) RxDevice implementation for testing
///
/// NativeRxDevice captures WS2812 edge timing data simulated by the
/// stub channel engine or ClocklessController stub.
///
/// Architecture:
/// 1. NativeRxDevice::begin() registers a callback with
///    fl::stub::setPinEdgeCallback() on its pin. This simulates arming the RX pin.
/// 2. The stub channel engine calls fl::stub::simulateWS2812Output() which
///    fires the pin callback with edge events.
/// 3. NativeRxDevice::wait() unregisters itself and checks if edges arrived.
/// 4. NativeRxDevice::decode() decodes the internally buffered edge timings.
/// 5. The decoded bytes are compared against the original LED data.

#pragma once

// IWYU pragma: private

#include "fl/rx_device.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

namespace fl {

/// @brief Native stub RxDevice for host/desktop testing
///
/// Registers a callback on its pin during begin() and captures all simulated
/// GPIO edge events into an internal buffer.
/// This simulates a physical jumper wire from TX pin to RX pin.
class NativeRxDevice : public RxDevice {
public:
    /// @brief Create a NativeRxDevice for the given pin
    /// @param pin GPIO pin number (used for identification only on native)
    static fl::shared_ptr<NativeRxDevice> create(int pin);

    // -------------------------------------------------------------------------
    // RxDevice interface implementation
    // -------------------------------------------------------------------------

    /// @brief Arm edge capture: register as SimEdgeObserver
    /// @param config RX configuration (ignored on native)
    /// @return true always (native capture always succeeds)
    bool begin(const RxConfig& config) override;

    /// @brief Check if capture is complete (synchronous on native)
    /// @return true after wait() has been called
    bool finished() const override;

    /// @brief Check if edges are available (synchronous on native)
    /// @param timeout_ms Ignored (native capture is synchronous)
    /// @return SUCCESS if edges were captured, TIMEOUT if no edges
    RxWaitResult wait(u32 timeout_ms) override;

    /// @brief Decode captured WS2812 edge timing to bytes
    /// @param timing 4-phase timing thresholds for bit detection
    /// @param out Output buffer to write decoded bytes
    /// @return Result with total bytes decoded, or DecodeError
    fl::Result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                        fl::span<u8> out) override;

    /// @brief Get raw edge timings (for debugging)
    /// @param out Output span to fill with EdgeTime entries
    /// @param offset Starting edge index
    /// @return Number of edges written
    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) override;

    /// @brief Device type name
    const char* name() const override;

    /// @brief Get the GPIO pin number
    int getPin() const override;

    /// @brief Inject edges directly (for testing)
    bool injectEdges(fl::span<const EdgeTime> edges) override;

    ~NativeRxDevice();

private:
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args);

    explicit NativeRxDevice(int pin);

    /// @brief Receive a simulated GPIO edge (called by pin callback)
    void onEdge(bool high, u32 duration_ns);

    int mPin;
    bool mFinished;
    bool mArmed;                    ///< true while callback is registered on pin
    fl::vector<EdgeTime> mEdges;    ///< Internally buffered simulated edge data
};

}  // namespace fl
