#pragma once

#include "constants.h"

/// @file fl/clockless/base.h
/// @brief Base template class for bulk LED controllers with dynamic strip
/// management
///
/// The BulkClockless API allows you to manage multiple LED strips using shared
/// hardware peripherals like LCD_I80, RMT, I2S, or SPI. This enables efficient
/// multi-strip control with per-strip color correction, temperature adjustment,
/// dithering, and RGBW support.
///
/// @par Example Usage (brace-init):
/// @code
/// CRGB strip1[100], strip2[100];
/// auto& bulk = FastLED.addClocklessLeds<Chipset::WS2812, fl::GRB, RMT>({
///     {2, strip1, 100, ScreenMap()},
///     {4, strip2, 100, ScreenMap()}
/// });
/// bulk.setCorrection(TypicalLEDStrip);
/// bulk.get(2)->setTemperature(Tungsten100W);  // Per-strip override
/// @endcode
///
/// @par Example Usage (runtime arrays):
/// @code
/// vector<BulkStripConfig> configs;
/// configs.push_back({2, strip1, 100, ScreenMap()});
/// configs.push_back({4, strip2, 100, ScreenMap()});
/// auto& bulk = FastLED.addClocklessLeds<Chipset::WS2812, fl::GRB, RMT>(
///     span<const BulkStripConfig>(configs.data(), configs.size()));
/// @endcode
///
/// @par Thread Safety:
/// This class is NOT thread-safe. Do not call add() or remove() while show() is
/// running.
///
/// @par Memory Management:
/// User owns LED buffers. Buffers must remain valid for the lifetime of the
/// controller.

#include "color.h"
#include "cpixel_ledcontroller.h"
#include "fl/clockless/peripheral_tags.h"
#include "fl/clockless/bulk_strip.h"
#include "fl/dbg.h"
#include "fl/engine_events.h"
#include "fl/initializer_list.h"
#include "fl/map.h"
#include "fl/move.h"
#include "fl/screenmap.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "rgbw.h"

namespace fl {

/// Configuration structure for adding strips to BulkClockless controller
struct BulkStripConfig {
    int pin;              ///< GPIO pin number for this strip
    CRGB *buffer;         ///< Non-owning pointer to LED data (user-owned)
    int count;            ///< Number of LEDs in this strip
    ScreenMap screenmap;  ///< Spatial positioning map for this strip

    /// Constructor
    BulkStripConfig(int p, CRGB *buf, int cnt, const ScreenMap &map)
        : pin(p), buffer(buf), count(cnt), screenmap(map) {}
};

/// Helper struct for BulkClockless - provides shared functionality between
/// base template and platform specializations
struct BulkClocklessHelper {
    /// Compute color adjustment for a strip
    /// @param brightness global brightness value
    /// @param settings per-strip settings
    /// @returns ColorAdjustment structure
    static ColorAdjustment computeAdjustment(u8 brightness,
                                      const BulkStrip::Settings &settings);

