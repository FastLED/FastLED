#pragma once

/// @file bulk_lcd_i80.h
/// LCD_I80 peripheral specialization for BulkClockless controller

#include "fl/compiler_control.h"

// Platform guard - LCD_I80 only on ESP32-S3 and ESP32-P4
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32P4)

#include "fl/clockless/base.h"
#include "fl/clockless/constants.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/map.h"
#include "lcd_driver_common.h"
#include "lcd_driver_i80.h"
#include "rgbw.h"

namespace fl {

// Chipset definitions (WS2812_CHIPSET and ChipsetTraits) are provided by
// peripheral_tags.h - included above at line 12

/// BulkClockless specialization for WS2812 + LCD_I80
///
/// Manages multiple LED strips using ESP32's LCD_I80 peripheral with parallel
/// transmission. All strips update simultaneously via parallel data lanes.
///
/// Features:
/// - Up to 16 strips supported (hardware limit of LCD_I80 peripheral)
/// - Parallel transmission (all strips update at the same time)
/// - Pin order matters (each pin maps to a specific data lane D0-D15)
/// - Efficient memory usage with shared DMA buffer
///
/// Constraints:
/// - Maximum 16 strips (LCD_I80 has 16 data lanes)
/// - Pins must be valid GPIO on ESP32-S3/P4 (see lcd_driver_common.h)
/// - All strips must have the same length (hardware limitation)
///
/// Usage:
/// @code
/// BulkClockless<Chipset::WS2812, GRB, LCD_I80>* bulk =
///     &FastLED.addClocklessLeds<Chipset::WS2812, GRB, LCD_I80>({
///         {8, strip1, 100, screenmap1},
///         {9, strip2, 100, screenmap2}
///     });
/// @endcode
template <typename CHIPSET, EOrder RGB_ORDER>
class BulkClockless<CHIPSET, RGB_ORDER, LCD_I80>
    : public CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK> {
  public:
    /// Maximum number of strips supported by LCD_I80 peripheral
    static constexpr int MAX_STRIPS = 16;

    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>()
        , mDriver()
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

    ~BulkClockless() {
        // Driver cleanup is handled by LcdI80Driver destructor
    }

    /// Add a new strip to the controller
    /// @param pin GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    /// @returns pointer to sub-controller on success, nullptr on error
    BulkStrip *add(int pin, CRGB *buffer, int count,
                                    const fl::ScreenMap &screenmap) {
        // 1. Validate pin for platform
        if (!validatePin(pin)) {
            FL_WARN("BulkClockless<LCD_I80>: Invalid pin " << pin
                                                           << " for platform");
            return nullptr;
        }

        // 2. Check duplicate
        if (mSubControllers.find(pin) != mSubControllers.end()) {
            FL_WARN("BulkClockless<LCD_I80>: Pin " << pin << " already in use");
            return nullptr;
        }

        // 3. Check max strips
        if (static_cast<int>(mSubControllers.size()) >= MAX_STRIPS) {
            FL_WARN("BulkClockless<LCD_I80>: Max strips (" << MAX_STRIPS
                                                           << ") exceeded");
            return nullptr;
        }

        // 4. Validate uniform strip length (LCD_I80 limitation)
        if (mSubControllers.size() > 0 && mLedCount != count) {
            FL_WARN("BulkClockless<LCD_I80>: All strips must have same length. "
                    "Existing: " << mLedCount << ", New: " << count);
            return nullptr;
        }

        // 5. Create sub-controller
        BulkStrip sub(pin, buffer, count, screenmap);
        sub.settings = mDefaultSettings; // Copy defaults
        mSubControllers[pin] = sub;

        // 6. Update ScreenMap binding
        fl::EngineEvents::onCanvasUiSet(this, screenmap);

        // 7. Set LED count on first strip
        if (mLedCount == 0) {
            mLedCount = count;
        }

        // 8. Mark driver as needing re-initialization
        mInitialized = false;

        return &mSubControllers[pin];
    }

    /// Remove a strip from the controller
    /// @param pin GPIO pin number of strip to remove
    /// @returns true if strip was removed, false if not found
    bool remove(int pin) {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return false;
        }

        // Remove from map
        mSubControllers.erase(it);

        // Mark driver as needing re-initialization
        mInitialized = false;

        return true;
    }

