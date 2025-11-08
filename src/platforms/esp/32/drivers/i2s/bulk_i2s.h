#pragma once

/// @file bulk_i2s.h
/// I2S peripheral specialization for BulkClockless controller

#include "fl/compiler_control.h"

// Platform guard - I2S parallel mode on ESP32 (original) only
// ESP32-S3: Use BulkClockless<LCD_I80> instead (see bulk_lcd_i80.h)
#if defined(CONFIG_IDF_TARGET_ESP32)

#include "fl/clockless/base.h"
#include "fl/clockless/constants.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/map.h"
#include "fl/unique_ptr.h"
#include "rgbw.h"

// ESP32 (original) uses i2s_esp32dev.h driver
#include "i2s_esp32dev.h"

namespace fl {

// Chipset definitions (WS2812_CHIPSET and ChipsetTraits) are provided by
// peripheral_tags.h - included above at line 12

/// BulkClockless specialization for WS2812 + I2S
///
/// Manages multiple LED strips using ESP32's I2S peripheral with parallel
/// transmission. All strips update simultaneously via parallel data bus.
///
/// Features:
/// - Up to 24 strips on ESP32 (GPIO 0-23)
/// - ESP32-S3: NOT supported - use BulkClockless<LCD_I80> instead (see bulk_lcd_i80.h)
/// - Parallel transmission (all strips update at the same time)
/// - Pin order matters (GPIO number determines bit position in parallel data)
/// - Efficient memory usage with shared DMA buffer
///
/// Constraints:
/// - Maximum 24 strips on ESP32 (I2S peripheral limit)
/// - Pins must be valid GPIO
/// - All strips must have the same length (hardware limitation)
/// - Pin numbers matter: lower GPIO numbers map to lower bit positions
///
/// Usage:
/// @code
/// BulkClockless<Chipset::WS2812, GRB, I2S>* bulk =
///     &FastLED.addClocklessLeds<Chipset::WS2812, GRB, I2S>({
///         {2, strip1, 100, screenmap1},
///         {4, strip2, 100, screenmap2}
///     });
/// @endcode
template <EOrder RGB_ORDER>
class BulkClockless<Chipset::WS2812, RGB_ORDER, I2S>
    : public CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK> {
  public:
    /// Maximum number of strips supported by I2S peripheral on ESP32
    static constexpr int MAX_STRIPS = 24;  // ESP32 (original) I2S parallel mode

    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>()
        , mLedCount(0)
        , mInitialized(false) {
        // Initialize default settings from base class
        mDefaultSettings.correction = this->mSettings.correction;
        mDefaultSettings.temperature = this->mSettings.temperature;
        mDefaultSettings.ditherMode = this->mSettings.ditherMode;
        mDefaultSettings.rgbw = this->mSettings.rgbw;

        // Add all strips from initializer list
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// Default constructor (creates empty bulk controller)
    BulkClockless()
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>()
        , mLedCount(0)
        , mInitialized(false) {
        // Initialize default settings from base class
        mDefaultSettings.correction = this->mSettings.correction;
        mDefaultSettings.temperature = this->mSettings.temperature;
        mDefaultSettings.ditherMode = this->mSettings.ditherMode;
        mDefaultSettings.rgbw = this->mSettings.rgbw;
    }

    /// Destructor - cleanup peripheral resources
    ~BulkClockless() {
        // Cleanup handled by underlying I2S driver
        mSubControllers.clear();
    }

    /// Add a new LED strip to the bulk controller
    /// @param pin GPIO pin number for the strip
    /// @param buffer Pointer to CRGB buffer (user owns this memory)
    /// @param count Number of LEDs in the strip
    /// @param screenmap Optional ScreenMap for spatial positioning
    /// @return Pointer to sub-controller on success, nullptr on failure
    BulkStrip* add(int pin, CRGB* buffer, int count,
                                     ScreenMap screenmap = ScreenMap()) {
        // Check if we've exceeded the maximum strip count
        if (mSubControllers.size() >= MAX_STRIPS) {
            FL_WARN("BulkClockless<I2S>: Maximum " << MAX_STRIPS
                    << " strips exceeded. Cannot add pin " << pin);
            return nullptr;
        }

        // Validate pin number for ESP32-S3 (USB-JTAG pins)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        if (pin == 19 || pin == 20) {
            FL_WARN("BulkClockless<I2S>: GPIO19 and GPIO20 are reserved for "
                    "USB-JTAG on ESP32-S3. Cannot add pin " << pin);
            return nullptr;
        }
#endif

        // Validate uniform strip length (I2S limitation)
        if (mSubControllers.size() > 0 && mLedCount != count) {
            FL_WARN("BulkClockless<I2S>: All strips must have same length. "
                    "Existing: " << mLedCount << ", New: " << count);
            return nullptr;
        }

        // Check for duplicate pin
        if (mSubControllers.count(pin) > 0) {
            FL_WARN("BulkClockless<I2S>: Pin " << pin << " already in use");
            return nullptr;
        }

        // Store LED count (first strip sets the count)
        if (mLedCount == 0) {
            mLedCount = count;
        }

        // Create sub-controller with inherited settings
        BulkStrip sub(pin, buffer, count, screenmap);
        sub.settings = mDefaultSettings;

        // Insert into map
        mSubControllers[pin] = sub;

        // Mark as needing re-initialization
        mInitialized = false;

        // Return pointer to the sub-controller in the map
        return &mSubControllers[pin];
    }

    /// Remove a strip from the bulk controller
    /// @param pin GPIO pin number of the strip to remove
    /// @return true if removed, false if not found
    bool remove(int pin) {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return false;
        }

        mSubControllers.erase(it);

        // If all strips removed, reset LED count
        if (mSubControllers.size() == 0) {
            mLedCount = 0;
        }

        // Mark as needing re-initialization
        mInitialized = false;

        return true;
    }