    /// Apply color adjustment to a buffer of pixels
    /// Handles both legacy and HD color mixing modes automatically
    /// @param buffer pointer to LED data buffer
    /// @param count number of LEDs in the buffer
    /// @param adj color adjustment (brightness + correction + temperature)
    static void applyAdjustmentToBuffer(CRGB* buffer, int count,
                                        const ColorAdjustment& adj);
};

/// @brief Base template class for bulk LED controllers.
///
/// Manages multiple LED strips using shared hardware peripherals (LCD_I80, RMT,
/// SPI, I2S, PARLIO). Each peripheral has different capabilities:
///
/// @par Peripheral Capabilities:
/// - **RMT** (ESP32): Flexible, per-channel configuration, up to 8 strips
/// - **LCD_I80** (ESP32-S3/P4): Parallel output, uniform settings, up to 16
/// strips
/// - **I2S** (ESP32/S3): Parallel output, uniform settings, up to 24 strips
/// (ESP32) or 16 (S3)
/// - **PARLIO** (ESP32-P4): Parallel output, per-strip settings, up to 16
/// strips
/// - **SPI** (various): Similar to LCD_I80 (implementation pending)
///
/// @par Key Features:
/// - Dynamic add/remove of strips at runtime
/// - Per-strip settings (color correction, temperature, dither, RGBW)
/// - ScreenMap integration for spatial positioning
/// - Seamless integration with FastLED.show() pipeline
///
/// @par Peripheral Resource Limits:
/// Each hardware peripheral has a maximum strip count. Attempting to add more
/// strips than the limit will fail with a warning message. Check peripheral
/// documentation for specific limits.
///
/// @par Usage Pattern:
/// 1. Create controller with initializer list via FastLED.addClocklessLeds()
/// 2. Optionally set global settings via setCorrection(), setTemperature(),
/// etc.
/// 3. Optionally override per-strip settings via get(pin)->setCorrection(),
/// etc.
/// 4. Update LED buffers in your code
/// 5. Call FastLED.show() to output to all strips
/// 6. Optionally add/remove strips dynamically (NOT during show())
///
/// @par Settings Propagation Model:
///
/// Global settings (setCorrection, setTemperature, setDither, setRgbw) affect
/// FUTURE strips only. To modify existing strips, use updateAllSettings() or
/// per-strip settings.
///
/// @code
/// auto& bulk = FastLED.addClocklessLeds<WS2812, fl::GRB, RMT>({
///     {2, strip1, 100, map},  // Gets default settings
///     {4, strip2, 100, map}   // Gets default settings
/// });
///
/// bulk.setCorrection(TypicalLEDStrip);  // Future strips only!
/// // strip1 and strip2 still use UncorrectedColor
///
/// // To update ALL strips (existing + future):
/// bulk.updateAllSettings().setCorrection(TypicalLEDStrip);
/// // Now strip1, strip2, AND future strips use TypicalLEDStrip
///
/// // To update a single strip:
/// bulk.get(2)->setCorrection(TypicalSMD5050);
/// // Only strip on pin 2 uses TypicalSMD5050
/// @endcode
///
/// @warning **CRITICAL**: Do NOT call add() or remove() during FastLED.show().
/// Wait for show() to complete before modifying strip configuration.
///
/// @tparam CHIPSET LED chipset type (e.g., Chipset::WS2812)
/// @tparam RGB_ORDER RGB color channel ordering (e.g., RGB, GRB, BRG)
/// @tparam PERIPHERAL Hardware peripheral type (e.g., LCD_I80, RMT, I2S)
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
class BulkClockless : public CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK> {
  public:
    /// Constructor with initializer list
    /// @param strips initializer list of strip configurations
    BulkClockless(initializer_list<BulkStripConfig> strips);

    /// Constructor with span (for runtime arrays/vectors)
    /// @param strips span of strip configurations
    BulkClockless(span<const BulkStripConfig> strips);

    /// @brief Copy constructor (deleted)
    ///
    /// BulkClockless manages hardware peripheral resources that cannot be safely
    /// copied. Use move semantics instead.
    BulkClockless(const BulkClockless &) = delete;

    /// @brief Copy assignment operator (deleted)
    ///
    /// BulkClockless manages hardware peripheral resources that cannot be safely
    /// copied. Use move semantics instead.
    BulkClockless &operator=(const BulkClockless &) = delete;

    /// @brief Move constructor
    ///
    /// Transfers ownership of peripheral resources from another controller.
    /// The source controller is left in a valid but empty state.
    ///
    /// @param other controller to move from
    BulkClockless(BulkClockless &&other) noexcept;

    /// @brief Move assignment operator
    ///
    /// Transfers ownership of peripheral resources from another controller.
    /// Existing strips are removed before the move. The source controller is
    /// left in a valid but empty state.
    ///
    /// @param other controller to move from
    /// @returns reference to this controller
    BulkClockless &operator=(BulkClockless &&other) noexcept;

    /// @brief Add a new strip to the controller dynamically
    ///
    /// Adds a new LED strip on the specified pin. The strip inherits default
    /// settings from the controller but can be individually configured via the
    /// returned pointer.
    ///
    /// @par Error Conditions:
    /// Returns nullptr and logs FL_WARN if:
    /// - Pin is invalid for the target platform
    /// - Pin is already in use by another strip
    /// - Maximum strip count for peripheral exceeded
    ///
    /// @warning Returns nullptr on error. Always check the return value:
    /// @code
    /// auto* strip = bulk.add(pin, buffer, count, map);
    /// if (!strip) {
    ///     // Handle error: invalid pin, duplicate, or limit exceeded
    ///     Serial.println("ERROR: Failed to add strip");
    ///     return;
    /// }
    /// // Safe to use strip pointer
    /// strip->setCorrection(TypicalLEDStrip);
    /// @endcode
    ///
    /// @par Settings Inheritance:
    /// The new strip inherits current default settings (correction,
    /// temperature, dither, RGBW). To override, use:
    /// `bulk->get(pin)->setCorrection(...)` after adding.
    ///
    /// @warning Do NOT call during FastLED.show() - wait for completion first.
    ///
    /// @param pin GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned, must remain
    /// valid)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    /// @returns pointer to sub-controller on success, nullptr on error
    BulkStrip *add(int pin, CRGB *buffer, int count,
                   const ScreenMap &screenmap);

