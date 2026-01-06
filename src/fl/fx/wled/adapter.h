#pragma once

#include "fl/fx/wled/ifastled.h"
#include "fl/stl/shared_ptr.h"

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
    explicit FastLEDAdapter(uint8_t controllerIndex = 0);

    /**
     * @brief Destructor
     */
    ~FastLEDAdapter() override = default;

    // IFastLED interface implementation

    fl::span<CRGB> getLEDs() override;
    size_t getNumLEDs() const override;

    void show() override;
    void show(uint8_t brightness) override;
    void clear(bool writeToStrip = false) override;

    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;

    void setCorrection(CRGB correction) override;
    void setTemperature(CRGB temperature) override;

    void delay(unsigned long ms) override;
    void setMaxRefreshRate(uint16_t fps) override;
    uint16_t getMaxRefreshRate() const override;

    void setSegment(size_t start, size_t end) override;
    void clearSegment() override;

private:
    uint8_t mControllerIndex; // Index of the LED controller in FastLED
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
fl::shared_ptr<IFastLED> createFastLEDController(uint8_t controllerIndex = 0);

} // namespace fl
