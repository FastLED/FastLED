#pragma once

// Guard against PROGMEM redefinition on platforms that have their own definition
#if !defined(PROGMEM) && !defined(__IMXRT1062__) && !defined(__MK20DX128__) && !defined(__MK20DX256__) && !defined(__MK66FX1M0__) && !defined(__MK64FX512__) && !defined(__MKL26Z64__)
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