    /// @brief Remove a strip from the controller dynamically
    ///
    /// Removes the LED strip associated with the given pin. Peripheral
    /// resources are released and the pin becomes available for reuse.
    ///
    /// @warning Do NOT call during FastLED.show() - wait for completion first.
    ///
    /// @param pin GPIO pin number of strip to remove
    /// @returns true if strip was removed, false if pin not found
    bool remove(int pin);

    /// @brief Get sub-controller for a specific pin (const version)
    ///
    /// Retrieves the sub-controller managing the strip on the specified pin.
    /// Use this to read or modify per-strip settings.
    ///
    /// @warning Returns nullptr if pin not found. Always check the return value:
    /// @code
    /// const auto* strip = bulk.get(9);
    /// if (!strip) {
    ///     Serial.println("ERROR: Strip on pin 9 not found");
    ///     return;
    /// }
    /// // Safe to access strip settings
    /// auto correction = strip->settings.correction;
    /// @endcode
    ///
    /// @param pin GPIO pin number
    /// @returns const pointer to sub-controller, or nullptr if not found
    const BulkStrip *get(int pin) const;

    /// @brief Get sub-controller for a specific pin (non-const version)
    ///
    /// Retrieves the sub-controller managing the strip on the specified pin.
    /// Use this to read or modify per-strip settings.
    ///
    /// @warning Returns nullptr if pin not found. Always check the return value:
    /// @code
    /// auto* strip = bulk.get(9);
    /// if (!strip) {
    ///     Serial.println("ERROR: Strip on pin 9 not found");
    ///     return;
    /// }
    /// // Safe to modify strip settings
    /// strip->setCorrection(TypicalLEDStrip);
    /// @endcode
    ///
    /// @param pin GPIO pin number
    /// @returns pointer to sub-controller, or nullptr if not found
    BulkStrip *get(int pin);

    /// Get total number of LEDs across all strips
    /// @returns total LED count summed across all managed strips
    /// @note This overrides CLEDController::size() to maintain Liskov
    /// Substitution Principle
    int size() const noexcept override;

    /// Get number of active strips
    /// @returns number of strips currently managed by this controller
    int stripCount() const noexcept;

    /// Check if a pin is being used
    /// @param pin GPIO pin number to check
    /// @returns true if pin has an active strip
    bool has(int pin) const noexcept;

    /// @brief Get all active pin numbers
    ///
    /// Returns a vector containing all GPIO pin numbers currently managing LED
    /// strips. Useful for iterating over strips or checking which pins are in
    /// use.
    ///
    /// @par Example:
    /// @code
    /// auto pins = bulk.getAllPins();
    /// for (int pin : pins) {
    ///     Serial.print("Strip on pin ");
    ///     Serial.println(pin);
    ///     auto* strip = bulk.get(pin);
    ///     // Configure strip...
    /// }
    /// @endcode
    ///
    /// @returns vector of active pin numbers (unordered)
    vector<int> getAllPins() const;

    /// @brief Remove all strips from the controller
    ///
    /// Removes all LED strips and releases all peripheral resources. Pins
    /// become available for reuse. This is useful for complete controller reset
    /// or reconfiguration.
    ///
    /// @warning Do NOT call during FastLED.show() - wait for completion first.
    ///
    /// @par Example:
    /// @code
    /// int count = bulk.removeAll();
    /// Serial.print("Removed ");
    /// Serial.print(count);
    /// Serial.println(" strips");
    /// @endcode
    ///
    /// @returns number of strips that were removed
    int removeAll();

    /// @brief Iterator support - begin iterator
    ///
    /// Enables range-based for loops over all strips. Each iteration yields a
    /// pair of (pin, BulkStrip&).
    ///
    /// @par Example:
    /// @code
    /// for (auto& [pin, strip] : bulk) {
    ///     Serial.print("Configuring pin ");
    ///     Serial.println(pin);
    ///     strip.setCorrection(TypicalLEDStrip);
    /// }
    /// @endcode
    ///
    /// @returns iterator to first strip
    typename fl_map<int, BulkStrip>::iterator begin() noexcept {
        return mSubControllers.begin();
    }

