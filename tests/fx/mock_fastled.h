#pragma once

#include "fl/fx/wled.h"
#include "fl/stl/vector.h"

namespace fl {

/**
 * @brief Mock FastLED controller for unit testing
 *
 * This mock implementation of IFastLED provides a test double that:
 * - Maintains its own in-memory LED array
 * - Tracks all method calls for verification
 * - Records state changes for assertion
 * - Supports segment operations
 *
 * The mock allows testing WLED and other integrations without requiring
 * actual hardware or the global FastLED singleton.
 *
 * Example usage:
 * @code
 * auto mock = fl::make_shared<MockFastLED>(50);
 * WLEDClient wled(mock);
 *
 * wled.setBrightness(128);
 * wled.update();
 *
 * REQUIRE(mock->getShowCallCount() == 1);
 * REQUIRE(mock->getLastBrightness() == 128);
 * @endcode
 *
 * @see IFastLED for the interface definition
 */
class MockFastLED : public IFastLED {
public:
    /**
     * @brief Construct mock controller with specified LED count
     * @param numLeds Number of LEDs to simulate
     */
    explicit MockFastLED(size_t numLeds)
        : mLeds(numLeds, CRGB::Black)
        , mNumLeds(numLeds)
        , mBrightness(255)
        , mCorrection(CRGB(255, 255, 255))
        , mTemperature(CRGB(255, 255, 255))
        , mMaxRefreshRate(0)
        , mTotalDelayMs(0)
        , mSegmentStart(0)
        , mSegmentEnd(numLeds)
        , mHasSegment(false)
        , mShowCallCount(0)
        , mClearCallCount(0)
    {
    }

    /**
     * @brief Destructor
     */
    ~MockFastLED() override = default;

    // IFastLED interface implementation

    fl::span<CRGB> getLEDs() override {
        if (mHasSegment) {
            return fl::span<CRGB>(mLeds.data() + mSegmentStart, mSegmentEnd - mSegmentStart);
        }
        return fl::span<CRGB>(mLeds.data(), mNumLeds);
    }

    size_t getNumLEDs() const override {
        if (mHasSegment) {
            return mSegmentEnd - mSegmentStart;
        }
        return mNumLeds;
    }

    void show() override {
        mShowCallCount++;
    }

    void show(uint8_t brightness) override {
        setBrightness(brightness);
        mShowCallCount++;
    }

    void clear(bool writeToStrip = false) override {
        mClearCallCount++;

        if (mHasSegment) {
            // Clear only the segment
            for (size_t i = mSegmentStart; i < mSegmentEnd; i++) {
                mLeds[i] = CRGB::Black;
            }
        } else {
            // Clear all LEDs
            for (size_t i = 0; i < mNumLeds; i++) {
                mLeds[i] = CRGB::Black;
            }
        }

        if (writeToStrip) {
            mShowCallCount++;
        }
    }

    void setBrightness(uint8_t brightness) override {
        mBrightness = brightness;
        mBrightnessHistory.push_back(brightness);
    }

    uint8_t getBrightness() const override {
        return mBrightness;
    }

    void setCorrection(CRGB correction) override {
        mCorrection = correction;
    }

    void setTemperature(CRGB temperature) override {
        mTemperature = temperature;
    }

    void delay(unsigned long ms) override {
        mTotalDelayMs += ms;
    }

    void setMaxRefreshRate(uint16_t fps) override {
        mMaxRefreshRate = fps;
    }

    uint16_t getMaxRefreshRate() const override {
        return mMaxRefreshRate;
    }

    void setSegment(size_t start, size_t end) override {
        // Validate bounds
        if (start >= mNumLeds) {
            start = mNumLeds > 0 ? mNumLeds - 1 : 0;
        }
        if (end > mNumLeds) {
            end = mNumLeds;
        }
        if (end <= start) {
            end = start + 1;
            if (end > mNumLeds) {
                end = mNumLeds;
                start = end > 0 ? end - 1 : 0;
            }
        }

        mSegmentStart = start;
        mSegmentEnd = end;
        mHasSegment = true;
    }

