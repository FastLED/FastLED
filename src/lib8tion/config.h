#pragma once



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
