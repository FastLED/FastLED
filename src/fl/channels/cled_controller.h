#pragma once

/// @file fl/channels/cled_controller.h
/// base definitions used by led controllers for writing out led data

#include "color.h"
#include "pixel_controller.h"  // IWYU pragma: keep  (ColorAdjustment)

#include "fl/stl/compiler_control.h"
#include "dither_mode.h"
#include "fl/system/engine_events.h"
#include "fl/math/screenmap.h"
#include "fl/stl/int.h"
#include "fl/stl/bit_cast.h"
#include "fl/channels/legacy_settings.h"
#include "fl/log/log.h"
#include "platforms/is_platform.h"  // for FL_IS_ESP32, FL_IS_AVR
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"
#include "fl/spi_bus.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED Controller interface definition
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Base definition for an LED controller.  Pretty much the methods that every LED controller object will make available.
/// If you want to pass LED controllers around to methods, make them references to this type, keeps your code saner. However,
/// most people won't be seeing/using these objects directly at all.
/// @note That the methods for eventual checking of background writing of data (I'm looking at you, Teensy 3.0 DMA controller!)
/// are not yet implemented.

namespace fl {

class Channel;

namespace detail {
/// Lets the power limiter charge a controller's RGBW conversion. Called by
/// CLEDController::applyRgbw -- the one path in-tree that stores an Rgbw into
/// a controller -- and by Channel when its options carry one, so a sketch
/// that never configures RGBW does not link the conversion into the limiter.
/// Defined in power_mgt.cpp.hpp.
void enable_rgbw_power_estimate() FL_NO_EXCEPT;
}  // namespace detail

class CLEDController {
protected:
    friend class CFastLED;
    fl::span<CRGB> mLeds;     ///< span of LED data used by this controller
    CLEDController *mPNext = nullptr;   ///< pointer to the next LED controller in the linked list
    LegacySettings mLegacySettings;
    // Bit-fields share one byte; a fixed-white flag costs no RAM and no
    // vtable slot on AVR (#4788).
    bool mEnabled : 1;
    bool mFixedWhiteChannel : 1;  ///< chipset wire format owns the white policy

    enum class SettingsChange : u8 { Correction, Temperature, Dither, White };
#ifndef FL_IS_AVR
    // Derived APIs can react to settings changed through a base reference.
    // AVR has neither this slot nor a notification call.
    virtual void onSettingsChanged(SettingsChange change) FL_NO_EXCEPT {
        FL_UNUSED(change);
    }
#endif
    void settingsChanged(SettingsChange change) FL_NO_EXCEPT {
#ifndef FL_IS_AVR
        onSettingsChanged(change);
#else
        FL_UNUSED(change);
#endif
    }
    static CLEDController *mPHead;  ///< pointer to the first LED controller in the linked list
    static CLEDController *mPTail;  ///< pointer to the last LED controller in the linked list

    /// @brief Registration mode for constructor
    enum class RegistrationMode {
        AutoRegister,   ///< Automatically add to linked list (default, backward compatible)
        DeferRegister   ///< Defer registration until addToList() is called
    };

    /// @brief Protected constructor with registration mode
    /// @param mode Registration mode (AutoRegister or DeferRegister)
    /// @note Subclasses can use DeferRegister to control when they join the linked list.
    ///       `fl::Channel` uses DeferRegister and joins the draw list when
    ///       `FastLED.add(ChannelPtr)` calls `addToList()`.
    CLEDController(RegistrationMode mode) FL_NO_EXCEPT;

    void applyRgbw(const Rgbw& arg) FL_NO_EXCEPT {
        if (!arg.active()) {
            mLegacySettings.mWhiteCfg.reset();
        } else {
            prepare_rgbw_colorimetric(arg);
            detail::enable_rgbw_power_estimate();
            mLegacySettings.mWhiteCfg = arg;
        }
    }

    /// Configure a chipset-owned RGBW policy that public white-channel
    /// setters cannot replace.
    void setFixedRgbw(const Rgbw& arg) FL_NO_EXCEPT {
        applyRgbw(arg);
        mFixedWhiteChannel = true;
    }

