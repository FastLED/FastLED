#ifndef __INC_LIB8TION_H
#define __INC_LIB8TION_H

/*

 Fast, efficient 8-bit math functions specifically
 designed for high-performance LED programming.

 Because of the AVR(Arduino) and ARM assembly language
 implementations provided, using these functions often
 results in smaller and faster code than the equivalent
 program using plain "C" arithmetic and logic.


 Included are:


 - Saturating unsigned 8-bit add and subtract.
   Instead of wrapping around if an overflow occurs,
   these routines just 'clamp' the output at a maxumum
   of 255, or a minimum of 0.  Useful for adding pixel
   values.  E.g., qadd8( 200, 100) = 255.

     qadd8( i, j) == MIN( (i + j), 0xFF )
     qsub8( i, j) == MAX( (i - j), 0 )

 - Saturating signed 8-bit ("7-bit") add.
     qadd7( i, j) == MIN( (i + j), 0x7F)


 - Scaling (down) of unsigned 8- and 16- bit values.
   Scaledown value is specified in 1/256ths.
     scale8( i, sc) == (i * sc) / 256
     scale16by8( i, sc) == (i * sc) / 256

   Example: scaling a 0-255 value down into a
   range from 0-99:
     downscaled = scale8( originalnumber, 100);

   A special version of scale8 is provided for scaling
   LED brightness values, to make sure that they don't
   accidentally scale down to total black at low
   dimming levels, since that would look wrong:
     scale8_video( i, sc) = ((i * sc) / 256) +? 1

   Example: reducing an LED brightness by a
   dimming factor:
     new_bright = scale8_video( orig_bright, dimming);


 - Fast 8- and 16- bit unsigned random numbers.
   Significantly faster than Arduino random(), but
   also somewhat less random.  You can add entropy.
     random8()       == random from 0..255
     random8( n)     == random from 0..(N-1)
     random8( n, m)  == random from N..(M-1)

     random16()      == random from 0..65535
     random16( n)    == random from 0..(N-1)
     random16( n, m) == random from N..(M-1)

     random16_set_seed( k)    ==  seed = k
     random16_add_entropy( k) ==  seed += k


 - Absolute value of a signed 8-bit value.
     abs8( i)     == abs( i)


 - 8-bit math operations which return 8-bit values.
   These are provided mostly for completeness,
   not particularly for performance.
     mul8( i, j)  == (i * j) & 0xFF
     add8( i, j)  == (i + j) & 0xFF
     sub8( i, j)  == (i - j) & 0xFF


 - Fast 16-bit approximations of sin and cos.
   Input angle is a uint16_t from 0-65535.
   Output is a signed int16_t from -32767 to 32767.
      sin16( x)  == sin( (x/32768.0) * pi) * 32767
      cos16( x)  == cos( (x/32768.0) * pi) * 32767
   Accurate to more than 99% in all cases.

 - Fast 8-bit approximations of sin and cos.
   Input angle is a uint8_t from 0-255.
   Output is an UNsigned uint8_t from 0 to 255.
       sin8( x)  == (sin( (x/128.0) * pi) * 128) + 128
       cos8( x)  == (cos( (x/128.0) * pi) * 128) + 128
   Accurate to within about 2%.


 - Fast 8-bit "easing in/out" function.
     ease8InOutCubic(x) == 3(x^i) - 2(x^3)
     ease8InOutApprox(x) ==
       faster, rougher, approximation of cubic easing
     ease8InOutQuad(x) == quadratic (vs cubic) easing

 - Cubic, Quadratic, and Triangle wave functions.
   Input is a uint8_t representing phase withing the wave,
     similar to how sin8 takes an angle 'theta'.
   Output is a uint8_t representing the amplitude of
     the wave at that point.
       cubicwave8( x)
       quadwave8( x)
       triwave8( x)

 - Square root for 16-bit integers.  About three times
   faster and five times smaller than Arduino's built-in
   generic 32-bit sqrt routine.
     sqrt16( uint16_t x ) == sqrt( x)
 
 - Dimming and brightening functions for 8-bit
   light values.
     dim8_video( x)  == scale8_video( x, x)
     dim8_raw( x)    == scale8( x, x)
     dim8_lin( x)    == (x<128) ? ((x+1)/2) : scale8(x,x)
     brighten8_video( x) == 255 - dim8_video( 255 - x)
     brighten8_raw( x) == 255 - dim8_raw( 255 - x)
     brighten8_lin( x) == 255 - dim8_lin( 255 - x)
   The dimming functions in particular are suitable
   for making LED light output appear more 'linear'.


 - Linear interpolation between two values, with the
   fraction between them expressed as an 8- or 16-bit
   fixed point fraction (fract8 or fract16).
     lerp8by8(   fromU8, toU8, fract8 )
     lerp16by8(  fromU16, toU16, fract8 )
     lerp15by8(  fromS16, toS16, fract8 )
       == from + (( to - from ) * fract8) / 256)
     lerp16by16( fromU16, toU16, fract16 )
       == from + (( to - from ) * fract16) / 65536)
     map8( in, rangeStart, rangeEnd)
       == map( in, 0, 255, rangeStart, rangeEnd);

 - Optimized memmove, memcpy, and memset, that are
   faster than standard avr-libc 1.8.
      memmove8( dest, src,  bytecount)
      memcpy8(  dest, src,  bytecount)
      memset8(  buf, value, bytecount)

 - Beat generators which return sine or sawtooth
   waves in a specified number of Beats Per Minute.
   Sine wave beat generators can specify a low and
   high range for the output.  Sawtooth wave beat
   generators always range 0-255 or 0-65535.
     beatsin8( BPM, low8, high8) 
         = (sine(beatphase) * (high8-low8)) + low8
     beatsin16( BPM, low16, high16)
         = (sine(beatphase) * (high16-low16)) + low16
     beat8( BPM)  = 8-bit repeating sawtooth wave
     beat16( BPM) = 16-bit repeating sawtooth wave


Lib8tion is pronounced like 'libation': lie-BAY-shun

*/



#include <stdint.h>

#define LIB8STATIC __attribute__ ((unused)) static inline

#if !defined(__AVR__)
#include <string.h>
// for memmove, memcpy, and memset if not defined here
#endif

#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
#define LIB8_ATTINY 1
#endif


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
#endif

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


#elif defined(__AVR__)

// AVR ATmega and friends Arduino

#define QADD8_C 0
#define QADD7_C 0
#define QSUB8_C 0
#define ABS8_C 0
#define ADD8_C 0
#define SUB8_C 0

