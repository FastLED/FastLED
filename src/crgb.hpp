
#pragma once

#include <stdint.h>
#include "chsv.h"
#include "crgb.h"
#include "lib8tion.h"
#include "namespace.h"
#include "force_inline.h"

#if FASTLED_IS_USING_NAMESPACE
#define FUNCTION_SCALE8(a,b) FASTLED_NAMESPACE::scale8(a,b)
#else
#define FUNCTION_SCALE8(a,b) ::scale8(a,b)
#endif

FASTLED_NAMESPACE_BEGIN

/// Add one CRGB to another, saturating at 0xFF for each channel
FASTLED_FORCE_INLINE CRGB& CRGB::operator+= (const CRGB& rhs )
{
    r = qadd8( r, rhs.r);
    g = qadd8( g, rhs.g);
    b = qadd8( b, rhs.b);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::addToRGB (uint8_t d )
{
    r = qadd8( r, d);
    g = qadd8( g, d);
    b = qadd8( b, d);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::operator-= (const CRGB& rhs )
{
    r = qsub8( r, rhs.r);
    g = qsub8( g, rhs.g);
    b = qsub8( b, rhs.b);
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
    r = qsub8( r, d);
    g = qsub8( g, d);
    b = qsub8( b, d);
    return *this;
}

FASTLED_FORCE_INLINE CRGB& CRGB::operator*= (uint8_t d )
{
    r = qmul8( r, d);
    g = qmul8( g, d);
    b = qmul8( b, d);
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

FASTLED_FORCE_INLINE CRGB& CRGB::nscale8 (uint8_t scaledown )
{
    nscale8x3( r, g, b, scaledown);
    return *this;
}

constexpr CRGB CRGB::nscale8_constexpr(const CRGB scaledown) const
{
    return CRGB(
        scale8_constexpr(r, scaledown.r),
        scale8_constexpr(g, scaledown.g),
        scale8_constexpr(b, scaledown.b)
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

inline CRGB& CRGB::fadeToBlackBy (uint8_t fadefactor )
{
    nscale8x3( r, g, b, 255 - fadefactor);
    return *this;
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

FASTLED_FORCE_INLINE CRGB CRGB::lerp8( const CRGB& other, fract8 frac) const
{
    CRGB ret;

    ret.r = lerp8by8(r,other.r,frac);
    ret.g = lerp8by8(g,other.g,frac);
    ret.b = lerp8by8(b,other.b,frac);

    return ret;
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
    return CRGB( qadd8( p1.r, p2.r),
                 qadd8( p1.g, p2.g),
                 qadd8( p1.b, p2.b));
}

/// @copydoc CRGB::operator-=
FASTLED_FORCE_INLINE CRGB operator-( const CRGB& p1, const CRGB& p2)
{
    return CRGB( qsub8( p1.r, p2.r),
                 qsub8( p1.g, p2.g),
                 qsub8( p1.b, p2.b));
}

/// @copydoc CRGB::operator*=
FASTLED_FORCE_INLINE CRGB operator*( const CRGB& p1, uint8_t d)
{
    return CRGB( qmul8( p1.r, d),
                 qmul8( p1.g, d),
                 qmul8( p1.b, d));
}

/// Scale using CRGB::nscale8_video()
FASTLED_FORCE_INLINE CRGB operator%( const CRGB& p1, uint8_t d)
{
    CRGB retval( p1);
    retval.nscale8_video( d);
    return retval;
}

FASTLED_NAMESPACE_END

#undef FUNCTION_SCALE8