    bool rejectFixedWhiteChannelChange(const char* operation) const FL_NO_EXCEPT {
        FL_UNUSED(operation);  // only consumed by FL_WARN, a no-op on small platforms
        if (!mFixedWhiteChannel) {
            return false;
        }
        FL_WARN("Chipset has fixed R,G,B,W output; " << operation << " is unsupported and was ignored");
        return true;
    }

public:
#ifndef FL_IS_AVR
    // Identify Channels without retaining Channels configuration in this base.
    // Standalone Channels need the same integration as registered Channels.
    virtual const Channel* asChannel() const FL_NO_EXCEPT { return nullptr; }
#endif
    /// Select an ESP-IDF SPI host for this clocked controller before init().
    /// Non-ESP32 and non-SPI controllers ignore this setting. Only ESP32 SPI
    /// controllers override it, so other platforms carry no vtable slot (#4788).
#if defined(FL_IS_ESP32)
    virtual
#endif
    CLEDController &setSpiBus(Esp32SpiBus bus) FL_NO_EXCEPT {
        (void)bus;
        return *this;
    }
    /// @brief Add this controller to the linked list
    /// @note Used with DeferRegister mode to explicitly add controller to list
    ///       (for example, `fl::Channel` via `FastLED.add(ChannelPtr)`)
    /// @note Safe to call multiple times - won't add if already in list
    void addToList() FL_NO_EXCEPT;

    /// @brief Check if this controller is in the linked list
    /// @return true if controller is in the list, false otherwise
    bool isInList() const FL_NO_EXCEPT;

    /// Set all the LEDs to a given color. 
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale the rgb scaling value for outputting color
    virtual void showColor(const CRGB & data, int nLeds, fl::u8 brightness) FL_NO_EXCEPT = 0;

    /// Write the passed in RGB data out to the LEDs managed by this controller. 
    /// @param data the rgb data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale the rgb scaling to apply to each led before writing it out
    virtual void show(const CRGB *data, int nLeds, fl::u8 brightness) FL_NO_EXCEPT = 0;

    CLEDController& setRgbw(const Rgbw& arg = RgbwDefault::value()) FL_NO_EXCEPT {
        if (rejectFixedWhiteChannelChange("setRgbw()")) {
            return *this;
        }
        // (#2558) mLegacySettings.mWhiteCfg is now a fl::variant<Empty, Rgbw, Rgbww>;
        // assigning Rgbw selects the 4-channel alternative. The legacy
        // "setRgbw(RgbwInvalid::value()) → disable" semantics are preserved
        // by translating an inactive Rgbw into Empty so observers see the
        // same "no white channel" state they did before the variant migration.
        applyRgbw(arg);
        settingsChanged(SettingsChange::White);
        return *this;  // builder pattern.
    }

    /// @brief Configure this channel for 5-channel RGBWW (RGB + warm-W + cool-W)
    /// output. See issue #2558. Driver-side support arrives in later phases of
    /// the RGBWW work; today this just sets the configuration alternative.
    /// Symmetric with setRgbw: passing RgbwwInvalid::value() clears the channel
    /// to plain RGB rather than storing an inactive Rgbww.
    CLEDController& setRgbww(const Rgbww& arg = RgbwwDefault::value()) FL_NO_EXCEPT {
        if (rejectFixedWhiteChannelChange("setRgbww()")) {
            return *this;
        }
        if (!arg.active()) {
            mLegacySettings.mWhiteCfg.reset();
        } else {
            mLegacySettings.mWhiteCfg = arg;
        }
        settingsChanged(SettingsChange::White);
        return *this;
    }

    /// @brief Reset this channel to plain 3-channel RGB (clears any RGBW/RGBWW
    /// configuration). Equivalent to assigning an empty mWhiteCfg.
    CLEDController& clearWhiteChannel() FL_NO_EXCEPT {
        if (rejectFixedWhiteChannelChange("clearWhiteChannel()")) {
            return *this;
        }
        mLegacySettings.mWhiteCfg.reset();
        settingsChanged(SettingsChange::White);
        return *this;
    }

    void setEnabled(bool enabled) FL_NO_EXCEPT { mEnabled = enabled; }
    bool getEnabled() FL_NO_EXCEPT { return mEnabled; }