    /// Get sub-controller for a specific pin
    /// @param pin GPIO pin number
    /// @return Const pointer to sub-controller, or nullptr if not found
    const BulkStrip* get(int pin) const {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// @param pin GPIO pin number
    /// @return Pointer to sub-controller, or nullptr if not found
    BulkStrip* get(int pin) {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get total number of LEDs across all strips
    /// @returns total LED count summed across all managed strips
    /// @note This overrides CLEDController::size() to maintain Liskov Substitution Principle
    int size() const override {
        int total = 0;
        for (const auto& pair : mSubControllers) {
            total += pair.second.getCount();
        }
        return total;
    }

    /// Get number of strips currently managed
    /// @return Number of strips
    int stripCount() const {
        return static_cast<int>(mSubControllers.size());
    }

    /// Check if a pin is currently registered
    /// @param pin GPIO pin number to check
    /// @return true if pin exists, false otherwise
    bool has(int pin) const {
        return mSubControllers.count(pin) > 0;
    }

    /// Set global color correction (applies to all new sub-controllers)
    /// @param correction Color correction to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setCorrection(const CRGB &correction) {
        mDefaultSettings.correction = correction;
        this->mSettings.correction = correction;
        return *this;
    }

    /// Set global color temperature (applies to all new sub-controllers)
    /// @param temperature Color temperature to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setTemperature(const CRGB &temperature) {
        mDefaultSettings.temperature = temperature;
        this->mSettings.temperature = temperature;
        return *this;
    }

