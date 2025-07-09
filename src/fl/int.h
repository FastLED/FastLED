#pragma once


/// IMPORTANT!
/// This file MUST not

#include "fl/stdint.h"  // For uintptr_t and size_t

// Platform-specific integer type definitions
// This includes platform-specific 16/32/64-bit types
#include "platforms/int.h"

namespace fl {
    // 8-bit types - char is reliably 8 bits on all supported platforms
    // These must be defined BEFORE platform includes so fractional types can use them
    typedef signed char i8;
    typedef unsigned char u8;
    typedef unsigned int uint;
    
    // Pointer and size types are defined per-platform in platforms/int.h
    // uptr (pointer type) and size (size type) are defined per-platform

}



namespace fl {
    ///////////////////////////////////////////////////////////////////////
    ///
    /// Fixed-Point Fractional Types. 
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

    /// ANSI: unsigned short _Fract. 
    /// Range is 0 to 0.99609375 in steps of 0.00390625.  
    /// Should be interpreted as unsigned 256ths.
    typedef u8   fract8;

    /// ANSI: signed short _Fract. 
    /// Range is -0.9921875 to 0.9921875 in steps of 0.0078125.  
    /// Should be interpreted as signed 128ths.
    typedef i8    sfract7;

    /// ANSI: unsigned _Fract.
    /// Range is 0 to 0.99998474121 in steps of 0.00001525878.  
    /// Should be interpreted as unsigned 65536ths.
    typedef u16  fract16;

    typedef i32   sfract31; ///< ANSI: signed long _Fract. 31 bits int, 1 bit fraction

    typedef u32  fract32;   ///< ANSI: unsigned long _Fract. 32 bits int, 32 bits fraction

    /// ANSI: signed _Fract.
    /// Range is -0.99996948242 to 0.99996948242 in steps of 0.00003051757.  
    /// Should be interpreted as signed 32768ths.
    typedef i16   sfract15;

    typedef u16  accum88;    ///< ANSI: unsigned short _Accum. 8 bits int, 8 bits fraction
    typedef i16   saccum78;   ///< ANSI: signed   short _Accum. 7 bits int, 8 bits fraction
    typedef u32  accum1616;  ///< ANSI: signed         _Accum. 16 bits int, 16 bits fraction
    typedef i32   saccum1516; ///< ANSI: signed         _Accum. 15 bits int, 16 bits fraction
    typedef u16  accum124;   ///< no direct ANSI counterpart. 12 bits int, 4 bits fraction
    typedef i32   saccum114;  ///< no direct ANSI counterpart. 1 bit int, 14 bits fraction
}

namespace fl {    
    // Size assertions moved to src/platforms/compile_test.cpp.hpp
}

// Make fractional types available in global namespace
using fl::fract8;
using fl::sfract7;
using fl::fract16;
using fl::sfract31;
using fl::fract32;
using fl::sfract15;
using fl::accum88;
using fl::saccum78;
using fl::accum1616;
using fl::saccum1516;
using fl::accum124;
using fl::saccum114;
