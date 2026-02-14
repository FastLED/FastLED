// ok no namespace fl
#pragma once

// IWYU pragma: private

#define PROGMEM
#define FL_PROGMEM

#define FL_PGM_READ_BYTE_NEAR(x) (*((const fl::u8 *)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const fl::u16 *)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const fl::u32 *)(x)))

// Aligned 4-byte PROGMEM read. On WASM, data lives in normal memory.
#define FL_PGM_READ_DWORD_ALIGNED(addr) (*((const fl::u32*)(addr)))

// On WASM (platforms without PROGMEM), alignment can help with cache performance
#define FL_ALIGN_PROGMEM(N) __attribute__((aligned(N)))