    /// Get sub-controller for a specific pin (const version)
    /// @param pin GPIO pin number
    /// @returns const pointer to sub-controller, or nullptr if not found
    const BulkStrip *get(int pin) const {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return nullptr;
        }
        return &it->second;
    }

    /// Get sub-controller for a specific pin (non-const version)
    /// @param pin GPIO pin number
    /// @returns pointer to sub-controller, or nullptr if not found
    BulkStrip *get(int pin) {
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

    /// Get number of active strips
    /// @returns number of strips currently managed by this controller
    int stripCount() const {
        return static_cast<int>(mSubControllers.size());
    }

    /// Check if a pin is being used
    /// @param pin GPIO pin number to check
    /// @returns true if pin has an active strip
    bool has(int pin) const {
        return mSubControllers.find(pin) != mSubControllers.end();
    }

    /// Set global color correction (affects new strips only)
    /// @param correction color correction to apply
    /// @returns reference to this controller for chaining
    CLEDController &setCorrection(CRGB correction) {
        CLEDController::setCorrection(correction);
        mDefaultSettings.correction = correction;
        return *this;
    }

    /// Set global color temperature (affects new strips only)
    /// @param temperature color temperature to apply
    /// @returns reference to this controller for chaining
    CLEDController &setTemperature(CRGB temperature) {
        CLEDController::setTemperature(temperature);
        mDefaultSettings.temperature = temperature;
        return *this;
    }

    /// Set global dither mode (affects new strips only)
    /// @param ditherMode dither mode to apply
    /// @returns reference to this controller for chaining
    CLEDController &setDither(fl::u8 ditherMode) {
        CLEDController::setDither(ditherMode);
        mDefaultSettings.ditherMode = ditherMode;
        return *this;
    }

    /// Set global RGBW configuration (affects new strips only)
    /// @param arg RGBW configuration to apply
    /// @returns reference to this controller for chaining
    CLEDController &setRgbw(const Rgbw &arg) {
        CLEDController::setRgbw(arg);
        mDefaultSettings.rgbw = arg;
        return *this;
    }

    /// Initialize the controller
    void init() override {
        initPeripheral();
    }

    /// Get maximum refresh rate
    uint16_t getMaxRefreshRate() const override { return 800; }

    /// Show single color (not typically used for bulk controllers)
    void showColor(const CRGB &data, int nLeds, fl::u8 brightness) override {
        // Not typically used for bulk controllers
        (void)data;
        (void)nLeds;
        (void)brightness;
    }

    /// Override show() to capture brightness
    void show(const CRGB *data, int nLeds, fl::u8 brightness) override {
        // Store brightness for use in showPixelsInternal()
        mBrightness = brightness;
        // Call base class which will eventually call showPixels()
        CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>::show(data, nLeds, brightness);
    }

    /// Show LED data (override from CPixelLEDController)
    void showPixels(PixelController<RGB_ORDER, 1, ALL_LANES_MASK> &pixels) override {
        // This is called by base class show() method
        // For BulkClockless, we iterate all sub-controllers
        (void)pixels; // Not used - we use sub-controller buffers
        showPixelsInternal();
    }

  protected:
    /// Initialize LCD_I80 peripheral
    void initPeripheral() {
        if (mSubControllers.size() == 0) {
            return; // Nothing to initialize
        }

        // Build pin configuration array
        LcdDriverConfig config;
        config.num_lanes = 0;

        // Sort pins into lane configuration
        // LCD_I80 uses parallel lanes D0-D15, we map pins in order
        for (auto &pair : mSubControllers) {
            int pin = pair.first;
            if (config.num_lanes < MAX_STRIPS) {
                config.gpio_pins[config.num_lanes] = pin;
                config.num_lanes++;
            }
        }

        config.use_psram = true; // Use PSRAM for DMA buffers
        using ChipsetTiming = typename ChipsetTraits<CHIPSET>::VALUE;
        config.latch_us = ChipsetTiming::RESET;

        // Initialize driver with uniform LED count
        bool success = mDriver.begin(config, mLedCount);
        if (!success) {
            FL_WARN("BulkClockless<LCD_I80>: Failed to initialize driver");
            return;
        }

        // Attach strips to driver in same order as config
        buildStripPointerArray();
        mDriver.attachStrips(mStripPointers);

        mInitialized = true;
    }

    /// Build array of CRGB pointers for driver (matches pin order in config)
    void buildStripPointerArray() {
        int idx = 0;
        for (auto &pair : mSubControllers) {
            BulkStrip &sub = pair.second;
            if (idx < MAX_STRIPS) {
                mStripPointers[idx] = sub.getBuffer();
                idx++;
            }
        }
    }

    /// Validate GPIO pin for ESP32-S3/P4
    /// @param pin GPIO pin number to validate
    /// @returns true if pin is valid, false otherwise
    bool validatePin(int pin) {
        // Use common validation from lcd_driver_common.h
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        auto result = validate_esp32s3_lcd_pin(pin, false);
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
        auto result = validate_esp32p4_lcd_pin(pin);
#else
        PinValidationResult result = {false, "Unsupported platform"};
#endif
        return result.valid;
    }

    /// Show all strips via LCD_I80 peripheral
    void showPixelsInternal() {
        if (!mInitialized) {
            initPeripheral();
        }

        if (!mInitialized) {
            return; // Initialization failed
        }

        // Apply per-strip settings by modifying buffers in-place
        // Note: This is a simplified approach. Full implementation would need
        // to apply color correction, temperature, dither, and RGBW conversion
        // per strip before passing to driver.

        // For now, just show the data as-is
        // TODO: Apply per-strip adjustments before calling driver.show()

        mDriver.show();
    }

    /// Map of pin number to sub-controller
    fl::fl_map<int, BulkStrip> mSubControllers;

    /// LCD I80 driver instance (template parameter = chipset timing)
    LcdI80Driver<typename ChipsetTraits<CHIPSET>::VALUE> mDriver;

    /// Array of strip pointers for driver (matches driver's lane order)
    CRGB *mStripPointers[MAX_STRIPS];

    /// Number of LEDs per strip (uniform across all strips)
    int mLedCount;

    /// Default settings for new strips
    BulkStrip::Settings mDefaultSettings;

    /// Cached brightness value from last show() call
    fl::u8 mBrightness = 255;

    /// Flag indicating driver is initialized
    bool mInitialized;
};

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