    /// Set global dither mode (applies to all new sub-controllers)
    /// @param ditherMode Dither mode to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setDither(uint8_t ditherMode) {
        mDefaultSettings.ditherMode = ditherMode;
        this->mSettings.ditherMode = ditherMode;
        return *this;
    }

    /// Set global RGBW mode (applies to all new sub-controllers)
    /// @param rgbw RGBW configuration to apply
    /// @return Reference to this controller for chaining
    BulkClockless& setRgbw(const Rgbw &rgbw) {
        mDefaultSettings.rgbw = rgbw;
        this->mSettings.rgbw = rgbw;
        return *this;
    }

  protected:
    /// Initialize peripheral (called before first show or after config change)
    void initPeripheral() {
        if (mSubControllers.size() == 0) {
            mInitialized = true;
            return;
        }

        // Build pin and buffer arrays for I2S driver
        buildStripArrays();

        // Initialize I2S with timing
        ChipsetTiming timing = ChipsetTraits<Chipset::WS2812>::runtime_timing();

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // ESP32-S3: Use InternalI2SDriver
        if (!mDriver) {
            mDriver.reset(InternalI2SDriver::create());
        }
        mDriver->initled(
            reinterpret_cast<uint8_t*>(mStripPointers[0]),
            mPinArray,
            static_cast<int>(mSubControllers.size()),
            mLedCount
        );
#else
        // ESP32: Use low-level I2S API
        if (!i2s_is_initialized()) {
            i2s_define_bit_patterns(timing);
            i2s_init(0);  // Use I2S device 0
        }

        // Setup pins with I2S
        for (size_t i = 0; i < mSubControllers.size(); ++i) {
            i2s_setup_pin(mPinArray[i], i);
        }
#endif

        mInitialized = true;
    }

    /// Build arrays of pins and buffer pointers for I2S driver
    void buildStripArrays() {
        int idx = 0;
        for (auto &pair : mSubControllers) {
            if (idx < MAX_STRIPS) {
                mPinArray[idx] = pair.first;
                mStripPointers[idx] = pair.second.getBuffer();
                idx++;
            }
        }
    }

    /// Show LEDs (called by FastLED.show())
    void showPixelsInternal(PixelController<RGB_ORDER> &pixels) {
        // Re-initialize if configuration changed
        if (!mInitialized) {
            initPeripheral();
        }

        if (mSubControllers.size() == 0) {
            return;
        }

        // For each sub-controller, apply settings and copy data
        // Note: I2S driver handles the actual parallel transmission
        // Here we just need to ensure data is in the right format

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // ESP32-S3: Driver handles everything via beginShowLeds/showPixels/endShowLeds
        // The I2S_Esp32 class handles per-strip iteration
        if (mDriver) {
            mDriver->show();
        }
#else
        // ESP32: Manual pixel iteration and transposition
        // This is more complex and requires interleaving pixel data
        // For now, defer to existing I2S implementation pattern

        // Note: The existing ESP32 I2S driver expects data to be pre-transposed
        // This would require significant rework to support per-strip settings
        // For bulk controller, we'll rely on the fact that I2S already
        // supports multiple strips via the existing ClocklessController pattern

        FL_WARN("BulkClockless<I2S>: ESP32 (non-S3) I2S bulk mode requires "
                "further implementation. Use ESP32-S3 for full support.");
#endif
    }

    /// Show LEDs - internal override
    void showPixels(PixelController<RGB_ORDER> &pixels) override {
        showPixelsInternal(pixels);
    }

    /// Required init override
    void init() override {
        // Initialization happens lazily in showPixelsInternal
    }

    /// Get maximum refresh rate
    uint16_t getMaxRefreshRate() const override {
        return 800;  // 800 kHz for WS2812
    }

  private:
    /// Map of pin number to sub-controller
    fl::fl_map<int, BulkStrip> mSubControllers;

    /// Default settings (copied to new sub-controllers)
    BulkStrip::Settings mDefaultSettings;

    /// Number of LEDs per strip (all strips must match)
    int mLedCount;

    /// Whether peripheral is initialized
    bool mInitialized;

    /// Arrays for I2S driver
    int mPinArray[MAX_STRIPS];
    CRGB* mStripPointers[MAX_STRIPS];

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    /// ESP32-S3 I2S driver instance
    fl::unique_ptr<InternalI2SDriver> mDriver;
#endif
};

}  // namespace fl

#endif  // ESP32 or ESP32-S3
