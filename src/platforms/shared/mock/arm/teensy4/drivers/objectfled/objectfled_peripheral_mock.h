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
    ObjectFLEDInstanceMock(u32 totalBytes, bool busyAfterShow = false)
 FL_NO_EXCEPT : mBuffer(totalBytes, 0), mShowCount(0),
                 mBusyAfterShow(busyAfterShow), mBusy(false) {}

    ~ObjectFLEDInstanceMock() override = default;

    u8* getFrameBuffer() FL_NO_EXCEPT override {
        return mBuffer.data();
    }

    u32 getFrameBufferSize() const FL_NO_EXCEPT override {
        return static_cast<u32>(mBuffer.size());
    }

    void show() FL_NO_EXCEPT override {
        ++mShowCount;
        mBusy = mBusyAfterShow;
    }

    bool isBusy() FL_NO_EXCEPT override {
        return mBusy;
    }

    // =========================================================================
    // Mock-Specific API (for unit tests)
    // =========================================================================

    /// @brief Get number of show() calls
    u32 getShowCount() const FL_NO_EXCEPT { return mShowCount; }

    /// @brief Get raw buffer contents (written by channel engine before show())
    const fl::vector<u8>& getRawBuffer() const FL_NO_EXCEPT { return mBuffer; }

    /// @brief Complete the simulated DMA/latch wait
    void complete() FL_NO_EXCEPT { mBusy = false; }

    /// @brief Configure whether show() leaves the instance busy
    void setBusyAfterShow(bool busyAfterShow) FL_NO_EXCEPT {
        mBusyAfterShow = busyAfterShow;
    }

private:
    fl::vector<u8> mBuffer;
    u32 mShowCount;
    bool mBusyAfterShow;
    bool mBusy;
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

    ObjectFLEDPeripheralMock() FL_NO_EXCEPT
        : mForceCreateFailure(false), mBusyAfterShow(false) {}
    ~ObjectFLEDPeripheralMock() override = default;

    // =========================================================================
    // IObjectFLEDPeripheral Interface
    // =========================================================================

    ObjectFLEDPinResult validatePin(u8 pin) const FL_NO_EXCEPT override {
        for (auto& p : mInvalidPins) {
            if (p.pin == pin) {
                return {false, p.message};
            }
        }
        return {true, nullptr};
    }

    fl::unique_ptr<IObjectFLEDInstance> createInstance(
            int totalLeds, bool isRgbw, u32 numPins, const u8* pinList,
            u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NO_EXCEPT override {

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

        auto instance = fl::make_unique<ObjectFLEDInstanceMock>(totalBytes, mBusyAfterShow);
        mLastInstance = instance.get();
        mInstances.push_back(instance.get());
        return instance;
    }

    // =========================================================================
    // Mock-Specific API (for unit tests)
    // =========================================================================

    /// @brief Mark a pin as invalid (for error path testing)
    void setInvalidPin(u8 pin, const char* message = "Pin invalid (mock)") FL_NO_EXCEPT {
        mInvalidPins.push_back({pin, message});
    }

    /// @brief Clear all invalid pin settings
    void clearInvalidPins() FL_NO_EXCEPT { mInvalidPins.clear(); }

    /// @brief Force createInstance() to return nullptr
    void setCreateFailure(bool fail) FL_NO_EXCEPT { mForceCreateFailure = fail; }

    /// @brief Force newly-created instances to remain busy after show()
    void setBusyAfterShow(bool busy) FL_NO_EXCEPT { mBusyAfterShow = busy; }

    /// @brief Get total number of createInstance() calls
    size_t getCreateCount() const FL_NO_EXCEPT { return mCreateRecords.size(); }

    /// @brief Get all createInstance() call records
    const fl::vector<CreateRecord>& getCreateRecords() const FL_NO_EXCEPT { return mCreateRecords; }

    /// @brief Get the most recent createInstance() record
    const CreateRecord* getLastCreateRecord() const FL_NO_EXCEPT {
        if (mCreateRecords.empty()) return nullptr;
        return &mCreateRecords[mCreateRecords.size() - 1];
    }

    /// @brief Get the last created instance (raw pointer, not owned)
    ObjectFLEDInstanceMock* getLastInstance() const FL_NO_EXCEPT { return mLastInstance; }

    /// @brief Get a created instance by creation index (raw pointer, not owned)
    ObjectFLEDInstanceMock* getInstance(size_t index) const FL_NO_EXCEPT {
        if (index >= mInstances.size()) {
            return nullptr;
        }
        return mInstances[index];
    }

    /// @brief Reset all mock state
    void reset() FL_NO_EXCEPT {
        mInvalidPins.clear();
        mCreateRecords.clear();
        mInstances.clear();
        mLastInstance = nullptr;
        mForceCreateFailure = false;
        mBusyAfterShow = false;
    }

private:
    struct InvalidPin {
        u8 pin;
        const char* message;
    };

    fl::vector<InvalidPin> mInvalidPins;
    fl::vector<CreateRecord> mCreateRecords;
    fl::vector<ObjectFLEDInstanceMock*> mInstances;
    ObjectFLEDInstanceMock* mLastInstance = nullptr;
    bool mForceCreateFailure;
    bool mBusyAfterShow;
};

} // namespace fl
