/// @file objectfled_peripheral_mock.h
/// @brief Mock ObjectFLED peripheral for unit testing
///
/// Simulates ObjectFLED hardware behavior for host-based unit tests.
/// Provides:
/// - Configurable pin validation
/// - Frame buffer allocation and data capture
/// - Instance tracking for test inspection

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/iobjectfled_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// ObjectFLEDInstanceMock: allocates a vector<u8> buffer, counts show() calls
// ============================================================================

class ObjectFLEDInstanceMock : public IObjectFLEDInstance {
public:
    ObjectFLEDInstanceMock(u32 totalBytes)
 FL_NOEXCEPT : mBuffer(totalBytes, 0), mShowCount(0) {}

    ~ObjectFLEDInstanceMock() override = default;

    u8* getFrameBuffer() FL_NOEXCEPT override {
        return mBuffer.data();
    }

    u32 getFrameBufferSize() const FL_NOEXCEPT override {
        return static_cast<u32>(mBuffer.size());
    }

    void show() FL_NOEXCEPT override {
        ++mShowCount;
    }

    // =========================================================================
    // Mock-Specific API (for unit tests)
    // =========================================================================

    /// @brief Get number of show() calls
    u32 getShowCount() const FL_NOEXCEPT { return mShowCount; }

    /// @brief Get raw buffer contents (written by channel engine before show())
    const fl::vector<u8>& getRawBuffer() const FL_NOEXCEPT { return mBuffer; }

private:
    fl::vector<u8> mBuffer;
    u32 mShowCount;
};

// ============================================================================
// ObjectFLEDPeripheralMock: configurable pin validation, tracks instances
// ============================================================================

class ObjectFLEDPeripheralMock : public IObjectFLEDPeripheral {
public:
    /// @brief Record of a createInstance() call
    struct CreateRecord {
        int totalLeds;
        bool isRgbw;
        u32 numPins;
        fl::vector<u8> pinList;
        u32 t1_ns, t2_ns, t3_ns, reset_us;
    };

    ObjectFLEDPeripheralMock() FL_NOEXCEPT : mForceCreateFailure(false) {}
    ~ObjectFLEDPeripheralMock() override = default;

    // =========================================================================
    // IObjectFLEDPeripheral Interface
    // =========================================================================

    ObjectFLEDPinResult validatePin(u8 pin) const FL_NOEXCEPT override {
        for (auto& p : mInvalidPins) {
            if (p.pin == pin) {
                return {false, p.message};
            }
        }
        return {true, nullptr};
    }

    fl::unique_ptr<IObjectFLEDInstance> createInstance(
            int totalLeds, bool isRgbw, u32 numPins, const u8* pinList,
            u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NOEXCEPT override {

        // Record the call
        CreateRecord record;
        record.totalLeds = totalLeds;
        record.isRgbw = isRgbw;
        record.numPins = numPins;
        record.pinList.assign(pinList, pinList + numPins);
        record.t1_ns = t1_ns;
        record.t2_ns = t2_ns;
        record.t3_ns = t3_ns;
        record.reset_us = reset_us;
        mCreateRecords.push_back(fl::move(record));

        if (mForceCreateFailure) {
            return nullptr;
        }

        int bytesPerLed = isRgbw ? 4 : 3;
        u32 totalBytes = static_cast<u32>(totalLeds * bytesPerLed);

        auto instance = fl::make_unique<ObjectFLEDInstanceMock>(totalBytes);
        mLastInstance = instance.get();
        return instance;
    }

    // =========================================================================
    // Mock-Specific API (for unit tests)
    // =========================================================================

    /// @brief Mark a pin as invalid (for error path testing)
    void setInvalidPin(u8 pin, const char* message = "Pin invalid (mock)") FL_NOEXCEPT {
        mInvalidPins.push_back({pin, message});
    }

    /// @brief Clear all invalid pin settings
    void clearInvalidPins() FL_NOEXCEPT { mInvalidPins.clear(); }

    /// @brief Force createInstance() to return nullptr
    void setCreateFailure(bool fail) FL_NOEXCEPT { mForceCreateFailure = fail; }

    /// @brief Get total number of createInstance() calls
    size_t getCreateCount() const FL_NOEXCEPT { return mCreateRecords.size(); }

    /// @brief Get all createInstance() call records
    const fl::vector<CreateRecord>& getCreateRecords() const FL_NOEXCEPT { return mCreateRecords; }

    /// @brief Get the most recent createInstance() record
    const CreateRecord* getLastCreateRecord() const FL_NOEXCEPT {
        if (mCreateRecords.empty()) return nullptr;
        return &mCreateRecords[mCreateRecords.size() - 1];
    }

    /// @brief Get the last created instance (raw pointer, not owned)
    ObjectFLEDInstanceMock* getLastInstance() const FL_NOEXCEPT { return mLastInstance; }

    /// @brief Reset all mock state
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
    ObjectFLEDInstanceMock* mLastInstance = nullptr;
    bool mForceCreateFailure;
};

} // namespace fl
