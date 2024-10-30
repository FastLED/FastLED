#pragma once

/// @file controller.h
/// base definitions used by led controllers for writing out led data

#include <stddef.h>

#include "FastLED.h"
#include "led_sysdefs.h"
#include "pixeltypes.h"
#include "color.h"
#include "eorder.h"

#include "force_inline.h"
#include "pixel_controller.h"
#include "cled_controller.h"

FASTLED_NAMESPACE_BEGIN



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
    virtual void showColor(const CRGB& data, int nLeds, uint8_t brightness) {
        // CRGB premixed, color_correction;
        // getAdjustmentData(brightness, &premixed, &color_correction);
        // ColorAdjustment color_adjustment = {premixed, color_correction, brightness};
        ColorAdjustment color_adjustment = getAdjustmentData(brightness);
        PixelController<RGB_ORDER, LANES, MASK> pixels(data, nLeds, color_adjustment, getDither());
        showPixels(pixels);
    }

    /// Write the passed in RGB data out to the LEDs managed by this controller
    /// @param data the RGB data to write out to the strip
    /// @param nLeds the number of LEDs being written out
    /// @param scale_pre_mixed the RGB scaling of color adjustment + global brightness to apply to each LED (in RGB8 mode).
    virtual void show(const struct CRGB *data, int nLeds, uint8_t brightness) {
        ColorAdjustment color_adjustment = getAdjustmentData(brightness);
        PixelController<RGB_ORDER, LANES, MASK> pixels(data, nLeds < 0 ? -nLeds : nLeds, color_adjustment, getDither());
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

