/// @file bulk_clockless_parlio.h
/// @brief Thin BulkClockless adapter for ESP32-P4 PARLIO peripheral
///
/// This file provides a minimal template specialization that converts compile-time
/// CHIPSET parameters to runtime values and delegates all operations to BulkControllerImpl.
///
/// Architecture:
/// - BulkClockless<CHIPSET, PARLIO> (this file) - Thin template adapter
///   └─> BulkControllerImpl - Concrete implementation with all business logic
///       └─> IParlioTransmitter::getOrCreate<CHIPSET>() - Runtime interface singleton
///           └─> ParlioHub - Coordinates multiple transmitters
///               └─> IParlioEngine - DMA hardware controller
///
/// @note This is an internal ESP32 platform header. Do not include directly.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "fl/clockless/base.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/engine_events.h"
#include "parlio_transmitter.h"
#include "bulk_controller_impl.h"

namespace fl {

/// @brief Thin BulkClockless specialization for ESP32-P4 PARLIO peripheral
///
/// This adapter is intentionally minimal - it only converts compile-time template
/// parameters (CHIPSET) to runtime values and delegates to BulkControllerImpl.
/// All business logic lives in the concrete implementation class.
///
/// @tparam CHIPSET LED chipset timing (e.g., Chipset::WS2812, TIMING_WS2812_800KHZ)
template <typename CHIPSET>
class BulkClockless<CHIPSET, PARLIO>
    : public CPixelLEDController<RGB, 1, ALL_LANES_MASK> {
public:
    /// Maximum number of strips supported by PARLIO peripheral
    static constexpr int MAX_STRIPS = BulkControllerImpl::MAX_STRIPS;

    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(fl::initializer_list<BulkStripConfig> strips)
        : CPixelLEDController<RGB, 1, ALL_LANES_MASK>()
        , mImpl(IParlioTransmitter::getOrCreate<CHIPSET>(), initDefaultSettings()) {
        // Add all strips from initializer list
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// Constructor with span (for runtime arrays/vectors)
    /// @param strips span of strip configurations
    BulkClockless(span<const BulkStripConfig> strips)
        : CPixelLEDController<RGB, 1, ALL_LANES_MASK>()
        , mImpl(IParlioTransmitter::getOrCreate<CHIPSET>(), initDefaultSettings()) {
        // Add all strips from span
        for (const auto &config : strips) {
            add(config.pin, config.buffer, config.count, config.screenmap);
        }
    }

    /// @brief Add a new strip to the controller
    ///
    /// Validates the pin, creates a BulkStrip descriptor, and registers with transmitter.
    ///
    /// @param pin GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    /// @returns pointer to sub-controller on success, nullptr on error
    BulkStrip* add(int pin, CRGB* buffer, int count, const fl::ScreenMap& screenmap) {
        auto* result = mImpl.add(pin, buffer, count, screenmap);
        if (result) {
            fl::EngineEvents::onCanvasUiSet(this, screenmap);
        }
        return result;
    }

    /// @brief Remove a strip from the controller
    ///
    /// @param pin GPIO pin number of strip to remove
    /// @returns true if strip was removed, false if not found
    bool remove(int pin) {
        return mImpl.remove(pin);
    }

    /// Get sub-controller for a specific pin (const version)
    const BulkStrip* get(int pin) const {
        return mImpl.get(pin);
    }

    /// Get sub-controller for a specific pin (non-const version)
    BulkStrip* get(int pin) {
        return mImpl.get(pin);
    }

    /// Get total number of LEDs across all strips
    int size() const override {
        return mImpl.size();
    }

    /// Get number of active strips
    int stripCount() const {
        return mImpl.stripCount();
    }

    /// Check if a pin is being used
    bool has(int pin) const {
        return mImpl.has(pin);
    }

    /// Set global color correction (affects new strips only)
    CLEDController& setCorrection(CRGB correction) {
        CLEDController::setCorrection(correction);
        mImpl.setCorrection(correction);
        return *this;
    }

    /// Set global color temperature (affects new strips only)
    CLEDController& setTemperature(CRGB temperature) {
        CLEDController::setTemperature(temperature);
        mImpl.setTemperature(temperature);
        return *this;
    }

    /// Set global dither mode (affects new strips only)
    CLEDController& setDither(fl::u8 ditherMode) {
        CLEDController::setDither(ditherMode);
        mImpl.setDither(ditherMode);
        return *this;
    }

    /// Set global RGBW configuration (affects new strips only)
    CLEDController& setRgbw(const Rgbw& arg) {
        CLEDController::setRgbw(arg);
        mImpl.setRgbw(arg);
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
    /// This method delegates to BulkControllerImpl which handles all the coordination.
    void showPixels() override {
        mImpl.showPixels(mBrightness);
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

    /// Implementation class containing all business logic
    BulkControllerImpl mImpl;

    /// Brightness captured from show() call
    fl::u8 mBrightness = 255;
};

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
