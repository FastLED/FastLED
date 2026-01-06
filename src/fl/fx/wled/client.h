#pragma once

#include "fl/fx/wled/ifastled.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"


namespace fl {

/**
 * @brief WLED Client for controlling LEDs through FastLED interface
 *
 * Provides a simplified interface for controlling LEDs with WLED-style
 * operations (brightness, on/off, clear). Uses dependency injection to
 * allow both real FastLED control and mock implementations for testing.
 *
 * Example usage:
 *   // Real usage
 *   auto controller = createFastLEDController(leds, NUM_LEDS);
 *   WLEDClient client(controller);
 *   client.setBrightness(128);
 *   client.setOn(true);
 *
 *   // Test usage
 *   auto mock = fl::make_shared<MockFastLEDController>(100);
 *   WLEDClient client(mock);
 *   client.setBrightness(128);
 *   REQUIRE(mock->getLastBrightness() == 128);
 */
class WLEDClient {
public:
    /**
     * @brief Construct WLEDClient with FastLED controller
     * @param controller Shared pointer to IFastLED implementation
     */
    explicit WLEDClient(fl::shared_ptr<IFastLED> controller);

    /**
     * @brief Set brightness level
     * @param brightness Brightness value (0-255)
     *
     * Updates the brightness and applies to controller if client is on.
     */
    void setBrightness(uint8_t brightness);

    /**
     * @brief Get current brightness level
     * @return Brightness value (0-255)
     */
    uint8_t getBrightness() const { return mBrightness; }

    /**
     * @brief Set on/off state
     * @param on True to turn on, false to turn off
     *
     * When turning on, applies current brightness to controller.
     * When turning off, sets controller brightness to 0 but preserves internal brightness.
     */
    void setOn(bool on);

    /**
     * @brief Get on/off state
     * @return True if on, false if off
     */
    bool getOn() const { return mOn; }

    /**
     * @brief Clear all LEDs
     * @param writeToStrip If true, immediately writes to strip (calls show)
     *
     * Sets all LEDs to black/off. If writeToStrip is true, also calls
     * controller->show() to update the physical strip.
     */
    void clear(bool writeToStrip = false);

    /**
     * @brief Update physical LED strip
     *
     * Calls controller->show() to write LED data to the physical strip.
     * Typically called after making changes to LED colors.
     */
    void update();

    /**
     * @brief Get access to LED array
     * @return Span view of LED array
     *
     * Allows direct manipulation of LED colors through the controller.
     */
    fl::span<CRGB> getLEDs();

    /**
     * @brief Get number of LEDs
     * @return LED count
     */
    size_t getNumLEDs() const;

    /**
     * @brief Set a segment range for subsequent operations
     * @param start Starting LED index (inclusive)
     * @param end Ending LED index (exclusive)
     *
     * After calling this, operations will affect only the specified segment.
     * Delegates to controller's setSegment method.
     */
    void setSegment(size_t start, size_t end);

    /**
     * @brief Clear the segment range (operate on full LED array)
     *
     * Restores operations to affect the entire LED array.
     * Delegates to controller's clearSegment method.
     */
    void clearSegment();

    /**
     * @brief Set color correction
     * @param correction Color correction RGB values
     *
     * Adjusts color output to compensate for LED characteristics.
     * Delegates to controller's setCorrection method.
     */
    void setCorrection(CRGB correction);

    /**
     * @brief Set color temperature
     * @param temperature Color temperature RGB values
     *
     * Adjusts color output for perceived color temperature.
     * Delegates to controller's setTemperature method.
     */
    void setTemperature(CRGB temperature);

    /**
     * @brief Set maximum refresh rate
     * @param fps Maximum frames per second (0 = no limit)
     *
     * Limits how often the LED strip can be updated.
     * Delegates to controller's setMaxRefreshRate method.
     */
    void setMaxRefreshRate(uint16_t fps);

    /**
     * @brief Get maximum refresh rate
     * @return Maximum frames per second (0 = no limit)
     */
    uint16_t getMaxRefreshRate() const;

private:
    fl::shared_ptr<IFastLED> mController;  // FastLED controller interface
    uint8_t mBrightness;                    // Current brightness (0-255)
    bool mOn;                                // On/off state
};

} // namespace fl
