/// @file fastspi_types.h
/// Data types and constants used by SPI interfaces

#ifndef __INC_FASTSPI_TYPES_H
#define __INC_FASTSPI_TYPES_H

#include "FastLED.h"
#include "force_inline.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

/// @name Byte Re-Order Macros
/// Some helper macros for getting at mis-ordered byte values.
/// @todo Unused. Remove?
///
/// @{

/// Get SPI byte 0 offset
#define SPI_B0 (RGB_BYTE0(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
/// Get SPI byte 1 offset
#define SPI_B1 (RGB_BYTE1(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
/// Get SPI byte 2 offset
#define SPI_B2 (RGB_BYTE2(RGB_ORDER) + (MASK_SKIP_BITS & SKIP))
/// Advance SPI data pointer
#define SPI_ADVANCE (3 + (MASK_SKIP_BITS & SKIP))
/// @}

/// Dummy class for output controllers that need no data transformations. 
/// Some of the SPI controllers will need to perform a transform on each byte before doing
/// anything with it.  Creating a class of this form and passing it in as a template parameter to
/// writeBytes()/writeBytes3() will ensure that the body of this method will get called on every
/// byte worked on. 
/// @note Recommendation: make the adjust method aggressively inlined.
/// @todo Convinience macro for building these
class DATA_NOP {
public:
    /// Hook called to adjust a byte of data before writing it to the output. 
    /// In this dummy version, no adjustment is made.
    static FASTLED_FORCE_INLINE uint8_t adjust(FASTLED_REGISTER uint8_t data) { return data; }

    /// @copybrief adjust(FASTLED_REGISTER uint8_t)
    /// @param data input byte
    /// @param scale scale value
    /// @returns input byte rescaled using ::scale8(uint8_t, uint8_t)
    static FASTLED_FORCE_INLINE uint8_t adjust(FASTLED_REGISTER uint8_t data, FASTLED_REGISTER uint8_t scale) { return scale8(data, scale); }

    /// Hook called after a block of data is written to the output. 
    /// In this dummy version, no action is performed.
    static FASTLED_FORCE_INLINE void postBlock(int /* len */, void* context = NULL) { }
};

/// Flag for the start of an SPI transaction
#define FLAG_START_BIT 0x80

/// Bitmask for the lower 6 bits of a byte
/// @todo Unused. Remove?
#define MASK_SKIP_BITS 0x3F

/// @name Clock speed dividers
/// @{

/// Divisor for clock speed by 2
#define SPEED_DIV_2 2
/// Divisor for clock speed by 4
#define SPEED_DIV_4 4
/// Divisor for clock speed by 8
#define SPEED_DIV_8 8
/// Divisor for clock speed by 16
#define SPEED_DIV_16 16
/// Divisor for clock speed by 32
#define SPEED_DIV_32 32
/// Divisor for clock speed by 64
#define SPEED_DIV_64 64
/// Divisor for clock speed by 128
#define SPEED_DIV_128 128
/// @}

/// Max SPI data rate
/// @todo Unused. Remove?
#define MAX_DATA_RATE 0

FASTLED_NAMESPACE_END

#endif
