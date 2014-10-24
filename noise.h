#ifndef __INC_NOISE_H
#define __INC_NOISE_H

#if 0
/// Class for accessing 16 bit noise.  Provides methods for setting origin, scale,
/// frequency, alplitude, time, octave blurring
class CFastNoise16 {
public:
  CFastNoise16 &setOrigin(uint32_t x, uint32_t y, uint32_t z);
  CFastNoise16 &setOrigin(uint32_t x, uint32_t y);
  CFastNoise16 &setOrigin(uint32_t x);

  uint32_t getX();
  uint32_t getY();
  uint32_t getZ();
  uint32_t getTime();

  uint32_t getOrigin(uint32_t & x, uint32_t & y, uint32_t & z);
  uint32_t getOrigin(uint32_t & x, uint32_t & y);
  uint32_t getOrigin(uint32_t & x);

  CFastNoise16 &advance(int32_t x, int32_t y, int32_t z);
  CFastNoise16 &advance(int32_t x, int32_t y;
  CFastNoise16 &advance(int32_t x);

  CFastNoise16 &advanceTime(int32_t t);

  CFastNoise16 &setScale(int32_t x_scale, int32_t y_scale, int32_t z_scale);
  CFastNoise16 &setScale(int32_t x_scale, int32_t y_scale);
  CFastNoise16 &setScale(int32_t x_scale);

  int32_t getScaleX();
  int32_t getScaleY();
  int32_t getScaleZ();
  void getScale(int32_t & x, int32_t & y, int32_t & z);
  void getScale(int32_t & x, int32_t & y);
  void getScale(int32_t & x);

  CFastNoise16 &setAmplitude(fract16 amplitude);

  CFastNoise16 &setFrequency(q88 frequency);
  CFastNoise16 &setTime(uint32_t time);

  CFastNoise16 &setOctaves(int octaves);

  CFastNoise16 &setOctaveBlur(bool blurOctaves);

  void getNoise(uint32_t x, uint32_t y, uint32_t z);
  void getNoise(uint32_t x, uint32_t y);
  void getNoise(uint32_t x);

  void fillNoise(uint16_t *pData, int size);
  void fillNoise(uint16_t *pData, int width, int height);
  void fillNoise(uint16_t *pData, int width, int height, int depth);

  void fillNoise(uint8_t *pData, int size);
  void fillNoise(uint8_t *pData, int width, int height);
  void fillNoise(uint8_t *pData, int width, int height, int depth);
};
#endif

// 16 bit, fixed point implementation of perlin's Simplex Noise.  Coordinates are
// 16.16 fixed point values, 32 bit integers with integral coordinates in the high 16
// bits and fractional in the low 16 bits, and the function takes 1d, 2d, and 3d coordinate
// values.  These functions are scaled to return 0-65535
extern uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
extern uint16_t inoise16(uint32_t x, uint32_t y);
extern uint16_t inoise16(uint32_t x);

// 16 bit raw versions of the noise functions.  These values are not scaled/altered and have
// output values roughly in the range (-18k,18k)
extern int16_t inoise16_raw(uint32_t x, uint32_t y, uint32_t z);
extern int16_t inoise16_raw(uint32_t x, uint32_t y);
extern int16_t inoise16_raw(uint32_t x);

// 8 bit, fixed point implementation of perlin's Simplex Noise.  Coordinates are
// 8.8 fixed point values, 16 bit integers with integral coordinates in the high 8
// bits and fractional in the low 8 bits, and the function takes 1d, 2d, and 3d coordinate
// values.  These functions are scaled to return 0-255
extern uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);
extern uint8_t inoise8(uint16_t x, uint16_t y);
extern uint8_t inoise8(uint16_t x);

// 8 bit raw versions of the noise functions.  These values are not scaled/altered and have
// output values roughly in the range (-70,70)
extern int8_t inoise8_raw(uint16_t x, uint16_t y, uint16_t z);
extern int8_t inoise8_raw(uint16_t x, uint16_t y);
extern int8_t inoise8_raw(uint16_t x);

// Raw noise fill functions - fill into a 1d or 2d array of 8-bit values using either 8-bit noise or 16-bit noise
// functions.
void fill_raw_noise8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint16_t x, int scale, uint16_t time);
void fill_raw_noise16into8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint32_t x, int scale, uint32_t time);
void fill_raw_2dnoise8(uint8_t *pData, int width, int height, uint8_t octaves, uint16_t x, int scalex, uint16_t y, int scaley, uint16_t time);
void fill_raw_2dnoise16into8(uint8_t *pData, int width, int height, uint8_t octaves, uint32_t x, int scalex, uint32_t y, int scaley, uint32_t time);

void fill_raw_2dnoise16(uint16_t *pData, int width, int height, uint8_t octaves, q88 freq88, fract16 amplitude, int skip, uint32_t x, int scalex, uint32_t y, int scaley, uint32_t time);
void fill_raw_2dnoise16into8(uint8_t *pData, int width, int height, uint8_t octaves, q44 freq44, fract8 amplitude, int skip, uint32_t x, int scalex, uint32_t y, int scaley, uint32_t time);

// fill functions to fill leds with values based on noise functions.  These functions use the fill_raw_* functions as appropriate.
void fill_noise8(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time);
void fill_noise16(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time);
void fill_2dnoise8(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint16_t x, int xscale, uint16_t y, int yscale, uint16_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time,bool blend);
void fill_2dnoise16(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint32_t x, int xscale, uint32_t y, int yscale, uint32_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time, bool blend, uint16_t hue_shift=0);

#endif