#define QADD8_AVRASM 1
#define QADD7_AVRASM 1
#define QSUB8_AVRASM 1
#define ABS8_AVRASM 1
#define ADD8_AVRASM 1
#define SUB8_AVRASM 1

// Note: these require hardware MUL instruction
//       -- sorry, ATtiny!
#if !defined(LIB8_ATTINY)
#define SCALE8_C 0
#define SCALE16BY8_C 0
#define SCALE16_C 0
#define MUL8_C 0
#define QMUL8_C 0
#define EASE8_C 0
#define SCALE8_AVRASM 1
#define SCALE16BY8_AVRASM 1
#define SCALE16_AVRASM 1
#define MUL8_AVRASM 1
#define QMUL8_AVRASM 1
#define EASE8_AVRASM 1
#define CLEANUP_R1_AVRASM 1
#else
// On ATtiny, we just use C implementations
#define SCALE8_C 1
#define SCALE16BY8_C 1
#define SCALE16_C 1
#define MUL8_C 1
#define QMUL8_C 1
#define EASE8_C 1
#define SCALE8_AVRASM 0
#define SCALE16BY8_AVRASM 0
#define SCALE16_AVRASM 0
#define MUL8_AVRASM 0
#define QMUL8_AVRASM 0
#define EASE8_AVRASM 0
#endif

#else

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

#endif


///////////////////////////////////////////////////////////////////////
//
// typdefs for fixed-point fractional types.
//
// sfract7 should be interpreted as signed 128ths.
// fract8 should be interpreted as unsigned 256ths.
// sfract15 should be interpreted as signed 32768ths.
// fract16 should be interpreted as unsigned 65536ths.
//
// Example: if a fract8 has the value "64", that should be interpreted
//          as 64/256ths, or one-quarter.
//
//
//  fract8   range is 0 to 0.99609375
//                 in steps of 0.00390625
//
//  sfract7  range is -0.9921875 to 0.9921875
//                 in steps of 0.0078125
//
//  fract16  range is 0 to 0.99998474121
//                 in steps of 0.00001525878
//
//  sfract15 range is -0.99996948242 to 0.99996948242
//                 in steps of 0.00003051757
//

typedef uint8_t   fract8;   // ANSI: unsigned short _Fract
typedef int8_t    sfract7;  // ANSI: signed   short _Fract
typedef uint16_t  fract16;  // ANSI: unsigned       _Fract
typedef int16_t   sfract15; // ANSI: signed         _Fract


// accumXY types should be interpreted as X bits of integer,
//         and Y bits of fraction.
//         E.g., accum88 has 8 bits of int, 8 bits of fraction

typedef uint16_t  accum88;  // ANSI: unsigned short _Accum
typedef int16_t   saccum78; // ANSI: signed   short _Accum
typedef uint32_t  accum1616;// ANSI: signed         _Accum
typedef int32_t   saccum1516;//ANSI: signed         _Accum
typedef uint16_t  accum124; // no direct ANSI counterpart
typedef int32_t   saccum114;// no direct ANSI counterpart


// typedef for IEEE754 "binary32" float type internals

typedef union {
    uint32_t i;
    float    f;
    struct {
        uint32_t mantissa: 23;
        uint32_t exponent:  8;
        uint32_t signbit:   1;
    };
    struct {
        uint32_t mant7 :  7;
        uint32_t mant16: 16;
        uint32_t exp_  :  8;
        uint32_t sb_   :  1;
    };
    struct {
        uint32_t mant_lo8 : 8;
        uint32_t mant_hi16_exp_lo1 : 16;
        uint32_t sb_exphi7 : 8;
    };
} IEEE754binary32_t;



///////////////////////////////////////////////////////////////////////

