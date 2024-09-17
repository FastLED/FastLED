#ifndef __INC_HSV2RGB_H
#define __INC_HSV2RGB_H

#include "FastLED.h"

#include "pixeltypes.h"

/// @file hsv2rgb.h
/// Functions to convert from the HSV colorspace to the RGB colorspace

/// @defgroup HSV2RGB HSV to RGB Conversion Functions
/// Functions to convert from the HSV colorspace to the RGB colorspace. 
///
/// These basically fall into two groups: spectra, and rainbows.
/// pectra and rainbows are not the same thing.  Wikipedia has a good
/// llustration that shows a "spectrum" and a "rainbow" side by side:  
///   [![Spectra and Rainbow comparison](http://upload.wikimedia.org/wikipedia/commons/f/f6/Prism_compare_rainbow_01.png)](https://commons.wikimedia.org/wiki/File:Prism_compare_rainbow_01.png)
///
///   <sup>Source: http://en.wikipedia.org/wiki/Rainbow#Number_of_colours_in_spectrum_or_rainbow</sup>
///
/// Among other differences, you'll see that a "rainbow" has much more yellow than
/// a plain spectrum.  "Classic" LED color washes are spectrum based, and
/// usually show very little yellow.
///
/// Take a look Wikipedia's page on HSV color space, with pseudocode for conversion
/// to RGB color space: http://en.wikipedia.org/wiki/HSL_and_HSV
///
/// Note that their conversion algorithm, which is (naturally) very popular
/// is in the "maximum brightness at any given hue" style, vs. the "uniform
/// brightness for all hues" style.
///
/// You can't have both; either purple is the same brightness as red, e.g:  
///   @code
///      red = 0xFF0000
///   purple = 0x800080
///   @endcode
///
/// Where you have the same "total light" output. OR purple is "as bright
/// as it can be", e.g.:  
///   @code
///      red = 0xFF0000
///   purple = 0xFF00FF
///   @endcode
///
/// Where purple is much brighter than red.
///
/// The colorspace conversions here try to keep the apparent brightness
/// constant even as the hue varies.
///
/// Adafruit's "Wheel" function, discussed [here](http://forums.adafruit.com/viewtopic.php?f=47&t=22483)
/// is also of the "constant apparent brightness" variety.
///
/// @todo Provide the "maximum brightness no matter what" variation.
///
/// @see [Some good, clear Arduino C code from Kasper Kamperman](http://www.kasperkamperman.com/blog/arduino/arduino-programming-hsb-to-rgb/),
/// which in turn [was based on Windows C code from "nico80"](http://www.codeproject.com/Articles/9207/An-HSB-RGBA-colour-picker)

/// @{

FASTLED_NAMESPACE_BEGIN


/// Convert an HSV value to RGB using a visually balanced rainbow. 
/// This "rainbow" yields better yellow and orange than a straight
/// mathematical "spectrum".
///
/// ![FastLED 'Rainbow' Hue Chart](https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-rainbow-with-desc.jpg)
///
/// @param hsv CHSV struct to convert to RGB. Max hue supported is HUE_MAX_RAINBOW
/// @param rgb CRGB struct to store the result of the conversion (will be modified)
void hsv2rgb_rainbow( const struct CHSV& hsv, struct CRGB& rgb);

/// @copybrief hsv2rgb_rainbow(const struct CHSV&, struct CRGB&)
/// @see hsv2rgb_rainbow(const struct CHSV&, struct CRGB&)
/// @param phsv CHSV array to convert to RGB. Max hue supported is HUE_MAX_RAINBOW
/// @param prgb CRGB array to store the result of the conversion (will be modified)
/// @param numLeds the number of array values to process
void hsv2rgb_rainbow( const struct CHSV* phsv, struct CRGB * prgb, int numLeds);

/// Max hue accepted for the hsv2rgb_rainbow() function
#define HUE_MAX_RAINBOW 255


/// Convert an HSV value to RGB using a mathematically straight spectrum. 
/// This "spectrum" will have more green and blue than a "rainbow",
/// and less yellow and orange.
///
/// ![FastLED 'Spectrum' Hue Chart](https://raw.githubusercontent.com/FastLED/FastLED/gh-pages/images/HSV-spectrum-with-desc.jpg)
///
/// @note This function wraps hsv2rgb_raw() and rescales the hue value to fit
/// the smaller range.
///
/// @param hsv CHSV struct to convert to RGB. Max hue supported is HUE_MAX_SPECTRUM
/// @param rgb CRGB struct to store the result of the conversion (will be modified)
void hsv2rgb_spectrum( const struct CHSV& hsv, struct CRGB& rgb);

