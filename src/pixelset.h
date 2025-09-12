#pragma once


#include "fl/force_inline.h"
#include "fl/namespace.h"
#include "fl/unused.h"
#include "fl/colorutils.h"

#include "fl/fill.h"
#include "fl/blur.h"

#include "FastLED.h"

#define FUNCTION_FILL_RAINBOW(a,b,c,d) fl::fill_rainbow(a,b,c,d)
#define FUNCTION_NAPPLY_GAMMA(a,b,c) fl::napplyGamma_video(a,b,c)
#define FUNCTION_NAPPLY_GAMMA_RGB(a,b,c,d,e) fl::napplyGamma_video(a,b,c,d,e)
#define FUNCTION_BLUR1D(a,b,c) fl::blur1d(a,b,c)
#define FUNCTION_FILL_GRADIENT(a,b,c,d,e) fl::fill_gradient(a,b,c,d,e)
#define FUNCTION_FILL_GRADIENT3(a,b,c,d,e,f) fl::fill_gradient(a,b,c,d,e,f)
#define FUNCTION_FILL_GRADIENT4(a,b,c,d,e,f,g) fl::fill_gradient(a,b,c,d,e,f,g)
#define FUNCTION_NBLEND(a,b,c) fl::nblend(a,b,c)
#define FUNCTION_FILL_GRADIENT_RGB(a,b,c,d) fl::fill_gradient_RGB(a,b,c,d)
#define FUNCTION_FILL_GRADIENT_RGB3(a,b,c,d,e) fl::fill_gradient_RGB(a,b,c,d,e)
#define FUNCTION_FILL_GRADIENT_RGB4(a,b,c,d,e,f) fl::fill_gradient_RGB(a,b,c,d,e,f)

#ifndef abs
#include <stdlib.h>
#endif


#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

template<class PIXEL_TYPE>
class CPixelView;

/// @brief CPixelView specialized for CRGB pixel arrays - the most commonly used pixel view type.
///
/// CRGBSet provides all the functionality of CPixelView optimized for CRGB pixels.
/// This is the primary interface for working with LED strips in FastLED.
///
/// **Quick Start:**
/// ```cpp
/// CRGB leds[NUM_LEDS];
/// CRGBSet pixels(leds, NUM_LEDS);
/// 
/// // Basic operations
/// pixels[0] = CRGB::Red;
/// pixels.fill_solid(CRGB::Blue);
/// pixels.fadeToBlackBy(64);
/// 
/// // Advanced effects
/// pixels(0, 10).fill_rainbow(0, 25);      // Rainbow on first 10 LEDs
/// pixels(20, 10).blur1d(128);             // Blur segment (reverse order)
/// ```
///
/// @see CPixelView for full API documentation
typedef CPixelView<CRGB> CRGBSet;

/// Retrieve a pointer to a CRGB array, using a CRGBSet and an LED offset
FASTLED_FORCE_INLINE
CRGB *operator+(const CRGBSet & pixels, int offset);


/// @file pixelset.h
/// Declares classes for managing logical groups of LEDs


/// @defgroup PixelSet Pixel Data Sets
/// @brief Classes for managing logical groups of LEDs
/// @{

