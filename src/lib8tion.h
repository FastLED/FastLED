#ifndef __INC_LIB8TION_H
#define __INC_LIB8TION_H

#include "FastLED.h"

#ifndef __INC_LED_SYSDEFS_H
#error WTH?  led_sysdefs needs to be included first
#endif

/// @file lib8tion.h
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming. 


FASTLED_NAMESPACE_BEGIN


#include <stdint.h>

/// Define a LIB8TION member function as static inline with an "unused" attribute
#define LIB8STATIC __attribute__ ((unused)) static inline
/// Define a LIB8TION member function as always static inline
#define LIB8STATIC_ALWAYS_INLINE __attribute__ ((always_inline)) static inline

#if !defined(__AVR__)
#include <string.h>
// for memmove, memcpy, and memset if not defined here
#endif // end of !defined(__AVR__)

#if defined(__arm__)

#if defined(FASTLED_TEENSY3)
// Can use Cortex M4 DSP instructions
#define QADD8_C 0
#define QADD7_C 0
#define QADD8_ARM_DSP_ASM 1
#define QADD7_ARM_DSP_ASM 1
#else
// Generic ARM
#define QADD8_C 1
#define QADD7_C 1
#endif // end of defined(FASTLED_TEENSY3)

#define QSUB8_C 1
#define SCALE8_C 1
#define SCALE16BY8_C 1
#define SCALE16_C 1
#define ABS8_C 1
#define MUL8_C 1
#define QMUL8_C 1
#define ADD8_C 1
#define SUB8_C 1
#define EASE8_C 1
#define AVG8_C 1
#define AVG8R_C 1
#define AVG7_C 1
#define AVG16_C 1
#define AVG16R_C 1
#define AVG15_C 1
#define BLEND8_C 1

// end of #if defined(__arm__)

#elif defined(ARDUINO_ARCH_APOLLO3)

// Default to using the standard C functions for now
#define QADD8_C 1
#define QADD7_C 1
#define QSUB8_C 1
#define SCALE8_C 1
#define SCALE16BY8_C 1
#define SCALE16_C 1
#define ABS8_C 1
#define MUL8_C 1
#define QMUL8_C 1
#define ADD8_C 1
#define SUB8_C 1
#define EASE8_C 1
#define AVG8_C 1
#define AVG8R_C 1
#define AVG7_C 1
#define AVG16_C 1
#define AVG16R_C 1
#define AVG15_C 1
#define BLEND8_C 1

// end of #elif defined(ARDUINO_ARCH_APOLLO3)

#elif defined(__AVR__)

// AVR ATmega and friends Arduino

#define QADD8_C 0
#define QADD7_C 0
#define QSUB8_C 0
#define ABS8_C 0
#define ADD8_C 0
#define SUB8_C 0
#define AVG8_C 0
#define AVG8R_C 0
#define AVG7_C 0
#define AVG16_C 0
#define AVG16R_C 0
#define AVG15_C 0

#define QADD8_AVRASM 1
#define QADD7_AVRASM 1
#define QSUB8_AVRASM 1
#define ABS8_AVRASM 1
#define ADD8_AVRASM 1
#define SUB8_AVRASM 1
#define AVG8_AVRASM 1
#define AVG8R_AVRASM 1
#define AVG7_AVRASM 1
#define AVG16_AVRASM 1
#define AVG16R_AVRASM 1
#define AVG15_AVRASM 1

// Note: these require hardware MUL instruction
//       -- sorry, ATtiny!
#if !defined(LIB8_ATTINY)
#define SCALE8_C 0
#define SCALE16BY8_C 0
#define SCALE16_C 0
#define MUL8_C 0
#define QMUL8_C 0
#define EASE8_C 0
#define BLEND8_C 0
#define SCALE8_AVRASM 1
#define SCALE16BY8_AVRASM 1
#define SCALE16_AVRASM 1
#define MUL8_AVRASM 1
#define QMUL8_AVRASM 1
#define EASE8_AVRASM 1
#define CLEANUP_R1_AVRASM 1
#define BLEND8_AVRASM 1
#else
// On ATtiny, we just use C implementations
#define SCALE8_C 1
#define SCALE16BY8_C 1
#define SCALE16_C 1
#define MUL8_C 1
#define QMUL8_C 1
#define EASE8_C 1
#define BLEND8_C 1
#define SCALE8_AVRASM 0
#define SCALE16BY8_AVRASM 0
#define SCALE16_AVRASM 0
#define MUL8_AVRASM 0
#define QMUL8_AVRASM 0
#define EASE8_AVRASM 0
#define BLEND8_AVRASM 0
#endif // end of !defined(LIB8_ATTINY)

// end of #elif defined(__AVR__)

#else

// Doxygen: ignore these macros
/// @cond

// unspecified architecture, so
// no ASM, everything in C
#define QADD8_C 1
#define QADD7_C 1
#define QSUB8_C 1
#define SCALE8_C 1
#define SCALE16BY8_C 1
#define SCALE16_C 1
#define ABS8_C 1
#define MUL8_C 1
#define QMUL8_C 1
#define ADD8_C 1
#define SUB8_C 1
#define EASE8_C 1
#define AVG8_C 1
#define AVG8R_C 1
#define AVG7_C 1
#define AVG16_C 1
#define AVG16R_C 1
#define AVG15_C 1
#define BLEND8_C 1

/// @endcond

#endif