// qadd8: add one byte to another, saturating at 0xFF
LIB8STATIC uint8_t qadd8( uint8_t i, uint8_t j)
{
#if QADD8_C == 1
    int t = i + j;
    if( t > 255) t = 255;
    return t;
#elif QADD8_AVRASM == 1
    asm volatile(
         /* First, add j to i, conditioning the C flag */
         "add %0, %1    \n\t"

         /* Now test the C flag.
           If C is clear, we branch around a load of 0xFF into i.
           If C is set, we go ahead and load 0xFF into i.
         */
         "brcc L_%=     \n\t"
         "ldi %0, 0xFF  \n\t"
         "L_%=: "
         : "+a" (i)
         : "a"  (j) );
    return i;
#elif QADD8_ARM_DSP_ASM == 1
    asm volatile( "uqadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
#else
#error "No implementation for qadd8 available."
#endif
}


// qadd7: add one signed byte to another,
//        saturating at 0x7F.
LIB8STATIC int8_t qadd7( int8_t i, int8_t j)
{
#if QADD7_C == 1
    int16_t t = i + j;
    if( t > 127) t = 127;
    return t;
#elif QADD7_AVRASM == 1
    asm volatile(
         /* First, add j to i, conditioning the V flag */
         "add %0, %1    \n\t"

         /* Now test the V flag.
          If V is clear, we branch around a load of 0x7F into i.
          If V is set, we go ahead and load 0x7F into i.
          */
         "brvc L_%=     \n\t"
         "ldi %0, 0x7F  \n\t"
         "L_%=: "
         : "+a" (i)
         : "a"  (j) );

    return i;
#elif QADD7_ARM_DSP_ASM == 1
    asm volatile( "qadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
#else
#error "No implementation for qadd7 available."
#endif
}

// qsub8: subtract one byte from another, saturating at 0x00
LIB8STATIC uint8_t qsub8( uint8_t i, uint8_t j)
{
#if QSUB8_C == 1
    int t = i - j;
    if( t < 0) t = 0;
    return t;
#elif QSUB8_AVRASM == 1

    asm volatile(
         /* First, subtract j from i, conditioning the C flag */
         "sub %0, %1    \n\t"

         /* Now test the C flag.
          If C is clear, we branch around a load of 0x00 into i.
          If C is set, we go ahead and load 0x00 into i.
          */
         "brcc L_%=     \n\t"
         "ldi %0, 0x00  \n\t"
         "L_%=: "
         : "+a" (i)
         : "a"  (j) );

    return i;
#else
#error "No implementation for qsub8 available."
#endif
}

// add8: add one byte to another, with one byte result
LIB8STATIC uint8_t add8( uint8_t i, uint8_t j)
{
#if ADD8_C == 1
    int t = i + j;
    return t;
#elif ADD8_AVRASM == 1
    // Add j to i, period.
    asm volatile( "add %0, %1" : "+a" (i) : "a" (j));
    return i;
#else
#error "No implementation for add8 available."
#endif
}


// sub8: subtract one byte from another, 8-bit result
LIB8STATIC uint8_t sub8( uint8_t i, uint8_t j)
{
#if SUB8_C == 1
    int t = i - j;
    return t;
#elif SUB8_AVRASM == 1
    // Subtract j from i, period.
    asm volatile( "sub %0, %1" : "+a" (i) : "a" (j));
    return i;
#else
#error "No implementation for sub8 available."
#endif
}


// scale8: scale one byte by a second one, which is treated as
//         the numerator of a fraction whose denominator is 256
//         In other words, it computes i * (scale / 256)
//         4 clocks AVR, 2 clocks ARM
LIB8STATIC uint8_t scale8( uint8_t i, fract8 scale)
{
#if SCALE8_C == 1
    return
    ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
#if defined(LIB8_ATTINY)
    uint8_t work=0;
    uint8_t cnt=0x80;
    asm volatile(
        "LOOP_%=:                             \n\t"
        /*"  sbrc %[scale], 0             \n\t"
        "  add %[work], %[i]            \n\t"
        "  ror %[work]                  \n\t"
        "  lsr %[scale]                 \n\t"
        "  clc                          \n\t"*/
        "  sbrc %[scale], 0             \n\t"
        "  add %[work], %[i]            \n\t"
        "  ror %[work]                  \n\t"
        "  lsr %[scale]                 \n\t"
        "  lsr %[cnt]                   \n\t"
        "brcc LOOP_%="
        : [work] "+r" (work), [cnt] "+r" (cnt)
        : [scale] "r" (scale), [i] "r" (i)
        :
      );
    return work;
#else
    asm volatile(
         /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
         "mul %0, %1          \n\t"
         /* Move the high 8-bits of the product (r1) back to i */
         "mov %0, r1          \n\t"
         /* Restore r1 to "0"; it's expected to always be that */
         "clr __zero_reg__    \n\t"

         : "+a" (i)      /* writes to i */
         : "a"  (scale)  /* uses scale */
         : "r0", "r1"    /* clobbers r0, r1 */ );

    /* Return the result */
    return i;
#endif
#else
#error "No implementation for scale8 available."
#endif
}


//  The "video" version of scale8 guarantees that the output will
//  be only be zero if one or both of the inputs are zero.  If both
//  inputs are non-zero, the output is guaranteed to be non-zero.
//  This makes for better 'video'/LED dimming, at the cost of
//  several additional cycles.
LIB8STATIC uint8_t scale8_video( uint8_t i, fract8 scale)
{
#if SCALE8_C == 1 || defined(LIB8_ATTINY)
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i&&scale)?1:0);
    // uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    // uint8_t j = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    return j;
#elif SCALE8_AVRASM == 1
    uint8_t j=0;
    asm volatile(
        "  tst %[i]\n\t"
        "  breq L_%=\n\t"
        "  mul %[i], %[scale]\n\t"
        "  mov %[j], r1\n\t"
        "  clr __zero_reg__\n\t"
        "  cpse %[scale], r1\n\t"
        "  subi %[j], 0xFF\n\t"
        "L_%=: \n\t"
        : [j] "+a" (j)
        : [i] "a" (i), [scale] "a" (scale)
        : "r0", "r1");

    return j;
    // uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    // asm volatile(
    //      "      tst %0           \n"
    //      "      breq L_%=        \n"
    //      "      mul %0, %1       \n"
    //      "      mov %0, r1       \n"
    //      "      add %0, %2       \n"
    //      "      clr __zero_reg__ \n"
    //      "L_%=:                  \n"

    //      : "+a" (i)
    //      : "a" (scale), "a" (nonzeroscale)
    //      : "r0", "r1");

    // // Return the result
    // return i;
#else
#error "No implementation for scale8_video available."
#endif
}


// This version of scale8 does not clean up the R1 register on AVR
// If you are doing several 'scale8's in a row, use this, and
// then explicitly call cleanup_R1.
LIB8STATIC uint8_t scale8_LEAVING_R1_DIRTY( uint8_t i, fract8 scale)
{
#if SCALE8_C == 1
    return ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    asm volatile(
         /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
         "mul %0, %1    \n\t"
         /* Move the high 8-bits of the product (r1) back to i */
         "mov %0, r1    \n\t"
         /* R1 IS LEFT DIRTY HERE; YOU MUST ZERO IT OUT YOURSELF  */
         /* "clr __zero_reg__    \n\t" */

         : "+a" (i)      /* writes to i */
         : "a"  (scale)  /* uses scale */
         : "r0", "r1"    /* clobbers r0, r1 */ );

    // Return the result
    return i;
#else
#error "No implementation for scale8_LEAVING_R1_DIRTY available."
#endif
}

//   THIS FUNCTION ALWAYS MODIFIES ITS ARGUMENT DIRECTLY IN PLACE

LIB8STATIC void nscale8_LEAVING_R1_DIRTY( uint8_t& i, fract8 scale)
{
#if SCALE8_C == 1
    i = ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    asm volatile(
         /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
         "mul %0, %1    \n\t"
         /* Move the high 8-bits of the product (r1) back to i */
         "mov %0, r1    \n\t"
         /* R1 IS LEFT DIRTY HERE; YOU MUST ZERO IT OUT YOURSELF */
         /* "clr __zero_reg__    \n\t" */

         : "+a" (i)      /* writes to i */
         : "a"  (scale)  /* uses scale */
         : "r0", "r1"    /* clobbers r0, r1 */ );
#else
#error "No implementation for nscale8_LEAVING_R1_DIRTY available."
#endif
}



LIB8STATIC uint8_t scale8_video_LEAVING_R1_DIRTY( uint8_t i, fract8 scale)
{
#if SCALE8_C == 1 || defined(LIB8_ATTINY)
    uint8_t j = (((int)i * (int)scale) >> 8) + ((i&&scale)?1:0);
    // uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    // uint8_t j = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    return j;
#elif SCALE8_AVRASM == 1
    uint8_t j=0;
    asm volatile(
        "  tst %[i]\n\t"
        "  breq L_%=\n\t"
        "  mul %[i], %[scale]\n\t"
        "  mov %[j], r1\n\t"
        "  breq L_%=\n\t"
        "  subi %[j], 0xFF\n\t"
        "L_%=: \n\t"
        : [j] "+a" (j)
        : [i] "a" (i), [scale] "a" (scale)
        : "r0", "r1");

    return j;
    // uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    // asm volatile(
    //      "      tst %0           \n"
    //      "      breq L_%=        \n"
    //      "      mul %0, %1       \n"
    //      "      mov %0, r1       \n"
    //      "      add %0, %2       \n"
    //      "      clr __zero_reg__ \n"
    //      "L_%=:                  \n"

    //      : "+a" (i)
    //      : "a" (scale), "a" (nonzeroscale)
    //      : "r0", "r1");

    // // Return the result
    // return i;
#else
#error "No implementation for scale8_video_LEAVING_R1_DIRTY available."
#endif
}

