#pragma once

#if !defined(PROGMEM)
#define PROGMEM
#endif

#if !defined(FL_PROGMEM)
#define FL_PROGMEM PROGMEM
#endif

#define FL_PGM_READ_BYTE_NEAR(x) (*((const uint8_t *)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const uint16_t *)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const uint32_t *)(x)))
#define FL_ALIGN_PROGMEM

#define FL_PROGMEM_USES_NULL 1