/// @defgroup lib8tion Fast Math Functions
/// Fast, efficient 8-bit math functions specifically
/// designed for high-performance LED programming. 
///
/// Because of the AVR (Arduino) and ARM assembly language
/// implementations provided, using these functions often
/// results in smaller and faster code than the equivalent
/// program using plain "C" arithmetic and logic.
///
/// Included are:
///
///  - Saturating unsigned 8-bit add and subtract.
///    Instead of wrapping around if an overflow occurs,
///    these routines just 'clamp' the output at a maxumum
///    of 255, or a minimum of 0.  Useful for adding pixel
///    values.  E.g., qadd8( 200, 100) = 255.
///      @code
///      qadd8( i, j) == MIN( (i + j), 0xFF )
///      qsub8( i, j) == MAX( (i - j), 0 )
///      @endcode
///
///  - Saturating signed 8-bit ("7-bit") add.
///      @code
///      qadd7( i, j) == MIN( (i + j), 0x7F)
///      @endcode
///
///  - Scaling (down) of unsigned 8- and 16- bit values.
///    Scaledown value is specified in 1/256ths.
///      @code
///      scale8( i, sc) == (i * sc) / 256
///      scale16by8( i, sc) == (i * sc) / 256
///      @endcode
///
///    Example: scaling a 0-255 value down into a
///    range from 0-99:
///      @code
///      downscaled = scale8( originalnumber, 100);
///      @endcode
///
///    A special version of scale8 is provided for scaling
///    LED brightness values, to make sure that they don't
///    accidentally scale down to total black at low
///    dimming levels, since that would look wrong:
///      @code
///      scale8_video( i, sc) = ((i * sc) / 256) +? 1
///      @endcode
///
///    Example: reducing an LED brightness by a
///    dimming factor:
///      @code
///      new_bright = scale8_video( orig_bright, dimming);
///      @endcode
///
///  - Fast 8- and 16- bit unsigned random numbers.
///    Significantly faster than Arduino random(), but
///    also somewhat less random.  You can add entropy.
///      @code
///      random8()       == random from 0..255
///      random8( n)     == random from 0..(N-1)
///      random8( n, m)  == random from N..(M-1)
///
///      random16()      == random from 0..65535
///      random16( n)    == random from 0..(N-1)
///      random16( n, m) == random from N..(M-1)
///
///      random16_set_seed( k)    ==  seed = k
///      random16_add_entropy( k) ==  seed += k
///      @endcode
///
///  - Absolute value of a signed 8-bit value.
///      @code
///      abs8( i)     == abs( i)
///      @endcode
///
///  - 8-bit math operations which return 8-bit values.
///    These are provided mostly for completeness,
///    not particularly for performance.
///      @code
///      mul8( i, j)  == (i * j) & 0xFF
///      add8( i, j)  == (i + j) & 0xFF
///      sub8( i, j)  == (i - j) & 0xFF
///      @endcode
///
///  - Fast 16-bit approximations of sin and cos.
///    Input angle is a uint16_t from 0-65535.
///    Output is a signed int16_t from -32767 to 32767.
///      @code
///      sin16( x)  == sin( (x/32768.0) * pi) * 32767
///      cos16( x)  == cos( (x/32768.0) * pi) * 32767
///      @endcode
///
///    Accurate to more than 99% in all cases.
///
///  - Fast 8-bit approximations of sin and cos.
///    Input angle is a uint8_t from 0-255.
///    Output is an UNsigned uint8_t from 0 to 255.
///      @code
///      sin8( x)  == (sin( (x/128.0) * pi) * 128) + 128
///      cos8( x)  == (cos( (x/128.0) * pi) * 128) + 128
///      @endcode
///
///    Accurate to within about 2%.
///
///  - Fast 8-bit "easing in/out" function.
///      @code
///      ease8InOutCubic(x) == 3(x^2) - 2(x^3)
///      ease8InOutApprox(x) ==
///        faster, rougher, approximation of cubic easing
///      ease8InOutQuad(x) == quadratic (vs cubic) easing
///      @endcode
///
///  - Cubic, Quadratic, and Triangle wave functions.
///    Input is a uint8_t representing phase withing the wave,
///      similar to how sin8 takes an angle 'theta'.
///    Output is a uint8_t representing the amplitude of
///    the wave at that point.
///      @code
///      cubicwave8( x)
///      quadwave8( x)
///      triwave8( x)
///      @endcode
///
///  - Square root for 16-bit integers.  About three times
///    faster and five times smaller than Arduino's built-in
///    generic 32-bit sqrt routine.
///      @code
///      sqrt16( uint16_t x ) == sqrt( x)
///      @endcode
///
///  - Dimming and brightening functions for 8-bit
///    light values.
///      @code
///      dim8_video( x)  == scale8_video( x, x)
///      dim8_raw( x)    == scale8( x, x)
///      dim8_lin( x)    == (x<128) ? ((x+1)/2) : scale8(x,x)
///      brighten8_video( x) == 255 - dim8_video( 255 - x)
///      brighten8_raw( x) == 255 - dim8_raw( 255 - x)
///      brighten8_lin( x) == 255 - dim8_lin( 255 - x)
///      @endcode
///
///    The dimming functions in particular are suitable
///    for making LED light output appear more 'linear'.
///
///  - Linear interpolation between two values, with the
///    fraction between them expressed as an 8- or 16-bit
///    fixed point fraction (fract8 or fract16).
///      @code
///      lerp8by8(   fromU8, toU8, fract8 )
///      lerp16by8(  fromU16, toU16, fract8 )
///      lerp15by8(  fromS16, toS16, fract8 )
///        == from + (( to - from ) * fract8) / 256)
///      lerp16by16( fromU16, toU16, fract16 )
///        == from + (( to - from ) * fract16) / 65536)
///      map8( in, rangeStart, rangeEnd)
///        == map( in, 0, 255, rangeStart, rangeEnd);
///      @endcode
///
///  - Optimized memmove, memcpy, and memset, that are
///    faster than standard avr-libc 1.8.
///      @code
///      memmove8( dest, src,  bytecount)
///      memcpy8(  dest, src,  bytecount)
///      memset8(  buf, value, bytecount)
///      @endcode
///
///  - Beat generators which return sine or sawtooth
///    waves in a specified number of Beats Per Minute.
///    Sine wave beat generators can specify a low and
///    high range for the output.  Sawtooth wave beat
///    generators always range 0-255 or 0-65535.
///      @code
///      beatsin8( BPM, low8, high8)
///          = (sine(beatphase) * (high8-low8)) + low8
///      beatsin16( BPM, low16, high16)
///          = (sine(beatphase) * (high16-low16)) + low16
///      beatsin88( BPM88, low16, high16)
///          = (sine(beatphase) * (high16-low16)) + low16
///      beat8( BPM)  = 8-bit repeating sawtooth wave
///      beat16( BPM) = 16-bit repeating sawtooth wave
///      beat88( BPM88) = 16-bit repeating sawtooth wave
///      @endcode
///
///    BPM is beats per minute in either simple form
///    e.g. 120, or Q8.8 fixed-point form.
///    BPM88 is beats per minute in ONLY Q8.8 fixed-point
///    form.
///
/// Lib8tion is pronounced like 'libation': lie-BAY-shun
///
/// @{


