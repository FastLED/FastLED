#pragma once

#define PROGMEM
#define FL_PROGMEM

#define FL_PGM_READ_BYTE_NEAR(x) (*((const uint8_t *)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const uint16_t *)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const uint32_t *)(x)))
#define FL_ALIGN_PROGMEM