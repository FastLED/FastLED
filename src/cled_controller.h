#pragma once


/// @file cled_controller.h
/// base definitions used by led controllers for writing out led data

#include "FastLED.h"
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
#include "fl/bit_cast.h"

FASTLED_NAMESPACE_BEGIN


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
class CLEDController {
protected:
    friend class CFastLED;
    CRGB *m_Data;              ///< pointer to the LED data used by this controller
    CLEDController *m_pNext;   ///< pointer to the next LED controller in the linked list
    CRGB m_ColorCorrection;    ///< CRGB object representing the color correction to apply to the strip on show()  @see setCorrection
    CRGB m_ColorTemperature;   ///< CRGB object representing the color temperature to apply to the strip on show() @see setTemperature
    EDitherMode m_DitherMode;  ///< the current dither mode of the controller
    bool m_enabled = true;
    int m_nLeds;               ///< the number of LEDs in the LED data array
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
    virtual void show(const struct CRGB *data, int nLeds, fl::u8 brightness) = 0;


    Rgbw mRgbMode = RgbwInvalid::value();
    CLEDController& setRgbw(const Rgbw& arg = RgbwDefault::value()) {
        // Note that at this time (Sept 13th, 2024) this is only implemented in the ESP32 driver
        // directly. For an emulated version please see RGBWEmulatedController in chipsets.h
        mRgbMode = arg;
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

    Rgbw getRgbw() const { return mRgbMode; }

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
        void* data = beginShowLeds(m_nLeds);
        showLedsInternal(brightness);
        endShowLeds(data);
    }

    ColorAdjustment getAdjustmentData(fl::u8 brightness);

    /// @copybrief show(const struct CRGB*, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the LED data to write to the strip
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see show(const struct CRGB*, int, CRGB)
    void showInternal(const struct CRGB *data, int nLeds, fl::u8 brightness) {
        if (m_enabled) {
           show(data, nLeds,brightness);
        }
    }

    /// @copybrief showColor(const struct CRGB&, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const struct CRGB&, int, CRGB)
    void showColorInternal(const struct CRGB &data, int nLeds, fl::u8 brightness) {
        if (m_enabled) {
            showColor(data, nLeds, brightness);
        }
    }

    /// Write the data to the LEDs managed by this controller
    /// @param brightness the brightness of the LEDs
    /// @see show(const struct CRGB*, int, fl::u8)
    void showLedsInternal(fl::u8 brightness) {
        if (m_enabled) {
            show(m_Data, m_nLeds, brightness);
        }
    }

    /// @copybrief showColor(const struct CRGB&, int, CRGB)
    ///
    /// @param data the CRGB color to set the LEDs to
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const struct CRGB&, int, CRGB)
    void showColorInternal(const struct CRGB & data, fl::u8 brightness) {
        if (m_enabled) {
            showColor(data, m_nLeds, brightness);
        }
    }

    /// Get the first LED controller in the linked list of controllers
    /// @returns CLEDController::m_pHead
    static CLEDController *head() { return m_pHead; }

    /// Get the next controller in the linked list after this one.  Will return NULL at the end of the linked list.
    /// @returns CLEDController::m_pNext
    CLEDController *next() { return m_pNext; }

    /// Set the default array of LEDs to be used by this controller
    /// @param data pointer to the LED data
    /// @param nLeds the number of LEDs in the LED data
    CLEDController & setLeds(CRGB *data, int nLeds) {
        m_Data = data;
        m_nLeds = nLeds;
        return *this;
    }

    /// Zero out the LED data managed by this controller
    void clearLedDataInternal(int nLeds = -1);

    /// How many LEDs does this controller manage?
    /// @returns CLEDController::m_nLeds
    virtual int size() { return m_nLeds; }

    /// How many Lanes does this controller manage?
    /// @returns 1 for a non-Parallel controller
    virtual int lanes() { return 1; }

    /// Pointer to the CRGB array for this controller
    /// @returns CLEDController::m_Data
    CRGB* leds() { return m_Data; }

    /// Reference to the n'th LED managed by the controller
    /// @param x the LED number to retrieve
    /// @returns reference to CLEDController::m_Data[x]
    CRGB &operator[](int x) { return m_Data[x]; }

    /// Set the dithering mode for this controller to use
    /// @param ditherMode the dithering mode to set
    /// @returns a reference to the controller
    inline CLEDController & setDither(fl::u8 ditherMode = BINARY_DITHER) { m_DitherMode = ditherMode; return *this; }

    CLEDController& setScreenMap(const fl::XYMap& map, float diameter = -1.f) {
        // EngineEvents::onCanvasUiSet(this, map);
        fl::ScreenMap screenmap = map.toScreenMap();
        if (diameter <= 0.0f) {
            // screen map was not set.
            if (map.getTotal() <= (64*64)) {
                screenmap.setDiameter(.1f); // Assume small matrix is being used.
            }
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
    /// @return the currently set dithering option (CLEDController::m_DitherMode)
    inline fl::u8 getDither() { return m_DitherMode; }

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
    CLEDController & setCorrection(CRGB correction) { m_ColorCorrection = correction; return *this; }

    /// @copydoc setCorrection()
    CLEDController & setCorrection(LEDColorCorrection correction) { m_ColorCorrection = correction; return *this; }

    /// Get the correction value used by this controller
    /// @returns the current color correction (CLEDController::m_ColorCorrection)
    CRGB getCorrection() { return m_ColorCorrection; }

    /// Set the color temperature, aka white point, for this controller
    /// @param temperature the color temperature to set
    /// @returns a reference to the controller
    CLEDController & setTemperature(CRGB temperature) { m_ColorTemperature = temperature; return *this; }

    /// @copydoc setTemperature()
    CLEDController & setTemperature(ColorTemperature temperature) { m_ColorTemperature = temperature; return *this; }

    /// Get the color temperature, aka white point, for this controller
    /// @returns the current color temperature (CLEDController::m_ColorTemperature)
    CRGB getTemperature() { return m_ColorTemperature; }

    /// Get the combined brightness/color adjustment for this controller
    /// @param scale the brightness scale to get the correction for
    /// @returns a CRGB object representing the total adjustment, including color correction and color temperature
    CRGB getAdjustment(fl::u8 scale) {
        return CRGB::computeAdjustment(scale, m_ColorCorrection, m_ColorTemperature);
    }

    /// Gets the maximum possible refresh rate of the strip
    /// @returns the maximum refresh rate, in frames per second (FPS)
    virtual fl::u16 getMaxRefreshRate() const { return 0; }
};

FASTLED_NAMESPACE_END
