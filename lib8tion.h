#ifndef __INC_LIB8TION_H
#define __INC_LIB8TION_H


// Assembly language implementations of useful functions,
// with generic C implementaions as fallbacks as well.

// Assembly implementations of some functions for:
//   AVR (ATmega/ATtiny)
//   ARM (generic)
//   ARM (Cortex M4 with DSP)


// TODO: merge asm blocks to prevent splitting/reordering


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
#define SUB8_C 1


#elif defined(__AVR__)

// AVR ATmega and friends Arduino

#define QADD8_C 0
#define QADD7_C 0
#define QSUB8_C 0
#define ABS8_C 0
#define SUB8_C 0

#define QADD8_AVRASM 1
#define QADD7_AVRASM 1
#define QSUB8_AVRASM 1
#define ABS8_AVRASM 1
#define SUB8_AVRASM 1

// Note: these require hardware MUL instruction
//       -- sorry, ATtiny!
#if !defined(LIB8_ATTINY)
#define SCALE8_C 0
#define MUL8_C 0
#define SCALE8_AVRASM 1
#define MUL8_AVRASM 1
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
    // First, add j to i, conditioning the C flag
    asm volatile( "add %0, %1" : "+a" (i) : "a" (j));
    
    // Now test the C flag.
    // If C is clear, we branch around a load of 0xFF into i.
    // If C is set, we go ahead and load 0xFF into i.
    asm volatile( "brcc L_%=\n\t"
                 "ldi %0, 0xFF\n\t"
                 "L_%=: " : "+a" (i) : "a" (i) );
    return i;
#elif QADD8_ARM_DSP_ASM == 1
    asm volatile( "uqadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
#else
#error "No implementation for qadd8 available."
#endif
}


// qadd7: add one nonnegative signed uint8_t to another,
//        saturating at 0x7F.
LIB8STATIC uint8_t qadd7( uint8_t i, uint8_t j)
{
#if QADD7_C == 1
    int t = i + j;
    if( t > 127) t = 127;
    return t;
#elif QADD7_AVRASM == 1
    // First, add j to i, conditioning the C flag
    asm volatile( "add %0, %1" : "+a" (i) : "a" (j));
    
    // Now test the V flag.
    // If V is clear, we branch around a load of 0x7F into i.
    // If V is set, we go ahead and load 0x7F into i.
    asm volatile( "brvc L_%=\n\t"
                 "ldi %0, 0x7F\n\t"
                 "L_%=: " : "+a" (i) : "a" (i) );
    return i;
#elif QADD7_ARM_DSP_ASM == 1
    asm volatile( "qadd8 %0, %0, %1" : "+r" (i) : "r" (j));
    return i;
#else
#error "No implementation for qadd7 available."
#endif
}

// qsub8: subtract one byte to another, saturating at 0x00
LIB8STATIC uint8_t qsub8( uint8_t i, uint8_t j)
{
#if QSUB8_C == 1
    int t = i - j;
    if( t < 0) t = 0;
    return t;
#elif QSUB8_AVRASM == 1
    // First, subtract j from i, conditioning the C flag
    asm volatile( "sub %0, %1" : "+a" (i) : "a" (j));
    
    // Now test the C flag.
    // If C is clear, we branch around a load of 0x00 into i.
    // If C is set, we go ahead and load 0x00 into i.
    asm volatile( "brcc L_%=\n\t"
                 "ldi %0, 0x00\n\t"
                 "L_%=: " : "+a" (i) : "a" (i) );
    return i;
#else
#error "No implementation for qsub8 available."
#endif
}

// sub8: subtract one byte from another, keeping
//       everything within 8 bits
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
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes i * (scale / 256)
LIB8STATIC uint8_t scale8( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    return ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (i) : "a" (scale));
    // Extract the high 8-bits (r1)
    asm volatile( "mov %0, r1\n\t" : "+a" (i) : "a" (i) : "r0", "r1");
    
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
    // Return the result
    return i;
#else
#error "No implementation for scale8 available."
#endif
}

// scale8: scale one byte by a second one, which is treated as
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes i * (scale / 256)

LIB8STATIC uint8_t scale8_LEAVING_R1_DIRTY( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    return ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (i) : "a" (scale));
    // Extract the high 8-bits (r1)
    asm volatile( "mov %0, r1\n\t" : "+a" (i) : "a" (i) : "r0", "r1");
    
    // Return the result
    return i;
