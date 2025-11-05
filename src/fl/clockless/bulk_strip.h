#pragma once

/// @file fl/clockless/bulk_strip.h
/// Strip descriptor for BulkClockless - manages per-strip settings and data
///
/// BulkStrip is NOT a controller - it's a configuration/settings holder that
/// describes an individual LED strip within a bulk controller.

#include "color.h"
#include "dither_mode.h"
#include "fl/led_settings.h"
#include "fl/screenmap.h"
#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "rgbw.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"

namespace fl {

/// Forward declaration
template <typename CHIPSET, typename PERIPHERAL> class BulkClockless;

/// Strip descriptor for individual LED strips within a BulkClockless controller.
///
/// BulkStrip is NOT a controller itself - it's a configuration/settings holder
/// that combines:
/// - Per-strip settings (color correction, temperature, dither, RGBW)
/// - Reference to LED buffer (non-owning pointer)
/// - ScreenMap for spatial positioning
/// - Pin number identifier
///
/// BulkStrip objects are value objects stored in a map within BulkClockless.
/// Settings can be modified via chainable setter methods.
///
/// @note This is distinct from fl::ClocklessController, which is an actual
/// controller class with a show() method. BulkStrip is purely a descriptor.
class BulkStrip {
  public:
    /// Per-strip settings structure (public for save/restore pattern in API.md)
    using Settings = LEDSettings;

    /// Public settings member (for API.md save/restore pattern)
    Settings settings;

    /// Default constructor
    BulkStrip();

    /// Constructor with initialization
    /// @param pin the GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    BulkStrip(int pin, CRGB *buffer, int count,
              const fl::ScreenMap &screenmap);

    /// Set color correction for this strip
    /// @param correction the color correction to apply
    /// @returns reference to this strip descriptor for chaining
    BulkStrip &setCorrection(CRGB correction);

    /// @copydoc setCorrection(CRGB)
    BulkStrip &setCorrection(LEDColorCorrection correction);

    /// Set color temperature for this strip
    /// @param temperature the color temperature to apply
    /// @returns reference to this strip descriptor for chaining
    BulkStrip &setTemperature(CRGB temperature);

    /// @copydoc setTemperature(CRGB)
    BulkStrip &setTemperature(ColorTemperature temperature);

    /// Set dither mode for this strip
    /// @param ditherMode the dither mode to apply
    /// @returns reference to this strip descriptor for chaining
    BulkStrip &setDither(fl::u8 ditherMode);

    /// Set RGBW configuration for this strip
    /// @param arg the RGBW configuration to apply
    /// @returns reference to this strip descriptor for chaining
    BulkStrip &setRgbw(const Rgbw &arg);

    /// Set ScreenMap for this strip
    /// @param map the ScreenMap to apply
    /// @returns reference to this strip descriptor for chaining
    BulkStrip &setScreenMap(const fl::ScreenMap &map);

    /// Get the pin number for this strip
    /// @returns the GPIO pin number
    int getPin() const;

    /// Get the LED buffer pointer (non-owning)
    /// @returns pointer to LED data
    CRGB *getBuffer() const;

    /// Get the number of LEDs in this strip
    /// @returns LED count
    int getCount() const;

    /// Get the ScreenMap for this strip
    /// @returns const reference to ScreenMap
    const fl::ScreenMap &getScreenMap() const;

  private:
    int mPin;                 ///< GPIO pin number (identifier)
    CRGB *mBuffer;            ///< Non-owning pointer to LED data
    int mCount;               ///< Number of LEDs in this strip
    fl::ScreenMap mScreenMap; ///< Spatial positioning map
    ActiveStripTracker mTracker; ///< Tracker for LED data capture (WASM visualization & CPU fallback)

    // Allow BulkClockless to access private members
    template <typename CHIPSET, typename PERIPHERAL> friend class BulkClockless;
};

} // namespace fl