LIB8STATIC void nscale8_video_LEAVING_R1_DIRTY( uint8_t & i, fract8 scale)
{
#if SCALE8_C == 1 || defined(LIB8_ATTINY)
    i = (((int)i * (int)scale) >> 8) + ((i&&scale)?1:0);
#elif SCALE8_AVRASM == 1
    asm volatile(
        "  tst %[i]\n\t"
        "  breq L_%=\n\t"
        "  mul %[i], %[scale]\n\t"
        "  mov %[i], r1\n\t"
        "  breq L_%=\n\t"
        "  subi %[i], 0xFF\n\t"
        "L_%=: \n\t"
        : [i] "+a" (i)
        : [scale] "a" (scale)
        : "r0", "r1");
#else
#error "No implementation for scale8_video_LEAVING_R1_DIRTY available."
#endif
}


LIB8STATIC void cleanup_R1()
{
#if CLEANUP_R1_AVRASM == 1
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "clr __zero_reg__  \n\t" : : : "r1" );
#endif
}


// nscale8x3: scale three one byte values by a fourth one, which is treated as
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes r,g,b * (scale / 256)
//
//         THIS FUNCTION ALWAYS MODIFIES ITS ARGUMENTS IN PLACE

LIB8STATIC void nscale8x3( uint8_t& r, uint8_t& g, uint8_t& b, fract8 scale)
{
#if SCALE8_C == 1
    r = ((int)r * (int)(scale) ) >> 8;
    g = ((int)g * (int)(scale) ) >> 8;
    b = ((int)b * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    r = scale8_LEAVING_R1_DIRTY(r, scale);
    g = scale8_LEAVING_R1_DIRTY(g, scale);
    b = scale8_LEAVING_R1_DIRTY(b, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x3 available."
#endif
}


LIB8STATIC void nscale8x3_video( uint8_t& r, uint8_t& g, uint8_t& b, fract8 scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scale) ) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scale) ) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scale) ) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    nscale8_video_LEAVING_R1_DIRTY( r, scale);
    nscale8_video_LEAVING_R1_DIRTY( g, scale);
    nscale8_video_LEAVING_R1_DIRTY( b, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x3 available."
#endif
}

// nscale8x2: scale two one byte values by a third one, which is treated as
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes i,j * (scale / 256)
//
//         THIS FUNCTION ALWAYS MODIFIES ITS ARGUMENTS IN PLACE