    /// @brief Iterator support - end iterator
    /// @returns iterator past last strip
    typename fl_map<int, BulkStrip>::iterator end() noexcept {
        return mSubControllers.end();
    }

    /// @brief Iterator support - begin iterator (const version)
    /// @returns const iterator to first strip
    typename fl_map<int, BulkStrip>::const_iterator begin() const noexcept {
        return mSubControllers.begin();
    }

    /// @brief Iterator support - end iterator (const version)
    /// @returns const iterator past last strip
    typename fl_map<int, BulkStrip>::const_iterator end() const noexcept {
        return mSubControllers.end();
    }

    /// Get default settings for newly added strips
    /// @returns const reference to default LED settings
    const LEDSettings &getDefaultSettings() const noexcept { return mDefaultSettings; }

    /// @brief Proxy class for updating settings on ALL strips (existing +
    /// future)
    ///
    /// This class provides a fluent interface for bulk updates to all strips.
    /// It updates both existing strips AND the default settings for future
    /// strips.
    ///
    /// @par Usage:
    /// @code
    /// bulk.updateAllSettings()
    ///     .setCorrection(TypicalLEDStrip)
    ///     .setTemperature(Tungsten100W)
    ///     .setDither(BINARY_DITHER);
    /// @endcode
    class AllStripsProxy {
        BulkClockless &bulk;

      public:
        AllStripsProxy(BulkClockless &b) : bulk(b) {}

        /// Set color correction for ALL strips (existing + future)
        AllStripsProxy &setCorrection(CRGB correction);

        /// Set color temperature for ALL strips (existing + future)
        AllStripsProxy &setTemperature(CRGB temperature);

        /// Set dither mode for ALL strips (existing + future)
        AllStripsProxy &setDither(u8 ditherMode);

        /// Set RGBW mode for ALL strips (existing + future)
        AllStripsProxy &setRgbw(const Rgbw &rgbw);
    };

    /// @brief Update settings for ALL strips (existing + future)
    ///
    /// Returns a proxy object that allows fluent chaining of setting updates.
    /// Unlike the regular setters (setCorrection, etc.) which only affect newly
    /// added strips, this method updates both existing strips AND defaults.
    ///
    /// @par Example - Regular setters (future strips only):
    /// @code
    /// auto& bulk = FastLED.addClocklessLeds<WS2812, fl::GRB, RMT>({
    ///     {2, strip1, 100, map},  // Uses default correction
    ///     {4, strip2, 100, map}   // Uses default correction
    /// });
    /// bulk.setCorrection(TypicalLEDStrip);  // Only affects future strips!
    /// // strip1 and strip2 still use uncorrected color
    /// @endcode
    ///
    /// @par Example - updateAllSettings (all strips):
    /// @code
    /// auto& bulk = FastLED.addClocklessLeds<WS2812, fl::GRB, RMT>({
    ///     {2, strip1, 100, map},
    ///     {4, strip2, 100, map}
    /// });
    /// bulk.updateAllSettings().setCorrection(TypicalLEDStrip);  // Updates
    /// ALL!
    /// // strip1, strip2, AND future strips now use TypicalLEDStrip
    /// @endcode
    ///
    /// @returns proxy object for method chaining
    AllStripsProxy updateAllSettings() { return AllStripsProxy(*this); }

    /// @brief Set global color correction default for new strips
    ///
    /// Sets the default color correction that will be inherited by newly added
    /// strips. Does NOT affect existing strips - use get(pin)->setCorrection()
    /// to change those.
    ///
    /// @param correction color correction to apply (e.g., TypicalLEDStrip)
    /// @returns reference to this controller for method chaining
    BulkClockless &setCorrection(CRGB correction);

    /// @brief Set global color temperature default for new strips
    ///
    /// Sets the default color temperature that will be inherited by newly added
    /// strips. Does NOT affect existing strips - use get(pin)->setTemperature()
    /// to change those.
    ///
    /// @param temperature color temperature to apply (e.g., Tungsten100W,
    /// Candle)
    /// @returns reference to this controller for method chaining
    BulkClockless &setTemperature(CRGB temperature);