///////////////////////////////////////////////////////////////////////
///
/// @defgroup FractionalTypes Fixed-Point Fractional Types. 
/// Types for storing fractional data. 
///
/// * ::sfract7 should be interpreted as signed 128ths.
/// * ::fract8 should be interpreted as unsigned 256ths.
/// * ::sfract15 should be interpreted as signed 32768ths.
/// * ::fract16 should be interpreted as unsigned 65536ths.
///
/// Example: if a fract8 has the value "64", that should be interpreted
///          as 64/256ths, or one-quarter.
///
/// accumXY types should be interpreted as X bits of integer,
///         and Y bits of fraction.  
/// E.g., ::accum88 has 8 bits of int, 8 bits of fraction
///
/// @{

/// ANSI: unsigned short _Fract. 
/// Range is 0 to 0.99609375 in steps of 0.00390625.  
/// Should be interpreted as unsigned 256ths.
typedef uint8_t   fract8;

/// ANSI: signed short _Fract. 
/// Range is -0.9921875 to 0.9921875 in steps of 0.0078125.  
/// Should be interpreted as signed 128ths.
typedef int8_t    sfract7;

/// ANSI: unsigned _Fract.
/// Range is 0 to 0.99998474121 in steps of 0.00001525878.  
/// Should be interpreted as unsigned 65536ths.
typedef uint16_t  fract16;

/// ANSI: signed _Fract.
/// Range is -0.99996948242 to 0.99996948242 in steps of 0.00003051757.  
/// Should be interpreted as signed 32768ths.
typedef int16_t   sfract15;


typedef uint16_t  accum88;    ///< ANSI: unsigned short _Accum. 8 bits int, 8 bits fraction
typedef int16_t   saccum78;   ///< ANSI: signed   short _Accum. 7 bits int, 8 bits fraction
typedef uint32_t  accum1616;  ///< ANSI: signed         _Accum. 16 bits int, 16 bits fraction
typedef int32_t   saccum1516; ///< ANSI: signed         _Accum. 15 bits int, 16 bits fraction
typedef uint16_t  accum124;   ///< no direct ANSI counterpart. 12 bits int, 4 bits fraction
typedef int32_t   saccum114;  ///< no direct ANSI counterpart. 1 bit int, 14 bits fraction


/// typedef for IEEE754 "binary32" float type internals
/// @see https://en.wikipedia.org/wiki/IEEE_754
typedef union {
    uint32_t i;  ///< raw value, as an integer
    float    f;  ///< raw value, as a float
    struct {
        uint32_t mantissa: 23;  ///< 23-bit mantissa
        uint32_t exponent:  8;  ///< 8-bit exponent
        uint32_t signbit:   1;  ///< sign bit
    };
    struct {
        uint32_t mant7 :  7;  ///< @todo Doc: what is this for?
        uint32_t mant16: 16;  ///< @todo Doc: what is this for?
        uint32_t exp_  :  8;  ///< @todo Doc: what is this for?
        uint32_t sb_   :  1;  ///< @todo Doc: what is this for?
    };
    struct {
        uint32_t mant_lo8 : 8;  ///< @todo Doc: what is this for?
        uint32_t mant_hi16_exp_lo1 : 16;  ///< @todo Doc: what is this for?
        uint32_t sb_exphi7 : 8;  ///< @todo Doc: what is this for?
    };
} IEEE754binary32_t;

/// @} FractionalTypes


#include "lib8tion/math8.h"
#include "lib8tion/scale8.h"
#include "lib8tion/random8.h"
#include "lib8tion/trig8.h"

///////////////////////////////////////////////////////////////////////







///////////////////////////////////////////////////////////////////////
///
/// @defgroup FloatConversions Float-to-Fixed and Fixed-to-Float Conversions
/// Functions to convert between floating point and fixed point types. 
/// @note Anything involving a "float" on AVR will be slower.
/// @{

/// Conversion from 16-bit fixed point (::sfract15) to IEEE754 32-bit float.
LIB8STATIC float sfract15ToFloat( sfract15 y)
{
    return y / 32768.0;
}

/// Conversion from IEEE754 float in the range (-1,1) to 16-bit fixed point (::sfract15).
/// @note The extremes of one and negative one are NOT representable! The
/// representable range is 0.99996948242 to -0.99996948242, in steps of 0.00003051757.
LIB8STATIC sfract15 floatToSfract15( float f)
{
    return f * 32768.0;
}

/// @} FloatConversions



///////////////////////////////////////////////////////////////////////
///
/// @defgroup FastMemory Fast Memory Functions for AVR
/// Alternatives to memmove, memcpy, and memset that are
/// faster on AVR than standard avr-libc 1.8. 
/// @{

#if defined(__AVR__) || defined(FASTLED_DOXYGEN)
extern "C" {
void * memmove8( void * dst, const void * src, uint16_t num );  ///< Faster alternative to memmove() on AVR
void * memcpy8 ( void * dst, const void * src, uint16_t num )  __attribute__ ((noinline));  ///< Faster alternative to memcpy() on AVR
void * memset8 ( void * ptr, uint8_t value, uint16_t num ) __attribute__ ((noinline)) ;  ///< Faster alternative to memset() on AVR
}
#else
// on non-AVR platforms, these names just call standard libc.
#define memmove8 memmove
#define memcpy8 memcpy
#define memset8 memset
#endif

/// @} FastMemory


///////////////////////////////////////////////////////////////////////
///
/// @defgroup LinearInterpolation Linear Interpolation
/// Fast linear interpolation functions, such as could be used for Perlin noise, etc.
///
/// A note on the structure of the lerp functions:
/// The cases for b>a and b<=a are handled separately for
/// speed. Without knowing the relative order of a and b,
/// the value (a-b) might be overflow the width of a or b,
/// and have to be promoted to a wider, slower type.
/// To avoid that, we separate the two cases, and are able
/// to do all the math in the same width as the arguments,
/// which is much faster and smaller on AVR.
/// @{

/// Linear interpolation between two unsigned 8-bit values,
/// with 8-bit fraction
LIB8STATIC uint8_t lerp8by8( uint8_t a, uint8_t b, fract8 frac)
{
    uint8_t result;
    if( b > a) {
        uint8_t delta = b - a;
        uint8_t scaled = scale8( delta, frac);
        result = a + scaled;
    } else {
        uint8_t delta = a - b;
        uint8_t scaled = scale8( delta, frac);
        result = a - scaled;
    }
    return result;
}

