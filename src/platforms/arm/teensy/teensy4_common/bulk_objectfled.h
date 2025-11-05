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
#include "fl/namespace.h"
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
/// auto& bulk = FastLED.addBulkLeds<WS2812B, OFLED>({
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
///     auto& bulk = FastLED.addBulkLeds<WS2812B, OFLED>({
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
/// auto& bulk1 = FastLED.addBulkLeds<WS2812B, OFLED>({
///     {2, strip1, 100, map1},
///     {5, strip2, 100, map2},
///     {9, strip3, 100, map3}
/// });
///
/// // SK6812 RGBW group (pins 10,11,12 - 200 LEDs each)
/// auto& bulk2 = FastLED.addBulkLeds<SK6812, OFLED>({
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
/// auto& bulk = FastLED.addBulkLeds<WS2812B, OFLED>();  // Start empty
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
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB, 1, ALL_LANES_MASK>()
        , mMaxLedCount(0)
        , mNeedsInit(true)
        , mHasCustomTiming(false) {

        // Validate clockless chipset at compile time
        static_assert(ChipsetTraits<Timing>::isClockless(),
            "BulkClockless<OFLED> only supports clockless chipsets. "
            "Detected SPI chipset (APA102, SK9822, etc.). "
            "Use a clockless chipset like WS2812B, SK6812, or WS2811.");

        // Initialize default settings from base class
        mDefaultSettings.correction = this->mSettings.correction;
        mDefaultSettings.temperature = this->mSettings.temperature;
        mDefaultSettings.ditherMode = this->mSettings.ditherMode;
        mDefaultSettings.rgbw = this->mSettings.rgbw;

        // Add all strips from initializer list
        for (const auto& config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// Default constructor (creates empty bulk controller)
    BulkClockless()
        : CPixelLEDController<RGB, 1, ALL_LANES_MASK>()
        , mMaxLedCount(0)
        , mNeedsInit(true)
        , mHasCustomTiming(false) {

        // Validate clockless chipset at compile time
        static_assert(ChipsetTraits<Timing>::isClockless(),
            "BulkClockless<OFLED> only supports clockless chipsets. "
            "Detected SPI chipset (APA102, SK9822, etc.). "
            "Use a clockless chipset like WS2812B, SK6812, or WS2811.");

        // Initialize default settings from base class
        mDefaultSettings.correction = this->mSettings.correction;
        mDefaultSettings.temperature = this->mSettings.temperature;
        mDefaultSettings.ditherMode = this->mSettings.ditherMode;
        mDefaultSettings.rgbw = this->mSettings.rgbw;
    }

    /// Destructor - cleanup resources
    ~BulkClockless() {
        mSubControllers.clear();
        mObjectFLED.reset();
    }

    /// Add a new LED strip to the bulk controller
    /// @param pin GPIO pin number for the strip
    /// @param buffer Pointer to CRGB buffer (user owns this memory)
    /// @param count Number of LEDs in the strip
    /// @param screenmap Optional ScreenMap for spatial positioning
    /// @return Pointer to sub-controller on success, nullptr on failure
    /// @note Strips can have different lengths - shorter strips are padded with black during DMA transposition
    BulkStrip* add(int pin, CRGB* buffer, int count,
                   ScreenMap screenmap = ScreenMap()) {
        // Validate max strips
        if (mSubControllers.size() >= MAX_STRIPS) {
            FL_WARN("BulkClockless<OFLED>: Maximum " << MAX_STRIPS
                    << " strips exceeded");
            return nullptr;
        }

        // Check duplicate pin
        if (mSubControllers.count(pin) > 0) {
            FL_WARN("BulkClockless<OFLED>: Pin " << pin << " already in use");
            return nullptr;
        }

        // Validate pin number (Teensy 4.x has 0-41 or 0-53 depending on model)
        if (pin < 0 || pin >= NUM_DIGITAL_PINS) {
            FL_WARN("BulkClockless<OFLED>: Invalid pin " << pin);
            return nullptr;
        }

        // Track maximum LED count across all strips (for DMA buffer allocation)
        if (count > mMaxLedCount) {
            mMaxLedCount = count;
        }

        // Create sub-controller with inherited settings
        BulkStrip sub(pin, buffer, count, screenmap);
        sub.settings = mDefaultSettings;

        // Insert into map
        mSubControllers[pin] = sub;

        // Notify engine (for WASM visualization)
        fl::EngineEvents::onStripAdded(this, count);
        fl::EngineEvents::onCanvasUiSet(this, screenmap);

        // Mark as needing re-initialization
        mNeedsInit = true;

        return &mSubControllers[pin];
    }

    /// Remove a strip by pin number
    /// @param pin GPIO pin number to remove
    /// @return true if strip was removed, false if not found
    bool remove(int pin) {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return false;
        }

        // Remove from map
        mSubControllers.erase(it);

        // Recalculate max LED count across remaining strips
        mMaxLedCount = 0;
        for (const auto& pair : mSubControllers) {
            if (pair.second.getCount() > mMaxLedCount) {
                mMaxLedCount = pair.second.getCount();
            }
        }

        // Mark as needing re-initialization
        mNeedsInit = true;

        return true;
    }

    /// Remove all strips
    /// @return Number of strips removed
    int removeAll() {
        int count = static_cast<int>(mSubControllers.size());
        mSubControllers.clear();
        mMaxLedCount = 0;
        mObjectFLED.reset();
        mNeedsInit = true;
        return count;
    }

    /// Get strip by pin number
    /// @param pin GPIO pin number
    /// @return Pointer to strip or nullptr if not found
    BulkStrip* get(int pin) {
        auto it = mSubControllers.find(pin);
        if (it != mSubControllers.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Get strip by pin number (const version)
    const BulkStrip* get(int pin) const {
        auto it = mSubControllers.find(pin);
        if (it != mSubControllers.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Get number of strips
    size_t size() const { return mSubControllers.size(); }

    /// Get number of strips (alias for size)
    size_t stripCount() const { return mSubControllers.size(); }

    /// Check if pin is in use
    bool has(int pin) const { return mSubControllers.count(pin) > 0; }

    /// Get all pin numbers
    fl::vector<int> getAllPins() const {
        fl::vector<int> pins;
        pins.reserve(mSubControllers.size());
        for (const auto& pair : mSubControllers) {
            pins.push_back(pair.first);
        }
        return pins;
    }

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

    /// Override chipset timing with custom values (advanced users)
    /// @param period_ns Total bit period in nanoseconds (T1 + T2 + T3)
    /// @param t0h_ns High time for '0' bit in nanoseconds
    /// @param t1h_ns High time for '1' bit in nanoseconds
    /// @param reset_us Reset/latch time in microseconds
    /// @return Reference to this controller for chaining
    /// @note This overrides the automatic timing from chipset traits
    BulkClockless& setCustomTiming(uint16_t period_ns, uint16_t t0h_ns,
                                    uint16_t t1h_ns, uint16_t reset_us = 300) {
        mCustomTiming.period_ns = period_ns;
        mCustomTiming.t0h_ns = t0h_ns;
        mCustomTiming.t1h_ns = t1h_ns;
        mCustomTiming.reset_us = reset_us;
        mHasCustomTiming = true;
        mNeedsInit = true;  // Force re-init with new timing
        return *this;
    }

  protected:
    /// Initialize peripheral (called before first show or after config change)
    void init() override {
        if (!mNeedsInit) return;
        rebuildObjectFLED();
    }

    /// Show LEDs (called by FastLED.show())
    void showPixels(PixelController<RGB, 1, ALL_LANES_MASK>& pixels) override {
        // Re-initialize if configuration changed
        if (mNeedsInit) {
            init();
        }

        if (!mObjectFLED || mSubControllers.size() == 0) {
            FL_WARN("BulkClockless<OFLED>: Not initialized");
            return;
        }

        // Render all strips to ObjectFLED's internal frameBuffer
        renderAllStrips();

        // Transmit via ObjectFLED (will acquire/release DMA)
        mObjectFLED->show();
    }

  private:
    /// Rebuild ObjectFLED instance with updated pin list
    void rebuildObjectFLED() {
        if (mSubControllers.size() == 0) {
            mObjectFLED.reset();
            mNeedsInit = false;
            return;
        }

        // Build pin list from current sub-controllers
        fl::vector<uint8_t> pinList;
        pinList.reserve(mSubControllers.size());
        for (const auto& pair : mSubControllers) {
            pinList.push_back(static_cast<uint8_t>(pair.first));
        }

        // Allocate based on max LED count (shorter strips will be padded during rendering)
        int totalLeds = mMaxLedCount * static_cast<int>(mSubControllers.size());

        // Determine if any strip uses RGBW
        bool hasRgbw = false;
        for (const auto& pair : mSubControllers) {
            if (pair.second.settings.rgbw.active()) {
                hasRgbw = true;
                break;
            }
        }

        // Create new ObjectFLED instance
        mObjectFLED.reset(new ObjectFLED(
            totalLeds,
            nullptr,  // ObjectFLED allocates frameBuffer internally
            hasRgbw ? CORDER_RGBW : CORDER_RGB,
            static_cast<uint8_t>(pinList.size()),
            pinList.data(),
            0  // No serpentine
        ));

        // Get timing (custom or from chipset traits)
        uint16_t period_ns, t0h_ns, t1h_ns, reset_us;

        if (mHasCustomTiming) {
            // Use custom timing
            period_ns = mCustomTiming.period_ns;
            t0h_ns = mCustomTiming.t0h_ns;
            t1h_ns = mCustomTiming.t1h_ns;
            reset_us = mCustomTiming.reset_us;
        } else {
            // Get timing from chipset traits
            ChipsetTiming timing = ChipsetTraits<Timing>::runtime_timing();

            // Convert FastLED timing (T1, T2, T3) to ObjectFLED format (period, t0h, t1h)
            // FastLED: T1=high for '1', T2=high for '0', T3=total period
            // ObjectFLED: period=total, t0h=high for '0', t1h=high for '1'
            period_ns = static_cast<uint16_t>(timing.T1 + timing.T2 + timing.T3);
            t0h_ns = static_cast<uint16_t>(timing.T2);  // High time for '0'
            t1h_ns = static_cast<uint16_t>(timing.T1);  // High time for '1'
            reset_us = static_cast<uint16_t>(timing.RESET);
        }

        // Validate timing
        if (period_ns < 800 || period_ns > 5000) {
            FL_WARN("BulkClockless<OFLED>: Unusual period: " << period_ns << "ns");
        }

        // Initialize with timing
        mObjectFLED->begin(period_ns, t0h_ns, t1h_ns, reset_us);

        mNeedsInit = false;
    }

    /// Render all strips to ObjectFLED's frameBuffer
    void renderAllStrips() {
        if (!mObjectFLED) return;

        // Get ObjectFLED's internal frameBuffer
        uint8_t* frameBuffer = mObjectFLED->frameBufferLocal;
        if (!frameBuffer) return;

        // Determine bytes per LED (3 for RGB, 4 for RGBW)
        bool hasRgbw = false;
        for (const auto& pair : mSubControllers) {
            if (pair.second.settings.rgbw.active()) {
                hasRgbw = true;
                break;
            }
        }
        int bytesPerLed = hasRgbw ? 4 : 3;

        // Render each strip to its section of frameBuffer
        // Each strip gets mMaxLedCount bytes, shorter strips are padded with black
        int pinIndex = 0;
        for (auto& pair : mSubControllers) {
            BulkStrip& strip = pair.second;
            uint8_t* dest = frameBuffer + (pinIndex * mMaxLedCount * bytesPerLed);
            renderStrip(pinIndex, strip, dest, bytesPerLed);
            pinIndex++;
        }
    }

    /// Render a single strip using PixelIterator, padding shorter strips with black
    /// @param pinIndex Index of this strip (for offset calculation)
    /// @param strip BulkStrip to render
    /// @param dest Destination buffer (section of frameBuffer)
    /// @param bytesPerLed Bytes per LED (3 for RGB, 4 for RGBW)
    void renderStrip(int pinIndex, BulkStrip& strip, uint8_t* dest, int bytesPerLed) {
        // Compute color adjustment for this strip (brightness + correction + temperature)
        ColorAdjustment adj = BulkClocklessHelper::computeAdjustment(
            this->getBrightness(), strip.settings);

        // Create pixel controller with per-strip settings
        PixelController<RGB, 1, ALL_LANES_MASK> pixels(
            strip.getBuffer(),
            strip.getCount(),
            adj,
            strip.settings.ditherMode,
            strip.settings.rgbw
        );

        // Check if RGBW mode is active
        bool isRgbw = strip.settings.rgbw.active();

        int pixelsRendered = 0;

        // Render actual pixels from the strip buffer
        if (isRgbw) {
            // RGBW mode - output 4 bytes per pixel
            while (pixels.has(1)) {
                uint8_t r, g, b, w;
                pixels.loadAndScaleRGBW(&r, &g, &b, &w);
                *dest++ = r;
                *dest++ = g;
                *dest++ = b;
                *dest++ = w;
                pixels.advanceData();
                pixels.stepDithering();
                pixelsRendered++;
            }
        } else {
            // RGB mode - output 3 bytes per pixel
            while (pixels.has(1)) {
                *dest++ = pixels.loadAndScale0();  // R
                *dest++ = pixels.loadAndScale1();  // G
                *dest++ = pixels.loadAndScale2();  // B
                pixels.advanceData();
                pixels.stepDithering();
                pixelsRendered++;
            }
        }

        // Pad remaining space with black (for strips shorter than mMaxLedCount)
        // This avoids needing a full RectangularDrawBuffer and saves memory
        int pixelsToPad = mMaxLedCount - pixelsRendered;
        for (int i = 0; i < pixelsToPad; i++) {
            for (int j = 0; j < bytesPerLed; j++) {
                *dest++ = 0;  // Black padding
            }
        }
    }

    // Member variables
    fl_map<int, BulkStrip> mSubControllers;  ///< Map of pin → BulkStrip
    fl::unique_ptr<ObjectFLED> mObjectFLED;  ///< ObjectFLED driver instance
    int mMaxLedCount;                        ///< Maximum LEDs across all strips (for DMA buffer sizing)
    bool mNeedsInit;                         ///< Needs initialization flag
    PixelControllerSettings mDefaultSettings; ///< Default settings for new strips

    /// Custom timing parameters (used when mHasCustomTiming is true)
    struct CustomTimingParams {
        uint16_t period_ns;   ///< Total bit period in nanoseconds
        uint16_t t0h_ns;      ///< High time for '0' bit
        uint16_t t1h_ns;      ///< High time for '1' bit
        uint16_t reset_us;    ///< Reset/latch time in microseconds
    };
    CustomTimingParams mCustomTiming;  ///< Custom timing parameters
    bool mHasCustomTiming;             ///< Whether custom timing is set
};

} // namespace fl

#endif // __IMXRT1062__
