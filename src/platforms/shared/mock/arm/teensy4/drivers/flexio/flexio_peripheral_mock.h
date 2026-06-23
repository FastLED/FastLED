/// @file flexio_peripheral_mock.h
/// @brief Mock FlexIO peripheral for unit testing
///
/// Simulates FlexIO2 hardware behavior for host-based unit tests.
/// Provides:
/// - Pin validation (same FlexIO2 pin table as real hardware)
/// - Transmission data capture for validation
/// - Instant completion (synchronous simulation)
/// - State inspection for assertions

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Mock FlexIO peripheral for unit testing
///
/// Simulates FlexIO2 without real hardware. Captures transmitted pixel data
/// and provides instant completion for deterministic testing.
class FlexIOPeripheralMock : public IFlexIOPeripheral {
public:
    /// @brief Transmission record (captured pixel data)
    struct TransmissionRecord {
        u8 pin;              ///< Teensy pin used
        fl::vector<u8> data; ///< Copy of transmitted pixel data
        u32 t0h_ns;          ///< T0H timing
        u32 t1h_ns;          ///< T1H timing
        u32 period_ns;       ///< Total bit period
        u32 reset_us;        ///< Reset time
    };

    FlexIOPeripheralMock() FL_NOEXCEPT : mInitialized(false), mCurrentPin(0xFF),
                              mForceInitFailure(false), mForceShowFailure(false) {}
    ~FlexIOPeripheralMock() override = default;

    // =========================================================================
    // IFlexIOPeripheral Interface
    // =========================================================================

    bool canHandlePin(u8 teensy_pin) const FL_NOEXCEPT override {
        // Same pins as real FlexIO2: 6, 7, 8, 9, 10, 11, 12, 13, 32
        static const u8 kValidPins[] = {6, 7, 8, 9, 10, 11, 12, 13, 32};
        for (u8 p : kValidPins) {
            if (p == teensy_pin) return true;
        }
        return false;
    }

    bool init(u8 teensy_pin, u32 t0h_ns, u32 t1h_ns,
              u32 period_ns, u32 reset_us) FL_NOEXCEPT override {
        if (mForceInitFailure) return false;
        if (!canHandlePin(teensy_pin)) return false;

        mCurrentPin = teensy_pin;
        mCurrentT0H = t0h_ns;
        mCurrentT1H = t1h_ns;
        mCurrentPeriod = period_ns;
        mCurrentReset = reset_us;
        mInitialized = true;
        return true;
    }

    bool show(const u8* pixel_data, u32 num_bytes) FL_NOEXCEPT override {
        if (mForceShowFailure) return false;
        if (!mInitialized || !pixel_data || num_bytes == 0) return false;

        // Capture transmission data
        TransmissionRecord record;
        record.pin = mCurrentPin;
        record.data.assign(pixel_data, pixel_data + num_bytes);
        record.t0h_ns = mCurrentT0H;
        record.t1h_ns = mCurrentT1H;
        record.period_ns = mCurrentPeriod;
        record.reset_us = mCurrentReset;
        mTransmissions.push_back(fl::move(record));

        return true;
    }

    bool isDone() const FL_NOEXCEPT override {
        return true;  // Mock completes instantly
    }

    void wait() FL_NOEXCEPT override {
        // No-op: mock completes instantly
    }

    void deinit() FL_NOEXCEPT override {
        mInitialized = false;
        mCurrentPin = 0xFF;
    }

    // =========================================================================
    // Mock-Specific API (for unit tests)
    // =========================================================================

    /// @brief Get all transmission records
    const fl::vector<TransmissionRecord>& getTransmissions() const FL_NOEXCEPT {
        return mTransmissions;
    }

    /// @brief Get the most recent transmission data
    const TransmissionRecord* getLastTransmission() const FL_NOEXCEPT {
        if (mTransmissions.empty()) return nullptr;
        return &mTransmissions[mTransmissions.size() - 1];
    }

    /// @brief Get total number of show() calls
    size_t getTransmissionCount() const FL_NOEXCEPT { return mTransmissions.size(); }

    /// @brief Check if hardware is currently initialized
    bool isInitialized() const FL_NOEXCEPT { return mInitialized; }

    /// @brief Get currently configured pin
    u8 getCurrentPin() const FL_NOEXCEPT { return mCurrentPin; }

    /// @brief Clear all state for next test
    void reset() FL_NOEXCEPT {
        mTransmissions.clear();
        mInitialized = false;
        mCurrentPin = 0xFF;
        mForceInitFailure = false;
        mForceShowFailure = false;
    }

    /// @brief Force init() to fail (for error path testing)
    void setInitFailure(bool fail) FL_NOEXCEPT { mForceInitFailure = fail; }

    /// @brief Force show() to fail (for error path testing)
    void setShowFailure(bool fail) FL_NOEXCEPT { mForceShowFailure = fail; }

private:
    bool mInitialized;
    u8 mCurrentPin;
    u32 mCurrentT0H = 0;
    u32 mCurrentT1H = 0;
    u32 mCurrentPeriod = 0;
    u32 mCurrentReset = 0;
    bool mForceInitFailure;
    bool mForceShowFailure;
    fl::vector<TransmissionRecord> mTransmissions;
};

} // namespace fl
