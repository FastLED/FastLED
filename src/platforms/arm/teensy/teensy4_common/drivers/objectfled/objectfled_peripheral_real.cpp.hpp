// IWYU pragma: private

/// @file objectfled_peripheral_real.cpp.hpp
/// @brief Real ObjectFLED peripheral implementation for Teensy 4.x

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/iobjectfled_peripheral.h"

#include "fl/stl/cstring.h"
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLED.h"
#include "third_party/object_fled/src/ObjectFLEDPinValidation.h"
// IWYU pragma: end_keep

namespace fl {

// ============================================================================
// ObjectFLEDInstanceReal: wraps a real ObjectFLED instance
// ============================================================================

class ObjectFLEDInstanceReal : public IObjectFLEDInstance {
public:
    ObjectFLEDInstanceReal(ObjectFLED* ofled, u32 totalBytes)
        : mObjectFLED(ofled), mTotalBytes(totalBytes) {}

    ~ObjectFLEDInstanceReal() override {
        delete mObjectFLED;  // ok bare allocation
    }

    u8* getFrameBuffer() override {
        return mObjectFLED->frameBufferLocal;
    }

    u32 getFrameBufferSize() const override {
        return mTotalBytes;
    }

    void show() override {
        mObjectFLED->show();
    }

private:
    ObjectFLED* mObjectFLED;
    u32 mTotalBytes;
};

// ============================================================================
// ObjectFLEDPeripheralReal: factory/validator for real hardware
// ============================================================================

class ObjectFLEDPeripheralReal : public IObjectFLEDPeripheral {
public:
    ObjectFLEDPeripheralReal() = default;
    ~ObjectFLEDPeripheralReal() override = default;

    ObjectFLEDPinResult validatePin(u8 pin) const override {
        auto result = objectfled::validate_teensy4_pin(pin);
        return {result.valid, result.error_message};
    }

    fl::unique_ptr<IObjectFLEDInstance> createInstance(
            int totalLeds, bool isRgbw, u32 numPins, const u8* pinList,
            u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) override {

        auto* ofled = new ObjectFLED(  // ok bare allocation
            totalLeds,
            nullptr,  // ObjectFLED allocates frameBufferLocal
            isRgbw ? CORDER_RGBW : CORDER_RGB,
            static_cast<u8>(numPins),
            pinList,
            0  // No serpentine
        );

        ofled->begin(
            static_cast<u16>(t1_ns),
            static_cast<u16>(t2_ns),
            static_cast<u16>(t3_ns),
            static_cast<u16>(reset_us)
        );

        // CRITICAL: Set drawBuffer to frameBufferLocal so genFrameBuffer()
        // (called inside show()) doesn't dereference nullptr. We write
        // pixel data directly into frameBufferLocal, so genFrameBuffer's
        // copy from drawBuffer→frameBufferLocal becomes a self-copy (no-op
        // at default brightness=255).
        ofled->drawBuffer = ofled->frameBufferLocal;

        int bytesPerLed = isRgbw ? 4 : 3;
        u32 totalBytes = static_cast<u32>(totalLeds * bytesPerLed);

        // Clear frame buffer for padding
        fl::memset(ofled->frameBufferLocal, 0, totalBytes);

        return fl::make_unique<ObjectFLEDInstanceReal>(ofled, totalBytes);
    }
};

// Factory: create real peripheral on Teensy 4.x
fl::shared_ptr<IObjectFLEDPeripheral> IObjectFLEDPeripheral::create() {
    return fl::make_shared<ObjectFLEDPeripheralReal>();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
