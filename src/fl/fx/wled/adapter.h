#pragma once

#include "fl/fx/wled/ifastled.h"
#include "fl/stl/noexcept.h"

namespace fl {

/**
 * @brief Real FastLED implementation adapter
 *
 * This class adapts the global FastLED singleton to the IFastLED interface.
 * It provides a thin wrapper that delegates all operations to the global
 * FastLED object while optionally managing segment control for specific
 * LED controller indices.
 *
 * This adapter allows WLED and other integrations to work with FastLED through
 * a standard interface while maintaining full compatibility with the existing
 * FastLED API.
 *
 * Example usage:
 * @code
 * CRGB leds[100];
 * FastLED.addLeds<WS2812, 5>(leds, 100);
 *
 * auto controller = fl::make_shared<FastLEDAdapter>();
 * controller->setBrightness(128);
 * controller->show();
 * @endcode
 *
 * @see IFastLED for the interface definition
 * @see createFastLEDController() for helper functions to create adapters
 */
class FastLEDAdapter : public IFastLED {
public:
    /**
     * @brief Construct adapter wrapping the global FastLED object
     * @param controllerIndex Index of the LED controller to use (default: 0)
     *                        Use 0 for the first controller, 1 for second, etc.
     */
    explicit FastLEDAdapter(u8 controllerIndex = 0) FL_NO_EXCEPT;

    /**
     * @brief Destructor
     */
    ~FastLEDAdapter() FL_NO_EXCEPT override = default;

    // IFastLED interface implementation

    fl::span<CRGB> getLEDs() FL_NO_EXCEPT override;
    size_t getNumLEDs() const FL_NO_EXCEPT override;

    void show() FL_NO_EXCEPT override;
    void show(u8 brightness) FL_NO_EXCEPT override;
    void clear(bool writeToStrip = false) FL_NO_EXCEPT override;

    void setBrightness(u8 brightness) FL_NO_EXCEPT override;
    u8 getBrightness() const FL_NO_EXCEPT override;

    void setCorrection(CRGB correction) FL_NO_EXCEPT override;
    void setTemperature(CRGB temperature) FL_NO_EXCEPT override;

    void delay(unsigned long ms) FL_NO_EXCEPT override;
    void setMaxRefreshRate(u16 fps) FL_NO_EXCEPT override;
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override;

    void setSegment(size_t start, size_t end) FL_NO_EXCEPT override;
    void clearSegment() FL_NO_EXCEPT override;

private:
    u8 mControllerIndex; // Index of the LED controller in FastLED
    size_t mSegmentStart;     // Start of current segment (0 if no segment)
    size_t mSegmentEnd;       // End of current segment (numLeds if no segment)
    bool mHasSegment;         // True if a segment is active
};

/**
 * @brief Create a FastLED controller adapter
 * @param controllerIndex Index of the LED controller to use (default: 0)
 * @return Shared pointer to the adapter
 *
 * Example:
 * @code
 * CRGB leds[100];
 * FastLED.addLeds<WS2812, 5>(leds, 100);
 * auto controller = createFastLEDController();  // Uses controller index 0
 * @endcode
 */
fl::shared_ptr<IFastLED> createFastLEDController(u8 controllerIndex = 0) FL_NO_EXCEPT;

} // namespace fl
