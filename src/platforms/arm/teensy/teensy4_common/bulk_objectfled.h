#pragma once

/// @file bulk_objectfled.h
/// @brief OFLED peripheral specialization for BulkClockless controller
///
/// Provides multi-strip LED control using Teensy 4.x DMA hardware with
/// automatic timing configuration based on chipset type.
/// OFLED is the FastLED peripheral tag for ObjectFLED hardware.

// Platform guard - ObjectFLED on Teensy 4.0/4.1 only
#if defined(__IMXRT1062__)

#include "fl/clockless/base.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/map.h"
#include "fl/unique_ptr.h"
#include "third_party/object_fled/src/ObjectFLED.h"
#include "third_party/object_fled/src/ObjectFLEDDmaManager.h"

namespace fl {

/// BulkClockless specialization for OFLED peripheral (Teensy 4.x)
///
/// Provides multi-strip LED control using Teensy 4.x DMA hardware with
/// automatic timing configuration based on chipset type.
/// Uses ObjectFLED implementation under the hood.
///
/// **HARDWARE CAPABILITIES:**
/// - Up to 42 parallel strips per instance (Teensy 4.1)
/// - Up to 16 parallel strips per instance (Teensy 4.0)
/// - Supports multiple instances with different chipsets
/// - Clockless chipsets only (WS2812, SK6812, WS2811, etc.)
/// - DMA-driven bit transposition for high performance
/// - Per-strip color correction, temperature, and dither control
/// - RGBW support (SK6812 RGBW, etc.)
///
/// **KEY FEATURES:**
/// 1. ✅ **Mixed-length strips supported** - Shorter strips are automatically padded with black
///    during DMA transposition (no extra frame buffer needed!)
/// 2. Multiple instances transmit SERIALLY, not in parallel
///    (shared DMA resources via ObjectFLEDDmaManager singleton)
/// 3. Total time = sum of all instance transmission times
/// 4. Only the LAST instance returns asynchronously
///
/// **CONCURRENCY MODEL:**
/// Multiple BulkClockless<CHIPSET, OFLED> instances share DMA hardware.
/// Calls to show() serialize automatically - each waits for prior transmission
/// to complete before starting.
///
/// **Example with mixed-length strips:**
/// @code
/// CRGB strip1[100];  // Short strip
/// CRGB strip2[300];  // Long strip
/// CRGB strip3[150];  // Medium strip
///
/// auto& bulk = FastLED.addClocklessLeds<WS2812B, fl::GRB, OFLED>({
///     {2, strip1, 100, ScreenMap()},  // 100 LEDs
///     {5, strip2, 300, ScreenMap()},  // 300 LEDs (longest)
///     {9, strip3, 150, ScreenMap()}   // 150 LEDs
/// });
/// // DMA buffer sized for 300 LEDs × 3 strips
/// // strip1 and strip3 padded with black automatically
/// @endcode
///
/// Example timing (3 instances):
/// - Instance 1: 100 LEDs × 8 strips = 3ms
/// - Instance 2: 200 LEDs × 4 strips = 6ms  (waits 3ms first)
/// - Instance 3:  50 LEDs × 6 strips = 1.5ms (waits 9ms first)
/// Total: 13.5ms (serial execution, last instance async)
///
/// **SUPPORTED CHIPSETS:**
/// - WS2812, WS2812B (standard RGB LEDs, 800kHz)
/// - SK6812, SK6812RGBW (RGB + white channel LEDs, 800kHz)
/// - WS2811 (older RGB LEDs, 400kHz)
/// - WS2813, WS2815 (RGB with backup data line)
/// - NOT SUPPORTED: APA102, SK9822, LPD8806 (SPI-based - require different driver)
///
/// **PERFORMANCE CHARACTERISTICS:**
/// - 100 LEDs per strip: ~30µs per LED = 3ms total transmission time
/// - 200 LEDs per strip: ~6ms total transmission time
/// - CPU overhead: Minimal during transmission (DMA-driven)
/// - Frame rate: Limited only by LED count and transmission time
/// - Typical 60fps: 16.67ms available, can drive 500+ LEDs per instance easily
///
/// **BASIC USAGE:**
/// @code
/// #include <FastLED.h>
///
/// #define NUM_LEDS 100
/// CRGB strip1[NUM_LEDS];
/// CRGB strip2[NUM_LEDS];
/// CRGB strip3[NUM_LEDS];
///
/// void setup() {
///     // Create bulk controller with WS2812 timing (automatic)
///     auto& bulk = FastLED.addClocklessLeds<WS2812B, fl::GRB, OFLED>({
///         {2, strip1, NUM_LEDS, ScreenMap()},
///         {5, strip2, NUM_LEDS, ScreenMap()},
///         {9, strip3, NUM_LEDS, ScreenMap()}
///     });
///
///     // Optional: Set per-strip color correction
///     bulk.get(2)->setCorrection(TypicalLEDStrip);
///     bulk.get(5)->setCorrection(TypicalSMD5050);
/// }
///
/// void loop() {
///     fill_rainbow(strip1, NUM_LEDS, 0, 255 / NUM_LEDS);
///     FastLED.show();  // DMA transmission happens automatically
/// }
/// @endcode
///
/// **ADVANCED USAGE - MULTIPLE CHIPSETS:**
/// @code
/// // WS2812 group (pins 2,5,9 - 100 LEDs each)
/// auto& bulk1 = FastLED.addClocklessLeds<WS2812B, fl::GRB, OFLED>({
///     {2, strip1, 100, map1},
///     {5, strip2, 100, map2},
///     {9, strip3, 100, map3}
/// });
///
/// // SK6812 RGBW group (pins 10,11,12 - 200 LEDs each)
/// auto& bulk2 = FastLED.addClocklessLeds<SK6812, fl::GRB, OFLED>({
///     {10, strip4, 200, map4},
///     {11, strip5, 200, map5},
///     {12, strip6, 200, map6}
/// });
/// bulk2.setRgbw(RgbwDefault());  // Enable RGBW mode with automatic white extraction
///
/// FastLED.show();  // Both groups transmit serially (bulk1 then bulk2)
/// @endcode
///
/// **DYNAMIC ADD/REMOVE:**
/// @code
/// auto& bulk = FastLED.addClocklessLeds<WS2812B, fl::GRB, OFLED>();  // Start empty
///
/// // Add strips dynamically
/// bulk.add(2, strip1, 100, ScreenMap());
/// bulk.add(5, strip2, 100, ScreenMap());
///
/// // Remove a strip
/// bulk.remove(2);
///
/// // Add it back
/// bulk.add(2, strip1, 100, ScreenMap());
/// @endcode
///
/// **CREDIT:**
/// Built on ObjectFLED by Kurt Funderburg, inspired by OctoWS2811 architecture.
/// Adapted for FastLED BulkClockless API with per-strip settings and RGBW support.
template <template <int, EOrder> class CHIPSET>
class BulkClockless<CHIPSET, OFLED>
    : public CPixelLEDController<RGB, 1, ALL_LANES_MASK> {
  private:
    // Extract timing struct from chipset template class
    using Timing = typename TimingHelper<CHIPSET>::VALUE;

  public:
    /// Maximum number of strips supported by ObjectFLED
    static constexpr int MAX_STRIPS = 42;  // Teensy 4.1 max (all GPIOs)

    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips);