#else
#error "No implementation for scale8_LEAVING_R1_DIRTY available."
#endif
}

LIB8STATIC void nscale8_LEAVING_R1_DIRTY( uint8_t& i, uint8_t scale)
{
#if SCALE8_C == 1
    i = ((int)i * (int)(scale) ) >> 8;
#elif SCALE8_AVRASM == 1
    // Multiply 8-bit i * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (i) : "a" (scale));
    // Extract the high 8-bits (r1)
    asm volatile( "mov %0, r1\n\t" : "+a" (i) : "a" (i) : "r0", "r1");
#else
#error "No implementation for nscale8_LEAVING_R1_DIRTY available."
#endif
}

LIB8STATIC void cleanup_R1()
{
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
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
    // Multiply 8-bit r * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (r) : "a" (scale));
    // Extract the high 8-bits (r1)
    asm volatile( "mov %0, r1\n\t" : "+a" (r) : "a" (r) : "r0", "r1");
    
    // Multiply 8-bit g * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (g) : "a" (scale));
    // Extract the high 8-bits (r1), store in g
    asm volatile( "mov %0, r1\n\t" : "+a" (g) : "a" (g) : "r0", "r1");
    
    // Multiply 8-bit b * 8-bit scale, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (b) : "a" (scale));
    // Extract the high 8-bits (r1), store in b
    asm volatile( "mov %0, r1\n\t" : "+a" (b) : "a" (b) : "r0", "r1");
    
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
    // Return the result
#else
#error "No implementation for nscale8x3 available."
#endif
}

// nscale8x3: scale three one byte values by a fourth one, which is treated as
//         the numerator of a fraction whose demominator is 256
//         In other words, it computes r,g,b * (scale / 256)
//
//         THIS FUNCTION ALWAYS MODIFIES ITS ARGUMENTS IN PLACE

LIB8STATIC void nscale8x3_video( uint8_t& r, uint8_t& g, uint8_t& b, uint8_t scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scale) ) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scale) ) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scale) ) >> 8) + nonzeroscale;
#elif SCALE8_AVRASM == 1
    
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    
    asm volatile("     tst %0          \n"
                 "     breq L_%=       \n"
                 "     mul %0, %1      \n"
                 "     mov %0, r1      \n"
                 "     add %0, %2      \n"
                 "L_%=:                \n"
                 
                 : "+a" (r)
                 : "a" (scale), "a" (nonzeroscale)
                 : "r0", "r1");
    
    asm volatile("     tst %0          \n"
                 "     breq L_%=       \n"
                 "     mul %0, %1      \n"
                 "     mov %0, r1      \n"
                 "     add %0, %2      \n"
                 "L_%=:                \n"
                 
                 : "+a" (g)
                 : "a" (scale), "a" (nonzeroscale)
                 : "r0", "r1");
    
    asm volatile("     tst %0          \n"
                 "     breq L_%=       \n"
                 "     mul %0, %1      \n"
                 "     mov %0, r1      \n"
                 "     add %0, %2      \n"
                 "L_%=:                \n"
                 
                 : "+a" (b)
                 : "a" (scale), "a" (nonzeroscale)
                 : "r0", "r1");
    
    
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
    // Return the result
#else
#error "No implementation for nscale8x3 available."
#endif
}

LIB8STATIC uint8_t scale8_video( uint8_t i, uint8_t scale)
{
#if SCALE8_C == 1
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    uint8_t j = (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + nonzeroscale;
    return j;
#elif SCALE8_AVRASM == 1
    
    uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
    asm volatile("      tst %0          \n"
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


// mul8: 8x8 bit multiplication, with 8 bit result
LIB8STATIC uint8_t mul8( uint8_t i, uint8_t j)
{
#if MUL8_C == 1
    return ((int)i * (int)(j) ) & 0xFF;
#elif MUL8_AVRASM == 1
    // Multiply 8-bit i * 8-bit j, giving 16-bit r1,r0
    asm volatile( "mul %0, %1"     : "+a" (i) : "a" (j));
    // Extract the high 8-bits (r1)
    asm volatile( "mov %0, r0\n\t" : "+a" (i) : "a" (i) : "r0", "r1");
    
    // Restore r1 to "0"; it's expected to always be that
    asm volatile( "eor r1, r1\n\t" : : : "r1" );
    // Return the result
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
                 // First, check the high bit, and prepare to skip if it's clear
                 "sbrc %0, 7 \n"
                 
                 // Negate the value
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



#endif
