// ok no namespace fl
#pragma once

/// @file progmem_stm32_particle.h
/// PROGMEM definitions for Particle (Photon/Electron) devices
///
/// Particle core does not define pgm_read_dword, so we provide it here.
/// STM32 has flat memory model - PROGMEM is effectively a no-op.

#include "fl/stl/stdint.h"

// PROGMEM - no-op on STM32 (flat memory model)
#define PROGMEM
#define FL_PROGMEM

// Particle core does not define pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_dword_near(addr) pgm_read_dword(addr)

// FL_* PROGMEM read macros - direct memory access (flat memory model)
#define FL_PGM_READ_BYTE_NEAR(x) (*((const fl::u8 *)(x)))
#define FL_PGM_READ_WORD_NEAR(x) (*((const fl::u16 *)(x)))
#define FL_PGM_READ_DWORD_NEAR(x) (*((const fl::u32 *)(x)))
#define FL_ALIGN_PROGMEM

// Aligned 4-byte PROGMEM read. STM32 has flat memory — direct dereference.
#define FL_PGM_READ_DWORD_ALIGNED(addr) (*((const fl::u32*)(addr)))