    CLEDController() FL_NO_EXCEPT;
    // If we added virtual to the AVR boards then we are going to add 600 bytes of memory to the binary
    // flash size. This is because the virtual destructor pulls in malloc and free, which are the largest
    // Testing shows that this virtual destructor adds a 600 bytes to the binary on
    // attiny85 and about 1k for the teensy 4.X series.
    // Attiny85:
    //   With CLEDController destructor virtual: 11018 bytes to binary.
    //   Without CLEDController destructor virtual: 10666 bytes to binary.
    VIRTUAL_IF_NOT_AVR ~CLEDController() FL_NO_EXCEPT;

    /// @return The Rgbw configuration if this channel is in 4-channel mode,
    /// otherwise RgbwInvalid::value(). Backward-compatible with the pre-#2558
    /// API: callers that don't know about Rgbww see the same shape as before.
    Rgbw getRgbw() const FL_NO_EXCEPT { return mLegacySettings.rgbw(); }

    /// @return The stored Rgbw configuration, or nullptr when the channel
    /// holds none. Unlike getRgbw() it does not copy the value, so a caller
    /// on the show path does not take and drop a reference on its profile.
    /// A subclass that writes mLegacySettings.mWhiteCfg itself must also call
    /// detail::enable_rgbw_power_estimate(), or the limiter charges it as RGB.
    const Rgbw* rgbwConfig() const FL_NO_EXCEPT {
        return mLegacySettings.mWhiteCfg.ptr<Rgbw>();
    }

    /// @return The Rgbww configuration if this channel is in 5-channel mode,
    /// otherwise RgbwwInvalid::value().
    Rgbww getRgbww() const FL_NO_EXCEPT { return mLegacySettings.rgbww(); }

    /// Initialize the LED controller
    virtual void init() FL_NO_EXCEPT = 0;

    /// Clear out/zero out the given number of LEDs.
    /// @param nLeds the number of LEDs to clear
    VIRTUAL_IF_NOT_AVR void clearLeds(int nLeds = -1) FL_NO_EXCEPT {
        clearLedDataInternal(nLeds);
        showLeds(0);
    }

    // Compatibility with the 3.8.x codebase.
    VIRTUAL_IF_NOT_AVR void showLeds(fl::u8 brightness) FL_NO_EXCEPT {
        fl::EngineEvents::onBeginFrame();
        void* data = beginShowLeds(mLeds.size());
        showLedsInternal(brightness);
        endShowLeds(data);
        fl::EngineEvents::onEndFrame();
#if FASTLED_HAS_ENGINE_EVENTS
        fl::EngineEvents::onEndShowLeds();
#endif
    }

    ColorAdjustment getAdjustmentData(fl::u8 brightness) FL_NO_EXCEPT;

    /// @copybrief show(const CRGB*, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the LED data to write to the strip
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see show(const CRGB*, int, CRGB)
    void showInternal(const CRGB *data, int nLeds, fl::u8 brightness) FL_NO_EXCEPT {
        if (getEnabled()) {
           show(data, nLeds,brightness);
        }
    }

    /// @copybrief showColor(const CRGB&, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const CRGB&, int, CRGB)
    void showColorInternal(const CRGB &data, int nLeds, fl::u8 brightness) FL_NO_EXCEPT {
        if (getEnabled()) {
            showColor(data, nLeds, brightness);
        }
    }

    /// Write the data to the LEDs managed by this controller
    /// @param brightness the brightness of the LEDs
    /// @see show(const CRGB*, int, fl::u8)
    void showLedsInternal(fl::u8 brightness) FL_NO_EXCEPT {
        if (getEnabled()) {
            show(mLeds.data(), mLeds.size(), brightness);
        }
    }

    /// @copybrief showColor(const CRGB&, int, CRGB)
    ///
    /// @param data the CRGB color to set the LEDs to
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const CRGB&, int, CRGB)
    void showColorInternal(const CRGB & data, fl::u8 brightness) FL_NO_EXCEPT {
        if (getEnabled()) {
            showColor(data, mLeds.size(), brightness);
        }
    }

    /// Get the first LED controller in the linked list of controllers
    /// @returns CLEDController::mPHead
    static CLEDController *head() FL_NO_EXCEPT { return mPHead; }

