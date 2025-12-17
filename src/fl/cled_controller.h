#pragma once


/// @file cled_controller.h
/// base definitions used by led controllers for writing out led data

#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "color.h"

#include "fl/force_inline.h"
#include "fl/unused.h"
#include "pixel_controller.h"
#include "dither_mode.h"
#include "pixel_iterator.h"
#include "fl/engine_events.h"
#include "fl/screenmap.h"
#include "fl/virtual_if_not_avr.h"
#include "fl/int.h"
#include "fl/stl/bit_cast.h"
#include "fl/channels/options.h"
#include "fl/stl/span.h"



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

class CLEDController {
protected:
    friend class CFastLED;
    fl::span<CRGB> m_Leds;     ///< span of LED data used by this controller
    CLEDController *m_pNext;   ///< pointer to the next LED controller in the linked list
    ChannelOptions mSettings;  ///< Optional channel settings (correction, temperature, dither, rgbw, affinity)
    bool m_enabled = true;
    static CLEDController *m_pHead;  ///< pointer to the first LED controller in the linked list
    static CLEDController *m_pTail;  ///< pointer to the last LED controller in the linked list

public:

    /// Set all the LEDs to a given color. 
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale the rgb scaling value for outputting color
    virtual void showColor(const CRGB & data, int nLeds, fl::u8 brightness) = 0;

    /// Write the passed in RGB data out to the LEDs managed by this controller. 
    /// @param data the rgb data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale the rgb scaling to apply to each led before writing it out
    virtual void show(const CRGB *data, int nLeds, fl::u8 brightness) = 0;

    CLEDController& setRgbw(const Rgbw& arg = RgbwDefault::value()) {
        // Note that at this time (Sept 13th, 2024) this is only implemented in the ESP32 driver
        // directly. For an emulated version please see RGBWEmulatedController in chipsets.h
        mSettings.mRgbw = arg;
        return *this;  // builder pattern.
    }

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool getEnabled() { return m_enabled; }

    CLEDController();
    // If we added virtual to the AVR boards then we are going to add 600 bytes of memory to the binary
    // flash size. This is because the virtual destructor pulls in malloc and free, which are the largest
    // Testing shows that this virtual destructor adds a 600 bytes to the binary on
    // attiny85 and about 1k for the teensy 4.X series.
    // Attiny85:
    //   With CLEDController destructor virtual: 11018 bytes to binary.
    //   Without CLEDController destructor virtual: 10666 bytes to binary.
    VIRTUAL_IF_NOT_AVR ~CLEDController();

    Rgbw getRgbw() const { return mSettings.mRgbw; }

    /// Initialize the LED controller
    virtual void init() = 0;

    /// Clear out/zero out the given number of LEDs.
    /// @param nLeds the number of LEDs to clear
    VIRTUAL_IF_NOT_AVR void clearLeds(int nLeds = -1) {
        clearLedDataInternal(nLeds);
        showLeds(0);
    }

    // Compatibility with the 3.8.x codebase.
    VIRTUAL_IF_NOT_AVR void showLeds(fl::u8 brightness) {
        void* data = beginShowLeds(m_Leds.size());
        showLedsInternal(brightness);
        endShowLeds(data);
    }

    ColorAdjustment getAdjustmentData(fl::u8 brightness);