    void clearSegment() override {
        mSegmentStart = 0;
        mSegmentEnd = mNumLeds;
        mHasSegment = false;
    }

    // Test verification methods

    /**
     * @brief Get the number of times show() was called
     * @return Call count
     */
    int getShowCallCount() const { return mShowCallCount; }

    /**
     * @brief Get the number of times clear() was called
     * @return Call count
     */
    int getClearCallCount() const { return mClearCallCount; }

    /**
     * @brief Get the last brightness value set
     * @return Last brightness (0-255)
     */
    uint8_t getLastBrightness() const { return mBrightness; }

    /**
     * @brief Get the brightness history
     * @return Vector of all brightness values set
     */
    const fl::vector<uint8_t>& getBrightnessHistory() const { return mBrightnessHistory; }

    /**
     * @brief Get the last color correction set
     * @return Last correction value
     */
    CRGB getLastCorrection() const { return mCorrection; }

    /**
     * @brief Get the last color temperature set
     * @return Last temperature value
     */
    CRGB getLastTemperature() const { return mTemperature; }

    /**
     * @brief Get the last max refresh rate set
     * @return Max refresh rate in FPS
     */
    uint16_t getLastMaxRefreshRate() const { return mMaxRefreshRate; }

    /**
     * @brief Get total delay time accumulated
     * @return Total milliseconds delayed
     */
    unsigned long getTotalDelayMs() const { return mTotalDelayMs; }

    /**
     * @brief Check if a segment is currently active
     * @return True if segment is set
     */
    bool hasSegment() const { return mHasSegment; }

    /**
     * @brief Get the current segment start index
     * @return Start index (0 if no segment)
     */
    size_t getSegmentStart() const { return mSegmentStart; }

    /**
     * @brief Get the current segment end index
     * @return End index (mNumLeds if no segment)
     */
    size_t getSegmentEnd() const { return mSegmentEnd; }

    /**
     * @brief Reset all counters and state
     *
     * Resets:
     * - Call counters to 0
     * - Brightness to 255
     * - History vectors to empty
     * - Corrections and temperature to default
     * - LEDs to black
     * - Segments to cleared
     */
    void reset() {
        // Reset counters
        mShowCallCount = 0;
        mClearCallCount = 0;

        // Reset state
        mBrightness = 255;
        mBrightnessHistory.clear();
        mCorrection = CRGB(255, 255, 255);
        mTemperature = CRGB(255, 255, 255);
        mMaxRefreshRate = 0;
        mTotalDelayMs = 0;

        // Reset segment
        mSegmentStart = 0;
        mSegmentEnd = mNumLeds;
        mHasSegment = false;

        // Clear LEDs
        for (size_t i = 0; i < mNumLeds; i++) {
            mLeds[i] = CRGB::Black;
        }
    }

private:
    // LED state
    fl::vector<CRGB> mLeds;     // Simulated LED array
    size_t mNumLeds;            // Total number of LEDs

    // Brightness tracking
    uint8_t mBrightness;                    // Current brightness
    fl::vector<uint8_t> mBrightnessHistory; // History of all brightness values

    // Color correction/temperature
    CRGB mCorrection;           // Current color correction
    CRGB mTemperature;          // Current color temperature

    // Timing
    uint16_t mMaxRefreshRate;   // Max FPS (0 = no limit)
    uint32_t mTotalDelayMs; // Accumulated delay time

    // Segment state
    size_t mSegmentStart;       // Start of current segment
    size_t mSegmentEnd;         // End of current segment
    bool mHasSegment;           // True if a segment is active

    // Call counters
    uint32_t mShowCallCount;         // Number of show() calls
    uint32_t mClearCallCount;        // Number of clear() calls
};

} // namespace fl