/// Inline version of hsv2rgb_spectrum which returns a CRGB object.
inline CRGB hsv2rgb_spectrum( const struct CHSV& hsv) {
    CRGB rgb;
    hsv2rgb_spectrum(hsv, rgb);
    return rgb;
}

/// @copybrief hsv2rgb_spectrum(const struct CHSV&, struct CRGB&)
/// @see hsv2rgb_spectrum(const struct CHSV&, struct CRGB&)
/// @param phsv CHSV array to convert to RGB. Max hue supported is HUE_MAX_SPECTRUM
/// @param prgb CRGB array to store the result of the conversion (will be modified)
/// @param numLeds the number of array values to process
void hsv2rgb_spectrum( const struct CHSV* phsv, struct CRGB * prgb, int numLeds);

/// Max hue accepted for the hsv2rgb_spectrum() function
#define HUE_MAX_SPECTRUM 255



/// @copybrief hsv2rgb_spectrum(const struct CHSV&, struct CRGB&)
/// @see hsv2rgb_spectrum(const struct CHSV&, struct CRGB&)
/// @note The hue is limited to the range 0-191 (HUE_MAX). This 
/// results in a slightly faster conversion speed at the expense
/// of color balance.
/// @param hsv CHSV struct to convert to RGB. Max hue supported is HUE_MAX
/// @param rgb CRGB struct to store the result of the conversion (will be modified)
void hsv2rgb_raw(const struct CHSV& hsv, struct CRGB & rgb);

/// @copybrief hsv2rgb_raw(const struct CHSV&, struct CRGB&)
/// @see hsv2rgb_raw(const struct CHSV&, struct CRGB&)
/// @param phsv CHSV array to convert to RGB. Max hue supported is HUE_MAX
/// @param prgb CRGB array to store the result of the conversion (will be modified)
/// @param numLeds the number of array values to process
void hsv2rgb_raw(const struct CHSV* phsv, struct CRGB * prgb, int numLeds);

/// Max hue accepted for the hsv2rgb_raw() function
#define HUE_MAX 191


/// Recover approximate HSV values from RGB. 
/// These values are *approximate*, not exact. Why is this "only" an approximation? 
/// Because not all RGB colors have HSV equivalents!  For example, there
/// is no HSV value that will ever convert to RGB(255,255,0) using
/// the code provided in this library.   So if you try to
/// convert RGB(255,255,0) "back" to HSV, you'll necessarily get
/// only an approximation.  Emphasis has been placed on getting
/// the "hue" as close as usefully possible, but even that's a bit
/// of a challenge.  The 8-bit HSV and 8-bit RGB color spaces
/// are not a "bijection".
///
/// Nevertheless, this function does a pretty good job, particularly
/// at recovering the 'hue' from fully saturated RGB colors that
/// originally came from HSV rainbow colors.  So if you start
/// with CHSV(hue_in,255,255), and convert that to RGB, and then
/// convert it back to HSV using this function, the resulting output
/// hue will either exactly the same, or very close (+/-1).
/// The more desaturated the original RGB color is, the rougher the
/// approximation, and the less accurate the results.
/// @note This function is a long-term work in progress; expect
/// results to change slightly over time as this function is
/// refined and improved.
/// @par
/// @note This function is most accurate when the input is an
/// RGB color that came from a fully-saturated HSV color to start
/// with.  E.g. CHSV( hue, 255, 255) -> CRGB -> CHSV will give
/// best results.
/// @par
/// @note This function is not nearly as fast as HSV-to-RGB.
/// It is provided for those situations when the need for this
/// function cannot be avoided, or when extremely high performance
/// is not needed.
/// @see https://en.wikipedia.org/wiki/Bijection
/// @param rgb an RGB value to convert
/// @returns the approximate HSV equivalent of the RGB value
CHSV rgb2hsv_approximate( const CRGB& rgb);


FASTLED_NAMESPACE_END

///@} HSV2RGB

#endif
