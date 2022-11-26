#ifndef __INC_PIXELSET_H
#define __INC_PIXELSET_H

#include "FastLED.h"

#ifndef abs
#include <stdlib.h>
#endif

/// @file pixelset.h
/// Declares classes for managing logical groups of LEDs


/// @defgroup PixelSet Pixel Data Sets
/// @brief Classes for managing logical groups of LEDs
/// @{

/// Represents a set of LED objects.  Provides the [] array operator, and works like a normal array in that case.
/// This should be kept in sync with the set of functions provided by the other @ref PixelTypes as well as functions in colorutils.h.
/// @tparam PIXEL_TYPE the type of LED data referenced in the class, e.g. CRGB.
/// @note A pixel set is a window into another set of LED data, it is not its own set of LED data.
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
    inline CPixelView operator()(int start, int end) { return CPixelView(leds, start, end); }

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
    inline CPixelView & operator++(int DUMMY_ARG) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)++; } return *this; }

    /// Decrement every pixel value in this set
    inline CPixelView & operator--() { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)--; } return *this; }
    /// Decrement every pixel value in this set
    inline CPixelView & operator--(int DUMMY_ARG) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel)--; } return *this; }

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
    inline CPixelView & fill_solid(const CHSV & color) { if(dir>0) { *this = color; return *this; } }

    /// Fill all of the LEDs with a rainbow of colors.
    /// @param initialhue the starting hue for the rainbow
    /// @param deltahue how many hue values to advance for each LED
    /// @see ::fill_rainbow(struct CRGB*, int, uint8_t, uint8_t)
    inline CPixelView & fill_rainbow(uint8_t initialhue, uint8_t deltahue=5) {
        if(dir >= 0) {
            ::fill_rainbow(leds,len,initialhue,deltahue);
        } else {
            ::fill_rainbow(leds+len+1,-len,initialhue,deltahue);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between two HSV colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelView & fill_gradient(const CHSV & startcolor, const CHSV & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
        if(dir >= 0) {
            ::fill_gradient(leds,len,startcolor, endcolor, directionCode);
        } else {
            ::fill_gradient(leds + len + 1, (-len), endcolor, startcolor, directionCode);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between three HSV colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelView & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV &  c3, TGradientDirectionCode directionCode = SHORTEST_HUES) {
        if(dir >= 0) {
            ::fill_gradient(leds, len, c1, c2, c3, directionCode);
        } else {
            ::fill_gradient(leds + len + 1, -len, c3, c2, c1, directionCode);
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
    inline CPixelView & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV & c3, const CHSV & c4, TGradientDirectionCode directionCode = SHORTEST_HUES) {
        if(dir >= 0) {
            ::fill_gradient(leds, len, c1, c2, c3, c4, directionCode);
        } else {
            ::fill_gradient(leds + len + 1, -len, c4, c3, c2, c1, directionCode);
        }
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between two RGB colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&)
    inline CPixelView & fill_gradient_RGB(const PIXEL_TYPE & startcolor, const PIXEL_TYPE & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
        if(dir >= 0) {
            ::fill_gradient_RGB(leds,len,startcolor, endcolor);
        } else {
            ::fill_gradient_RGB(leds + len + 1, (-len), endcolor, startcolor);
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
            ::fill_gradient_RGB(leds, len, c1, c2, c3);
        } else {
            ::fill_gradient_RGB(leds + len + 1, -len, c3, c2, c1);
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
            ::fill_gradient_RGB(leds, len, c1, c2, c3, c4);
        } else {
            ::fill_gradient_RGB(leds + len + 1, -len, c4, c3, c2, c1);
        }
        return *this;
    }

    /// Destructively modifies all LEDs, blending in a given fraction of an overlay color
    /// @param overlay the color to blend in
    /// @param amountOfOverlay the fraction of overlay to blend in
    /// @see ::nblend(CRGB&, const CRGB&, fract8)
    inline CPixelView & nblend(const PIXEL_TYPE & overlay, fract8 amountOfOverlay) { for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { ::nblend((*pixel), overlay, amountOfOverlay); } return *this; }

    /// Destructively blend another set of LEDs into this one
    /// @param rhs the set of LEDs to blend into this set
    /// @param amountOfOverlay the fraction of each color in the other set to blend in
    /// @see ::nblend(CRGB&, const CRGB&, fract8)
    inline CPixelView & nblend(const CPixelView & rhs, fract8 amountOfOverlay) { for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) { ::nblend((*pixel), (*rhspixel), amountOfOverlay); } return *this; }

    /// One-dimensional blur filter
    /// @param blur_amount the amount of blur to apply
    /// @note Only bringing in a 1d blur, not sure 2d blur makes sense when looking at sub arrays
    /// @see ::blur1d(CRGB*, uint16_t, fract8)
    inline CPixelView & blur1d(fract8 blur_amount) {
        if(dir >= 0) {
            ::blur1d(leds, len, blur_amount);
        } else {
            ::blur1d(leds + len + 1, -len, blur_amount);
        }
        return *this;
    }

    /// Destructively applies a gamma adjustment to all LEDs
    /// @param gamma the gamma value to apply
    /// @see ::napplyGamma_video(CRGB&, float)
    inline CPixelView & napplyGamma_video(float gamma) {
        if(dir >= 0) {
            ::napplyGamma_video(leds, len, gamma);
        } else {
            ::napplyGamma_video(leds + len + 1, -len, gamma);
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
            ::napplyGamma_video(leds, len, gammaR, gammaG, gammaB);
        } else {
            ::napplyGamma_video(leds + len + 1, -len, gammaR, gammaG, gammaB);
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
        __attribute__((always_inline)) inline pixelset_iterator_base(const pixelset_iterator_base & rhs) : leds(rhs.leds), dir(rhs.dir) {}

        /// Base constructor
        /// @tparam the type of the LED array data
        /// @param _leds pointer to LED array
        /// @param _dir direction of LED array
        __attribute__((always_inline)) inline pixelset_iterator_base(T * _leds, const char _dir) : leds(_leds), dir(_dir) {}

        __attribute__((always_inline)) inline pixelset_iterator_base& operator++() { leds += dir; return *this; }  ///< Increment LED pointer in data direction
        __attribute__((always_inline)) inline pixelset_iterator_base operator++(int) { pixelset_iterator_base tmp(*this); leds += dir; return tmp; }  ///< @copydoc operator++()

        __attribute__((always_inline)) inline bool operator==(pixelset_iterator_base & other) const { return leds == other.leds; /* && set==other.set; */ }    ///< Check if iterator is at the same position
        __attribute__((always_inline)) inline bool operator!=(pixelset_iterator_base & other) const { return leds != other.leds; /* || set != other.set; */ }  ///< Check if iterator is not at the same position

        __attribute__((always_inline)) inline PIXEL_TYPE& operator*() const { return *leds; }  ///< Dereference operator, to get underlying pointer to the LEDs
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

/// CPixelView for CRGB arrays
typedef CPixelView<CRGB> CRGBSet;

/// Retrieve a pointer to a CRGB array, using a CRGBSet and an LED offset
__attribute__((always_inline))
inline CRGB *operator+(const CRGBSet & pixels, int offset) { return (CRGB*)pixels + offset; }


/// A version of CPixelView<CRGB> with an included array of CRGB LEDs
/// @tparam SIZE the number of LEDs to include in the array
template<int SIZE>
class CRGBArray : public CPixelView<CRGB> {
    CRGB rawleds[SIZE];  ///< the LED data

public:
    CRGBArray() : CPixelView<CRGB>(rawleds, SIZE) {}
    using CPixelView::operator=;
};

/// @} PixelSet


#endif
