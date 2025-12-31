#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"  // For FASTLED_SHARED_PTR macros
#include "fl/pin.h"
#include "fl/ui.h"

namespace fl {

// Low-level potentiometer class for direct ADC reading without callbacks.
// This class provides raw hardware access without automatic updates.
class PotentiometerLowLevel {
  public:
    PotentiometerLowLevel(int pin);
    ~PotentiometerLowLevel();
    PotentiometerLowLevel(const PotentiometerLowLevel &other) = default;
    PotentiometerLowLevel &operator=(const PotentiometerLowLevel &other) = delete;
    PotentiometerLowLevel(PotentiometerLowLevel &&other) = delete;

    // Read raw ADC value (0-1023 for 10-bit, 0-4095 for 12-bit ADC)
    uint16_t read();

    int getPin() const { return mPin; }

  private:
    int mPin;
};

// High-level potentiometer class with automatic updates and callback support.
// This class hooks into FastLED EngineEvents to monitor value changes each frame.
// Includes hysteresis to prevent noise-induced callback spam.
// Supports calibration to map raw ADC values to normalized range (0.0-1.0).
class Potentiometer {
  public:
    // Constructor
    // @param pin: Analog pin number
    // @param hysteresis: Minimum raw ADC change to trigger callbacks (0 = auto: 1% of calibrated range)
    Potentiometer(int pin, uint16_t hysteresis = 0);

    // ========================================================================
    // Value Accessors
    // ========================================================================

    // Get raw ADC value (0-1023 for 10-bit, 0-4095 for 12-bit ADC)
    uint16_t raw() const { return mCurrentValue; }

    // Get normalized float value (0.0f - 1.0f) based on calibration range
    // Maps mMinValue -> 0.0f and mMaxValue -> 1.0f
    // Values outside range are clamped to [0.0, 1.0]
    float normalized() const;

    // Get fractional 16-bit value (0 - 65535) for high-precision integer math
    // Based on calibration range (mMinValue -> 0, mMaxValue -> 65535)
    uint16_t fractional16() const;

    // Check if value changed this frame (beyond hysteresis threshold)
    bool hasChanged() const { return mChangedThisFrame; }

    // ========================================================================
    // Callback Registration
    // ========================================================================

    // Register callback with Potentiometer reference (can access all value formats)
    int onChange(fl::function<void(Potentiometer &)> callback);

    // Register callback with normalized float value (convenience overload)
    int onChange(fl::function<void(float)> callback);

    // Remove callback by ID
    void removeOnChange(int id) {
        mOnChangeCallbacks.remove(id);
        mOnChangeNormalizedCallbacks.remove(id);
    }

    // ========================================================================
    // Configuration
    // ========================================================================

    // Set hysteresis threshold in raw ADC units (e.g., 10 = must change by 10 ADC counts)
    void setHysteresis(uint16_t threshold) { mHysteresis = threshold; }

    // Set hysteresis as percentage of calibrated range (0.0-100.0)
    // Example: 1.0 = 1% of (mMaxValue - mMinValue)
    void setHysteresisPercent(float percent);

    // Get current hysteresis threshold in raw ADC units
    uint16_t getHysteresis() const { return mHysteresis; }

    // ========================================================================
    // Calibration (Range Mapping)
    // ========================================================================

    // Set the raw ADC range that maps to normalized [0.0, 1.0]
    // @param min: Raw ADC value that maps to 0.0 (default: 0)
    // @param max: Raw ADC value that maps to 1.0 (default: ADC max value)
    // Example: setRange(100, 900) maps 100->0.0, 900->1.0, values outside are clamped
    void setRange(uint16_t min, uint16_t max);

    // Get the minimum raw value (maps to 0.0 in normalized)
    uint16_t getRangeMin() const { return mMinValue; }

    // Get the maximum raw value (maps to 1.0 in normalized)
    uint16_t getRangeMax() const { return mMaxValue; }

    // Calibrate to current position as minimum (0.0)
    void calibrateMin() { mMinValue = mCurrentValue; }

    // Calibrate to current position as maximum (1.0)
    void calibrateMax() { mMaxValue = mCurrentValue; }

    // Reset calibration to full ADC range
    void resetCalibration() {
        mMinValue = 0;
        mMaxValue = getAdcMaxValue();
    }

    // ========================================================================
    // Test Helpers (for unit testing only)
    // ========================================================================

#ifdef FASTLED_UNIT_TEST
    // Inject a test value directly (bypasses analogRead)
    // Only available in unit test builds
    void injectTestValue(uint16_t value) {
        mCurrentValue = value;
        // Manually trigger change detection logic
        uint16_t diff = (value > mLastValue) ? (value - mLastValue) : (mLastValue - value);
        mChangedThisFrame = (diff >= mHysteresis);
        if (mChangedThisFrame) {
            mLastValue = value;
            mOnChangeCallbacks.invoke(*this);
            float normalized_value = normalized();
            mOnChangeNormalizedCallbacks.invoke(normalized_value);
        }
    }
#endif

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(Potentiometer *owner);
        ~Listener();
        void addToEngineEventsOnce();

        // Update on end frame (before next frame is drawn, matching Button behavior)
        void onEndFrame() override;

      private:
        Potentiometer *mOwner;
        bool added = false;
    };

  private:
    PotentiometerLowLevel mPot;
    Listener mListener;

    uint16_t mCurrentValue = 0;     // Current raw ADC value
    uint16_t mLastValue = 0;        // Last value that triggered callbacks
    uint16_t mHysteresis = 0;       // Hysteresis threshold in raw ADC units
    bool mChangedThisFrame = false; // True if changed beyond hysteresis this frame

    // Calibration range: raw ADC values that map to normalized [0.0, 1.0]
    uint16_t mMinValue = 0;         // Raw value that maps to 0.0 (default: 0)
    uint16_t mMaxValue = 0;         // Raw value that maps to 1.0 (default: ADC max)

    fl::function_list<void(Potentiometer &)> mOnChangeCallbacks;
    fl::function_list<void(float)> mOnChangeNormalizedCallbacks;

    // Get ADC resolution (10-bit = 1023, 12-bit = 4095)
    uint16_t getAdcMaxValue() const;

    // Calculate default hysteresis (1% of calibrated range)
    uint16_t calculateDefaultHysteresis() const;
};

} // namespace fl