/// @brief Represents a view/window into a set of LED pixels, providing array-like access and rich color operations.
///
/// CPixelView provides a non-owning view into LED pixel data with extensive manipulation capabilities.
/// It supports forward and reverse iteration, subset operations, and a comprehensive set of color functions.
/// 
/// **Key Features:**
/// - Array-like access with `operator[]`
/// - Subset creation with `operator(start, end)` 
/// - Reverse iteration when `start > end`
/// - Rich color operations: fill, gradients, scaling, blending
/// - Automatic conversion to `fl::span<T>` for modern C++ interop
/// - Iterator support for range-based loops
/// 
/// **Common Usage Patterns:**
/// ```cpp
/// // Basic usage
/// CRGB leds[100];
/// CRGBSet pixels(leds, 100);
/// pixels[0] = CRGB::Red;                    // Set individual pixel
/// pixels.fill_solid(CRGB::Blue);           // Fill all pixels
/// 
/// // Subset operations  
/// auto segment = pixels(10, 50);           // Forward subset (indices 10-50)
/// auto reverse = pixels(50, 10);           // Reverse subset (50 down to 10)
/// segment.fill_rainbow(0, 5);              // Apply rainbow to segment
/// 
/// // Modern C++ interop
/// fl::span<CRGB> span = pixels;            // Automatic conversion
/// std::fill(pixels.begin(), pixels.end(), CRGB::Green);  // STL algorithms
/// ```
///
/// @tparam PIXEL_TYPE the type of LED data referenced, typically CRGB or CHSV
/// @note This is a non-owning view - it references existing LED data, doesn't own it
/// @see CRGBSet - typedef for CPixelView<CRGB>, the most common usage
/// @see CRGBArray - version that owns its LED data
template<class PIXEL_TYPE>
class CPixelView {
public:
    const int8_t dir;             ///< direction of the LED data, either 1 or -1. Determines how the pointer is incremented.
    const int len;                ///< length of the LED data, in PIXEL_TYPE units. More accurately, it's the distance from
                                  ///  the start of the CPixelView::leds array to the end of the set (CPixelView::end_pos)
    PIXEL_TYPE * const leds;      ///< pointer to the LED data
    PIXEL_TYPE * const end_pos;   ///< pointer to the end position of the LED data

public:
    /// PixelSet copy constructor
    inline CPixelView(const CPixelView & other) : dir(other.dir), len(other.len), leds(other.leds), end_pos(other.end_pos) {}

    /// PixelSet constructor for a pixel set starting at the given `PIXEL_TYPE*` and going for `_len` leds.  Note that the length
    /// can be backwards, creating a PixelSet that walks backwards over the data
    /// @param _leds pointer to the raw LED data
    /// @param _len how many LEDs in this set
    inline CPixelView(PIXEL_TYPE *_leds, int _len) : dir(_len < 0 ? -1 : 1), len(_len), leds(_leds), end_pos(_leds + _len) {}

    /// PixelSet constructor for the given set of LEDs, with start and end boundaries.  Note that start can be after
    /// end, resulting in a set that will iterate backwards
    /// @param _leds pointer to the raw LED data
    /// @param _start the start index of the LEDs for this array
    /// @param _end the end index of the LEDs for this array
    inline CPixelView(PIXEL_TYPE *_leds, int _start, int _end) : dir(((_end-_start)<0) ? -1 : 1), len((_end - _start) + dir), leds(_leds + _start), end_pos(_leds + _start + len) {}

    /// Get the size of this set
    /// @return the size of the set, in number of LEDs
    int size() { return abs(len); }

    /// Whether or not this set goes backwards
    /// @return whether or not the set is backwards
    bool reversed() { return len < 0; }

    /// Do these sets point to the same thing? Note that this is different from the contents of the set being the same.
    bool operator==(const CPixelView & rhs) const { return leds == rhs.leds && len == rhs.len && dir == rhs.dir; }

    /// Do these sets point to different things? Note that this is different from the contents of the set being the same.
    bool operator!=(const CPixelView & rhs) const { return leds != rhs.leds || len != rhs.len || dir != rhs.dir; }

    /// Access a single element in this set, just like an array operator
    inline PIXEL_TYPE & operator[](int x) const { if(dir & 0x80) { return leds[-x]; } else { return leds[x]; } }

    /// Access an inclusive subset of the LEDs in this set. 
    /// @note The start point can be greater than end, which will
    /// result in a reverse ordering for many functions (useful for mirroring).
    /// @param start the first element from this set for the new subset
    /// @param end the last element for the new subset
    inline CPixelView operator()(int start, int end) { if(dir & 0x80) { return CPixelView(leds+len+1, -len-start-1, -len-end-1); } else { return CPixelView(leds, start, end); } }

    // Access an inclusive subset of the LEDs in this set, starting from the first.
    // @param end the last element for the new subset
    // @todo Not sure i want this? inline CPixelView operator()(int end) { return CPixelView(leds, 0, end); }

    /// Return the reverse ordering of this set
    inline CPixelView operator-() { return CPixelView(leds, len - dir, 0); }

