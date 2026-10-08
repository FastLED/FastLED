#pragma once

#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Pure virtual interface for FastLED operations
 *
 * This interface abstracts FastLED operations to enable dependency injection
 * and testing. It represents the core LED control operations that WLED and
 * other integrations need to perform.
 *
 * This interface allows:
 * - Real implementations that delegate to the global FastLED singleton
 * - Mock implementations for unit testing without hardware
 * - Custom implementations for specialized control scenarios
 *
 * @see FastLEDAdapter for the real implementation
 * @see MockFastLEDController for the test mock implementation
 */
class IFastLED {
public:
    virtual ~IFastLED() FL_NO_EXCEPT = default;

    // LED array access

    /**
     * @brief Get the LED array as a span
     * @return Span over the LED array (may be full array or current segment)
     */
    virtual fl::span<CRGB> getLEDs() FL_NO_EXCEPT = 0;

    /**
     * @brief Get the number of LEDs
     * @return Number of LEDs in the current context (full array or segment)
     */
    virtual size_t getNumLEDs() const FL_NO_EXCEPT = 0;

    // Output control

    /**
     * @brief Send the LED data to the strip
     * Uses the current brightness setting
     */
    virtual void show() FL_NO_EXCEPT = 0;

    /**
     * @brief Send the LED data to the strip with a specific brightness
     * @param brightness Brightness level (0-255)
     */
    virtual void show(u8 brightness) FL_NO_EXCEPT = 0;

    /**
     * @brief Clear all LEDs (set to black)
     * @param writeToStrip If true, immediately write to strip (call show())
     */
    virtual void clear(bool writeToStrip = false) FL_NO_EXCEPT = 0;

    // Brightness

    /**
     * @brief Set the global brightness
     * @param brightness Brightness level (0-255)
     */
    virtual void setBrightness(u8 brightness) FL_NO_EXCEPT = 0;

    /**
     * @brief Get the current global brightness
     * @return Brightness level (0-255)
     */
    virtual u8 getBrightness() const FL_NO_EXCEPT = 0;

    // Color correction

    /**
     * @brief Set color correction
     * @param correction Color correction RGB values
     */
    virtual void setCorrection(CRGB correction) FL_NO_EXCEPT = 0;

    /**
     * @brief Set color temperature
     * @param temperature Color temperature RGB values
     */
    virtual void setTemperature(CRGB temperature) FL_NO_EXCEPT = 0;

    // Timing

    /**
     * @brief Delay for a specified number of milliseconds
     * @param ms Delay duration in milliseconds
     */
    virtual void delay(unsigned long ms) FL_NO_EXCEPT = 0;

    /**
     * @brief Set the maximum refresh rate
     * @param fps Maximum frames per second (0 = no limit)
     */
    virtual void setMaxRefreshRate(u16 fps) FL_NO_EXCEPT = 0;

    /**
     * @brief Get the maximum refresh rate
     * @return Maximum frames per second (0 = no limit)
     */
    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT = 0;

    // Advanced features (segment support for WLED)

    /**
     * @brief Set a segment range for subsequent operations
     * @param start Starting LED index (inclusive)
     * @param end Ending LED index (exclusive)
     *
     * After calling this, getLEDs() and getNumLEDs() will operate on the
     * specified segment only.
     */
    virtual void setSegment(size_t start, size_t end) FL_NO_EXCEPT = 0;

    /**
     * @brief Clear the segment range (operate on full LED array)
     */
    virtual void clearSegment() FL_NO_EXCEPT = 0;
};

} // namespace fl
