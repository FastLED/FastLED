/// @file lpuart_peripheral_mock.h
/// @brief Host-side mock for the iMXRT1062 LPUART peripheral
///        (issue #3023 workstream A).
///
/// Mirrors `ObjectFLEDPeripheralMock` — fields the abstract `ILPUARTPeripheral`
/// interface, captures per-instance configuration for test inspection, and
/// supports configurable failure injection. The engine TU (lands in a
/// follow-up PR) is host-testable by wiring this mock into
/// `ChannelEngineLPUART` instead of the real iMXRT register driver.
///
/// **Streaming-ISR plumbing intentionally omitted.** Issue #3023 workstream C
/// extracts the parlio mock's deferred-callback + `settleEngineState()`
/// pattern into a shared template fixture. Once that lands, this mock will
/// adopt the fixture from day one to keep `waitAllDone()` and
/// `deinitialize()` semantics aligned with the post-#2992 async engine
/// pattern. Until then this mock is sync-only and tests cover the sync
/// `show()` path that Phase 1 ships.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// LPUARTInstanceMock — allocates a vector<u8> TX buffer, counts show() calls
// ============================================================================

class LPUARTInstanceMock : public ILPUARTInstance {
public:
    explicit LPUARTInstanceMock(u32 buffer_bytes) FL_NOEXCEPT
        : mBuffer(buffer_bytes, 0), mShowCount(0) {}

    ~LPUARTInstanceMock() override = default;

    u8* getTxBuffer() FL_NOEXCEPT override {
        return mBuffer.data();
    }

    u32 getTxBufferSize() const FL_NOEXCEPT override {
        return static_cast<u32>(mBuffer.size());
    }

    void show() FL_NOEXCEPT override {
        ++mShowCount;
    }

    // ------------------------------------------------------------------
    // Mock-Specific API
    // ------------------------------------------------------------------

    /// @brief Number of `show()` calls.
    u32 getShowCount() const FL_NOEXCEPT { return mShowCount; }

    /// @brief Raw TX buffer contents (written by the engine before `show()`).
    const fl::vector<u8>& getRawBuffer() const FL_NOEXCEPT { return mBuffer; }

private:
    fl::vector<u8> mBuffer;
    u32 mShowCount;
};

// ============================================================================
// LPUARTPeripheralMock — accepts every kLPUARTTxPins[] entry by default,
//                       tracks createInstance() records, supports failure
//                       injection per-pin or globally on create.
// ============================================================================

class LPUARTPeripheralMock : public ILPUARTPeripheral {
public:
    /// @brief Record of a `createInstance()` call.
    struct CreateRecord {
        u8 tx_pin;
        u32 total_leds;
        bool is_rgbw;
        u32 t1_ns;
        u32 t2_ns;
        u32 t3_ns;
        u32 reset_us;
    };

    LPUARTPeripheralMock() FL_NOEXCEPT : mForceCreateFailure(false), mLastInstance(nullptr) {}
    ~LPUARTPeripheralMock() override = default;

    // ------------------------------------------------------------------
    // ILPUARTPeripheral interface
    // ------------------------------------------------------------------

    LPUARTPinResult validatePin(u8 pin) const FL_NOEXCEPT override {
        for (const auto& invalid : mInvalidPins) {
            if (invalid.pin == pin) {
                return {false, invalid.message};
            }
        }
        // Default acceptance: pin must appear in the kLPUARTTxPins[] table.
        for (u32 i = 0; i < kLPUARTTxPinCount; ++i) {
            if (kLPUARTTxPins[i] == pin) {
                return {true, nullptr};
            }
        }
        return {false, "pin not LPUART-TX capable on iMXRT1062"};
    }

    fl::unique_ptr<ILPUARTInstance> createInstance(
            u8 tx_pin, u32 total_leds, bool is_rgbw,
            u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NOEXCEPT override {
        CreateRecord record;
        record.tx_pin = tx_pin;
        record.total_leds = total_leds;
        record.is_rgbw = is_rgbw;
        record.t1_ns = t1_ns;
        record.t2_ns = t2_ns;
        record.t3_ns = t3_ns;
        record.reset_us = reset_us;
        mCreateRecords.push_back(record);

        if (mForceCreateFailure) {
            return nullptr;
        }

        const u32 bytes_per_led = is_rgbw ? 4u : 3u;
        // 4 UART bytes per LED byte (2-bit-per-frame wave8 packing).
        // Reject configurations whose buffer size cannot be represented in u32 —
        // otherwise the multiplication wraps and we allocate an undersized TX
        // buffer while still returning a seemingly-valid instance (CodeRabbit).
        if (total_leds > (0xFFFFFFFFu / bytes_per_led) / 4u) {
            return nullptr;
        }
        const u32 buffer_bytes = total_leds * bytes_per_led * 4u;
        auto instance = fl::make_unique<LPUARTInstanceMock>(buffer_bytes);
        mLastInstance = instance.get();
        return instance;
    }

    // ------------------------------------------------------------------
    // Mock-Specific API
    // ------------------------------------------------------------------

    /// @brief Mark a pin as invalid (overrides default `kLPUARTTxPins[]`
    ///        acceptance — useful for negative-path tests).
    void setInvalidPin(u8 pin, const char* message = "Pin invalid (mock)") FL_NOEXCEPT {
        mInvalidPins.push_back({pin, message});
    }

    /// @brief Clear all invalid-pin overrides.
    void clearInvalidPins() FL_NOEXCEPT { mInvalidPins.clear(); }

    /// @brief Force `createInstance()` to return `nullptr`.
    void setCreateFailure(bool fail) FL_NOEXCEPT { mForceCreateFailure = fail; }

    /// @brief Total number of `createInstance()` calls.
    size_t getCreateCount() const FL_NOEXCEPT { return mCreateRecords.size(); }

    /// @brief All `createInstance()` call records, in chronological order.
    const fl::vector<CreateRecord>& getCreateRecords() const FL_NOEXCEPT { return mCreateRecords; }

    /// @brief Most recent `createInstance()` record, or `nullptr` if none.
    const CreateRecord* getLastCreateRecord() const FL_NOEXCEPT {
        if (mCreateRecords.empty()) return nullptr;
        return &mCreateRecords[mCreateRecords.size() - 1];
    }

    /// @brief Most recent created instance (raw pointer, not owned).
    LPUARTInstanceMock* getLastInstance() const FL_NOEXCEPT { return mLastInstance; }

    /// @brief Reset all mock state.
    void reset() FL_NOEXCEPT {
        mInvalidPins.clear();
        mCreateRecords.clear();
        mLastInstance = nullptr;
        mForceCreateFailure = false;
    }

private:
    struct InvalidPin {
        u8 pin;
        const char* message;
    };

    fl::vector<InvalidPin> mInvalidPins;
    fl::vector<CreateRecord> mCreateRecords;
    LPUARTInstanceMock* mLastInstance;
    bool mForceCreateFailure;
};

} // namespace fl
