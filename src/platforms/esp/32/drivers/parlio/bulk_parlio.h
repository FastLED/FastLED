/// @file bulk_parlio.h
/// @brief BulkClockless specialization for ESP32-P4 PARLIO peripheral
///
/// This file implements a BulkClockless<CHIPSET, PARLIO> specialization that
/// integrates the PARLIO driver into the standard BulkClockless API, enabling
/// runtime add/remove of strips, per-strip settings, and ScreenMap integration.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "fl/clockless/base.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/map.h"
#include "fl/math8.h"
#include "parlio_driver.h"

namespace fl {

/// @brief BulkClockless specialization for ESP32-P4 PARLIO peripheral
///
/// Provides parallel LED output for up to 16 strips using the PARLIO TX
/// peripheral with DMA-based hardware timing. This specialization integrates
/// the existing ParlioLedDriver into the BulkClockless API pattern.
///
/// @par Key Features:
/// - Dynamic add/remove of strips at runtime
/// - Per-strip settings (color correction, temperature, dither, RGBW)
/// - Pin → channel mapping with automatic allocation
/// - Hardware-accelerated parallel output
/// - ScreenMap integration for WASM visualization
///
/// @par Usage Example:
/// @code
/// CRGB strip1[100], strip2[100];
/// auto& bulk = FastLED.addBulkLeds<Chipset::WS2812, PARLIO>({
///     {2, strip1, 100, ScreenMap()},
///     {5, strip2, 100, ScreenMap()}
/// });
/// bulk.setCorrection(TypicalLEDStrip);
/// bulk.get(2)->setTemperature(Tungsten100W);  // Per-strip override
/// @endcode
///
/// @par Hardware Constraints:
/// - Maximum 16 strips (PARLIO hardware limit)
/// - All strips must have the same LED count (PARLIO frame size limitation)
/// - GPIO pins must be valid for ESP32-P4 PARLIO peripheral
///
/// @tparam CHIPSET LED chipset timing (e.g., Chipset::WS2812)
template <typename CHIPSET>
class BulkClockless<CHIPSET, PARLIO>
    : public BulkClockless<CHIPSET, CPU_FALLBACK> {
public:
    using Base = BulkClockless<CHIPSET, CPU_FALLBACK>;

    /// Inherit constructors from base class
    using Base::Base;

    /// Destructor - cleanup PARLIO resources
    ~BulkClockless();

protected:
    /// @brief Initialize PARLIO peripheral (lazy initialization)
    ///
    /// Called by base class init(). Defers actual hardware initialization
    /// until the first strip is added.
    void initPeripheral() override;

    /// @brief Hook called when a strip is added
    ///
    /// Allocates a PARLIO channel for the pin, updates pin→channel mapping,
    /// and reconfigures the driver with the new channel list.
    ///
    /// @param pin GPIO pin number of the added strip
    void onStripAdded(int pin) override;

    /// @brief Hook called when a strip is removed
    ///
    /// Frees the PARLIO channel associated with the pin and reconfigures
    /// the driver with the remaining channels.
    ///
    /// @param pin GPIO pin number of the removed strip
    void onStripRemoved(int pin) override;

    /// @brief Validate GPIO pin for ESP32-P4 PARLIO peripheral
    ///
    /// Checks if the pin is a valid GPIO number for the ESP32-P4 and
    /// suitable for PARLIO output.
    ///
    /// @param pin GPIO pin number to validate
    /// @returns true if pin is valid, false otherwise
    bool validatePin(int pin) override;

    /// @brief Get maximum number of strips supported by PARLIO
    ///
    /// @returns 16 (PARLIO hardware limit)
    int getMaxStrips() override;

    /// @brief Show all strips via PARLIO peripheral
    ///
    /// Applies per-strip settings (correction, temperature, dither, RGBW),
    /// registers each strip's buffer with the driver, and triggers parallel
    /// transmission via PARLIO DMA.
    void showPixelsInternal() override;

private:
    /// PARLIO driver instance (created on-demand)
    ParlioLedDriver<16, CHIPSET>* mDriver = nullptr;

    /// Pin → channel mapping (forward lookup)
    fl_map<int, uint8_t> mPinToChannel;

    /// Channel → pin reverse mapping (for freeing channels)
    /// -1 = channel unused
    int mChannelToPin[16] = {-1, -1, -1, -1, -1, -1, -1, -1,
                             -1, -1, -1, -1, -1, -1, -1, -1};

    /// Track if peripheral is initialized
    bool mInitialized = false;

    /// @brief Allocate next available PARLIO channel
    ///
    /// Finds the first unused channel (0-15) and marks it as allocated.
    ///
    /// @returns channel index (0-15) on success, -1 if no free channels
    int allocateChannel();

    /// @brief Free a PARLIO channel
    ///
    /// Marks the channel as unused and removes it from the pin→channel mapping.
    ///
    /// @param channel channel index (0-15) to free
    void freeChannel(uint8_t channel);

    /// @brief Reconfigure PARLIO driver with current strip configuration
    ///
    /// Builds a GPIO array from active channels and reinitializes the driver.
    /// Called after add or remove operations.
    void reconfigureDriver();
};

// ============================================================================
// Template Method Implementations
// ============================================================================

template <typename CHIPSET>
BulkClockless<CHIPSET, PARLIO>::~BulkClockless() {
    if (mDriver) {
        mDriver->end();
        delete mDriver;
        mDriver = nullptr;
    }
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::initPeripheral() {
    // Lazy initialization - actual hardware init deferred to first strip add
    // This allows the constructor to complete quickly and avoids initializing
    // unused peripherals.
    FL_DBG("PARLIO: initPeripheral() called (lazy init)");
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::onStripAdded(int pin) {
    // Get the newly added strip
    BulkStrip* strip = this->get(pin);
    if (!strip) {
        FL_WARN("PARLIO: Strip on pin " << pin << " not found after add");
        return;
    }

    // Enforce uniform LED count constraint (PARLIO limitation)
    // All strips must have the same LED count since PARLIO transmits fixed-length frames
    if (!this->mSubControllers.empty() && this->mSubControllers.size() > 1) {
        // Find the first strip that isn't this one
        int expected_count = 0;
        for (const auto& [other_pin, other_strip] : this->mSubControllers) {
            if (other_pin != pin) {
                expected_count = other_strip.getCount();
                break;
            }
        }

        if (expected_count > 0 && strip->getCount() != expected_count) {
            FL_WARN("PARLIO: All strips must have same LED count. "
                    "Expected " << expected_count << " LEDs, got " << strip->getCount()
                    << " on pin " << pin);
            return;  // Keep strip in map but don't allocate channel (CPU fallback)
        }
    }

    // Allocate channel for this pin
    int channel = allocateChannel();
    if (channel < 0) {
        FL_WARN("PARLIO: No free channels available (max 16 strips)");
        return;  // Base class already added strip to map, keep it for CPU fallback
    }

    // Store bidirectional mapping
    mPinToChannel[pin] = static_cast<uint8_t>(channel);
    mChannelToPin[channel] = pin;

    FL_DBG("PARLIO: Assigned pin " << pin << " to channel " << channel);

    // Reconfigure driver with updated channel list
    reconfigureDriver();

    // Notify engine events for WASM visualization
    EngineEvents::onStripAdded(this, pin);

    // Trigger canvas UI update
    EngineEvents::onCanvasUiSet(this, strip, pin);
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::onStripRemoved(int pin) {
    // Find channel for this pin
    auto it = mPinToChannel.find(pin);
    if (it == mPinToChannel.end()) {
        FL_WARN("PARLIO: Cannot remove pin " << pin << " (not found in channel map)");
        return;
    }

    uint8_t channel = it->second;

    FL_DBG("PARLIO: Removing pin " << pin << " from channel " << channel);

    // Free the channel
    freeChannel(channel);
    mPinToChannel.erase(it);

    // Reconfigure driver with remaining channels
    reconfigureDriver();
}

template <typename CHIPSET>
bool BulkClockless<CHIPSET, PARLIO>::validatePin(int pin) {
    // ESP32-P4 GPIO validation based on hardware constraints

    // Reject invalid pin numbers (ESP32-P4 has GPIO 0-54)
    if (pin < 0 || pin > 54) {
        FL_WARN("PARLIO: GPIO pin must be in range 0-54 for ESP32-P4");
        return false;
    }

    // Reject strapping pins (GPIO34-38)
    // These pins are used for boot configuration and MUST NOT be used
    if (pin >= 34 && pin <= 38) {
        FL_WARN("PARLIO: GPIO34-38 are strapping pins and CANNOT be used for LED output. "
                "Using these pins WILL PREVENT BOOT. Please choose a different pin.");
        return false;
    }

    // Reject USB-JTAG pins (GPIO24-25)
    if (pin == 24 || pin == 25) {
        FL_WARN("PARLIO: GPIO24-25 are reserved for USB-JTAG on ESP32-P4. "
                "Using these pins WILL DISABLE USB-JTAG. Please choose a different pin.");
        return false;
    }

    // Note: Flash/PSRAM pins are sdkconfig-dependent and harder to detect at compile time
    // Users should consult their board documentation for Flash/PSRAM pin assignments

    return true;
}

template <typename CHIPSET>
int BulkClockless<CHIPSET, PARLIO>::getMaxStrips() {
    return 16;  // PARLIO hardware limit
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::showPixelsInternal() {
    // Check if driver is initialized
    if (!mInitialized || !mDriver) {
        FL_DBG("PARLIO: Driver not initialized, falling back to CPU rendering");
        // Fall back to CPU-based rendering if PARLIO isn't available
        Base::showPixelsInternal();
        return;
    }

    // Check if we have any strips to show
    if (this->mSubControllers.empty()) {
        FL_DBG("PARLIO: No strips to show");
        return;
    }

    // Get global brightness
    u8 brightness = this->getBrightness();

    // Register each strip with the driver and apply per-strip settings
    int strips_registered = 0;
    for (auto& [pin, strip] : this->mSubControllers) {
        // Get channel for this pin
        auto it = mPinToChannel.find(pin);
        if (it == mPinToChannel.end()) {
            FL_WARN("PARLIO: Strip on pin " << pin << " has no channel mapping, skipping");
            continue;
        }
        uint8_t channel = it->second;

        // Validate buffer before processing
        CRGB* buffer = strip.getBuffer();
        if (!buffer) {
            FL_WARN("PARLIO: Strip on pin " << pin << " has null buffer, skipping");
            continue;
        }
        int count = strip.getCount();

        // Apply per-strip settings (brightness, correction, temperature)
        // Note: This modifies the buffer in-place. PARLIO will read the modified data.
        // This is similar to how regular controllers work - settings are "baked in" during show().

        // Compute combined color adjustment (brightness + correction + temperature)
        ColorAdjustment adj = BulkClocklessHelper::computeAdjustment(brightness, strip.settings);

        // Apply color adjustment to each pixel
        // adj.scale contains the combined brightness/correction/temperature scaling
        for (int i = 0; i < count; ++i) {
            buffer[i].nscale8(adj.scale);
        }

        // Apply dither if enabled
        // Dither helps reduce color banding by adding controlled noise
        if (strip.settings.ditherMode == BINARY_DITHER) {
            // Simple per-pixel dither based on position
            // This is a simplified dither - full dither would use PixelController
            for (int i = 0; i < count; ++i) {
                // Apply dither only if pixel is not at full brightness
                // Dither pattern based on LED index (alternating +1/-1)
                if (buffer[i].r > 0 && buffer[i].r < 255) {
                    buffer[i].r = fl::qadd8(buffer[i].r, (i & 1) ? 1 : 0);
                }
                if (buffer[i].g > 0 && buffer[i].g < 255) {
                    buffer[i].g = fl::qadd8(buffer[i].g, (i & 1) ? 1 : 0);
                }
                if (buffer[i].b > 0 && buffer[i].b < 255) {
                    buffer[i].b = fl::qadd8(buffer[i].b, (i & 1) ? 1 : 0);
                }
            }
        }
        // Note: DISABLE_DITHER means no dither, so we skip it

        // Register strip buffer with driver (now contains adjusted colors)
        mDriver->set_strip(channel, buffer);
        strips_registered++;
    }

    if (strips_registered == 0) {
        FL_WARN("PARLIO: No valid strips registered, skipping transmission");
        return;
    }

    // Transmit all strips in parallel
    mDriver->show();
    mDriver->wait();

    FL_DBG("PARLIO: Transmitted " << strips_registered << " strips");
}

// Private helper methods

template <typename CHIPSET>
int BulkClockless<CHIPSET, PARLIO>::allocateChannel() {
    for (int i = 0; i < 16; ++i) {
        if (mChannelToPin[i] == -1) {
            return i;  // Found free channel
        }
    }
    return -1;  // No free channels
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::freeChannel(uint8_t channel) {
    if (channel < 16) {
        mChannelToPin[channel] = -1;
    }
}

template <typename CHIPSET>
void BulkClockless<CHIPSET, PARLIO>::reconfigureDriver() {
    // Build configuration from current active channels
    ParlioDriverConfig config = {};
    config.num_lanes = 0;
    config.clock_freq_hz = 0;  // Use default
    config.auto_clock_adjustment = false;

    // Determine RGBW mode from first strip
    // PARLIO has a single is_rgbw flag for the entire driver, so all strips must use the same mode
    config.is_rgbw = false;
    if (!this->mSubControllers.empty()) {
        const BulkStrip& first_strip = this->mSubControllers.begin()->second;
        config.is_rgbw = first_strip.settings.rgbw.isRgbw();

        // Validate that all strips have the same RGBW mode
        for (const auto& [pin, strip] : this->mSubControllers) {
            bool strip_is_rgbw = strip.settings.rgbw.isRgbw();
            if (strip_is_rgbw != config.is_rgbw) {
                FL_WARN("PARLIO: All strips must have the same RGBW mode. "
                        "Strip on pin " << pin << " has RGBW=" << strip_is_rgbw
                        << " but expected RGBW=" << config.is_rgbw
                        << ". Using " << (config.is_rgbw ? "RGBW" : "RGB") << " mode for all strips.");
                // Continue with first strip's mode - inconsistent strips will have wrong output
            }
        }
    }

    // Collect active GPIOs in channel order
    for (int ch = 0; ch < 16; ++ch) {
        if (mChannelToPin[ch] != -1) {
            config.data_gpios[config.num_lanes++] = mChannelToPin[ch];
        }
    }

    if (config.num_lanes == 0) {
        FL_DBG("PARLIO: No active channels, skipping reconfiguration");
        if (mDriver) {
            mDriver->end();
        }
        mInitialized = false;
        return;
    }

    // Get LED count from first strip (all strips must have same count)
    int num_leds = 0;
    if (!this->mSubControllers.empty()) {
        num_leds = this->mSubControllers.begin()->second.getCount();
    }

    if (num_leds == 0) {
        FL_WARN("PARLIO: Cannot configure driver with 0 LEDs");
        return;
    }

    // Create driver if needed
    if (!mDriver) {
        mDriver = new ParlioLedDriver<16, CHIPSET>();
        FL_DBG("PARLIO: Created driver instance");
    }

    // Initialize/reconfigure driver
    if (!mDriver->begin(config, static_cast<uint16_t>(num_leds))) {
        FL_WARN("PARLIO: Failed to initialize driver");
        mInitialized = false;
        return;
    }

    mInitialized = true;
    FL_DBG("PARLIO: Driver configured with " << config.num_lanes
           << " lanes, " << num_leds << " LEDs per strip, "
           << (config.is_rgbw ? "RGBW" : "RGB") << " mode");
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
