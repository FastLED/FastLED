/// @file bulk_clockless_parlio.h
/// @brief BulkClockless controller for ESP32-P4 PARLIO peripheral
///
/// This file provides the full controller implementation that manages LED strips
/// and coordinates write operations via the PARLIO singleton service.
///
/// Architecture:
/// - BulkClockless<CHIPSET, PARLIO> (this file) - Full controller (strip management + write operations)
///   ├─ Strip management: add/remove/get strips, store BulkStrip objects
///   └─> IParlioGroup::getOrCreate<CHIPSET>() - Write service (DMA packing, manager coordination)
///       └─> IParlioManager - Batches multiple groups
///           └─> IParlioEngine - DMA hardware controller
///
/// @note This is an internal ESP32 platform header. Do not include directly.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "fl/clockless/base.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/engine_events.h"
#include "fl/map.h"
#include "fl/warn.h"
#include "parlio_group.h"
#include "pixel_iterator.h"

namespace fl {

/// @brief Full BulkClockless controller for ESP32-P4 PARLIO peripheral
///
/// This specialization manages LED strips (add/remove/get) and coordinates
/// write operations via the IParlioGroup singleton service. Strip management
/// happens here (like RMT/LCD_I80), while IParlioGroup handles DMA write operations.
///
/// @tparam CHIPSET LED chipset timing (e.g., Chipset::WS2812, TIMING_WS2812_800KHZ)
template <typename CHIPSET, EOrder RGB_ORDER>
class BulkClockless<CHIPSET, RGB_ORDER, PARLIO>
    : public CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK> {
public:
    /// Maximum number of strips supported by PARLIO peripheral
    static constexpr int MAX_STRIPS = IParlioGroup::MAX_STRIPS;

    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>()
        , mGroup(IParlioGroup::getOrCreate<CHIPSET>())
        , mDefaultSettings(initDefaultSettings()) {
        // Add all strips from initializer list
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// Constructor with span (for runtime arrays/vectors)
    /// @param strips span of strip configurations
    BulkClockless(span<const BulkStripConfig> strips)
        : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>()
        , mGroup(IParlioGroup::getOrCreate<CHIPSET>())
        , mDefaultSettings(initDefaultSettings()) {
        // Add all strips from span
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// @brief Add a new strip to the controller
    ///
    /// Validates the pin, creates a BulkStrip descriptor, and registers with group.
    ///
    /// @param pin GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    /// @returns pointer to sub-controller on success, nullptr on error
    BulkStrip* add(int pin, CRGB* buffer, int count, const fl::ScreenMap& screenmap) {
        // Check if pin already in use
        if (mSubControllers.find(pin) != mSubControllers.end()) {
            FL_WARN("PARLIO: Pin " << pin << " already in use");
            return nullptr;
        }

        // Check max strips
        if (static_cast<int>(mSubControllers.size()) >= MAX_STRIPS) {
            FL_WARN("PARLIO: Max strips (" << MAX_STRIPS << ") exceeded");
            return nullptr;
        }

        // Create BulkStrip with default settings
        BulkStrip strip(pin, buffer, count, screenmap);
        strip.settings = mDefaultSettings;

        // Store in local map
        mSubControllers[pin] = strip;

        // Register with group for write operations
        bool is_rgbw = strip.settings.rgbw.active();
        mGroup.registerStrip(static_cast<uint8_t>(pin), buffer, count, is_rgbw);

        // Notify engine events
        fl::EngineEvents::onCanvasUiSet(this, screenmap);

        return &mSubControllers[pin];
    }

    /// @brief Remove a strip from the controller
    ///
    /// @param pin GPIO pin number of strip to remove
    /// @returns true if strip was removed, false if not found
    bool remove(int pin) {
        auto it = mSubControllers.find(pin);
        if (it == mSubControllers.end()) {
            return false;
        }

        // Unregister from group
        mGroup.unregisterStrip(static_cast<uint8_t>(pin));

        // Remove from local map
        mSubControllers.erase(it);
        return true;
    }

    /// Get sub-controller for a specific pin (const version)
    const BulkStrip* get(int pin) const {
        auto it = mSubControllers.find(pin);
        return (it != mSubControllers.end()) ? &it->second : nullptr;
    }

    /// Get sub-controller for a specific pin (non-const version)
    BulkStrip* get(int pin) {
        auto it = mSubControllers.find(pin);
        return (it != mSubControllers.end()) ? &it->second : nullptr;
    }

    /// Get total number of LEDs across all strips
    int size() const override {
        int total = 0;
        for (const auto& pair : mSubControllers) {
            total += pair.second.getCount();
        }
        return total;
    }

    /// Get number of active strips
    int stripCount() const {
        return static_cast<int>(mSubControllers.size());
    }

    /// Check if a pin is being used
    bool has(int pin) const {
        return mSubControllers.find(pin) != mSubControllers.end();
    }

    /// Set global color correction (affects new strips only)
    CLEDController& setCorrection(CRGB correction) {
        CLEDController::setCorrection(correction);
        mDefaultSettings.correction = correction;
        return *this;
    }

    /// Set global color temperature (affects new strips only)
    CLEDController& setTemperature(CRGB temperature) {
        CLEDController::setTemperature(temperature);
        mDefaultSettings.temperature = temperature;
        return *this;
    }

    /// Set global dither mode (affects new strips only)
    CLEDController& setDither(fl::u8 ditherMode) {
        CLEDController::setDither(ditherMode);
        mDefaultSettings.ditherMode = ditherMode;
        return *this;
    }

    /// Set global RGBW configuration (affects new strips only)
    CLEDController& setRgbw(const Rgbw& arg) {
        CLEDController::setRgbw(arg);
        mDefaultSettings.rgbw = arg;
        return *this;
    }

    /// Initialize the controller
    void init() override {
        // No initialization needed
    }

    /// Get maximum refresh rate
    uint16_t getMaxRefreshRate() const override {
        return 800;  // 800 Hz typical for WS2812
    }

    /// Show single color (not typically used for bulk controllers)
    void showColor(const CRGB& data, int nLeds, fl::u8 brightness) override {
        (void)data;
        (void)nLeds;
        (void)brightness;
    }

    /// Override show() to capture brightness
    void show(const CRGB* data, int nLeds, fl::u8 brightness) override {
        (void)data;
        (void)nLeds;
        mBrightness = brightness;
        showPixels();
    }

protected:
    /// @brief Show all strips via PARLIO peripheral
    ///
    /// This method iterates over local strips and coordinates write operations via IParlioGroup.
    void showPixels() override {
        if (mSubControllers.empty()) {
            return;
        }

        // Start frame queuing
        mGroup.onQueuingStart();

        // Write pixel data for each strip
        for (auto& [pin, strip] : mSubControllers) {
            CRGB* buffer = strip.getBuffer();
            if (!buffer) {
                FL_WARN("PARLIO: Strip on pin " << pin << " has null buffer, skipping");
                continue;
            }

            int count = strip.getCount();

            // Create PixelIterator with per-strip settings
            ColorAdjustment adj = BulkClocklessHelper::computeAdjustment(mBrightness, strip.settings);
            PixelIterator pixels(buffer, count, adj, strip.settings.ditherMode, strip.settings.rgbw);

            // Write to group's internal buffer
            mGroup.writePixels(static_cast<uint8_t>(pin), pixels);
        }

        // Notify queuing complete and flush
        mGroup.onQueuingDone();
        mGroup.flush();
    }

private:
    /// @brief Initialize default settings from base class
    BulkStrip::Settings initDefaultSettings() {
        BulkStrip::Settings settings;
        settings.correction = this->mSettings.correction;
        settings.temperature = this->mSettings.temperature;
        settings.ditherMode = this->mSettings.ditherMode;
        settings.rgbw = this->mSettings.rgbw;
        return settings;
    }

    /// Reference to the singleton IParlioGroup for this chipset
    IParlioGroup& mGroup;

    /// Map of pin → BulkStrip (local strip management)
    fl_map<int, BulkStrip> mSubControllers;

    /// Default settings for new strips
    BulkStrip::Settings mDefaultSettings;

    /// Brightness captured from show() call
    fl::u8 mBrightness = 255;
};

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
