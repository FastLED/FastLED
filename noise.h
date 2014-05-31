#ifndef __INC_NOISE_H
#define __INC_NOISE_H

// 16 bit, fixed point implementation of perlin's Simplex Noise.  Coordinates are
// 16.16 fixed point values, 32 bit integers with integral coordinates in the high 16
// bits and fractional in the low 16 bits, and the function takes 1d, 2d, and 3d coordinate
// values.
extern uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
extern uint16_t inoise16(uint32_t x, uint32_t y);
extern uint16_t inoise16(uint32_t x);

// 8 bit, fixed point implementation of perlin's Simplex Noise.  Coordinates are
// 8.8 fixed point values, 16 bit integers with integral coordinates in the high 8
// bits and fractional in the low 8 bits, and the function takes 1d, 2d, and 3d coordinate
// values.
extern uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);
extern uint8_t inoise8(uint16_t x, uint16_t y);
extern uint8_t inoise8(uint16_t x);

#endif