    /// Get the next controller in the linked list after this one.  Will return nullptr at the end of the linked list.
    /// @returns CLEDController::mPNext
    CLEDController *next() FL_NO_EXCEPT { return mPNext; }

    /// Visit all controllers in the linked list with a visitor
    /// The visitor must be a callable that accepts (const CLEDController*, fl::span<const CRGB>)
    /// @param visitor the visitor callable to call for each controller
    /// @tparam Visitor callable type (function, lambda, functor, etc.)
    template<typename Visitor>
    static void visitControllers(Visitor&& visitor) FL_NO_EXCEPT {
        const CLEDController *pCur = head();
        while(pCur) {
            visitor(pCur, fl::span<const CRGB>(pCur->leds(), pCur->size()));
            pCur = pCur->next();
        }
    }

    /// Get the next controller in the linked list after this one (const version).  Will return nullptr at the end of the linked list.
    /// @returns CLEDController::mPNext
    const CLEDController *next() const FL_NO_EXCEPT { return mPNext; }

    /// Set the default array of LEDs to be used by this controller
    /// @param data pointer to the LED data
    /// @param nLeds the number of LEDs in the LED data
    CLEDController & setLeds(CRGB *data, int nLeds) FL_NO_EXCEPT {
        mLeds = fl::span<CRGB>(data, nLeds);
        return *this;
    }

    /// Set the default array of LEDs to be used by this controller (span version)
    /// @param leds span of LED data
    CLEDController & setLeds(fl::span<CRGB> leds) FL_NO_EXCEPT {
        mLeds = leds;
        return *this;
    }

    /// Zero out the LED data managed by this controller
    void clearLedDataInternal(int nLeds = -1) FL_NO_EXCEPT;

    /// Remove this controller from the draw list
    /// @note Safe to call at any time - controllers currently drawing are protected by ownership
    void removeFromDrawList() FL_NO_EXCEPT {
        removeFromList(this);
    }

    /// Remove a controller from the linked list
    /// @param controller The controller to remove from the list
    /// @note Protected static method - subclasses can call this in their cleanup methods
    static void removeFromList(CLEDController* controller) FL_NO_EXCEPT;

    /// How many LEDs does this controller manage?
    /// @returns CLEDController::mLeds.size()
    virtual int size() const FL_NO_EXCEPT { return mLeds.size(); }

    /// How many Lanes does this controller manage?
    /// @returns 1 for a non-Parallel controller
    virtual int lanes() FL_NO_EXCEPT { return 1; }

    /// Pointer to the CRGB array for this controller
    /// @returns CLEDController::mLeds.data()
    CRGB* leds() FL_NO_EXCEPT { return mLeds.data(); }

    /// Const pointer to the CRGB array for this controller
    /// @returns CLEDController::mLeds.data()
    const CRGB* leds() const FL_NO_EXCEPT { return mLeds.data(); }

    /// Span of LEDs managed by this controller
    /// @returns CLEDController::mLeds
    fl::span<CRGB> ledsSpan() FL_NO_EXCEPT { return mLeds; }

    /// Reference to the n'th LED managed by the controller
    /// @param x the LED number to retrieve
    /// @returns reference to CLEDController::mLeds[x]
    CRGB &operator[](int x) FL_NO_EXCEPT { return mLeds[x]; }

    /// Set the dithering mode for this controller to use
    /// @param ditherMode the dithering mode to set
    /// @returns a reference to the controller
    inline CLEDController & setDither(fl::u8 ditherMode = BINARY_DITHER) FL_NO_EXCEPT { mLegacySettings.mDitherMode = ditherMode; settingsChanged(SettingsChange::Dither); return *this; }

    CLEDController& setScreenMap(const fl::XYMap& map, float diameter = -1.f) FL_NO_EXCEPT {
        // EngineEvents::onCanvasUiSet(this, map);
        fl::ScreenMap screenmap = map.toScreenMap();
        if (diameter <= 0.0f) {
            screenmap.setDiameter(.15f); // Assume small matrix is being used.
        }
        fl::EngineEvents::onCanvasUiSet(this, screenmap);
        return *this;
    }

