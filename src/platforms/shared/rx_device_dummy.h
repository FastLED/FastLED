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
 * Uses composition with RxDecoder for edge detection and decoding logic.
 */
class DummyRxDevice : public RxDevice {
public:
    explicit DummyRxDevice(const char* reason)
        : mReason(reason)
        , mWarned(false) {
        // Initialize decoder with default configuration
        // begin() can override this later if needed
        RxConfig default_config;
        mDecoder.configure(default_config, 256);
    }

    /**
     * @brief Add edge time for testing purposes
     * @param level High (true) or low (false) signal level
     * @param nanoseconds Duration in nanoseconds (must fit in 31 bits)
     *
     * Delegates to internal RxDecoder which handles edge detection automatically.
     * Edge detection behavior is configured via begin() call (or uses default).
     */
    void add(bool level, uint32_t nanoseconds) {
        FL_ASSERT(nanoseconds <= 0x7FFFFFFF, "Nanoseconds overflow: value must fit in 31 bits");
        mDecoder.pushEdge(level, nanoseconds);
    }

    bool begin(const RxConfig& config) override {
        // Configure decoder with edge detection settings
        mDecoder.configure(config, 256);  // Default buffer size for dummy device
        // Dummy device succeeds (for testing purposes)
        return true;
    }

    bool finished() const override {
        return mDecoder.finished();
    }

    RxWaitResult wait(uint32_t timeout_ms) override {
        (void)timeout_ms;
        warnOnce();
        return RxWaitResult::TIMEOUT;
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                               fl::span<uint8_t> out) override {
        // Delegate to decoder
        return mDecoder.decode(timing, out);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out) const override {
        // Delegate to decoder
        return mDecoder.getRawEdgeTimes(out);
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
    RxDecoder mDecoder;  ///< Composition: decoder handles edge detection and decoding
};

} // namespace fl