    /// @brief Set global dither mode default for new strips
    ///
    /// Sets the default dither mode that will be inherited by newly added
    /// strips. Does NOT affect existing strips - use get(pin)->setDither() to
    /// change those.
    ///
    /// @param ditherMode dither mode to apply (e.g., BINARY_DITHER,
    /// DISABLE_DITHER)
    /// @returns reference to this controller for method chaining
    BulkClockless &setDither(u8 ditherMode);

    /// @brief Set global RGBW configuration default for new strips
    ///
    /// Sets the default RGBW mode that will be inherited by newly added strips.
    /// Does NOT affect existing strips - use get(pin)->setRgbw() to change
    /// those.
    ///
    /// @param arg RGBW configuration to apply (e.g., RgbwDefault::value())
    /// @returns reference to this controller for method chaining
    BulkClockless &setRgbw(const Rgbw &arg);

    /// Initialize the controller
    void init() override { initPeripheral(); }

    /// Show single color (not typically used for bulk controllers)
    void showColor(const CRGB &data, int nLeds, u8 brightness) override;

    /// Show LED data (override from CPixelLEDController)
    void showPixels(PixelController<RGB_ORDER, 1, ALL_LANES_MASK> &pixels) override;

  protected:
    /// Peripheral-specific initialization
    ///
    /// Default implementation: CPU fallback (warning-only stub for now)
    /// Peripheral specializations override this method.
    virtual void initPeripheral();

    /// Peripheral-specific strip addition hook (optional override)
    virtual void onStripAdded(int pin) {
        // Default: no-op
        (void)pin;
    }

    /// Peripheral-specific strip removal hook (optional override)
    virtual void onStripRemoved(int pin) {
        // Default: no-op
        (void)pin;
    }

    /// Validate pin for platform
    ///
    /// Default implementation: Permissive validation (allows any positive pin
    /// number) Peripheral specializations override with platform-specific
    /// validation.
    ///
    /// @param pin GPIO pin number to validate
    /// @returns true if pin is valid, false otherwise
    virtual bool validatePin(int pin);

    /// Get maximum number of strips supported by peripheral
    ///
    /// Default implementation: 64 strips (MAX_CLED_CONTROLLERS limit)
    /// Peripheral specializations override with their specific limits.
    ///
    /// @returns maximum strip count
    virtual int getMaxStrips();

    /// Show all strips via peripheral
    ///
    /// Default implementation: CPU fallback (stub - does nothing for now)
    /// Peripheral specializations override with actual hardware output.
    ///
    /// @note CPU fallback is not yet fully implemented. This method currently
    /// does nothing. Future enhancement will output strips sequentially via
    /// individual CLEDController instances.
    virtual void showPixelsInternal();

  private:
    /// Common initialization logic for both constructors
    /// @param strips span of strip configurations
    void initFromSpan(span<const BulkStripConfig> strips);

    /// Map of pin number to sub-controller
    fl_map<int, BulkStrip> mSubControllers;

    /// Default settings for new strips
    BulkStrip::Settings mDefaultSettings;
};

// ============================================================================
// Template Method Implementations
// ============================================================================

// Common initialization helper
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
void BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::initFromSpan(span<const BulkStripConfig> strips) {
    // Calculate total LED count across all strips
    int totalLeds = 0;
    for (const auto &config : strips) {
        totalLeds += config.count;
    }

    // Initialize base class members
    // BulkClockless doesn't use a single buffer (each strip has its own)
    this->m_Data = nullptr;
    this->m_nLeds = totalLeds;

    // Initialize default settings from base class
    mDefaultSettings = this->mSettings;

    // Add all strips
    for (const auto &config : strips) {
        add(config.pin, config.buffer, config.count, config.screenmap);
    }
}

// Constructor with initializer list
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::BulkClockless(initializer_list<BulkStripConfig> strips)
    : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>() {
    // Delegate to span constructor via temporary vector
    vector<BulkStripConfig> configs(strips.begin(), strips.end());
    initFromSpan(span<const BulkStripConfig>(configs.data(), configs.size()));
}

// Constructor with span
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::BulkClockless(span<const BulkStripConfig> strips)
    : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>() {
    initFromSpan(strips);
}

// Move constructor
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::BulkClockless(BulkClockless &&other) noexcept
    : CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>(fl::move(other)),
      mSubControllers(fl::move(other.mSubControllers)),
      mDefaultSettings(fl::move(other.mDefaultSettings)) {
    // Base class move handled by base constructor
    // other.mSubControllers is now empty (moved from)
    // other.mDefaultSettings is now default-constructed (moved from)
}

// Move assignment operator
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL> &
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::operator=(BulkClockless &&other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        removeAll();

        // Move base class state
        CPixelLEDController<RGB_ORDER, 1, ALL_LANES_MASK>::operator=(
            fl::move(other));

        // Move members
        mSubControllers = fl::move(other.mSubControllers);
        mDefaultSettings = fl::move(other.mDefaultSettings);

        // other is now in a valid but empty state
    }
    return *this;
}

