#pragma once

/// @file bulk_rmt.h
/// RMT peripheral specialization for BulkClockless controller

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/clockless/constants.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/clockless/base.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/led_timing.h"
#include "fl/dbg.h"
#include "fl/map.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/rmt5_controller_lowlevel.h"
#include "rgbw.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
FL_EXTERN_C_END

namespace fl {

// Chipset definitions (WS2812_CHIPSET and ChipsetTraits) are provided by
// peripheral_tags.h - included above at line 14

/// BulkClockless specialization for WS2812 + RMT
///
/// Manages multiple LED strips using ESP32's RMT peripheral with worker pool
/// pattern. Each strip gets an independent RMT controller that borrows workers
/// during transmission.
///
/// Features:
/// - Up to 64 strips supported (limited by MAX_CLED_CONTROLLERS)
/// - Each strip has independent timing and settings
/// - Worker pool pattern allows N > K strips (N strips, K RMT channels)
/// - Order-independent (strips can be added in any order)
///
/// Usage:
/// @code
/// BulkClockless<Chipset::WS2812, GRB, RMT>* bulk = &FastLED.addClocklessLeds<Chipset::WS2812, GRB, RMT>({
///     {2, strip1, 100, screenmap1},
///     {4, strip2, 100, screenmap2}
/// });
/// @endcode
template <EOrder RGB_ORDER>
class BulkClockless<Chipset::WS2812, RGB_ORDER, RMT>
    : public CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK> {
  public:
    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>() {
        // Initialize default settings from base class
        mDefaultSettings.correction = this->mSettings.correction;
        mDefaultSettings.temperature = this->mSettings.temperature;
        mDefaultSettings.ditherMode = this->mSettings.ditherMode;
        mDefaultSettings.rgbw = this->mSettings.rgbw;

        // Initialize peripheral
        initPeripheral();

        // Add all strips from initializer list
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    ~BulkClockless() {
        // Clean up RMT controllers
        for (auto &pair : mRmtControllers) {
            delete pair.second;
        }
        mRmtControllers.clear();
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
            FL_WARN("BulkClockless<RMT>: Invalid pin " << pin
                                                       << " for platform");
            return nullptr;
        }

        // 2. Check duplicate
        if (mSubControllers.find(pin) != mSubControllers.end()) {
            FL_WARN("BulkClockless<RMT>: Pin " << pin << " already in use");
            return nullptr;
        }

        // 3. Check max strips
        if (static_cast<int>(mSubControllers.size()) >= getMaxStrips()) {
            FL_WARN("BulkClockless<RMT>: Max strips (" << getMaxStrips()
                                                       << ") exceeded");
            return nullptr;
        }

        // 4. Create sub-controller
        BulkStrip sub(pin, buffer, count, screenmap);
        sub.settings = mDefaultSettings; // Copy defaults
        mSubControllers[pin] = sub;

        // 5. Update ScreenMap binding
        fl::EngineEvents::onCanvasUiSet(this, screenmap);

        // 6. Create RMT controller for this pin
        onStripAdded(pin);

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

        // Peripheral-specific cleanup
        onStripRemoved(pin);

        // Remove from map
        mSubControllers.erase(it);
        return true;
    }

    /// Get sub-controller for a specific pin
    /// @param pin GPIO pin number
    /// @returns const pointer to sub-controller, or nullptr if not found
    const BulkStrip *get(int pin) const {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return nullptr;
        }
        return &it->second;
    }

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
    void init() override { initPeripheral(); }

    /// Get maximum refresh rate
    uint16_t getMaxRefreshRate() const override { return 800; }

    /// Show single color (not typically used for bulk controllers)
    void showColor(const CRGB &data, int nLeds, fl::u8 brightness) override {
        // Fill all strip buffers with solid color
        for (auto &pair : mSubControllers) {
            BulkStrip &sub = pair.second;
            CRGB* buffer = sub.getBuffer();
            for (int i = 0; i < sub.getCount(); i++) {
                buffer[i] = data;
            }
        }
        mBrightness = brightness;
        // Transmission happens via showPixels() -> showPixelsInternal()
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
        // Parameter unused: BulkClockless manages multiple independent buffers, each
        // requiring its own PixelController with per-strip settings. We create
        // per-strip PixelControllers in showPixelsInternal().
        (void)pixels;
        showPixelsInternal();
    }

  protected:
    /// Initialize RMT peripheral
    void initPeripheral() {
        // RMT initialization happens per-strip in onStripAdded()
        // No global peripheral initialization needed
    }

    /// Called when a strip is added - create RMT controller for it
    void onStripAdded(int pin) {
        // Create RMT controller for this pin
        auto timing = ChipsetTraits<Chipset::WS2812>::runtime_timing();
        RmtController5LowLevel *rmt =
            new RmtController5LowLevel(pin, timing, 280);
        mRmtControllers[pin] = rmt;
    }

    /// Called when a strip is removed - clean up RMT controller
    void onStripRemoved(int pin) {
        auto it = mRmtControllers.find(pin);
        if (it != mRmtControllers.end()) {
            delete it->second;
            mRmtControllers.erase(it);
        }
    }

    /// Validate GPIO pin for ESP32
    /// @param pin GPIO pin number to validate
    /// @returns true if pin is valid, false otherwise
    bool validatePin(int pin) {
        // Check if pin is valid GPIO on ESP32
        if (pin < 0 || pin >= GPIO_NUM_MAX) {
            return false;
        }

        // On ESP32-S3: Avoid USB-JTAG pins (19, 20) and SPI Flash pins (26-32)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        if (pin == 19 || pin == 20) {
            return false; // USB-JTAG
        }
        if (pin >= 26 && pin <= 32) {
            return false; // SPI Flash
        }
#endif

        // Check if pin is valid for output
        gpio_num_t gpio_pin = static_cast<gpio_num_t>(pin);
        if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio_pin)) {
            return false;
        }

        return true;
    }

    /// Get maximum number of strips (limited by FastLED controller limit)
    /// @returns maximum strip count
    int getMaxStrips() { return 64; }

    /// Show all strips via RMT peripheral
    void showPixelsInternal() {
        // Iterate all sub-controllers and show their pixels
        for (auto &pair : mSubControllers) {
            int pin = pair.first;
            BulkStrip &sub = pair.second;

            // Get RMT controller for this pin
            auto rmt_it = mRmtControllers.find(pin);
            if (rmt_it == mRmtControllers.end()) {
                continue; // Skip if no RMT controller
            }
            RmtController5LowLevel *rmt = rmt_it->second;

            // Create pixel iterator with per-strip settings
            ColorAdjustment adj =
                BulkClocklessHelper::computeAdjustment(mBrightness, sub.settings);
            PixelController<RGB_ORDER, 1, ALL_LANES_MASK> pixels(
                sub.getBuffer(), sub.getCount(), adj, sub.settings.ditherMode);

            // Convert to PixelIterator and load into RMT controller
            fl::PixelIterator iterator = pixels.as_iterator(sub.settings.rgbw);
            rmt->loadPixelData(iterator);
        }

        // Show all pixels (triggers transmission for all RMT controllers)
        for (auto &pair : mRmtControllers) {
            pair.second->showPixels();
        }
    }


    /// Map of pin number to sub-controller
    fl::fl_map<int, BulkStrip> mSubControllers;

    /// Map of pin number to RMT controller (low-level driver)
    fl::fl_map<int, RmtController5LowLevel *> mRmtControllers;

    /// Default settings for new strips
    BulkStrip::Settings mDefaultSettings;

    /// Cached brightness value from last show() call
    fl::u8 mBrightness = 255;
};

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32
