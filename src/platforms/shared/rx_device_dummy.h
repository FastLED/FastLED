/// @file rx_device_dummy.h
/// @brief Dummy RxDevice implementation for unsupported platforms or invalid types

#pragma once

#include "fl/rx_device.h"
#include "fl/warn.h"

namespace fl {

/**
 * @brief Dummy RxDevice implementation for unsupported platforms or invalid types
 *
 * Warns on first use, then returns failures for all operations.
 * This prevents null pointer dereferences while providing clear error messages.
 */
class DummyRxDevice : public RxDevice {
public:
    explicit DummyRxDevice(const char* reason) : mReason(reason), mWarned(false) {}

    bool begin(uint32_t signal_range_min_ns = 100,
              uint32_t signal_range_max_ns = 100000,
              uint32_t skip_signals = 0) override {
        (void)signal_range_min_ns;
        (void)signal_range_max_ns;
        (void)skip_signals;
        warnOnce();
        return false;
    }

    bool finished() const override {
        warnOnce();
        return true;  // Always "finished" (nothing to do)
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

private:
    void warnOnce() const {
        if (!mWarned) {
            FL_WARN("RxDevice not available: " << mReason);
            mWarned = true;
        }
    }

    const char* mReason;
    mutable bool mWarned;
};

} // namespace fl
