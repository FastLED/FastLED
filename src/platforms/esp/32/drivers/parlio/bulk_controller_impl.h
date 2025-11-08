/// @file bulk_controller_impl.h
/// @brief Concrete implementation class for BulkClockless PARLIO controllers
///
/// This class contains all the heavy lifting for managing multiple LED strips:
/// - Strip management (add/remove/get)
/// - Pin validation
/// - Settings management
/// - Multi-strip pixel iteration and transmission
///
/// The template BulkClockless<CHIPSET, PARLIO> is a thin wrapper that converts
/// compile-time CHIPSET types to runtime ChipsetTimingConfig and delegates to this class.
///
/// @note This is an internal ESP32 platform header. Do not include directly.

#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "fl/map.h"
#include "fl/clockless/bulk_strip.h"
#include "fl/initializer_list.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "pixeltypes.h"
#include "fl/screenmap.h"

namespace fl {

// Forward declarations
class IParlioTransmitter;
class PixelIterator;

/// @brief Concrete implementation class for BulkClockless PARLIO controllers
///
/// This class contains all the heavy lifting for managing multiple LED strips:
/// - Strip management (add/remove/get)
/// - Pin validation
/// - Settings management
/// - Multi-strip pixel iteration and transmission
///
/// The template BulkClockless<CHIPSET, PARLIO> is a thin wrapper that converts
/// compile-time CHIPSET types to runtime ChipsetTimingConfig and delegates to this class.
class BulkControllerImpl {
public:
    /// Maximum number of strips supported by PARLIO peripheral
    static constexpr int MAX_STRIPS = 16;

    /// Constructor
    /// @param transmitter Reference to the PARLIO transmitter for this chipset
    /// @param defaultSettings Initial default settings for new strips
    explicit BulkControllerImpl(IParlioTransmitter& transmitter,
                               const BulkStrip::Settings& defaultSettings);

    /// @brief Add a new strip to the controller
    ///
    /// Validates the pin, creates a BulkStrip descriptor, and registers with transmitter.
    ///
    /// @param pin GPIO pin number for this strip
    /// @param buffer non-owning pointer to LED data (user-owned)
    /// @param count number of LEDs in this strip
    /// @param screenmap spatial positioning map for this strip
    /// @returns pointer to sub-controller on success, nullptr on error
    BulkStrip* add(int pin, CRGB* buffer, int count, const ScreenMap& screenmap);

    /// @brief Remove a strip from the controller
    ///
    /// @param pin GPIO pin number of strip to remove
    /// @returns true if strip was removed, false if not found
    bool remove(int pin);

    /// Get sub-controller for a specific pin (const version)
    const BulkStrip* get(int pin) const;

    /// Get sub-controller for a specific pin (non-const version)
    BulkStrip* get(int pin);

    /// Get total number of LEDs across all strips
    int size() const;

    /// Get number of active strips
    int stripCount() const;

    /// Check if a pin is being used
    bool has(int pin) const;

    /// Update default settings for new strips
    void setDefaultSettings(const BulkStrip::Settings& settings);

    /// Get default settings
    const BulkStrip::Settings& getDefaultSettings() const { return mDefaultSettings; }

    /// Set color correction for new strips
    void setCorrection(CRGB correction);

    /// Set color temperature for new strips
    void setTemperature(CRGB temperature);

    /// Set dither mode for new strips
    void setDither(u8 ditherMode);

    /// Set RGBW configuration for new strips
    void setRgbw(const Rgbw& rgbw);

    /// @brief Show all strips via PARLIO peripheral
    ///
    /// This method delegates to IParlioTransmitter after applying per-strip settings.
    ///
    /// @param brightness Global brightness value
    void showPixels(u8 brightness);

    /// Get all active pin numbers
    vector<int> getAllPins() const;

    /// Remove all strips from the controller
    int removeAll();

    /// Begin iterator
    fl_map<int, BulkStrip>::iterator begin() { return mSubControllers.begin(); }

    /// End iterator
    fl_map<int, BulkStrip>::iterator end() { return mSubControllers.end(); }

    /// Begin const iterator
    fl_map<int, BulkStrip>::const_iterator begin() const { return mSubControllers.begin(); }

    /// End const iterator
    fl_map<int, BulkStrip>::const_iterator end() const { return mSubControllers.end(); }

private:
    /// @brief Validate GPIO pin for ESP32-P4 PARLIO peripheral
    bool validatePin(int pin) const;

    /// Reference to the singleton transmitter for this chipset
    IParlioTransmitter& mTransmitter;

    /// Map of pin → sub-controller
    fl_map<int, BulkStrip> mSubControllers;

    /// Default settings for new strips
    BulkStrip::Settings mDefaultSettings;
};

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