    /// Return a pointer to the first element in this set
    inline operator PIXEL_TYPE* () const { return leds; }

    /// Assign the passed in color to all elements in this set
    /// @param color the new color for the elements in the set
    inline CPixelView & operator=(const PIXEL_TYPE & color) {
        for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) = color; }
        return *this;
    }

    /// Print debug data to serial, disabled for release. 
    /// Edit this file to re-enable these for debugging purposes.
    void dump() const {
        /// @code
        /// Serial.print("len: "); Serial.print(len); Serial.print(", dir:"); Serial.print((int)dir);
        /// Serial.print(", range:"); Serial.print((uint32_t)leds); Serial.print("-"); Serial.print((uint32_t)end_pos);
        /// Serial.print(", diff:"); Serial.print((int32_t)(end_pos - leds));
        /// Serial.println("");
        /// @endcode
    }

    /// Copy the contents of the passed-in set to our set. 
    /// @note If one set is smaller than the other, only the
    /// smallest number of items will be copied over.
    inline CPixelView & operator=(const CPixelView & rhs) {
        for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) {
            (*pixel) = (*rhspixel);
        }
        return *this;
    }

    /// @name Modification/Scaling Operators
    /// @{

    /// Add the passed in value to all channels for all of the pixels in this set
    inline CPixelView & addToRGB(uint8_t inc) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) += inc; } return *this; }
    /// Add every pixel in the other set to this set
    inline CPixelView & operator+=(CPixelView & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) += (*rhspixel); } return *this; }

    /// Subtract the passed in value from all channels for all of the pixels in this set
    inline CPixelView & subFromRGB(uint8_t inc) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) -= inc; } return *this; }
    /// Subtract every pixel in the other set from this set
    inline CPixelView & operator-=(CPixelView & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) -= (*rhspixel); } return *this; }

    /// Increment every pixel value in this set
    inline CPixelView & operator++() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)++; } return *this; }
    /// Increment every pixel value in this set
    inline CPixelView & operator++(int DUMMY_ARG) {
        FASTLED_UNUSED(DUMMY_ARG);
        for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) {
            (*pixel)++;
        }
        return *this;
    }

    /// Decrement every pixel value in this set
    inline CPixelView & operator--() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)--; } return *this; }
    /// Decrement every pixel value in this set
    inline CPixelView & operator--(int DUMMY_ARG) {
        FASTLED_UNUSED(DUMMY_ARG);
        for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) {
            (*pixel)--;
        }
        return *this;
    }

    /// Divide every LED by the given value
    inline CPixelView & operator/=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) /= d; } return *this; }
    /// Shift every LED in this set right by the given number of bits
    inline CPixelView & operator>>=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) >>= d; } return *this; }
    /// Multiply every LED in this set by the given value
    inline CPixelView & operator*=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) *= d; } return *this; }

    /// Scale every LED by the given scale
    inline CPixelView & nscale8_video(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8_video(scaledown); } return *this;}
    /// Scale down every LED by the given scale
    inline CPixelView & operator%=(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8_video(scaledown); } return *this; }
    /// Fade every LED down by the given scale
    inline CPixelView & fadeLightBy(uint8_t fadefactor) { return nscale8_video(255 - fadefactor); }

    /// Scale every LED by the given scale
    inline CPixelView & nscale8(uint8_t scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8(scaledown); } return *this; }
    /// Scale every LED by the given scale
    inline CPixelView & nscale8(PIXEL_TYPE & scaledown) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel).nscale8(scaledown); } return *this; }
    /// Scale every LED in this set by every led in the other set
    inline CPixelView & nscale8(CPixelView & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel).nscale8((*rhspixel)); } return *this; }

    /// Fade every LED down by the given scale
    inline CPixelView & fadeToBlackBy(uint8_t fade) { return nscale8(255 - fade); }

    /// Apply the PIXEL_TYPE |= operator to every pixel in this set with the given PIXEL_TYPE value. 
    /// With CRGB, this brings up each channel to the higher of the two values
    /// @see CRGB::operator|=
    inline CPixelView & operator|=(const PIXEL_TYPE & rhs) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) |= rhs; } return *this; }
    /// Apply the PIXEL_TYPE |= operator to every pixel in this set with every pixel in the passed in set. 
    /// @copydetails operator|=(const PIXEL_TYPE&)
    inline CPixelView & operator|=(const CPixelView & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) |= (*rhspixel); } return *this; }
    /// Apply the PIXEL_TYPE |= operator to every pixel in this set. 
    /// @copydetails operator|=(const PIXEL_TYPE&)
    inline CPixelView & operator|=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) |= d; } return *this; }

    /// Apply the PIXEL_TYPE &= operator to every pixel in this set with the given PIXEL_TYPE value. 
    /// With CRGB, this brings up each channel down to the lower of the two values
    /// @see CRGB::operator&=
    inline CPixelView & operator&=(const PIXEL_TYPE & rhs) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) &= rhs; } return *this; }
    /// Apply the PIXEL_TYPE &= operator to every pixel in this set with every pixel in the passed in set. 
    /// @copydetails operator&=(const PIXEL_TYPE&)
    inline CPixelView & operator&=(const CPixelView & rhs) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { (*pixel) &= (*rhspixel); } return *this; }
    /// Apply the PIXEL_TYPE &= operator to every pixel in this set with the passed in value. 
    /// @copydetails operator&=(const PIXEL_TYPE&)
    inline CPixelView & operator&=(uint8_t d) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) &= d; } return *this; }

    /// @} Modification/Scaling Operators


    /// Returns whether or not any LEDs in this set are non-zero
    inline operator bool() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { if((*pixel)) return true; } return false; }


    /// @name Color Util Functions
    /// @{

    /// Fill all of the LEDs with a solid color
    /// @param color the color to fill with
    inline CPixelView & fill_solid(const PIXEL_TYPE & color) { *this = color; return *this; }
    /// @copydoc CPixelView::fill_solid(const PIXEL_TYPE&)
    inline CPixelView & fill_solid(const CHSV & color) { *this = color; return *this; }

    /// Fill all of the LEDs with a rainbow of colors.
    /// @param initialhue the starting hue for the rainbow
    /// @param deltahue how many hue values to advance for each LED
    /// @see ::fill_rainbow(struct CRGB*, int, uint8_t, uint8_t)
    inline CPixelView & fill_rainbow(uint8_t initialhue, uint8_t deltahue=5) {
        if(dir >= 0) {
            FUNCTION_FILL_RAINBOW(leds,len,initialhue,deltahue);
        } else {
            FUNCTION_FILL_RAINBOW(leds + len + 1, -len, initialhue - deltahue * (len+1), -deltahue);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between two HSV colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelView & fill_gradient(const CHSV & startcolor, const CHSV & endcolor, TGradientDirectionCode directionCode  = fl::SHORTEST_HUES) {
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT(leds,len,startcolor, endcolor, directionCode);
        } else {
            FUNCTION_FILL_GRADIENT(leds + len + 1, (-len), endcolor, startcolor, directionCode);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between three HSV colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelView & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV &  c3, TGradientDirectionCode directionCode = fl::SHORTEST_HUES) {
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT3(leds, len, c1, c2, c3, directionCode);
        } else {
            FUNCTION_FILL_GRADIENT3(leds + len + 1, -len, c3, c2, c1, directionCode);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between four HSV colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the first middle color for the gradient
    /// @param c3 the second middle color for the gradient
    /// @param c4 the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelView & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV & c3, const CHSV & c4, TGradientDirectionCode directionCode = fl::SHORTEST_HUES) {
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT4(leds, len, c1, c2, c3, c4, directionCode);
        } else {
            FUNCTION_FILL_GRADIENT4(leds + len + 1, -len, c4, c3, c2, c1, directionCode);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between two RGB colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&)
    inline CPixelView & fill_gradient_RGB(const PIXEL_TYPE & startcolor, const PIXEL_TYPE & endcolor, TGradientDirectionCode directionCode  = fl::SHORTEST_HUES) {
        FASTLED_UNUSED(directionCode); // TODO: why is this not used?
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT_RGB(leds,len,startcolor, endcolor);
        } else {
            FUNCTION_FILL_GRADIENT_RGB(leds + len + 1, (-len), endcolor, startcolor);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between three RGB colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&, const CRGB&)
    inline CPixelView & fill_gradient_RGB(const PIXEL_TYPE & c1, const PIXEL_TYPE & c2, const PIXEL_TYPE &  c3) {
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT_RGB3(leds, len, c1, c2, c3);
        } else {
            FUNCTION_FILL_GRADIENT_RGB3(leds + len + 1, -len, c3, c2, c1);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between four RGB colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the first middle color for the gradient
    /// @param c3 the second middle color for the gradient
    /// @param c4 the end color for the gradient
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&, const CRGB&, const CRGB&)
    inline CPixelView & fill_gradient_RGB(const PIXEL_TYPE & c1, const PIXEL_TYPE & c2, const PIXEL_TYPE & c3, const PIXEL_TYPE & c4) {
        if(dir >= 0) {
            FUNCTION_FILL_GRADIENT_RGB4(leds, len, c1, c2, c3, c4);
        } else {
            FUNCTION_FILL_GRADIENT_RGB4(leds + len + 1, -len, c4, c3, c2, c1);
        }
        return *this;
    }

    /// Destructively modifies all LEDs, blending in a given fraction of an overlay color
    /// @param overlay the color to blend in
    /// @param amountOfOverlay the fraction of overlay to blend in
    /// @see ::nblend(CRGB&, const CRGB&, fract8)
    inline CPixelView & nblend(const PIXEL_TYPE & overlay, fract8 amountOfOverlay) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { FUNCTION_NBLEND((*pixel), overlay, amountOfOverlay); } return *this; }

    /// Destructively blend another set of LEDs into this one
    /// @param rhs the set of LEDs to blend into this set
    /// @param amountOfOverlay the fraction of each color in the other set to blend in
    /// @see ::nblend(CRGB&, const CRGB&, fract8)
    inline CPixelView & nblend(const CPixelView & rhs, fract8 amountOfOverlay) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { FUNCTION_NBLEND((*pixel), (*rhspixel), amountOfOverlay); } return *this; }

    /// One-dimensional blur filter
    /// @param blur_amount the amount of blur to apply
    /// @note Only bringing in a 1d blur, not sure 2d blur makes sense when looking at sub arrays
    /// @see ::blur1d(CRGB*, uint16_t, fract8)
    inline CPixelView & blur1d(fract8 blur_amount) {
        if(dir >= 0) {
            FUNCTION_BLUR1D(leds, len, blur_amount);
        } else {
            FUNCTION_BLUR1D(leds + len + 1, -len, blur_amount);
        }
        return *this;
    }

    /// Destructively applies a gamma adjustment to all LEDs
    /// @param gamma the gamma value to apply
    /// @see ::napplyGamma_video(CRGB&, float)
    inline CPixelView & napplyGamma_video(float gamma) {
        if(dir >= 0) {
            FUNCTION_NAPPLY_GAMMA(leds, len, gamma);
        } else {
            FUNCTION_NAPPLY_GAMMA(leds + len + 1, -len, gamma);
        }
        return *this;
    }

    /// @copybrief CPixelView::napplyGamma_video(float)
    /// @param gammaR the gamma value to apply to the CRGB::red channel
    /// @param gammaG the gamma value to apply to the CRGB::green channel
    /// @param gammaB the gamma value to apply to the CRGB::blue channel
    /// @see ::napplyGamma_video(CRGB&, float, float, float)
    inline CPixelView & napplyGamma_video(float gammaR, float gammaG, float gammaB) {
        if(dir >= 0) {
            FUNCTION_NAPPLY_GAMMA_RGB(leds, len, gammaR, gammaG, gammaB);
        } else {
            FUNCTION_NAPPLY_GAMMA_RGB(leds + len + 1, -len, gammaR, gammaG, gammaB);
        }
        return *this;
    }

    /// @} Color Util Functions


    /// @name Iterator
    /// @{

    /// Iterator helper class for CPixelView
    /// @tparam the type of the LED array data
    /// @todo Make this a fully specified/proper iterator
    template <class T>
    class pixelset_iterator_base {
        T * leds;          ///< pointer to LED array
        const int8_t dir;  ///< direction of LED array, for incrementing the pointer

    public:
        /// Copy constructor
        FASTLED_FORCE_INLINE pixelset_iterator_base(const pixelset_iterator_base & rhs) : leds(rhs.leds), dir(rhs.dir) {}

        /// Base constructor
        /// @tparam the type of the LED array data
        /// @param _leds pointer to LED array
        /// @param _dir direction of LED array
        FASTLED_FORCE_INLINE pixelset_iterator_base(T * _leds, const char _dir) : leds(_leds), dir(_dir) {}

        FASTLED_FORCE_INLINE pixelset_iterator_base& operator++() { leds += dir; return *this; }  ///< Increment LED pointer in data direction
        FASTLED_FORCE_INLINE pixelset_iterator_base operator++(int) { pixelset_iterator_base tmp(*this); leds += dir; return tmp; }  ///< @copydoc operator++()

        FASTLED_FORCE_INLINE bool operator==(pixelset_iterator_base & other) const { return leds == other.leds; /* && set==other.set; */ }    ///< Check if iterator is at the same position
        FASTLED_FORCE_INLINE bool operator!=(pixelset_iterator_base & other) const { return leds != other.leds; /* || set != other.set; */ }  ///< Check if iterator is not at the same position

        FASTLED_FORCE_INLINE PIXEL_TYPE& operator*() const { return *leds; }  ///< Dereference operator, to get underlying pointer to the LEDs
    };

    typedef pixelset_iterator_base<PIXEL_TYPE> iterator;              ///< Iterator helper type for this class
    typedef pixelset_iterator_base<const PIXEL_TYPE> const_iterator;  ///< Const iterator helper type for this class

    iterator begin() { return iterator(leds, dir); }   ///< Makes an iterator instance for the start of the LED set
    iterator end() { return iterator(end_pos, dir); }  ///< Makes an iterator instance for the end of the LED set

    iterator begin() const { return iterator(leds, dir); }   ///< Makes an iterator instance for the start of the LED set, const qualified
    iterator end() const { return iterator(end_pos, dir); }  ///< Makes an iterator instance for the end of the LED set, const qualified

    const_iterator cbegin() const { return const_iterator(leds, dir); }   ///< Makes a const iterator instance for the start of the LED set, const qualified
    const_iterator cend() const { return const_iterator(end_pos, dir); }  ///< Makes a const iterator instance for the end of the LED set, const qualified

    /// @} Iterator
};

