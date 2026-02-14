

// ok no namespace fl
#pragma once

#include <pgmspace.h>  // ok platform headers
#include "fl/int.h"  // ok platform headers

#define FL_PROGMEM    PROGMEM

#define FL_PGM_READ_BYTE_NEAR(addr)   pgm_read_byte(addr)
#define FL_PGM_READ_WORD_NEAR(addr)   pgm_read_word(addr)
#define FL_PGM_READ_DWORD_NEAR(addr)  pgm_read_dword(addr)

/// Force alignment (for safe multibyte reads and cache-line optimization)
/// ESP8266 supports variable alignment - larger alignment can improve cache performance
#define FL_ALIGN_PROGMEM(N)  __attribute__((aligned(N)))

// Aligned 4-byte PROGMEM read using platform pgm_read_dword.
#define FL_PGM_READ_DWORD_ALIGNED(addr) ((fl::u32)pgm_read_dword(addr))
