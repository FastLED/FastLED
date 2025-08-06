

#pragma once

#include <pgmspace.h>

#define FL_PROGMEM    PROGMEM

#define FL_PGM_READ_BYTE_NEAR(addr)   pgm_read_byte(addr)
#define FL_PGM_READ_WORD_NEAR(addr)   pgm_read_word(addr)
#define FL_PGM_READ_DWORD_NEAR(addr)  pgm_read_dword(addr)

/// Force 4-byte alignment (for safe multibyte reads)
#define FL_ALIGN_PROGMEM  __attribute__((aligned(4)))
