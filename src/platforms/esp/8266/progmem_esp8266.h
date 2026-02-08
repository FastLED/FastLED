

// ok no namespace fl
#pragma once

#include <pgmspace.h>
#include "fl/int.h"

#define FL_PROGMEM    PROGMEM

#define FL_PGM_READ_BYTE_NEAR(addr)   pgm_read_byte(addr)
#define FL_PGM_READ_WORD_NEAR(addr)   pgm_read_word(addr)
#define FL_PGM_READ_DWORD_NEAR(addr)  pgm_read_dword(addr)

/// Force 4-byte alignment (for safe multibyte reads)
#define FL_ALIGN_PROGMEM  __attribute__((aligned(4)))

// Aligned 4-byte PROGMEM read using platform pgm_read_dword.
#define FL_PGM_READ_DWORD_ALIGNED(addr) ((fl::u32)pgm_read_dword(addr))
