/// @file rx_device_dummy.h
/// @brief Dummy RxDevice implementation for unsupported platforms or invalid types

#pragma once

// IWYU pragma: private

#include "fl/channels/rx.h"
#include "fl/log/log.h"
#include "fl/log/log.h"
#include "fl/stl/assert.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Dummy RxDevice implementation for unsupported platforms or invalid types
 *
 * Warns on first use, then returns failures for all operations.
 * This prevents null pointer dereferences while providing clear error messages.
 */
class DummyRxDevice : public RxDevice {
public:
    explicit DummyRxDevice(const char* reason)
        : mReason(reason)
        , mWarned(false) {
    }

    bool begin(const RxConfig& config) FL_NO_EXCEPT override {
        (void)config;
        warnOnce();
        return false;
    }

    bool finished() const FL_NO_EXCEPT override {
        return true;  // Always finished (no data)
    }

    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override {
        (void)timeout_ms;
        warnOnce();
        return RxWaitResult::TIMEOUT;
    }

    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                               fl::span<u8> out) override FL_NO_EXCEPT {
        (void)timing;
        (void)out;
        warnOnce();
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) FL_NO_EXCEPT override {
        (void)out;
        (void)offset;
        warnOnce();
        return 0;
    }

    const char* name() const FL_NO_EXCEPT override {
        return "dummy";
    }

    int getPin() const FL_NO_EXCEPT override {
        return -1;  // Dummy device has no pin
    }

    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override {
        (void)edges;
        warnOnce();
        return false;  // Not supported
    }

private:
    void warnOnce() const FL_NO_EXCEPT {
        if (!mWarned) {
            FL_ERROR_F("RxDevice not available: %s, falling back to DummyRxDevice", mReason);
            mWarned = true;
        }
    }

    const char* mReason;
    mutable bool mWarned;
};

} // namespace fl
