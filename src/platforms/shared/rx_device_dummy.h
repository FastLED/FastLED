/// @file rx_device_dummy.h
/// @brief Dummy RxDevice implementation for unsupported platforms or invalid types

#pragma once

#include "fl/rx_device.h"
#include "fl/warn.h"
#include "ftl/assert.h"
#include "ftl/vector.h"

namespace fl {

/**
 * @brief Dummy RxDevice implementation for unsupported platforms or invalid types
 *
 * Warns on first use, then returns failures for all operations.
 * This prevents null pointer dereferences while providing clear error messages.
 *
 * Testable variant: Can push edge times directly for testing purposes.
 */
class DummyRxDevice : public RxDevice {
public:
    explicit DummyRxDevice(const char* reason) : mReason(reason), mWarned(false) {}

    /**
     * @brief Add edge time for testing purposes
     * @param level High (true) or low (false) signal level
     * @param nanoseconds Duration in nanoseconds (must fit in 31 bits)
     */
    void add(bool level, uint32_t nanoseconds) {
        FL_ASSERT(nanoseconds <= 0x7FFFFFFF, "Nanoseconds overflow: value must fit in 31 bits");
        mEdgeTimes.push_back(EdgeTime(level, nanoseconds));
    }

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

    size_t getRawEdgeTimes(fl::span<EdgeTime> out) const override {
        if (mEdgeTimes.empty()) {
            warnOnce();
            return 0;
        }

        size_t count = (mEdgeTimes.size() < out.size()) ? mEdgeTimes.size() : out.size();
        for (size_t i = 0; i < count; i++) {
            out[i] = mEdgeTimes[i];
        }
        return count;
    }

    const char* name() const override {
        return "dummy";
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
    fl::vector<EdgeTime> mEdgeTimes;
};

} // namespace fl