    CLEDController& setScreenMap(const fl::ScreenMap& map) FL_NO_EXCEPT {
        fl::EngineEvents::onCanvasUiSet(this, map);
        return *this;
    }

    CLEDController& setScreenMap(fl::u16 width, fl::u16 height, float diameter = -1.f) FL_NO_EXCEPT {
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(width, height);
        return setScreenMap(xymap, diameter);
    }

    /// Get the dithering option currently set for this controller
    /// @return the currently set dithering option (CLEDController::mLegacySettings.mDitherMode)
    inline fl::u8 getDither() const FL_NO_EXCEPT { return mLegacySettings.mDitherMode; }

    virtual void* beginShowLeds(int size) FL_NO_EXCEPT {
        FASTLED_UNUSED(size);
        // By default, emit an integer. This integer will, by default, be passed back.
        // If you override beginShowLeds() then
        // you should also override endShowLeds() to match the return state.
        //
        // For async led controllers this should be used as a sync point to block
        // the caller until the leds from the last draw frame have completed drawing.
        // for each controller:
        //   beginShowLeds();
        // for each controller:
        //   showLeds();
        // for each controller:
        //   endShowLeds();
        uintptr_t d = getDither();
        void* out = fl::int_to_ptr<void>(d);
        return out;
    }

    virtual void endShowLeds(void* data) FL_NO_EXCEPT {
        // By default recieves the integer that beginShowLeds() emitted.
        //For async controllers this should be used to signal the controller
        // to begin transmitting the current frame to the leds.
        uintptr_t d = fl::ptr_to_int(data);
        setDither(static_cast<fl::u8>(d));
    }

    /// The color corrction to use for this controller, expressed as a CRGB object
    /// @param correction the color correction to set
    /// @returns a reference to the controller
    CLEDController & setCorrection(CRGB correction) FL_NO_EXCEPT {
        mLegacySettings.mCorrection = correction;
        settingsChanged(SettingsChange::Correction);
        return *this;
    }

    /// @copydoc setCorrection()
    CLEDController & setCorrection(LEDColorCorrection correction) FL_NO_EXCEPT {
        mLegacySettings.mCorrection = correction;
        settingsChanged(SettingsChange::Correction);
        return *this;
    }

    /// Get the correction value used by this controller
    /// @returns the current color correction (CLEDController::mLegacySettings.mCorrection)
    CRGB getCorrection() FL_NO_EXCEPT { return mLegacySettings.mCorrection; }

    /// Set the color temperature, aka white point, for this controller
    /// @param temperature the color temperature to set
    /// @returns a reference to the controller
    CLEDController & setTemperature(CRGB temperature) FL_NO_EXCEPT {
        mLegacySettings.mTemperature = temperature;
        settingsChanged(SettingsChange::Temperature);
        return *this;
    }

    /// @copydoc setTemperature()
    CLEDController & setTemperature(ColorTemperature temperature) FL_NO_EXCEPT {
        mLegacySettings.mTemperature = temperature;
        settingsChanged(SettingsChange::Temperature);
        return *this;
    }

    /// Get the color temperature, aka white point, for this controller
    /// @returns the current color temperature (CLEDController::mLegacySettings.mTemperature)
    CRGB getTemperature() FL_NO_EXCEPT { return mLegacySettings.mTemperature; }

    /// Get the combined brightness/color adjustment for this controller
    /// @param scale the brightness scale to get the correction for
    /// @returns a CRGB object representing the total adjustment, including color correction and color temperature
    CRGB getAdjustment(fl::u8 scale) FL_NO_EXCEPT {
        return CRGB::computeAdjustment(scale, mLegacySettings.mCorrection, mLegacySettings.mTemperature);
    }

    /// Gets the maximum possible refresh rate of the strip
    /// @returns the maximum refresh rate, in frames per second (FPS)
    virtual fl::u16 getMaxRefreshRate() const FL_NO_EXCEPT { return 0; }
};

#ifdef FL_IS_AVR
// Legacy controllers must stay compact even when Channels gains new options.
FL_STATIC_ASSERT(sizeof(CLEDController) <= 32, "Legacy AVR controller grew beyond its settings budget");
#endif

}  // namespace fl
