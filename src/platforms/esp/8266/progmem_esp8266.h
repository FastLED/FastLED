

// ok no namespace fl
#pragma once

// IWYU pragma: private

// IWYU pragma: begin_keep
#include <pgmspace.h>  // ok platform headers
// IWYU pragma: end_keep
#include "fl/stl/int.h"  // ok platform headers

#define FL_PROGMEM    PROGMEM

#define FL_PGM_READ_BYTE_NEAR(addr)   pgm_read_byte(addr)
#define FL_PGM_READ_WORD_NEAR(addr)   pgm_read_word(addr)
#define FL_PGM_READ_DWORD_NEAR(addr)  pgm_read_dword(addr)

/// Force 4-byte alignment for safe multibyte PROGMEM reads.
/// ESP8266 does not support memalign(), so alignment > default triggers
/// a linker error (undefined reference to memalign) when the compiler
/// emits aligned operator new. Cap at 4 bytes for safety.
#define FL_ALIGN_PROGMEM(N)  __attribute__((aligned(4)))

// Aligned PROGMEM reads using platform pgm_read_*.
#define FL_PGM_READ_WORD_ALIGNED(addr) ((fl::u16)pgm_read_word(addr))
#define FL_PGM_READ_DWORD_ALIGNED(addr) ((fl::u32)pgm_read_dword(addr))