// Add method
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkStrip *BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::add(int pin, CRGB *buffer, int count,
                                                     const ScreenMap &screenmap) {
    // 1. Validate pin for platform
    if (!validatePin(pin)) {
        FL_WARN("BulkClockless: Invalid pin " << pin << " for platform");
        return nullptr;
    }

    // 2. Check duplicate
    if (mSubControllers.find(pin) != mSubControllers.end()) {
        FL_WARN("BulkClockless: Pin " << pin << " already in use");
        return nullptr;
    }

    // 3. Check max strips
    if (static_cast<int>(mSubControllers.size()) >= getMaxStrips()) {
        FL_WARN("BulkClockless: Max strips (" << getMaxStrips()
                                              << ") exceeded");
        return nullptr;
    }

    // 4. Create sub-controller (uses move semantics to transfer tracker ID)
    BulkStrip sub(pin, buffer, count, screenmap);
    sub.settings = mDefaultSettings;
    mSubControllers[pin] = fl::move(sub);

    // 5. Notify engine about new strip (for WASM visualization and other listeners)
    fl::EngineEvents::onStripAdded(this, count);

    // 6. Update ScreenMap binding
    fl::EngineEvents::onCanvasUiSet(this, screenmap);

    // 7. Peripheral-specific initialization (if needed)
    onStripAdded(pin);

    return &mSubControllers[pin];
}

// Remove method
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
bool BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::remove(int pin) {
    auto it = mSubControllers.find(pin);
    if (it == mSubControllers.end()) {
        return false;
    }

    // Peripheral-specific cleanup (if needed)
    onStripRemoved(pin);

    // Remove from map
    mSubControllers.erase(it);
    return true;
}

// Get (const version)
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
const BulkStrip *BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::get(int pin) const {
    auto it = mSubControllers.find(pin);
    if (it == mSubControllers.end()) {
        return nullptr;
    }
    return &it->second;
}

// Get (non-const version)
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkStrip *BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::get(int pin) {
    auto it = mSubControllers.find(pin);
    if (it == mSubControllers.end()) {
        return nullptr;
    }
    return &it->second;
}

// Size
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
int BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::size() const noexcept {
    int total = 0;
    for (const auto &pair : mSubControllers) {
        total += pair.second.getCount();
    }
    return total;
}

// Strip count
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
int BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::stripCount() const noexcept {
    return static_cast<int>(mSubControllers.size());
}

// Has
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
bool BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::has(int pin) const noexcept {
    return mSubControllers.find(pin) != mSubControllers.end();
}

// Get all pins
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
vector<int> BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::getAllPins() const {
    vector<int> pins;
    pins.reserve(mSubControllers.size());
    for (const auto &pair : mSubControllers) {
        pins.push_back(pair.first);
    }
    return pins;
}

// Remove all
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
int BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::removeAll() {
    int count = static_cast<int>(mSubControllers.size());
    // Call peripheral cleanup for each strip before clearing
    for (const auto &pair : mSubControllers) {
        onStripRemoved(pair.first);
    }
    mSubControllers.clear();
    return count;
}

// Set correction
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL> &BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::setCorrection(CRGB correction) {
    CLEDController::setCorrection(correction);
    mDefaultSettings.correction = correction;
    return *this;
}

// Set temperature
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL> &BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::setTemperature(CRGB temperature) {
    CLEDController::setTemperature(temperature);
    mDefaultSettings.temperature = temperature;
    return *this;
}

// Set dither
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL> &BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::setDither(u8 ditherMode) {
    CLEDController::setDither(ditherMode);
    mDefaultSettings.ditherMode = ditherMode;
    return *this;
}

// Set RGBW
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL> &BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::setRgbw(const Rgbw &arg) {
    CLEDController::setRgbw(arg);
    mDefaultSettings.rgbw = arg;
    return *this;
}

