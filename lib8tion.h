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
 
 
 - Scaling (down) of unsigned 8-bit values.  
   Scaledown value is specified in 1/256ths.
     scale8( i, sc) == (i * sc) / 256
 
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
 
 
Lib8tion is pronounced like 'libation': lie-BAY-shun

*/
 
 

#include <stdint.h>

#define LIB8STATIC __attribute__ ((unused)) static


#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
#define LIB8_ATTINY 1
#endif


#if defined(__arm__)

#if defined(__MK20DX128__)
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
#define ABS8_C 1
#define MUL8_C 1
#define ADD8_C 1
#define SUB8_C 1


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
#define MUL8_C 0
#define SCALE8_AVRASM 1
#define MUL8_AVRASM 1
#define CLEANUP_R1_AVRASM 1
#else
// On ATtiny, we just use C implementations
#define SCALE8_C 1
#define MUL8_C 1
#define SCALE8_AVRASM 0
#define MUL8_AVRASM 0
#endif

#else

// unspecified architecture, so
// no ASM, everything in C
#define QADD8_C 1
#define QADD7_C 1
#define QSUB8_C 1
#define SCALE8_C 1
#define ABS8_C 1
#define MUL8_C 1
#define ADD8_C 1
#define SUB8_C 1

#endif

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
LIB8STATIC uint8_t scale8( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    return ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    asm volatile(
         /* Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0 */
         "mul %0, %1    \n\t"
         /* Move the high 8-bits of the product (r1) back to i */
         "mov %0, r1    \n\t"
         /* Restore r1 to "0"; it's expected to always be that */
         "eor r1, r1    \n\t"
         
         : "+a" (i)      /* writes to i */
         : "a"  (scale)  /* uses scale */
         : "r0", "r1"    /* clobbers r0, r1 */ );

    /* Return the result */
    return i;
#else
#error "No implementation for scale8 available."
#endif
}


//  The "video" version of scale8 guarantees that the output will
//  be only be zero if one or both of the inputs are zero.  If both
//  inputs are non-zero, the output is guaranteed to be non-zero.
//  This makes for better 'video'/LED dimming, at the cost of
//  several additional cycles.
LIB8STATIC uint8_t scale8_video( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    uint8_t j = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    return j;
#elif SCALE8_AVRASM == 1
    
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    asm volatile(
         "      tst %0          \n"
         "      breq L_%=       \n"
         "      mul %0, %1      \n"
         "      mov %0, r1      \n"
         "      add %0, %2      \n"
         "L_%=: eor r1, r1      \n"
         
         : "+a" (i)
         : "a" (scale), "a" (nonzeroscale)
         : "r0", "r1");
    
    // Return the result
    return i;
#else
#error "No implementation for scale8_video available."
#endif
}


// This version of scale8 does not clean up the R1 register on AVR
// If you are doing several 'scale8's in a row, use this, and
// then explicitly call cleanup_R1.
LIB8STATIC uint8_t scale8_LEAVING_R1_DIRTY( uint8_t i, uint8_t scale)
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
         /* "eor r1, r1    \n\t" */
         
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

LIB8STATIC void nscale8_LEAVING_R1_DIRTY( uint8_t& i, uint8_t scale)
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
         /* "eor r1, r1    \n\t" */
         
         : "+a" (i)      /* writes to i */
         : "a"  (scale)  /* uses scale */
         : "r0", "r1"    /* clobbers r0, r1 */ );
#else
#error "No implementation for nscale8_LEAVING_R1_DIRTY available."
#endif
}



LIB8STATIC uint8_t scale8_video_LEAVING_R1_DIRTY( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    uint8_t j = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    return j;
#elif SCALE8_AVRASM == 1
    
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    asm volatile(
         "      tst %0          \n"
         "      breq L_%=       \n"
         "      mul %0, %1      \n"
         "      mov %0, r1      \n"
         "      add %0, %2      \n"
         /* R1 IS LEFT DIRTY, YOU MUST ZERO IT OUT YOURSELF */
         "L_%=:                 \n"
         
         : "+a" (i)
         : "a" (scale), "a" (nonzeroscale)
         : "r0", "r1");
    
    // Return the result
    return i;
#else
#error "No implementation for scale8_video available."
#endif
}



LIB8STATIC void cleanup_R1()
{
#if CLEANUP_R1_AVRASM == 1
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
#endif
}


// nscale8x3: scale three one byte values by a fourth one, which is treated as
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes r,g,b * (scale / 256)
//
//         THIS FUNCTION ALWAYS MODIFIES ITS ARGUMENTS IN PLACE

LIB8STATIC void nscale8x3( uint8_t& r, uint8_t& g, uint8_t& b, uint8_t scale)
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


LIB8STATIC void nscale8x3_video( uint8_t& r, uint8_t& g, uint8_t& b, uint8_t scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scale) ) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scale) ) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scale) ) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    r = scale8_video_LEAVING_R1_DIRTY( r, scale);
    g = scale8_video_LEAVING_R1_DIRTY( g, scale);
    b = scale8_video_LEAVING_R1_DIRTY( b, scale);
    cleanup_R1();
#else
#error "No implementation for nscale8x3 available."
#endif
}


// mul8: 8x8 bit multiplication, with 8 bit result
LIB8STATIC uint8_t mul8( uint8_t i, uint8_t j)
{
#if MUL8_C == 1
    return ((int)i * (int)(j) ) & 0xFF;
#elif MUL8_AVRASM == 1
    asm volatile(
         /* Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0 */
         "mul %0, %1    \n\t"
         /* Extract the LOW 8-bits (r0) */
         "mov %0, r0    \n\t"
         /* Restore r1 to "0"; it's expected to always be that */
         "eor r1, r1    \n\t"
         : "+a" (i)
         : "a"  (j)
         : "r0", "r1");
    
    return i;
#else
#error "No implementation for mul8 available."
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

LIB8STATIC void random16_add_entropy( uint16_t entropy)
{
    rand16seed += entropy;
}


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

LIB8STATIC int16_t sin16( uint16_t theta )
{
    static const uint16_t base[] =
        { 0, 6393, 12539, 18204, 23170, 27245, 30273, 32137 };
    static const uint8_t slope[] =
        { 49, 48, 44, 38, 31, 23, 14, 4 };
    
    uint16_t offset = (theta & 0x3FFF) >> 4;
    if( theta & 0x4000 ) offset = 1023 - offset;
    
    uint8_t section = offset / 128;
    uint16_t b   = base[section];
    uint8_t  m   = slope[section];
    
    uint8_t secoffset8 = offset & 0x7F;
    uint16_t mx = m * secoffset8;
    int16_t  y  = mx + b;
    
    if( theta & 0x8000 ) y = -y;
    
    return y;
}

LIB8STATIC int16_t cos16( uint16_t theta)
{
    return sin16( theta + 16384);
}

#endif