FASTLED_FORCE_INLINE
CRGB *operator+(const CRGBSet & pixels, int offset) {
    return (CRGB*)pixels + offset;
}


/// A version of CPixelView<CRGB> with an included array of CRGB LEDs
/// @tparam SIZE the number of LEDs to include in the array
template<int SIZE>
class CRGBArray : public CPixelView<CRGB> {
    CRGB rawleds[SIZE] = {0};  ///< the LED data

public:
    CRGBArray() : CPixelView<CRGB>(rawleds, SIZE) {}
    using CPixelView::operator=;
    CRGB* get() { return &rawleds[0]; }
    const CRGB* get() const {  return &rawleds[0]; }
    size_t size() const { return SIZE; }
};

/// @} PixelSet

FASTLED_NAMESPACE_END

#undef FUNCTION_FILL_RAINBOW
#undef FUNCTION_NAPPLY_GAMMA
#undef FUNCTION_NAPPLY_GAMMA_RGB
#undef FUNCTION_BLUR1D
#undef FUNCTION_FILL_GRADIENT
#undef FUNCTION_FILL_GRADIENT3
#undef FUNCTION_FILL_GRADIENT4
#undef FUNCTION_NBLEND
#undef FUNCTION_FILL_GRADIENT_RGB
#undef FUNCTION_FILL_GRADIENT_RGB3
#undef FUNCTION_FILL_GRADIENT_RGB4