    /// @copybrief show(const CRGB*, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the LED data to write to the strip
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see show(const CRGB*, int, CRGB)
    void showInternal(const CRGB *data, int nLeds, fl::u8 brightness) {
        if (m_enabled) {
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
    void showColorInternal(const CRGB &data, int nLeds, fl::u8 brightness) {
        if (m_enabled) {
            showColor(data, nLeds, brightness);
        }
    }

    /// Write the data to the LEDs managed by this controller
    /// @param brightness the brightness of the LEDs
    /// @see show(const CRGB*, int, fl::u8)
    void showLedsInternal(fl::u8 brightness) {
        if (m_enabled) {
            show(m_Leds.data(), m_Leds.size(), brightness);
        }
    }

    /// @copybrief showColor(const CRGB&, int, CRGB)
    ///
    /// @param data the CRGB color to set the LEDs to
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const CRGB&, int, CRGB)
    void showColorInternal(const CRGB & data, fl::u8 brightness) {
        if (m_enabled) {
            showColor(data, m_Leds.size(), brightness);
        }
    }

    /// Get the first LED controller in the linked list of controllers
    /// @returns CLEDController::m_pHead
    static CLEDController *head() { return m_pHead; }

    /// Get the next controller in the linked list after this one.  Will return nullptr at the end of the linked list.
    /// @returns CLEDController::m_pNext
    CLEDController *next() { return m_pNext; }

    /// Set the default array of LEDs to be used by this controller
    /// @param data pointer to the LED data
    /// @param nLeds the number of LEDs in the LED data
    CLEDController & setLeds(CRGB *data, int nLeds) {
        m_Leds = fl::span<CRGB>(data, nLeds);
        return *this;
    }

    /// Set the default array of LEDs to be used by this controller (span version)
    /// @param leds span of LED data
    CLEDController & setLeds(fl::span<CRGB> leds) {
        m_Leds = leds;
        return *this;
    }

    /// Zero out the LED data managed by this controller
    void clearLedDataInternal(int nLeds = -1);

    /// Remove this controller from the draw list
    /// @note Safe to call at any time - controllers currently drawing are protected by ownership
    void removeFromDrawList() {
        removeFromList(this);
    }

    /// Remove a controller from the linked list
    /// @param controller The controller to remove from the list
    /// @note Protected static method - subclasses can call this in their cleanup methods
    static void removeFromList(CLEDController* controller);

    /// How many LEDs does this controller manage?
    /// @returns CLEDController::m_Leds.size()
    virtual int size() const { return m_Leds.size(); }

    /// How many Lanes does this controller manage?
    /// @returns 1 for a non-Parallel controller
    virtual int lanes() { return 1; }

    /// Pointer to the CRGB array for this controller
    /// @returns CLEDController::m_Leds.data()
    CRGB* leds() { return m_Leds.data(); }

    /// Span of LEDs managed by this controller
    /// @returns CLEDController::m_Leds
    fl::span<CRGB> ledsSpan() { return m_Leds; }

    /// Reference to the n'th LED managed by the controller
    /// @param x the LED number to retrieve
    /// @returns reference to CLEDController::m_Leds[x]
    CRGB &operator[](int x) { return m_Leds[x]; }

    /// Set the dithering mode for this controller to use
    /// @param ditherMode the dithering mode to set
    /// @returns a reference to the controller
    inline CLEDController & setDither(fl::u8 ditherMode = BINARY_DITHER) { mSettings.mDitherMode = ditherMode; return *this; }

    CLEDController& setScreenMap(const fl::XYMap& map, float diameter = -1.f) {
        // EngineEvents::onCanvasUiSet(this, map);
        fl::ScreenMap screenmap = map.toScreenMap();
        if (diameter <= 0.0f) {
            screenmap.setDiameter(.15f); // Assume small matrix is being used.
        }
        fl::EngineEvents::onCanvasUiSet(this, screenmap);
        return *this;
    }

    CLEDController& setScreenMap(const fl::ScreenMap& map) {
        fl::EngineEvents::onCanvasUiSet(this, map);
        return *this;
    }

    CLEDController& setScreenMap(fl::u16 width, fl::u16 height, float diameter = -1.f) {
        fl::XYMap xymap = fl::XYMap::constructRectangularGrid(width, height);
        return setScreenMap(xymap, diameter);
    }

    /// Get the dithering option currently set for this controller
    /// @return the currently set dithering option (CLEDController::mSettings.mDitherMode)
    inline fl::u8 getDither() { return mSettings.mDitherMode; }

    virtual void* beginShowLeds(int size) {
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

    virtual void endShowLeds(void* data) {
        // By default recieves the integer that beginShowLeds() emitted.
        //For async controllers this should be used to signal the controller
        // to begin transmitting the current frame to the leds.
        uintptr_t d = fl::ptr_to_int(data);
        setDither(static_cast<fl::u8>(d));
    }

    /// The color corrction to use for this controller, expressed as a CRGB object
    /// @param correction the color correction to set
    /// @returns a reference to the controller
    CLEDController & setCorrection(CRGB correction) { mSettings.mCorrection = correction; return *this; }

    /// @copydoc setCorrection()
    CLEDController & setCorrection(LEDColorCorrection correction) { mSettings.mCorrection = correction; return *this; }

    /// Get the correction value used by this controller
    /// @returns the current color correction (CLEDController::mSettings.mCorrection)
    CRGB getCorrection() { return mSettings.mCorrection; }

    /// Set the color temperature, aka white point, for this controller
    /// @param temperature the color temperature to set
    /// @returns a reference to the controller
    CLEDController & setTemperature(CRGB temperature) { mSettings.mTemperature = temperature; return *this; }

    /// @copydoc setTemperature()
    CLEDController & setTemperature(ColorTemperature temperature) { mSettings.mTemperature = temperature; return *this; }

    /// Get the color temperature, aka white point, for this controller
    /// @returns the current color temperature (CLEDController::mSettings.mTemperature)
    CRGB getTemperature() { return mSettings.mTemperature; }

    /// Get the combined brightness/color adjustment for this controller
    /// @param scale the brightness scale to get the correction for
    /// @returns a CRGB object representing the total adjustment, including color correction and color temperature
    CRGB getAdjustment(fl::u8 scale) {
        return CRGB::computeAdjustment(scale, mSettings.mCorrection, mSettings.mTemperature);
    }

    /// Gets the maximum possible refresh rate of the strip
    /// @returns the maximum refresh rate, in frames per second (FPS)
    virtual fl::u16 getMaxRefreshRate() const { return 0; }
};

}  // namespace fl