/// Linear interpolation between two unsigned 16-bit values,
/// with 16-bit fraction
LIB8STATIC uint16_t lerp16by16( uint16_t a, uint16_t b, fract16 frac)
{
    uint16_t result;
    if( b > a ) {
        uint16_t delta = b - a;
        uint16_t scaled = scale16(delta, frac);
        result = a + scaled;
    } else {
        uint16_t delta = a - b;
        uint16_t scaled = scale16( delta, frac);
        result = a - scaled;
    }
    return result;
}

/// Linear interpolation between two unsigned 16-bit values,
/// with 8-bit fraction
LIB8STATIC uint16_t lerp16by8( uint16_t a, uint16_t b, fract8 frac)
{
    uint16_t result;
    if( b > a) {
        uint16_t delta = b - a;
        uint16_t scaled = scale16by8( delta, frac);
        result = a + scaled;
    } else {
        uint16_t delta = a - b;
        uint16_t scaled = scale16by8( delta, frac);
        result = a - scaled;
    }
    return result;
}

/// Linear interpolation between two signed 15-bit values,
/// with 8-bit fraction
LIB8STATIC int16_t lerp15by8( int16_t a, int16_t b, fract8 frac)
{
    int16_t result;
    if( b > a) {
        uint16_t delta = b - a;
        uint16_t scaled = scale16by8( delta, frac);
        result = a + scaled;
    } else {
        uint16_t delta = a - b;
        uint16_t scaled = scale16by8( delta, frac);
        result = a - scaled;
    }
    return result;
}

/// Linear interpolation between two signed 15-bit values,
/// with 8-bit fraction
LIB8STATIC int16_t lerp15by16( int16_t a, int16_t b, fract16 frac)
{
    int16_t result;
    if( b > a) {
        uint16_t delta = b - a;
        uint16_t scaled = scale16( delta, frac);
        result = a + scaled;
    } else {
        uint16_t delta = a - b;
        uint16_t scaled = scale16( delta, frac);
        result = a - scaled;
    }
    return result;
}

/// Map from one full-range 8-bit value into a narrower
/// range of 8-bit values, possibly a range of hues.
///
/// E.g. map `myValue` into a hue in the range blue..purple..pink..red
///   @code
///   hue = map8( myValue, HUE_BLUE, HUE_RED);
///   @endcode
///
/// Combines nicely with the waveform functions (like sin8(), etc)
/// to produce continuous hue gradients back and forth:
///   @code
///   hue = map8( sin8( myValue), HUE_BLUE, HUE_RED);
///   @endcode
///
/// Mathematically simiar to lerp8by8(), but arguments are more
/// like Arduino's "map"; this function is similar to
///   @code
///   map( in, 0, 255, rangeStart, rangeEnd)
///   @endcode
///
/// but faster and specifically designed for 8-bit values.
LIB8STATIC uint8_t map8( uint8_t in, uint8_t rangeStart, uint8_t rangeEnd)
{
    uint8_t rangeWidth = rangeEnd - rangeStart;
    uint8_t out = scale8( in, rangeWidth);
    out += rangeStart;
    return out;
}

/// @} LinearInterpolation


///////////////////////////////////////////////////////////////////////
///
/// @defgroup Easing Easing Functions 
/// Specify the rate of change of a parameter over time.
/// @see http://easings.net
/// @{

/// 8-bit quadratic ease-in / ease-out function. 
/// Takes around 13 cycles on AVR.
#if (EASE8_C == 1) || defined(FASTLED_DOXYGEN)
LIB8STATIC uint8_t ease8InOutQuad( uint8_t i)
{
    uint8_t j = i;
    if( j & 0x80 ) {
        j = 255 - j;
    }
    uint8_t jj  = scale8(  j, j);
    uint8_t jj2 = jj << 1;
    if( i & 0x80 ) {
        jj2 = 255 - jj2;
    }
    return jj2;
}

#elif EASE8_AVRASM == 1
// This AVR asm version of ease8InOutQuad preserves one more
// low-bit of precision than the C version, and is also slightly
// smaller and faster.
LIB8STATIC uint8_t ease8InOutQuad(uint8_t val) {
    uint8_t j=val;
    asm volatile (
      "sbrc %[val], 7 \n"
      "com %[j]       \n"
      "mul %[j], %[j] \n"
      "add r0, %[j]   \n"
      "ldi %[j], 0    \n"
      "adc %[j], r1   \n"
      "lsl r0         \n" // carry = high bit of low byte of mul product
      "rol %[j]       \n" // j = (j * 2) + carry // preserve add'l bit of precision
      "sbrc %[val], 7 \n"
      "com %[j]       \n"
      "clr __zero_reg__   \n"
      : [j] "+&a" (j)
      : [val] "a" (val)
      : "r0", "r1"
      );
    return j;
}

#else
#error "No implementation for ease8InOutQuad available."
#endif

/// 16-bit quadratic ease-in / ease-out function. 
/// C implementation at this point.
LIB8STATIC uint16_t ease16InOutQuad( uint16_t i)
{
    uint16_t j = i;
    if( j & 0x8000 ) {
        j = 65535 - j;
    }
    uint16_t jj  = scale16( j, j);
    uint16_t jj2 = jj << 1;
    if( i & 0x8000 ) {
        jj2 = 65535 - jj2;
    }
    return jj2;
}


/// 8-bit cubic ease-in / ease-out function. 
/// Takes around 18 cycles on AVR.
LIB8STATIC fract8 ease8InOutCubic( fract8 i)
{
    uint8_t ii  = scale8_LEAVING_R1_DIRTY(  i, i);
    uint8_t iii = scale8_LEAVING_R1_DIRTY( ii, i);

    uint16_t r1 = (3 * (uint16_t)(ii)) - ( 2 * (uint16_t)(iii));

    /* the code generated for the above *'s automatically
       cleans up R1, so there's no need to explicitily call
       cleanup_R1(); */

    uint8_t result = r1;

    // if we got "256", return 255:
    if( r1 & 0x100 ) {
        result = 255;
    }
    return result;
}