    /// Default constructor (creates empty bulk controller)
    BulkClockless();

    /// Destructor - cleanup resources
    ~BulkClockless();

    /// Add a new LED strip to the bulk controller
    /// @param pin GPIO pin number for the strip
    /// @param buffer Pointer to CRGB buffer (user owns this memory)
    /// @param count Number of LEDs in the strip
    /// @param screenmap Optional ScreenMap for spatial positioning
    /// @return Pointer to sub-controller on success, nullptr on failure
    /// @note Strips can have different lengths - shorter strips are padded with black during DMA transposition
    BulkStrip* add(int pin, CRGB* buffer, int count,
                   ScreenMap screenmap = ScreenMap());

    /// Remove a strip by pin number
    /// @param pin GPIO pin number to remove
    /// @return true if strip was removed, false if not found
    bool remove(int pin);

    /// Remove all strips
    /// @return Number of strips removed
    int removeAll();

    /// Get strip by pin number
    /// @param pin GPIO pin number
    /// @return Pointer to strip or nullptr if not found
    BulkStrip* get(int pin);

    /// Get strip by pin number (const version)
    const BulkStrip* get(int pin) const;

    /// Get number of strips
    size_t size() const { return mSubControllers.size(); }

    /// Get number of strips (alias for size)
    size_t stripCount() const { return mSubControllers.size(); }

    /// Check if pin is in use
    bool has(int pin) const { return mSubControllers.count(pin) > 0; }

    /// Get all pin numbers
    fl::vector<int> getAllPins() const;

    /// Set global color correction (applies to all new sub-controllers)
    /// @param correction Color correction to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setCorrection(const ColorCorrection& correction) {
        mDefaultSettings.correction = correction;
        this->mSettings.correction = correction;
        return *this;
    }

    /// Set global color temperature (applies to all new sub-controllers)
    /// @param temperature Color temperature to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setTemperature(const ColorTemperature& temperature) {
        mDefaultSettings.temperature = temperature;
        this->mSettings.temperature = temperature;
        return *this;
    }

    /// Set global dither mode (applies to all new sub-controllers)
    /// @param ditherMode Dither mode to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setDither(uint8_t ditherMode = BINARY_DITHER) {
        mDefaultSettings.ditherMode = ditherMode;
        this->mSettings.ditherMode = ditherMode;
        return *this;
    }

    /// Set global RGBW mode (applies to all new sub-controllers)
    /// @param rgbw RGBW configuration to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setRgbw(const Rgbw& rgbw) {
        mDefaultSettings.rgbw = rgbw;
        this->mSettings.rgbw = rgbw;
        return *this;
    }

  protected:
    /// Initialize peripheral (called before first show or after config change)
    void init() override;

    /// Show LEDs (called by FastLED.show())
    void showPixels(PixelController<RGB, 1, ALL_LANES_MASK>& pixels) override;

  private:
    /// Rebuild ObjectFLED instance with updated pin list
    void rebuildObjectFLED();

    /// Render all strips to ObjectFLED's frameBuffer
    void renderAllStrips();

    /// Render a single strip using PixelIterator, padding shorter strips with black
    /// @param pinIndex Index of this strip (for offset calculation)
    /// @param strip BulkStrip to render
    /// @param dest Destination buffer (section of frameBuffer)
    /// @param bytesPerLed Bytes per LED (3 for RGB, 4 for RGBW)
    void renderStrip(int pinIndex, BulkStrip& strip, uint8_t* dest, int bytesPerLed);

    // Member variables
    fl_map<int, BulkStrip> mSubControllers;  ///< Map of pin → BulkStrip
    fl::unique_ptr<ObjectFLED> mObjectFLED;  ///< ObjectFLED driver instance
    int mMaxLedCount;                        ///< Maximum LEDs across all strips (for DMA buffer sizing)
    bool mNeedsInit;                         ///< Needs initialization flag
    PixelControllerSettings mDefaultSettings; ///< Default settings for new strips

};

} // namespace fl

#endif // __IMXRT1062__
