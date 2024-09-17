#ifndef __INC_CONTROLLER_H
#define __INC_CONTROLLER_H

/// @file controller.h
/// base definitions used by led controllers for writing out led data

#include "FastLED.h"
#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "color.h"
#include <stddef.h>
#include "force_inline.h"
#include "pixel_controller.h"
#include "dither_mode.h"
#include "pixel_iterator.h"
#include "assert.h"

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
    int m_nLeds;               ///< the number of LEDs in the LED data array
    static CLEDController *m_pHead;  ///< pointer to the first LED controller in the linked list
    static CLEDController *m_pTail;  ///< pointer to the last LED controller in the linked list


    /// Set all the LEDs to a given color. 
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale the rgb scaling value for outputting color
    virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) = 0;

    /// Write the passed in RGB data out to the LEDs managed by this controller. 
    /// @param data the rgb data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale the rgb scaling to apply to each led before writing it out
    virtual void show(const struct CRGB *data, int nLeds, CRGB scale) = 0;

public:

    Rgbw mRgbMode = RgbwInvalid::value();
    CLEDController& setRgbw(const Rgbw& arg = RgbwDefault::value()) {
        // Note that at this time (Sept 13th, 2024) this is only implemented in the ESP32 driver
        // directly. For an emulated version please see RGBWEmulatedController in chipsets.h
        mRgbMode = arg;
        return *this;  // builder pattern.
    }
    Rgbw getRgbw() const { return mRgbMode; }

    /// Create an led controller object, add it to the chain of controllers
    CLEDController() : m_Data(NULL), m_ColorCorrection(UncorrectedColor), m_ColorTemperature(UncorrectedTemperature), m_DitherMode(BINARY_DITHER), m_nLeds(0) {
        m_pNext = NULL;
        if(m_pHead==NULL) { m_pHead = this; }
        if(m_pTail != NULL) { m_pTail->m_pNext = this; }
        m_pTail = this;
    }

    /// Initialize the LED controller
    virtual void init() = 0;

    /// Clear out/zero out the given number of LEDs.
    /// @param nLeds the number of LEDs to clear
    virtual void clearLeds(int nLeds) { showColor(CRGB::Black, nLeds, CRGB::Black); }

    /// @copybrief show(const struct CRGB*, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the LED data to write to the strip
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see show(const struct CRGB*, int, CRGB)
    void show(const struct CRGB *data, int nLeds, uint8_t brightness) {
        show(data, nLeds, getAdjustment(brightness));
    }

    /// @copybrief showColor(const struct CRGB&, int, CRGB)
    ///
    /// Will scale for color correction and temperature. Can accept LED data not attached to this controller.
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs in the data array
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const struct CRGB&, int, CRGB)
    void showColor(const struct CRGB &data, int nLeds, uint8_t brightness) {
        showColor(data, nLeds, getAdjustment(brightness));
    }

    /// Write the data to the LEDs managed by this controller
    /// @param brightness the brightness of the LEDs
    /// @see show(const struct CRGB*, int, uint8_t)
    void showLeds(uint8_t brightness=255) {
        show(m_Data, m_nLeds, getAdjustment(brightness));
    }

    /// @copybrief showColor(const struct CRGB&, int, CRGB)
    ///
    /// @param data the CRGB color to set the LEDs to
    /// @param brightness the brightness of the LEDs
    /// @see showColor(const struct CRGB&, int, CRGB)
    void showColor(const struct CRGB & data, uint8_t brightness=255) {
        showColor(data, m_nLeds, getAdjustment(brightness));
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
    void clearLedData() {
        if(m_Data) {
            memset8((void*)m_Data, 0, sizeof(struct CRGB) * m_nLeds);
        }
    }

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
    inline CLEDController & setDither(uint8_t ditherMode = BINARY_DITHER) { m_DitherMode = ditherMode; return *this; }

    /// Get the dithering option currently set for this controller
    /// @return the currently set dithering option (CLEDController::m_DitherMode)
    inline uint8_t getDither() { return m_DitherMode; }

    virtual void* beginShowLeds() {
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
        void* out = reinterpret_cast<void*>(d);
        return out;
    }
    virtual void endShowLeds(void* data) {
        // By default recieves the integer that beginShowLeds() emitted.
        //For async controllers this should be used to signal the controller
        // to begin transmitting the current frame to the leds.
        uintptr_t d = reinterpret_cast<uintptr_t>(data);
        setDither(static_cast<uint8_t>(d));
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

    /// Get the color temperature, aka whipe point, for this controller
    /// @returns the current color temperature (CLEDController::m_ColorTemperature)
    CRGB getTemperature() { return m_ColorTemperature; }

    /// Get the combined brightness/color adjustment for this controller
    /// @param scale the brightness scale to get the correction for
    /// @returns a CRGB object representing the total adjustment, including color correction and color temperature
    CRGB getAdjustment(uint8_t scale) {
        return computeAdjustment(scale, m_ColorCorrection, m_ColorTemperature);
    }

    /// Calculates the combined color adjustment to the LEDs at a given scale, color correction, and color temperature
    /// @param scale the scale value for the RGB data (i.e. brightness)
    /// @param colorCorrection color correction to apply
    /// @param colorTemperature color temperature to apply
    /// @returns a CRGB object representing the adjustment, including color correction and color temperature
    static CRGB computeAdjustment(uint8_t scale, const CRGB & colorCorrection, const CRGB & colorTemperature) {
      #if defined(NO_CORRECTION) && (NO_CORRECTION==1)
              return CRGB(scale,scale,scale);
      #else
              CRGB adj(0,0,0);

              if(scale > 0) {
                  for(uint8_t i = 0; i < 3; ++i) {
                      uint8_t cc = colorCorrection.raw[i];
                      uint8_t ct = colorTemperature.raw[i];
                      if(cc > 0 && ct > 0) {
                          uint32_t work = (((uint32_t)cc)+1) * (((uint32_t)ct)+1) * scale;
                          work /= 0x10000L;
                          adj.raw[i] = work & 0xFF;
                      }
                  }
              }

              return adj;
      #endif
    }

    /// Gets the maximum possible refresh rate of the strip
    /// @returns the maximum refresh rate, in frames per second (FPS)
    virtual uint16_t getMaxRefreshRate() const { return 0; }
};


/// Template extension of the CLEDController class
/// @tparam RGB_ORDER the rgb ordering for the LEDs (e.g. what order red, green, and blue data is written out in)
/// @tparam LANES how many parallel lanes of output to write
/// @tparam MASK bitmask for the output lanes
template<EOrder RGB_ORDER, int LANES=1, uint32_t MASK=0xFFFFFFFF> class CPixelLEDController : public CLEDController {
protected:


    /// Set all the LEDs on the controller to a given color
    /// @param data the CRGB color to set the LEDs to
    /// @param nLeds the number of LEDs to set to this color
    /// @param scale_pre_mixed the RGB scaling of color adjustment + global brightness to apply to each LED (in RGB8 mode).
    virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale_pre_mixed) {
        PixelController<RGB_ORDER, LANES, MASK> pixels(data, nLeds, scale_pre_mixed, getDither());
        showPixels(pixels);
    }

    /// Write the passed in RGB data out to the LEDs managed by this controller
    /// @param data the RGB data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale_pre_mixed the RGB scaling of color adjustment + global brightness to apply to each LED (in RGB8 mode).
    virtual void show(const struct CRGB *data, int nLeds, CRGB scale_pre_mixed) {
        PixelController<RGB_ORDER, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, scale_pre_mixed, getDither());
        if(nLeds < 0) {
            // nLeds < 0 implies that we want to show them in reverse
            pixels.mAdvance = -pixels.mAdvance;
        }
        showPixels(pixels);
    }

public:
    static const EOrder RGB_ORDER_VALUE = RGB_ORDER; ///< The RGB ordering for this controller
    static const int LANES_VALUE = LANES;             ///< The number of lanes for this controller
    static const uint32_t MASK_VALUE = MASK;         ///< The mask for the lanes for this controller
    CPixelLEDController() : CLEDController() {}

    /// Send the LED data to the strip
    /// @param pixels the PixelController object for the LED data
    virtual void showPixels(PixelController<RGB_ORDER,LANES,MASK> & pixels) = 0;

    /// Get the number of lanes of the Controller
    /// @returns LANES from template
    int lanes() { return LANES; }
};


FASTLED_NAMESPACE_END

#endif