// Show color
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
void BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::showColor(const CRGB &data, int nLeds, u8 brightness) {
    // Default implementation: show color to all strips
    // Parameter unused: Each strip in BulkClockless has its own LED count tracked
    // in BulkStrip. The nLeds parameter comes from CLEDController interface but
    // doesn't apply to our multi-strip architecture.
    (void)nLeds;

    for (auto &pair : mSubControllers) {
        BulkStrip &sub = pair.second;
        ColorAdjustment adj = BulkClocklessHelper::computeAdjustment(brightness, sub.settings);
        PixelController<RGB_ORDER, 1, ALL_LANES_MASK> pixels(
            data, sub.getCount(), adj, sub.settings.ditherMode);
        showPixels(pixels);
    }
}

// Show pixels
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
void BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::showPixels(PixelController<RGB_ORDER, 1, ALL_LANES_MASK> &pixels) {
    // Parameter unused: BulkClockless manages multiple independent buffers, each
    // requiring its own PixelController with per-strip settings. The global
    // PixelController passed by base class doesn't apply to our multi-strip
    // architecture - we create per-strip PixelControllers in showPixelsInternal().
    (void)pixels;

    // This is called by base class show() method
    // For BulkClockless, we need to iterate all sub-controllers
    // Note: In practice, showLedsInternal() is the better override point
    showPixelsInternal();
}

// Init peripheral
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
void BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::initPeripheral() {
    // CPU fallback: Warn user that peripheral is not available
    // Include specific peripheral name in warning message
    FL_WARN("BulkClockless: " << PeripheralName<PERIPHERAL>::name()
                              << " peripheral not available on this "
                                 "platform, using CPU fallback");
    FL_WARN("BulkClockless: CPU fallback is not yet fully implemented - "
            "strips will not output");
}

// Validate pin
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
bool BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::validatePin(int pin) {
    // CPU fallback: Accept any reasonable pin number
    // Platform-specific specializations will have stricter validation
    return pin >= 0 && pin < MAX_GPIO_PIN;
}

// Get max strips
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
int BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::getMaxStrips() {
    // CPU fallback: Use MAX_CLED_CONTROLLERS as limit
    return 64;
}

// Show pixels internal
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
void BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::showPixelsInternal() {
    // CPU fallback: Iterate all strips and update their trackers
    // This enables WASM visualization and provides basic CPU fallback
    for (auto &pair : mSubControllers) {
        BulkStrip &strip = pair.second;
        // Convert CRGB buffer to RGB byte span for tracker
        const uint8_t *data = reinterpret_cast<const uint8_t *>(strip.mBuffer);
        size_t size = strip.mCount * 3; // 3 bytes per CRGB (RGB)
        strip.mTracker.update(fl::span<const uint8_t>(data, size));
    }
}

// AllStripsProxy::setCorrection
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
typename BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy &
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy::setCorrection(CRGB correction) {
    for (auto &pair : bulk.mSubControllers) {
        pair.second.settings.correction = correction;
    }
    bulk.mDefaultSettings.correction = correction;
    bulk.CLEDController::setCorrection(correction);
    return *this;
}

// AllStripsProxy::setTemperature
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
typename BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy &
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy::setTemperature(CRGB temperature) {
    for (auto &pair : bulk.mSubControllers) {
        pair.second.settings.temperature = temperature;
    }
    bulk.mDefaultSettings.temperature = temperature;
    bulk.CLEDController::setTemperature(temperature);
    return *this;
}

// AllStripsProxy::setDither
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
typename BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy &
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy::setDither(u8 ditherMode) {
    for (auto &pair : bulk.mSubControllers) {
        pair.second.settings.ditherMode = ditherMode;
    }
    bulk.mDefaultSettings.ditherMode = ditherMode;
    bulk.CLEDController::setDither(ditherMode);
    return *this;
}

// AllStripsProxy::setRgbw
template <typename CHIPSET, EOrder RGB_ORDER, typename PERIPHERAL>
typename BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy &
BulkClockless<CHIPSET, RGB_ORDER, PERIPHERAL>::AllStripsProxy::setRgbw(const Rgbw &rgbw) {
    for (auto &pair : bulk.mSubControllers) {
        pair.second.settings.rgbw = rgbw;
    }
    bulk.mDefaultSettings.rgbw = rgbw;
    bulk.CLEDController::setRgbw(rgbw);
    return *this;
}

} // namespace fl
