/// @file rx_device_dummy.h
/// @brief Dummy RxDevice implementation for unsupported platforms or invalid types

#pragma once

#include "fl/rx_device.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/stl/assert.h"
#include "fl/stl/vector.h"

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

    bool begin(const RxConfig& config) override {
        (void)config;
        warnOnce();
        return false;
    }

    bool finished() const override {
        return true;  // Always finished (no data)
    }

    RxWaitResult wait(uint32_t timeout_ms) override {
        (void)timeout_ms;
        warnOnce();
        return RxWaitResult::TIMEOUT;
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                               fl::span<uint8_t> out) override {
        (void)timing;
        (void)out;
        warnOnce();
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) override {
        (void)out;
        (void)offset;
        warnOnce();
        return 0;
    }

    const char* name() const override {
        return "dummy";
    }

    int getPin() const override {
        return -1;  // Dummy device has no pin
    }

    bool injectEdges(fl::span<const EdgeTime> edges) override {
        (void)edges;
        warnOnce();
        return false;  // Not supported
    }

private:
    void warnOnce() const {
        if (!mWarned) {
            FL_ERROR("RxDevice not available: " << mReason << ", falling back to DummyRxDevice");
            mWarned = true;
        }
    }

    const char* mReason;
    mutable bool mWarned;
};

} // namespace fl
