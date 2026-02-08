// ok no namespace fl
#pragma once

#define PROGMEM
#define FL_PROGMEM

#define FL_PGM_READ_BYTE_NEAR(x) (*((const fl::u8 *)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const fl::u16 *)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const fl::u32 *)(x)))
#define FL_ALIGN_PROGMEM