LIB8STATIC void nscale8x2( uint8_t& i, uint8_t& j, fract8 scale)
{
#if SCALE8_C == 1
    i = ((int)i * (int)(scale) ) >> 8;
    j = ((int)j * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    i = scale8_LEAVING_R1_DIRTY(i, scale);
    j = scale8_LEAVING_R1_DIRTY(j, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x2 available."
#endif
}


LIB8STATIC void nscale8x2_video( uint8_t& i, uint8_t& j, fract8 scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    i = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    j = (j == 0) ? 0 : (((int)j * (int)(scale) ) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    nscale8_video_LEAVING_R1_DIRTY( i, scale);
    nscale8_video_LEAVING_R1_DIRTY( j, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x2 available."
#endif
}


// scale16by8: scale a 16-bit unsigned value by an 8-bit value,
//         considered as numerator of a fraction whose denominator
//         is 256. In other words, it computes i * (scale / 256)

#if SCALE16BY8_C == 1
LIB8STATIC uint16_t scale16by8( uint16_t i, fract8 scale )
{
    uint16_t result;
    result = (i * scale) / 256;
    return result;
}
#elif SCALE16BY8_AVRASM == 1
LIB8STATIC uint16_t scale16by8( uint16_t i, fract8 scale )
{
    uint16_t result;
    asm volatile(
         // result.A = HighByte(i.A x j )
         "  mul %A[i], %[scale]                 \n\t"
         "  mov %A[result], r1                  \n\t"
         "  clr %B[result]                      \n\t"

         // result.A-B += i.B x j
         "  mul %B[i], %[scale]                 \n\t"
         "  add %A[result], r0                  \n\t"
         "  adc %B[result], r1                  \n\t"

         // cleanup r1
         "  clr __zero_reg__                    \n\t"

         : [result] "=r" (result)
         : [i] "r" (i), [scale] "r" (scale)
         : "r0", "r1"
         );
    return result;
}
#else
#error "No implementation for scale16by8 available."
#endif

// scale16: scale a 16-bit unsigned value by a 16-bit value,
//         considered as numerator of a fraction whose denominator
//         is 65536. In other words, it computes i * (scale / 65536)

#if SCALE16_C == 1
LIB8STATIC uint16_t scale16( uint16_t i, fract16 scale )
{
    uint16_t result;
    result = ((uint32_t)(i) * (uint32_t)(scale)) / 65536;
    return result;
}
#elif SCALE16_AVRASM == 1
LIB8STATIC
uint16_t scale16( uint16_t i, fract16 scale )
{
    uint32_t result = 0;
    const uint8_t  zero = 0;
    asm volatile(
                 // result.A-B  = i.A x scale.A
                 "  mul %A[i], %A[scale]                 \n\t"
                 //  save results...
                 // basic idea:
                 //"  mov %A[result], r0                 \n\t"
                 //"  mov %B[result], r1                 \n\t"
                 // which can be written as...
                 "  movw %A[result], r0                   \n\t"
                 // We actually need to do anything with r0,
                 // as result.A is never used again here, so we
                 // could just move the high byte, but movw is
                 // one clock cycle, just like mov, so might as
                 // well, in case we want to use this code for
                 // a generic 16x16 multiply somewhere.

                 // result.C-D  = i.B x scale.B
                 "  mul %B[i], %B[scale]                 \n\t"
                 //"  mov %C[result], r0                 \n\t"
                 //"  mov %D[result], r1                 \n\t"
                 "  movw %C[result], r0                   \n\t"

                 // result.B-D += i.B x scale.A
                 "  mul %B[i], %A[scale]                 \n\t"

                 "  add %B[result], r0                   \n\t"
                 "  adc %C[result], r1                   \n\t"
                 "  adc %D[result], %[zero]              \n\t"

                 // result.B-D += i.A x scale.B
                 "  mul %A[i], %B[scale]                 \n\t"

                 "  add %B[result], r0                   \n\t"
                 "  adc %C[result], r1                   \n\t"
                 "  adc %D[result], %[zero]              \n\t"

                 // cleanup r1
                 "  clr r1                               \n\t"

                 : [result] "+r" (result)
                 : [i] "r" (i),
                   [scale] "r" (scale),
                   [zero] "r" (zero)
                 : "r0", "r1"
                 );
    result = result >> 16;
    return result;
}
#else
#error "No implementation for scale16 available."
#endif



// mul8: 8x8 bit multiplication, with 8 bit result
LIB8STATIC uint8_t mul8( uint8_t i, uint8_t j)
{
#if MUL8_C == 1
    return ((int)i * (int)(j) ) & 0xFF;
#elif MUL8_AVRASM == 1
    asm volatile(
         /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
         "mul %0, %1          \n\t"
         /* Extract the LOW 8-bits (r0) */
         "mov %0, r0          \n\t"
         /* Restore r1 to "0"; it's expected to always be that */
         "clr __zero_reg__    \n\t"
         : "+a" (i)
         : "a"  (j)
         : "r0", "r1");

    return i;
#else
#error "No implementation for mul8 available."
#endif
}


// mul8: saturating 8x8 bit multiplication, with 8 bit result
LIB8STATIC uint8_t qmul8( uint8_t i, uint8_t j)
{
#if QMUL8_C == 1
    int p = ((int)i * (int)(j) );
    if( p > 255) p = 255;
    return p;
#elif QMUL8_AVRASM == 1
    asm volatile(
                 /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
                 "  mul %0, %1          \n\t"
                 /* If high byte of result is zero, all is well. */
                 "  tst r1              \n\t"
                 "  breq Lnospill_%=    \n\t"
                 /* If high byte of result > 0, saturate low byte to 0xFF */
                 "  ldi %0,0xFF         \n\t"
                 "  rjmp Ldone_%=       \n\t"
                 "Lnospill_%=:          \n\t"
                 /* Extract the LOW 8-bits (r0) */
                 "  mov %0, r0          \n\t"
                 "Ldone_%=:             \n\t"
                 /* Restore r1 to "0"; it's expected to always be that */
                 "  clr __zero_reg__    \n\t"
                 : "+a" (i)
                 : "a"  (j)
                 : "r0", "r1");

    return i;
#else
#error "No implementation for qmul8 available."
#endif
}


// abs8: take abs() of a signed 8-bit uint8_t
LIB8STATIC int8_t abs8( int8_t i)
{
#if ABS8_C == 1
    if( i < 0) i = -i;
    return i;
#elif ABS8_AVRASM == 1


    asm volatile(
         /* First, check the high bit, and prepare to skip if it's clear */
         "sbrc %0, 7 \n"

         /* Negate the value */
         "neg %0     \n"

         : "+r" (i) : "r" (i) );
    return i;
#else
#error "No implementation for abs8 available."
#endif
}


///////////////////////////////////////////////////////////////////////
//
// float-to-fixed and fixed-to-float conversions
//
// Note that anything involving a 'float' on AVR will be slower.

// floatToSfract15: conversion from IEEE754 float in the range (-1,1)
//                  to 16-bit fixed point.  Note that the extremes of
//                  one and negative one are NOT representable.  The
//                  representable range is basically
//
// sfract15ToFloat: conversion from sfract15 fixed point to
//                  IEEE754 32-bit float.

LIB8STATIC
float sfract15ToFloat( sfract15 y)
{
    return y / 32768.0;
}

LIB8STATIC
sfract15 floatToSfract15( float f)
{
    return f * 32768.0;
}



///////////////////////////////////////////////////////////////////////

// Dimming and brightening functions
//
// The eye does not respond in a linear way to light.
// High speed PWM'd LEDs at 50% duty cycle appear far
// brighter then the 'half as bright' you might expect.
//
// If you want your midpoint brightness leve (128) to
// appear half as bright as 'full' brightness (255), you
// have to apply a 'dimming function'.
//
//

LIB8STATIC uint8_t dim8_raw( uint8_t x)
{
    return scale8( x, x);
}

LIB8STATIC uint8_t dim8_video( uint8_t x)
{
    return scale8_video( x, x);
}

LIB8STATIC uint8_t dim8_lin( uint8_t x )
{
    if( x & 0x80 ) {
        x = scale8( x, x);
    } else {
        x += 1;
        x /= 2;
    }
    return x;
}

LIB8STATIC uint8_t brighten8_raw( uint8_t x)
{
    uint8_t ix = 255 - x;
    return 255 - scale8( ix, ix);
}

LIB8STATIC uint8_t brighten8_video( uint8_t x)
{
    uint8_t ix = 255 - x;
    return 255 - scale8_video( ix, ix);
}

LIB8STATIC uint8_t brighten8_lin( uint8_t x )
{
    uint8_t ix = 255 - x;
    if( ix & 0x80 ) {
        ix = scale8( ix, ix);
    } else {
        ix += 1;
        ix /= 2;
    }
    return 255 - ix;
}

///////////////////////////////////////////////////////////////////////

// A 16-bit PNRG good enough for LED animations

// X(n+1) = (2053 * X(n)) + 13849)
#define RAND16_2053  2053
#define RAND16_13849 13849

extern uint16_t rand16seed;// = RAND16_SEED;


LIB8STATIC uint8_t random8()
{
    rand16seed = (rand16seed * RAND16_2053) + RAND16_13849;
    return rand16seed;
}

LIB8STATIC uint16_t random16()
{
    rand16seed = (rand16seed * RAND16_2053) + RAND16_13849;
    return rand16seed;
}


LIB8STATIC uint8_t random8(uint8_t lim)
{
    uint8_t r = random8();
    r = scale8( r, lim);
    return r;
}

LIB8STATIC uint8_t random8(uint8_t min, uint8_t lim)
{
    uint8_t delta = lim - min;
    uint8_t r = random8(delta) + min;
    return r;
}

LIB8STATIC uint16_t random16( uint16_t lim)
{
    uint16_t r = random16();
    uint32_t p = (uint32_t)lim * (uint32_t)r;
    r = p >> 16;
    return r;
}

LIB8STATIC uint16_t random16( uint16_t min, uint16_t lim)
{
    uint16_t delta = lim - min;
    uint16_t r = random16( delta) + min;
    return r;
}

LIB8STATIC void random16_set_seed( uint16_t seed)
{
    rand16seed = seed;
}

LIB8STATIC uint16_t random16_get_seed()
{
    return rand16seed;
}

LIB8STATIC void random16_add_entropy( uint16_t entropy)
{
    rand16seed += entropy;
}


///////////////////////////////////////////////////////////////////////

// sin16 & cos16:
//        Fast 16-bit approximations of sin(x) & cos(x).
//        Input angle is an unsigned int from 0-65535.
//        Output is signed int from -32767 to 32767.
//
//        This approximation never varies more than 0.69%
//        from the floating point value you'd get by doing
//          float s = sin( x ) * 32767.0;
//
//        Don't use this approximation for calculating the
//        trajectory of a rocket to Mars, but it's great
//        for art projects and LED displays.
//
//        On Arduino/AVR, this approximation is more than
//        10X faster than floating point sin(x) and cos(x)

#if defined(__AVR__)
#define sin16 sin16_avr
#else
#define sin16 sin16_C
#endif

LIB8STATIC int16_t sin16_avr( uint16_t theta )
{
    static const uint8_t data[] =
    { 0,         0,         49, 0, 6393%256,   6393/256, 48, 0,
      12539%256, 12539/256, 44, 0, 18204%256, 18204/256, 38, 0,
      23170%256, 23170/256, 31, 0, 27245%256, 27245/256, 23, 0,
      30273%256, 30273/256, 14, 0, 32137%256, 32137/256,  4 /*,0*/ };

    uint16_t offset = (theta & 0x3FFF);

    // AVR doesn't have a multi-bit shift instruction,
    // so if we say "offset >>= 3", gcc makes a tiny loop.
    // Inserting empty volatile statements between each
    // bit shift forces gcc to unroll the loop.
    offset >>= 1; // 0..8191
    asm volatile("");
    offset >>= 1; // 0..4095
    asm volatile("");
    offset >>= 1; // 0..2047

    if( theta & 0x4000 ) offset = 2047 - offset;

    uint8_t sectionX4;
    sectionX4 = offset / 256;
    sectionX4 *= 4;

    uint8_t m;

    union {
        uint16_t b;
        struct {
            uint8_t blo;
            uint8_t bhi;
        };
    } u;

    //in effect u.b = blo + (256 * bhi);
    u.blo = data[ sectionX4 ];
    u.bhi = data[ sectionX4 + 1];
    m     = data[ sectionX4 + 2];

    uint8_t secoffset8 = (uint8_t)(offset) / 2;

    uint16_t mx = m * secoffset8;

    int16_t  y  = mx + u.b;
    if( theta & 0x8000 ) y = -y;

    return y;
}

LIB8STATIC int16_t sin16_C( uint16_t theta )
{
    static const uint16_t base[] =
    { 0, 6393, 12539, 18204, 23170, 27245, 30273, 32137 };
    static const uint8_t slope[] =
    { 49, 48, 44, 38, 31, 23, 14, 4 };

    uint16_t offset = (theta & 0x3FFF) >> 3; // 0..2047
    if( theta & 0x4000 ) offset = 2047 - offset;

    uint8_t section = offset / 256; // 0..7
    uint16_t b   = base[section];
    uint8_t  m   = slope[section];

    uint8_t secoffset8 = (uint8_t)(offset) / 2;

    uint16_t mx = m * secoffset8;
    int16_t  y  = mx + b;

    if( theta & 0x8000 ) y = -y;

    return y;
}

LIB8STATIC int16_t cos16( uint16_t theta)
{
    return sin16( theta + 16384);
}

///////////////////////////////////////////////////////////////////////

// sin8 & cos8
//        Fast 8-bit approximations of sin(x) & cos(x).
//        Input angle is an unsigned int from 0-255.
//        Output is an unsigned int from 0 to 255.
//
//        This approximation can vary to to 2%
//        from the floating point value you'd get by doing
//          float s = (sin( x ) * 128.0) + 128;
//
//        Don't use this approximation for calculating the
//        "real" trigonometric calculations, but it's great
//        for art projects and LED displays.
//
//        On Arduino/AVR, this approximation is more than
//        20X faster than floating point sin(x) and cos(x)

#if defined(__AVR__) && !defined(LIB8_ATTINY)
#define sin8 sin8_avr
#else
#define sin8 sin8_C
#endif


const uint8_t b_m16_interleave[] = { 0, 49, 49, 41, 90, 27, 117, 10 };

LIB8STATIC uint8_t  sin8_avr( uint8_t theta)
{
    uint8_t offset = theta;

    asm volatile(
                 "sbrc %[theta],6         \n\t"
                 "com  %[offset]           \n\t"
                 : [theta] "+r" (theta), [offset] "+r" (offset)
                 );

    offset &= 0x3F; // 0..63

    uint8_t secoffset  = offset & 0x0F; // 0..15
    if( theta & 0x40) secoffset++;

    uint8_t m16; uint8_t b;

    uint8_t section = offset >> 4; // 0..3
    uint8_t s2 = section * 2;

    const uint8_t* p = b_m16_interleave;
    p += s2;
    b   = *p;
    p++;
    m16 = *p;

    uint8_t mx;
    uint8_t xr1;
    asm volatile(
                 "mul %[m16],%[secoffset]   \n\t"
                 "mov %[mx],r0              \n\t"
                 "mov %[xr1],r1             \n\t"
                 "eor  r1, r1               \n\t"
                 "swap %[mx]                \n\t"
                 "andi %[mx],0x0F           \n\t"
                 "swap %[xr1]               \n\t"
                 "andi %[xr1], 0xF0         \n\t"
                 "or   %[mx], %[xr1]        \n\t"
                 : [mx] "=r" (mx), [xr1] "=r" (xr1)
                 : [m16] "r" (m16), [secoffset] "r" (secoffset)
                 );

    int8_t y = mx + b;
    if( theta & 0x80 ) y = -y;

    y += 128;

    return y;
}


LIB8STATIC uint8_t sin8_C( uint8_t theta)
{
    uint8_t offset = theta;
    if( theta & 0x40 ) {
        offset = (uint8_t)255 - offset;
    }
    offset &= 0x3F; // 0..63

    uint8_t secoffset  = offset & 0x0F; // 0..15
    if( theta & 0x40) secoffset++;

    uint8_t section = offset >> 4; // 0..3
    uint8_t s2 = section * 2;
    const uint8_t* p = b_m16_interleave;
    p += s2;
    uint8_t b   =  *p;
    p++;
    uint8_t m16 =  *p;

    uint8_t mx = (m16 * secoffset) >> 4;

    int8_t y = mx + b;
    if( theta & 0x80 ) y = -y;

    y += 128;

    return y;
}


LIB8STATIC uint8_t cos8( uint8_t theta)
{
    return sin8( theta + 64);
}


///////////////////////////////////////////////////////////////////////
//
// memmove8, memcpy8, and memset8:
//   alternatives to memmove, memcpy, and memset that are
//   faster on AVR than standard avr-libc 1.8

#if defined(__AVR__)
extern "C" {
void * memmove8( void * dst, const void * src, uint16_t num );
void * memcpy8 ( void * dst, const void * src, uint16_t num )  __attribute__ ((noinline));
void * memset8 ( void * ptr, uint8_t value, uint16_t num ) __attribute__ ((noinline)) ;
}
#else
// on non-AVR platforms, these names just call standard libc.
#define memmove8 memmove
#define memcpy8 memcpy
#define memset8 memset
#endif


///////////////////////////////////////////////////////////////////////
//
// linear interpolation, such as could be used for Perlin noise, etc.
//

// linear interpolation between two unsigned 8-bit values,
// with 8-bit fraction
LIB8STATIC uint8_t lerp8by8( uint8_t a, uint8_t b, fract8 frac)
{
    uint8_t delta = b - a;
    uint8_t scaled = scale8( delta, frac);
    uint8_t result = a + scaled;
    return result;
}

// linear interpolation between two unsigned 16-bit values,
// with 16-bit fraction
LIB8STATIC uint16_t lerp16by16( uint16_t a, uint16_t b, fract16 frac)
{
    uint16_t delta = b - a;
    uint32_t prod = (uint32_t)delta * (uint32_t)frac;
    uint16_t scaled = prod >> 16;
    uint16_t result = a + scaled;
    return result;
}


// A note on the structure of lerp16by8 (and lerp15by8) :
// The cases for b>a and b<=a are handled separately for
// speed: without knowing the relative order of a and b,
// the value (a-b) might be a signed 17-bit value, which
// would have to be stored in a 32-bit signed int and
// processed as such.  To avoid that, we separate the
// two cases, and are able to do all the math with 16-bit
// unsigned values, which is much faster and smaller on AVR.

// linear interpolation between two unsigned 16-bit values,
// with 8-bit fraction
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

// linear interpolation between two signed 15-bit values,
// with 8-bit fraction
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

// linear interpolation between two signed 15-bit values,
// with 8-bit fraction
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

//  map8: map from one full-range 8-bit value into a narrower
//        range of 8-bit values, possibly a range of hues.
//
//        E.g. map myValue into a hue in the range blue..purple..pink..red
//        hue = map8( myValue, HUE_BLUE, HUE_RED);
//
//        Combines nicely with the waveform functions (like sin8, etc)
//        to produce continuous hue gradients back and forth:
//          hue = map8( sin8( myValue), HUE_BLUE, HUE_RED);
//
//        Mathematically simiar to lerp8by8, but arguments are more
//        like Arduino's "map"; this function is similar to
//          map( in, 0, 255, rangeStart, rangeEnd)
//        but faster and specifically designed for 8-bit values.
LIB8STATIC uint8_t map8( uint8_t in, uint8_t rangeStart, uint8_t rangeEnd)
{
    uint8_t rangeWidth = rangeEnd - rangeStart;
    uint8_t out = scale8( in, rangeWidth);
    out += rangeStart;
    return out;
}


///////////////////////////////////////////////////////////////////////
//
// easing functions; see http://easings.net
//

// ease8InOutQuad: 8-bit quadratic ease-in / ease-out function
//                Takes around 13 cycles on AVR
LIB8STATIC uint8_t ease8InOutQuad( uint8_t i)
{
    uint8_t j = i;
    if( j & 0x80 ) {
        j = 255 - j;
    }
    uint8_t jj  = scale8(  j, (j+1));
    uint8_t jj2 = jj << 1;
    if( i & 0x80 ) {
        jj2 = 255 - jj2;
    }
    return jj2;
}


// ease8InOutCubic: 8-bit cubic ease-in / ease-out function
//                 Takes around 18 cycles on AVR
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

// ease8InOutApprox: fast, rough 8-bit ease-in/ease-out function
//                   shaped approximately like 'ease8InOutCubic',
//                   it's never off by more than a couple of percent
//                   from the actual cubic S-curve, and it executes
//                   more than twice as fast.  Use when the cycles
//                   are more important than visual smoothness.
//                   Asm version takes around 7 cycles on AVR.

#if EASE8_C == 1
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
        : "r0", "r1"
        );
    return i;
}
#else
#error "No implementation for ease8 available."
#endif



// triwave8: triangle (sawtooth) wave generator.  Useful for
//           turning a one-byte ever-increasing value into a
//           one-byte value that oscillates up and down.
//
//           input         output
//           0..127        0..254 (positive slope)
//           128..255      254..0 (negative slope)
//
//           On AVR this function takes just three cycles.
//
LIB8STATIC uint8_t triwave8(uint8_t in)
{
    if( in & 0x80) {
        in = 255 - in;
    }
    uint8_t out = in << 1;
    return out;
}


// quadwave8 and cubicwave8: S-shaped wave generators (like 'sine').
//           Useful for turning a one-byte 'counter' value into a
//           one-byte oscillating value that moves smoothly up and down,
//           with an 'acceleration' and 'deceleration' curve.
//
//           These are even faster than 'sin8', and have
//           slightly different curve shapes.
//

// quadwave8: quadratic waveform generator.  Spends just a little more
//            time at the limits than 'sine' does.
LIB8STATIC uint8_t quadwave8(uint8_t in)
{
    return ease8InOutQuad( triwave8( in));
}

// cubicwave8: cubic waveform generator.  Spends visibly more time
//             at the limits than 'sine' does.
LIB8STATIC uint8_t cubicwave8(uint8_t in)
{
    return ease8InOutCubic( triwave8( in));
}



// sqrt16: square root for 16-bit integers
//         About three times faster and five times smaller
//         than Arduino's general sqrt on AVR.
LIB8STATIC uint8_t sqrt16(uint16_t x)
{
    if( x <= 1) {
        return x;
    }
    
    uint8_t low = 1; // lower bound
    uint8_t hi, mid;
    
    if( x > 7904) {
        hi = 255;
    } else {
        hi = (x >> 5) + 8; // initial estimate for upper bound
    }
    
    do {
        mid = (low + hi) >> 1;
        if ((uint16_t)(mid * mid) > x) {
            hi = mid - 1;
        } else {
            if( mid == 255) {
                return 255;
            }
            low = mid + 1;
        }
    } while (hi >= low);
    
    return low - 1;
}


template<class T, int F, int I> class q {
  T i:I;
  T f:F;
public:
  q(float fx) { i = fx; f = (fx-i) * (1<<F); }
  q(uint8_t _i, uint8_t _f) {i=_i; f=_f; }
  uint32_t operator*(uint32_t v) { return (v*i) + ((v*f)>>F); }
  uint16_t operator*(uint16_t v) { return (v*i) + ((v*f)>>F); }
  int32_t operator*(int32_t v) { return (v*i) + ((v*f)>>F); }
  int16_t operator*(int16_t v) { return (v*i) + ((v*f)>>F); }
#ifdef FASTLED_ARM
  int operator*(int v) { return (v*i) + ((v*f)>>F); }
#endif
};

template<class T, int F, int I> static uint32_t operator*(uint32_t v, q<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static uint16_t operator*(uint16_t v, q<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int32_t operator*(int32_t v, q<T,F,I> & q) { return q * v; }
template<class T, int F, int I> static int16_t operator*(int16_t v, q<T,F,I> & q) { return q * v; }
#ifdef FASTLED_ARM
template<class T, int F, int I> static int operator*(int v, q<T,F,I> & q) { return q * v; }
#endif

typedef q<uint8_t, 4,4> q44;
typedef q<uint8_t, 6,2> q62;
typedef q<uint16_t, 8,8> q88;
typedef q<uint16_t, 12,4> q124;



// Beat generators - These functions produce waves at a given
//                   number of 'beats per minute'.  Internally, they use
//                   the Arduino function 'millis' to track elapsed time.
//                   Accuracy is a bit better than one part in a thousand.
//
//       beat8( BPM ) returns an 8-bit value that cycles 'BPM' times
//                    per minute, rising from 0 to 255, resetting to zero,
//                    rising up again, etc..  The output of this function
//                    is suitable for feeding directly into sin8, and cos8,
//                    triwave8, quadwave8, and cubicwave8.
//       beat16( BPM ) returns a 16-bit value that cycles 'BPM' times
//                    per minute, rising from 0 to 65535, resetting to zero,
//                    rising up again, etc.  The output of this function is
//                    suitable for feeding directly into sin16 and cos16.
//
//       beatsin8( BPM, uint8_t low, uint8_t high) returns an 8-bit value that
//                    rises and falls in a sine wave, 'BPM' times per minute,
//                    between the values of 'low' and 'high'.
//       beatsin16( BPM, uint16_t low, uint16_t high) returns a 16-bit value
//                    that rises and falls in a sine wave, 'BPM' times per
//                    minute, between the values of 'low' and 'high'.
//
//  BPM can be supplied two ways.  The simpler way of specifying BPM is as
//  a simple 8-bit integer from 1-255, (e.g., "120").
//  The more sophisticated way of specifying BPM allows for fractional
//  "Q8.8" fixed point number (an 'accum88') with an 8-bit integer part and
//  an 8-bit fractional part.  The easiest way to construct this is to multiply
//  a floating point BPM value (e.g. 120.3) by 256, (e.g. resulting in 30796
//  in this case), and pass that as the 16-bit BPM argument.
//
//  Originally designed to make an entire animation project pulse with brightness.
//  For that effect, add this line just above your existing call to "FastLED.show()":
//
//     uint8_t bright = beatsin8( 60 /*BPM*/, 192 /*dimmest*/, 255 /*brightest*/ ));
//     FastLED.setBrightness( bright );
//     FastLED.show();
//
//  The entire animation will now pulse between brightness 192 and 255 once per second.


// The beat generators need access to a millisecond counter.
// On Arduino, this is "millis()".  On other platforms, you'll
// need to provide a function with this signature:
//   uint32_t get_millisecond_timer();
// that provides similar functionality.
// You can also force use of the get_millisecond_timer function
// by #defining USE_GET_MILLISECOND_TIMER.
#if defined(ARDUINO) && !defined(USE_GET_MILLISECOND_TIMER)
// Forward declaration of Arduino function 'millis'.
uint32_t millis();
#define GET_MILLIS (millis())
#else
uint32_t get_millisecond_timer();
#define GET_MILLIS (get_millisecond_timer())
#endif

// beat16 generates a 16-bit 'sawtooth' wave at a given BPM
LIB8STATIC uint16_t beat16( accum88 beats_per_minute)
{
    // Convert simple 8-bit BPM's to full Q8.8 accum88's if needed
    if( beats_per_minute < 256) beats_per_minute <<= 8;
    
    // BPM is 'beats per minute', or 'beats per 60000ms'.
    // To avoid using the (slower) division operator, we
    // want to convert 'beats per 60000ms' to 'beats per 65536ms',
    // and then use a simple, fast bit-shift to divide by 65536.
    //
    // The ratio 65536:60000 is 279.620266667:256; we'll call it 280:256.
    // The conversion is accurate to about 0.05%, more or less,
    // e.g. if you ask for "120 BPM", you'll get about "119.93".
    // If you need more precision than that, you can specify a
    // sixteen-bit BPM value in Q8.8 fixed-point (an 'accum88').
    return ((GET_MILLIS) * beats_per_minute * 280) >> 16;
}

// beat8 generates an 8-bit 'sawtooth' wave at a given BPM
LIB8STATIC uint8_t beat8( accum88 beats_per_minute)
{
    return beat16( beats_per_minute) >> 8;
}

// beatsin16 generates a 16-bit sine wave at a given BPM,
//           that oscillates within a given range.
LIB8STATIC uint16_t beatsin16( accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535)
{
    uint16_t beat = beat16( beats_per_minute);
    uint16_t beatsin = (sin16( beat) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

// beatsin8 generates an 8-bit sine wave at a given BPM,
//           that oscillates within a given range.
LIB8STATIC uint8_t beatsin8( accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255)
{
    uint8_t beat = beat8( beats_per_minute);
    uint8_t beatsin = sin8( beat);
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8( beatsin, rangewidth);
    uint8_t result = lowest + scaledbeat;
    return result;
}


#endif