/// Fast, rough 8-bit ease-in/ease-out function. 
/// Shaped approximately like ease8InOutCubic(),
/// it's never off by more than a couple of percent
/// from the actual cubic S-curve, and it executes
/// more than twice as fast.  Use when the cycles
/// are more important than visual smoothness.
/// Asm version takes around 7 cycles on AVR.
#if (EASE8_C == 1) || defined(FASTLED_DOXYGEN)
LIB8STATIC fract8 ease8InOutApprox( fract8 i)
{
    if( i < 64) {
        // start with slope 0.5
        i /= 2;
    } else if( i > (255 - 64)) {
        // end with slope 0.5
        i = 255 - i;
        i /= 2;
        i = 255 - i;
    } else {
        // in the middle, use slope 192/128 = 1.5
        i -= 64;
        i += (i / 2);
        i += 32;
    }

    return i;
}

#elif EASE8_AVRASM == 1
LIB8STATIC uint8_t ease8InOutApprox( fract8 i)
{
    // takes around 7 cycles on AVR
    asm volatile (
        "  subi %[i], 64         \n\t"
        "  cpi  %[i], 128        \n\t"
        "  brcc Lshift_%=        \n\t"

        // middle case
        "  mov __tmp_reg__, %[i] \n\t"
        "  lsr __tmp_reg__       \n\t"
        "  add %[i], __tmp_reg__ \n\t"
        "  subi %[i], 224        \n\t"
        "  rjmp Ldone_%=         \n\t"

        // start or end case
        "Lshift_%=:              \n\t"
        "  lsr %[i]              \n\t"
        "  subi %[i], 96         \n\t"

        "Ldone_%=:               \n\t"

        : [i] "+a" (i)
        :
        : "r0"
        );
    return i;
}
#else
#error "No implementation for ease8 available."
#endif

/// @} Easing


///////////////////////////////////////////////////////////////////////
///
/// @defgroup WaveformGenerators Waveform Generators
/// General purpose wave generator functions.
///
/// @{


/// Triangle wave generator. 
/// Useful for turning a one-byte ever-increasing value into a
/// one-byte value that oscillates up and down.
///   @code
///           input         output
///           0..127        0..254 (positive slope)
///           128..255      254..0 (negative slope)
///   @endcode
///
/// On AVR this function takes just three cycles.
///
LIB8STATIC uint8_t triwave8(uint8_t in)
{
    if( in & 0x80) {
        in = 255 - in;
    }
    uint8_t out = in << 1;
    return out;
}

/// Quadratic waveform generator. Spends just a little
/// more time at the limits than "sine" does. 
///
/// S-shaped wave generator (like "sine"). Useful 
/// for turning a one-byte "counter" value into a
/// one-byte oscillating value that moves smoothly up and down,
/// with an "acceleration" and "deceleration" curve.
///
/// This is even faster than "sin8()", and has
/// a slightly different curve shape.
LIB8STATIC uint8_t quadwave8(uint8_t in)
{
    return ease8InOutQuad( triwave8( in));
}

/// Cubic waveform generator. Spends visibly more time
/// at the limits than "sine" does. 
/// @copydetails quadwave8()
LIB8STATIC uint8_t cubicwave8(uint8_t in)
{
    return ease8InOutCubic( triwave8( in));
}


/// Square wave generator.
/// Useful for turning a one-byte ever-increasing value
/// into a one-byte value that is either 0 or 255.
/// The width of the output "pulse" is determined by
/// the pulsewidth argument:
///   @code
///   if pulsewidth is 255, output is always 255.
///   if pulsewidth < 255, then
///     if input < pulsewidth  then output is 255
///     if input >= pulsewidth then output is 0
///   @endcode
///
/// The output looking like:
///
///   @code
///     255   +--pulsewidth--+
///      .    |              |
///      0    0              +--------(256-pulsewidth)--------
///   @endcode
///
/// @param in input value
/// @param pulsewidth width of the output pulse
/// @returns square wave output
LIB8STATIC uint8_t squarewave8( uint8_t in, uint8_t pulsewidth=128)
{
    if( in < pulsewidth || (pulsewidth == 255)) {
        return 255;
    } else {
        return 0;
    }
}

/// @} WaveformGenerators



/// @addtogroup FractionalTypes
/// @{

