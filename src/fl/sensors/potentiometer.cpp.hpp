#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"
#include "fl/ui.h"
#include "fl/stl/assert.h"
#include "fl/sensors/potentiometer.h"
#include "fl/pin.h"
#include "fl/math_macros.h"

namespace fl {

// ============================================================================
// PotentiometerLowLevel Implementation
// ============================================================================

PotentiometerLowLevel::PotentiometerLowLevel(int pin)
    : mPin(pin) {
}

PotentiometerLowLevel::~PotentiometerLowLevel() {}

u16 PotentiometerLowLevel::read() {
    return fl::analogRead(mPin);
}

// ============================================================================
// Potentiometer Implementation
// ============================================================================

Potentiometer::Potentiometer(int pin, u16 hysteresis)
    : mPot(pin), mListener(this), mHysteresis(hysteresis) {
    // Initialize calibration range to full ADC range
    mMinValue = 0;
    mMaxValue = getAdcMaxValue();

    // Read initial value
    mCurrentValue = mPot.read();
    mLastValue = mCurrentValue;

    // Auto-calculate hysteresis if not specified (1% of calibrated range)
    if (mHysteresis == 0) {
        mHysteresis = calculateDefaultHysteresis();
    }
}

float Potentiometer::normalized() const {
    // Handle invalid range
    if (mMaxValue <= mMinValue) {
        return 0.0f;
    }

    // Clamp current value to calibrated range
    u16 clamped_value;
    if (mCurrentValue < mMinValue) {
        clamped_value = mMinValue;
    } else if (mCurrentValue > mMaxValue) {
        clamped_value = mMaxValue;
    } else {
        clamped_value = mCurrentValue;
    }

    // Map calibrated range to [0.0, 1.0]
    u16 range = mMaxValue - mMinValue;
    u16 offset = clamped_value - mMinValue;
    return static_cast<float>(offset) / static_cast<float>(range);
}

u16 Potentiometer::fractional16() const {
    // Handle invalid range
    if (mMaxValue <= mMinValue) {
        return 0;
    }

    // Clamp current value to calibrated range
    u16 clamped_value;
    if (mCurrentValue < mMinValue) {
        clamped_value = mMinValue;
    } else if (mCurrentValue > mMaxValue) {
        clamped_value = mMaxValue;
    } else {
        clamped_value = mCurrentValue;
    }

    // Map calibrated range to [0, 65535]
    // Use 32-bit intermediate to avoid overflow
    u32 range = mMaxValue - mMinValue;
    u32 offset = clamped_value - mMinValue;
    u32 scaled = (offset * 65535U) / range;
    return static_cast<u16>(scaled);
}

void Potentiometer::setHysteresisPercent(float percent) {
    // Clamp percent to valid range
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    // Calculate hysteresis based on calibrated range
    u16 range = (mMaxValue > mMinValue) ? (mMaxValue - mMinValue) : getAdcMaxValue();
    mHysteresis = static_cast<u16>((percent / 100.0f) * range);
}

void Potentiometer::setRange(u16 min, u16 max) {
    // Ensure min < max
    if (min >= max) {
        return;  // Invalid range, do nothing
    }
    mMinValue = min;
    mMaxValue = max;
}

u16 Potentiometer::getAdcMaxValue() const {
    // Platform detection for ADC resolution
    // Stub and AVR platforms use 10-bit (0-1023), modern platforms use 12-bit (0-4095)
#if defined(FL_IS_AVR) || defined(STUB_PLATFORM) || defined(FASTLED_USE_STUB_ARDUINO)
    return 1023;  // 10-bit ADC
#else
    return 4095;  // 12-bit ADC (ESP32, ESP8266, SAMD, STM32, etc.)
#endif
}

u16 Potentiometer::calculateDefaultHysteresis() const {
    // Default: 1% of calibrated range or minimum of 10 counts (whichever is larger)
    u16 range = (mMaxValue > mMinValue) ? (mMaxValue - mMinValue) : getAdcMaxValue();
    u16 one_percent = range / 100;
    return (one_percent > 10) ? one_percent : 10;
}

void Potentiometer::Listener::onEndFrame() {
    // Read current value
    u16 new_value = mOwner->mPot.read();
    mOwner->mCurrentValue = new_value;

    // Calculate absolute difference from last triggered value
    u16 diff;
    if (new_value > mOwner->mLastValue) {
        diff = new_value - mOwner->mLastValue;
    } else {
        diff = mOwner->mLastValue - new_value;
    }

    // Check if change exceeds hysteresis threshold
    const bool changed_beyond_hysteresis = (diff >= mOwner->mHysteresis);

    mOwner->mChangedThisFrame = changed_beyond_hysteresis;

    if (changed_beyond_hysteresis) {
        mOwner->mLastValue = new_value;

        // Invoke callbacks with Potentiometer reference
        mOwner->mOnChangeCallbacks.invoke(*mOwner);

        // Invoke callbacks with normalized float value
        float normalized_value = mOwner->normalized();
        mOwner->mOnChangeNormalizedCallbacks.invoke(normalized_value);
    } else {
        // Value didn't change beyond hysteresis
        mOwner->mChangedThisFrame = false;
    }
}

Potentiometer::Listener::Listener(Potentiometer *owner) : mOwner(owner) {
    addToEngineEventsOnce();
}

Potentiometer::Listener::~Listener() {
    if (added) {
        EngineEvents::removeListener(this);
    }
}

void Potentiometer::Listener::addToEngineEventsOnce() {
    if (added) {
        return;
    }
    EngineEvents::addListener(this, 1);  // Priority 1 (runs before UI elements)
    added = true;
}

int Potentiometer::onChange(fl::function<void(Potentiometer &)> callback) {
    int id = mOnChangeCallbacks.add(callback);
    return id;
}

int Potentiometer::onChange(fl::function<void(float)> callback) {
    int id = mOnChangeNormalizedCallbacks.add(callback);
    return id;
}

} // namespace fl
