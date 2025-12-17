/// @file crgb.hpp
/// Defines utility functions for the red, green, and blue (RGB) pixel struct

#pragma once

#include "fl/stl/stdint.h"
#include "chsv.h"
#include "crgb.h"
#include "lib8tion.h"
#include "fl/force_inline.h"
#include "fl/scale8.h"

#include "fl/compiler_control.h"

// Define namespace-aware scale8 macro
#define FUNCTION_SCALE8(a,b) fl::scale8(a,b)

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_UNUSED_PARAMETER
FL_DISABLE_WARNING_RETURN_TYPE
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION
FL_DISABLE_WARNING_SIGN_CONVERSION


FASTLED_FORCE_INLINE CRGB& CRGB::addToRGB (uint8_t d )
{
    r = fl::qadd8( r, d);
    g = fl::qadd8( g, d);
    b = fl::qadd8( b, d);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::operator-= (const CRGB& rhs )
{
    r = fl::qsub8( r, rhs.r);
    g = fl::qsub8( g, rhs.g);
    b = fl::qsub8( b, rhs.b);
    return *this;
}

/// Add a constant of '1' from each channel, saturating at 0xFF
FASTLED_FORCE_INLINE CRGB& CRGB::operator++ ()
{
    addToRGB(1);
    return *this;
}

/// @copydoc operator++
FASTLED_FORCE_INLINE CRGB CRGB::operator++ (int )
{
    CRGB retval(*this);
    ++(*this);
    return retval;
}

FASTLED_FORCE_INLINE CRGB& CRGB::subtractFromRGB(uint8_t d)
{
    r = fl::qsub8( r, d);
    g = fl::qsub8( g, d);
    b = fl::qsub8( b, d);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::operator*= (uint8_t d )
{
    r = fl::qmul8( r, d);
    g = fl::qmul8( g, d);
    b = fl::qmul8( b, d);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::nscale8_video(uint8_t scaledown )
{
    nscale8x3_video( r, g, b, scaledown);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::operator%= (uint8_t scaledown )
{
    nscale8x3_video( r, g, b, scaledown);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::fadeLightBy (uint8_t fadefactor )
{
    nscale8x3_video( r, g, b, 255 - fadefactor);
    return *this;
}

/// Subtract a constant of '1' from each channel, saturating at 0x00
FASTLED_FORCE_INLINE CRGB& CRGB::operator-- ()
{
    subtractFromRGB(1);
    return *this;
}

/// @copydoc operator--
FASTLED_FORCE_INLINE CRGB CRGB::operator-- (int )
{
    CRGB retval(*this);
    --(*this);
    return retval;
}


constexpr CRGB CRGB::nscale8_constexpr(const CRGB scaledown) const
{
    return CRGB(
        (((uint16_t)r) * (1 + (uint16_t)(scaledown.r))) >> 8,
        (((uint16_t)g) * (1 + (uint16_t)(scaledown.g))) >> 8,
        (((uint16_t)b) * (1 + (uint16_t)(scaledown.b))) >> 8
    );
}


FASTLED_FORCE_INLINE CRGB& CRGB::nscale8 (const CRGB & scaledown )
{
    r = FUNCTION_SCALE8(r, scaledown.r);
    g = FUNCTION_SCALE8(g, scaledown.g);
    b = FUNCTION_SCALE8(b, scaledown.b);
    return *this;
}

FASTLED_FORCE_INLINE CRGB CRGB::scale8 (uint8_t scaledown ) const
{
    CRGB out = *this;
    nscale8x3( out.r, out.g, out.b, scaledown);
    return out;
}

FASTLED_FORCE_INLINE CRGB CRGB::scale8 (const CRGB & scaledown ) const
{
    CRGB out;
    out.r = FUNCTION_SCALE8(r, scaledown.r);
    out.g = FUNCTION_SCALE8(g, scaledown.g);
    out.b = FUNCTION_SCALE8(b, scaledown.b);
    return out;
}


FASTLED_FORCE_INLINE uint8_t CRGB::getLuma( )  const {
    //Y' = 0.2126 R' + 0.7152 G' + 0.0722 B'
    //     54            183       18 (!)

    uint8_t luma = scale8_LEAVING_R1_DIRTY( r, 54) + \
    scale8_LEAVING_R1_DIRTY( g, 183) + \
    scale8_LEAVING_R1_DIRTY( b, 18);
    cleanup_R1();
    return luma;
}

FASTLED_FORCE_INLINE uint8_t CRGB::getAverageLight( )  const {
#if FASTLED_SCALE8_FIXED == 1
    const uint8_t eightyfive = 85;
#else
    const uint8_t eightyfive = 86;
#endif
    uint8_t avg = scale8_LEAVING_R1_DIRTY( r, eightyfive) + \
    scale8_LEAVING_R1_DIRTY( g, eightyfive) + \
    scale8_LEAVING_R1_DIRTY( b, eightyfive);
    cleanup_R1();
    return avg;
}



FASTLED_FORCE_INLINE CRGB CRGB::lerp16( const CRGB& other, fract16 frac) const
{
    CRGB ret;

    ret.r = lerp16by16(r<<8,other.r<<8,frac)>>8;
    ret.g = lerp16by16(g<<8,other.g<<8,frac)>>8;
    ret.b = lerp16by16(b<<8,other.b<<8,frac)>>8;

    return ret;
}


/// @copydoc CRGB::operator+=
FASTLED_FORCE_INLINE CRGB operator+( const CRGB& p1, const CRGB& p2)
{
    return CRGB( fl::qadd8( p1.r, p2.r),
                 fl::qadd8( p1.g, p2.g),
                 fl::qadd8( p1.b, p2.b));
}

/// @copydoc CRGB::operator-=
FASTLED_FORCE_INLINE CRGB operator-( const CRGB& p1, const CRGB& p2)
{
    return CRGB( fl::qsub8( p1.r, p2.r),
                 fl::qsub8( p1.g, p2.g),
                 fl::qsub8( p1.b, p2.b));
}

/// @copydoc CRGB::operator*=
FASTLED_FORCE_INLINE CRGB operator*( const CRGB& p1, uint8_t d)
{
    return CRGB( fl::qmul8( p1.r, d),
                 fl::qmul8( p1.g, d),
                 fl::qmul8( p1.b, d));
}

/// Scale using CRGB::nscale8_video()
FASTLED_FORCE_INLINE CRGB operator%( const CRGB& p1, uint8_t d)
{
    CRGB retval( p1);
    retval.nscale8_video( d);
    return retval;
}


#undef FUNCTION_SCALE8

FL_DISABLE_WARNING_POP