/// Template class for representing fractional ints.
/// @tparam T underlying type for data storage
/// @tparam F number of fractional bits
/// @tparam I number of integer bits
template<class T, int F, int I> class qfx {
    T i:I;  ///< Integer value of number
    T f:F;  ///< Fractional value of number
public:
    /// Constructor, storing a float as a fractional int
    qfx(float fx) { i = fx; f = (fx-i) * (1<<F); }
    /// Constructor, storing a fractional int directly
    qfx(uint8_t _i, uint8_t _f) {i=_i; f=_f; }

    /// Multiply the fractional int by a value
    uint32_t operator*(uint32_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    uint16_t operator*(uint16_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    int32_t operator*(int32_t v) { return (v*i) + ((v*f)>>F); }
    /// @copydoc operator*(uint32_t)
    int16_t operator*(int16_t v) { return (v*i) + ((v*f)>>F); }
#if defined(FASTLED_ARM) | defined(FASTLED_RISCV) | defined(FASTLED_APOLLO3)
    /// @copydoc operator*(uint32_t)
    int operator*(int v) { return (v*i) + ((v*f)>>F); }
#endif
};

template<class T, int F, int I> static uint32_t operator*(uint32_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static uint16_t operator*(uint16_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int32_t operator*(int32_t v, qfx<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int16_t operator*(int16_t v, qfx<T,F,I> & q) { return q * v; }
#if defined(FASTLED_ARM) | defined(FASTLED_RISCV) | defined(FASTLED_APOLLO3)
template<class T, int F, int I> static int operator*(int v, qfx<T,F,I> & q) { return q * v; }
#endif

/// A 4.4 integer (4 bits integer, 4 bits fraction)
typedef qfx<uint8_t, 4,4> q44;
/// A 6.2 integer (6 bits integer, 2 bits fraction)
typedef qfx<uint8_t, 6,2> q62;
/// A 8.8 integer (8 bits integer, 8 bits fraction)
typedef qfx<uint16_t, 8,8> q88;
/// A 12.4 integer (12 bits integer, 4 bits fraction)
typedef qfx<uint16_t, 12,4> q124;

/// @}

/// @} lib8tion (excluding the timekeeping functions from the nested group)


///////////////////////////////////////////////////////////////////////
///
/// @defgroup Timekeeping Timekeeping Functions
/// Tools for tracking and working with time
///
/// @{

#if ((defined(ARDUINO) || defined(SPARK) || defined(FASTLED_HAS_MILLIS)) && !defined(USE_GET_MILLISECOND_TIMER)) || defined(FASTLED_DOXYGEN)
// Forward declaration of Arduino function 'millis'.
//uint32_t millis();

/// The a number of functions need access to a millisecond counter
/// in order to keep time. On Arduino, this is "millis()".
/// On other platforms, you'll need to provide a function with this
/// signature which provides similar functionality:
///   @code{.cpp}
///   uint32_t get_millisecond_timer();
///   @endcode
///
/// You can also force use of the get_millisecond_timer() function
/// by \#defining `USE_GET_MILLISECOND_TIMER`.
#define GET_MILLIS millis
#else
uint32_t get_millisecond_timer();
#define GET_MILLIS get_millisecond_timer
#endif

/// @} Timekeeping


/// @addtogroup lib8tion
/// @{


///////////////////////////////////////////////////////////////////////
///
/// @defgroup BeatGenerators Waveform Beat Generators
/// Waveform generators that reset at a given number
/// of "beats per minute" (BPM).
///
/// The standard "beat" functions generate "sawtooth" waves which rise from
/// 0 up to a max value and then reset, continuously repeating that cycle at
/// the specified frequency (BPM).
///
/// The "sin" versions function similarly, but create an oscillating sine wave
/// at the specified frequency.
///
/// BPM can be supplied two ways. The simpler way of specifying BPM is as
/// a simple 8-bit integer from 1-255, (e.g., "120").
/// The more sophisticated way of specifying BPM allows for fractional
/// "Q8.8" fixed point number (an ::accum88) with an 8-bit integer part and
/// an 8-bit fractional part.  The easiest way to construct this is to multiply
/// a floating point BPM value (e.g. 120.3) by 256, (e.g. resulting in 30796
/// in this case), and pass that as the 16-bit BPM argument.
///
/// Originally these functions were designed to make an entire animation project pulse.
/// with brightness. For that effect, add this line just above your existing call to
/// "FastLED.show()":
///   @code
///   uint8_t bright = beatsin8( 60 /*BPM*/, 192 /*dimmest*/, 255 /*brightest*/ ));
///   FastLED.setBrightness( bright );
///   FastLED.show();
///   @endcode
///
/// The entire animation will now pulse between brightness 192 and 255 once per second.
///
/// @warning Any "BPM88" parameter **MUST** always be provided in Q8.8 format!
/// @note The beat generators need access to a millisecond counter
/// to track elapsed time. See ::GET_MILLIS for reference. When using the Arduino
/// `millis()` function, accuracy is a bit better than one part in a thousand.
///
/// @{


/// Generates a 16-bit "sawtooth" wave at a given BPM, with BPM
/// specified in Q8.8 fixed-point format.
/// @param beats_per_minute_88 the frequency of the wave, in Q8.8 format
/// @param timebase the time offset of the wave from the millis() timer
/// @warning The BPM parameter **MUST** be provided in Q8.8 format! E.g.
/// for 120 BPM it would be 120*256 = 30720. If you just want to specify
/// "120", use beat16() or beat8().
LIB8STATIC uint16_t beat88( accum88 beats_per_minute_88, uint32_t timebase = 0)
{
    // BPM is 'beats per minute', or 'beats per 60000ms'.
    // To avoid using the (slower) division operator, we
    // want to convert 'beats per 60000ms' to 'beats per 65536ms',
    // and then use a simple, fast bit-shift to divide by 65536.
    //
    // The ratio 65536:60000 is 279.620266667:256; we'll call it 280:256.
    // The conversion is accurate to about 0.05%, more or less,
    // e.g. if you ask for "120 BPM", you'll get about "119.93".
    return (((GET_MILLIS()) - timebase) * beats_per_minute_88 * 280) >> 16;
}

/// Generates a 16-bit "sawtooth" wave at a given BPM
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param timebase the time offset of the wave from the millis() timer
LIB8STATIC uint16_t beat16( accum88 beats_per_minute, uint32_t timebase = 0)
{
    // Convert simple 8-bit BPM's to full Q8.8 accum88's if needed
    if( beats_per_minute < 256) beats_per_minute <<= 8;
    return beat88(beats_per_minute, timebase);
}

/// Generates an 8-bit "sawtooth" wave at a given BPM
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param timebase the time offset of the wave from the millis() timer
LIB8STATIC uint8_t beat8( accum88 beats_per_minute, uint32_t timebase = 0)
{
    return beat16( beats_per_minute, timebase) >> 8;
}


/// Generates a 16-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute_88 the frequency of the wave, in Q8.8 format
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
/// @warning The BPM parameter **MUST** be provided in Q8.8 format! E.g.
/// for 120 BPM it would be 120*256 = 30720. If you just want to specify
/// "120", use beatsin16() or beatsin8().
LIB8STATIC uint16_t beatsin88( accum88 beats_per_minute_88, uint16_t lowest = 0, uint16_t highest = 65535,
                              uint32_t timebase = 0, uint16_t phase_offset = 0)
{
    uint16_t beat = beat88( beats_per_minute_88, timebase);
    uint16_t beatsin = (sin16( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

/// Generates a 16-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
LIB8STATIC uint16_t beatsin16( accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535,
                               uint32_t timebase = 0, uint16_t phase_offset = 0)
{
    uint16_t beat = beat16( beats_per_minute, timebase);
    uint16_t beatsin = (sin16( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

/// Generates an 8-bit sine wave at a given BPM that oscillates within
/// a given range.
/// @param beats_per_minute the frequency of the wave, in decimal
/// @param lowest the lowest output value of the sine wave
/// @param highest the highest output value of the sine wave
/// @param timebase the time offset of the wave from the millis() timer
/// @param phase_offset phase offset of the wave from the current position
LIB8STATIC uint8_t beatsin8( accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                            uint32_t timebase = 0, uint8_t phase_offset = 0)
{
    uint8_t beat = beat8( beats_per_minute, timebase);
    uint8_t beatsin = sin8( beat + phase_offset);
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8( beatsin, rangewidth);
    uint8_t result = lowest + scaledbeat;
    return result;
}

/// @} BeatGenerators

/// @} lib8tion, to exclude timekeeping functions


///////////////////////////////////////////////////////////////////////
///
/// @addtogroup Timekeeping
/// @{

/// Return the current seconds since boot in a 16-bit value.  Used as part of the
/// "every N time-periods" mechanism
LIB8STATIC uint16_t seconds16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t s16;
    s16 = ms / 1000;
    return s16;
}

/// Return the current minutes since boot in a 16-bit value.  Used as part of the
/// "every N time-periods" mechanism
LIB8STATIC uint16_t minutes16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t m16;
    m16 = (ms / (60000L)) & 0xFFFF;
    return m16;
}

/// Return the current hours since boot in an 8-bit value.  Used as part of the
/// "every N time-periods" mechanism
LIB8STATIC uint8_t hours8()
{
    uint32_t ms = GET_MILLIS();
    uint8_t h8;
    h8 = (ms / (3600000L)) & 0xFF;
    return h8;
}


/// Helper routine to divide a 32-bit value by 1024, returning
/// only the low 16 bits. 
/// You'd think this would be just
///   @code
///   result = (in32 >> 10) & 0xFFFF;
///   @endcode
/// And on ARM, that's what you want and all is well.
/// But on AVR that code turns into a loop that executes
/// a four-byte shift ten times: 40 shifts in all, plus loop
/// overhead. This routine gets exactly the same result with
/// just six shifts (vs 40), and no loop overhead.
/// Used to convert millis to "binary seconds" aka bseconds:
/// one bsecond == 1024 millis.
LIB8STATIC uint16_t div1024_32_16( uint32_t in32)
{
    uint16_t out16;
#if defined(__AVR__)
    asm volatile (
        "  lsr %D[in]  \n\t"
        "  ror %C[in]  \n\t"
        "  ror %B[in]  \n\t"
        "  lsr %D[in]  \n\t"
        "  ror %C[in]  \n\t"
        "  ror %B[in]  \n\t"
        "  mov %B[out],%C[in] \n\t"
        "  mov %A[out],%B[in] \n\t"
        : [in] "+r" (in32),
        [out] "=r" (out16)
    );
#else
    out16 = (in32 >> 10) & 0xFFFF;
#endif
    return out16;
}

/// Returns the current time-since-boot in
/// "binary seconds", which are actually 1024/1000 of a
/// second long.
LIB8STATIC uint16_t bseconds16()
{
    uint32_t ms = GET_MILLIS();
    uint16_t s16;
    s16 = div1024_32_16( ms);
    return s16;
}

/// Preprocessor-based class "template" for ::CEveryNTime, used with `EVERY_N_TIME` timekeepers. 
/// Classes to implement ::EVERY_N_MILLIS, ::EVERY_N_SECONDS,
/// ::EVERY_N_MINUTES, ::EVERY_N_HOURS, and ::EVERY_N_BSECONDS.
#if 1
#define INSTANTIATE_EVERY_N_TIME_PERIODS(NAME,TIMETYPE,TIMEGETTER) \
class NAME {    \
public: \
    TIMETYPE mPrevTrigger;  \
    TIMETYPE mPeriod;   \
    \
    NAME() { reset(); mPeriod = 1; }; \
    NAME(TIMETYPE period) { reset(); setPeriod(period); };    \
    void setPeriod( TIMETYPE period) { mPeriod = period; }; \
    TIMETYPE getTime() { return (TIMETYPE)(TIMEGETTER()); };    \
    TIMETYPE getPeriod() { return mPeriod; };   \
    TIMETYPE getElapsed() { return getTime() - mPrevTrigger; }  \
    TIMETYPE getRemaining() { return mPeriod - getElapsed(); }  \
    TIMETYPE getLastTriggerTime() { return mPrevTrigger; }  \
    bool ready() { \
        bool isReady = (getElapsed() >= mPeriod);   \
        if( isReady ) { reset(); }  \
        return isReady; \
    }   \
    void reset() { mPrevTrigger = getTime(); }; \
    void trigger() { mPrevTrigger = getTime() - mPeriod; }; \
        \
    operator bool() { return ready(); } \
};

/// @name CEveryNTime Base Classes
/// These macros define the time interval checking classes
/// used in the `EVERY_N_TIME` time macros.
/// @{

#if defined(FASTLED_DOXYGEN)
/// Time interval checking class. 
/// Keeps track of a time interval in order to limit how often code
/// is executed.
/// @note TIMETYPE is specific to the initialized class, and is in the
/// units used by the time function. E.g. for ::EVERY_N_MILLIS it's uint32_t
/// and milliseconds, for ::EVERY_N_HOURS it's uint8_t and hours, etc.
/// @warning This specific class isn't actually part of the library! It's created
/// using a preprocessor macro (::INSTANTIATE_EVERY_N_TIME_PERIODS) as
/// a new class for every different time unit. It has been recreated
/// specifically for the documentation, so that the methods can be documented
/// as usual.
/// @see INSTANTIATE_EVERY_N_TIME_PERIODS
class CEveryNTime {
public:
    TIMETYPE mPrevTrigger;  ///< Timestamp of the last time the class was "ready"
    TIMETYPE mPeriod;       ///< Timing interval to check

    /// Default constructor
    CEveryNTime() { reset(); mPeriod = 1; };
    /// Constructor
    /// @param period the time interval between triggers
    CEveryNTime(TIMETYPE period) { reset(); setPeriod(period); };

    /// Set the time interval between triggers
    void setPeriod( TIMETYPE period) { mPeriod = period; };

    /// Get the current time according to the class' timekeeper
    TIMETYPE getTime() { return (TIMETYPE)(TIMEGETTER()); };

    /// Get the time interval between triggers
    TIMETYPE getPeriod() { return mPeriod; };

    /// Get the time elapsed since the last trigger event
    TIMETYPE getElapsed() { return getTime() - mPrevTrigger; }

    /// Get the time until the next trigger event
    TIMETYPE getRemaining() { return mPeriod - getElapsed(); }

    /// Get the timestamp of the most recent trigger event
    TIMETYPE getLastTriggerTime() { return mPrevTrigger; }

    /// Check if the time interval has elapsed
    bool ready() {
        bool isReady = (getElapsed() >= mPeriod);
        if( isReady ) { reset(); }
        return isReady;
    }

    /// Reset the timestamp to the current time
    void reset() { mPrevTrigger = getTime(); };

    /// Reset the timestamp so it is ready() on next call
    void trigger() { mPrevTrigger = getTime() - mPeriod; };

    /// @copydoc ready()
    operator bool() { return ready(); }
};
#endif  // FASTLED_DOXYGEN

/// Create the CEveryNMillis class for millisecond intervals
INSTANTIATE_EVERY_N_TIME_PERIODS(CEveryNMillis,uint32_t,GET_MILLIS);

/// Create the CEveryNSeconds class for second intervals
INSTANTIATE_EVERY_N_TIME_PERIODS(CEveryNSeconds,uint16_t,seconds16);

/// Create the CEveryNBSeconds class for bsecond intervals
INSTANTIATE_EVERY_N_TIME_PERIODS(CEveryNBSeconds,uint16_t,bseconds16);

/// Create the CEveryNMinutes class for minutes intervals
INSTANTIATE_EVERY_N_TIME_PERIODS(CEveryNMinutes,uint16_t,minutes16);

/// Create the CEveryNHours class for hours intervals
INSTANTIATE_EVERY_N_TIME_PERIODS(CEveryNHours,uint8_t,hours8);

/// Alias for CEveryNMillis
#define CEveryNMilliseconds CEveryNMillis
/// @} CEveryNTime Base Classes

#else

// Under C++11 rules, we would be allowed to use not-external
// -linkage-type symbols as template arguments,
// e.g., LIB8STATIC seconds16, and we'd be able to use these
// templates as shown below.
// However, under C++03 rules, we cannot do that, and thus we
// have to resort to the preprocessor to 'instantiate' 'templates',
// as handled above.
template<typename timeType,timeType (*timeGetter)()>
class CEveryNTimePeriods {
public:
    timeType mPrevTrigger;
    timeType mPeriod;

    CEveryNTimePeriods() { reset(); mPeriod = 1; };
    CEveryNTimePeriods(timeType period) { reset(); setPeriod(period); };
    void setPeriod( timeType period) { mPeriod = period; };
    timeType getTime() { return (timeType)(timeGetter()); };
    timeType getPeriod() { return mPeriod; };
    timeType getElapsed() { return getTime() - mPrevTrigger; }
    timeType getRemaining() { return mPeriod - getElapsed(); }
    timeType getLastTriggerTime() { return mPrevTrigger; }
    bool ready() {
        bool isReady = (getElapsed() >= mPeriod);
        if( isReady ) { reset(); }
        return isReady;
    }
    void reset() { mPrevTrigger = getTime(); };
    void trigger() { mPrevTrigger = getTime() - mPeriod; };

    operator bool() { return ready(); }
};
typedef CEveryNTimePeriods<uint16_t,seconds16> CEveryNSeconds;
typedef CEveryNTimePeriods<uint16_t,bseconds16> CEveryNBSeconds;
typedef CEveryNTimePeriods<uint32_t,millis> CEveryNMillis;
typedef CEveryNTimePeriods<uint16_t,minutes16> CEveryNMinutes;
typedef CEveryNTimePeriods<uint8_t,hours8> CEveryNHours;
#endif


/// @name "EVERY_N_TIME" Macros
/// Check whether to excecute a block of code every N amount of time.
/// These are useful for limiting how often code runs. For example,
/// you can use ::fill_rainbow() to fill a strip of LEDs with color,
/// combined with an ::EVERY_N_MILLIS block to limit how fast the colors
/// change:
///   @code{.cpp}
///   static uint8_t hue = 0;
///   fill_rainbow(leds, NUM_LEDS, hue);
///   EVERY_N_MILLIS(20) { hue++; }  // advances hue every 20 milliseconds
///   @endcode
/// Note that in order for these to be accurate, the EVERY_N block must
/// be evaluated at a regular basis.  
/// @{

/// @cond
#define CONCAT_HELPER( x, y ) x##y
#define CONCAT_MACRO( x, y ) CONCAT_HELPER( x, y )
/// @endcond


/// Checks whether to execute a block of code every N milliseconds
/// @see GET_MILLIS
#define EVERY_N_MILLIS(N) EVERY_N_MILLIS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)

/// Checks whether to execute a block of code every N milliseconds, using a custom instance name
/// @copydetails EVERY_N_MILLIS
#define EVERY_N_MILLIS_I(NAME,N) static CEveryNMillis NAME(N); if( NAME )


/// Checks whether to execute a block of code every N seconds
/// @see seconds16()
#define EVERY_N_SECONDS(N) EVERY_N_SECONDS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)

/// Checks whether to execute a block of code every N seconds, using a custom instance name
/// @copydetails EVERY_N_SECONDS
#define EVERY_N_SECONDS_I(NAME,N) static CEveryNSeconds NAME(N); if( NAME )


/// Checks whether to execute a block of code every N bseconds
/// @see bseconds16()
#define EVERY_N_BSECONDS(N) EVERY_N_BSECONDS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)

/// Checks whether to execute a block of code every N bseconds, using a custom instance name
/// @copydetails EVERY_N_BSECONDS
#define EVERY_N_BSECONDS_I(NAME,N) static CEveryNBSeconds NAME(N); if( NAME )


/// Checks whether to execute a block of code every N minutes
/// @see minutes16()
#define EVERY_N_MINUTES(N) EVERY_N_MINUTES_I(CONCAT_MACRO(PER, __COUNTER__ ),N)

/// Checks whether to execute a block of code every N minutes, using a custom instance name
/// @copydetails EVERY_N_MINUTES
#define EVERY_N_MINUTES_I(NAME,N) static CEveryNMinutes NAME(N); if( NAME )


/// Checks whether to execute a block of code every N hours
/// @see hours8()
#define EVERY_N_HOURS(N) EVERY_N_HOURS_I(CONCAT_MACRO(PER, __COUNTER__ ),N)

/// Checks whether to execute a block of code every N hours, using a custom instance name
/// @copydetails EVERY_N_HOURS
#define EVERY_N_HOURS_I(NAME,N) static CEveryNHours NAME(N); if( NAME )


/// Alias for ::EVERY_N_MILLIS
#define EVERY_N_MILLISECONDS(N) EVERY_N_MILLIS(N)
/// Alias for ::EVERY_N_MILLIS_I
#define EVERY_N_MILLISECONDS_I(NAME,N) EVERY_N_MILLIS_I(NAME,N)

/// @} Every_N
/// @} Timekeeping


// These defines are used to declare hidden or commented symbols for the
// purposes of Doxygen documentation generation. They do not affect your program.
#ifdef FASTLED_DOXYGEN
/// Set this flag to use the get_millisecond_timer() function in place
/// of the default millis() function.
/// @ingroup Timekeeping
#define USE_GET_MILLISECOND_TIMER
#endif

FASTLED_NAMESPACE_END

#endif
