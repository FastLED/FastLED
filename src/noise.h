#pragma once

#include <stdint.h>

#include "crgb.h"
#include "namespace.h"

/// @file noise.h
/// Functions to generate and fill arrays with noise.

FASTLED_NAMESPACE_BEGIN

/// @defgroup Noise Noise Functions
/// Functions to generate and fill arrays with noise. 
/// These functions use [Perlin noise](https://en.wikipedia.org/wiki/Perlin_noise)
/// as the noise generation algorithm.
/// @{


/// @defgroup NoiseGeneration Noise Generation Functions
/// Functions to generate noise.
/// @{

/// @name 16-Bit Scaled Noise Functions
/// @{

/// @copydoc inoise16(uint32_t, uint32_t)
/// @param z z-axis coordinate on noise map (3D)
extern uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);

/// @copydoc inoise16(uint32_t)
/// @param y y-axis coordinate on noise map (2D)
extern uint16_t inoise16(uint32_t x, uint32_t y);

/// 16-bit, fixed point implementation of Perlin's noise. 
/// @see inoise16_raw()
/// @returns scaled noise value as an unsigned integer, 0-65535
/// @param x x-axis coordinate on noise map (1D)
extern uint16_t inoise16(uint32_t x);

/// @} 16-Bit Scaled Noise Functions


/// @name 16-Bit Raw Noise Functions
/// @{

/// @copydoc inoise16_raw(uint32_t, uint32_t)
/// @param z z-axis coordinate on noise map (3D)
extern int16_t inoise16_raw(uint32_t x, uint32_t y, uint32_t z);

/// @copydoc inoise16_raw(uint32_t)
/// @param y y-axis coordinate on noise map (2D)
extern int16_t inoise16_raw(uint32_t x, uint32_t y);

/// 16-bit, fixed point implementation of Perlin's noise without scaling. 
/// Coordinates are 16.16 fixed point values, 32 bit integers with
/// integral coordinates in the high 16-bits and fractional in the low 16-bits.
/// @returns unscaled noise value as a signed integer, roughly -18k to 18k
/// @param x x-axis coordinate on noise map (1D)
extern int16_t inoise16_raw(uint32_t x);

/// @} 16-Bit Raw Noise Functions


/// @name 8-Bit Scaled Noise Functions
/// @{

/// @copydoc inoise8(uint16_t, uint16_t)
/// @param z z-axis coordinate on noise map (3D)
extern uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);

/// @copydoc inoise8(uint16_t)
/// @param y y-axis coordinate on noise map (2D)
extern uint8_t inoise8(uint16_t x, uint16_t y);

/// 8-Bit, fixed point implementation of Perlin's noise. 
/// @see inoise8_raw()
/// @returns scaled noise value as an unsigned integer, 0-255
/// @param x x-axis coordinate on noise map (1D)
extern uint8_t inoise8(uint16_t x);

/// @} 8-Bit Scaled Noise Functions


/// @name 8-Bit Raw Noise Functions
/// @{

/// @copydoc inoise8_raw(uint16_t, uint16_t)
/// @param z z-axis coordinate on noise map (3D)
extern int8_t inoise8_raw(uint16_t x, uint16_t y, uint16_t z);

/// @copydoc inoise8_raw(uint16_t)
/// @param y y-axis coordinate on noise map (2D)
extern int8_t inoise8_raw(uint16_t x, uint16_t y);

/// 8-bit, fixed point implementation of Perlin's noise without scaling. 
/// Coordinates are 8.8 fixed point values, 16-bit integers with
/// integral coordinates in the high 8-bits and fractional in the low 8-bits.
/// @returns unscaled noise value as a signed integer, roughly -70 to 70
/// @param x x-axis coordinate on noise map (1D)
extern int8_t inoise8_raw(uint16_t x);

/// @} 8-Bit Raw Noise Functions
/// @} NoiseGeneration


/// @name 32 bit simplex noise functions
///@{
/// 32 bit, fixed point implementation of simplex noise functions.
/// The inputs are 20.12 fixed-point value. The result covers the full
/// range of a uint16_t averaging around 32768.
uint16_t snoise16(uint32_t x);
uint16_t snoise16(uint32_t x, uint32_t y);
uint16_t snoise16(uint32_t x, uint32_t y, uint32_t z);
uint16_t snoise16(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
///@}



/// @defgroup NoiseFill Noise Fill Functions
/// Functions to fill a buffer with noise data.
/// @{

/// @name Raw Fill Functions
/// Fill a 1D or 2D array with generated noise.
/// @{

/// Fill a 1D 8-bit buffer with noise, using inoise8() 
/// @param pData the array of data to fill with noise values
/// @param num_points the number of points of noise to compute
/// @param octaves the number of octaves to use for noise. More octaves = more noise.
/// @param x x-axis coordinate on noise map (1D)
/// @param scalex the scale (distance) between x points when filling in noise
/// @param time the time position for the noise field
void fill_raw_noise8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint16_t x, int scalex, uint16_t time);

/// Fill a 1D 8-bit buffer with noise, using inoise16()
/// @copydetails fill_raw_noise8()
void fill_raw_noise16into8(uint8_t *pData, uint8_t num_points, uint8_t octaves, uint32_t x, int scalex, uint32_t time);

/// Fill a 2D 8-bit buffer with noise, using inoise8() 
/// @param pData the array of data to fill with noise values
/// @param width the width of the 2D buffer
/// @param height the height of the 2D buffer
/// @param octaves the number of octaves to use for noise. More octaves = more noise.
/// @param x x-axis coordinate on noise map (1D)
/// @param scalex the scale (distance) between x points when filling in noise
/// @param y y-axis coordinate on noise map (2D)
/// @param scaley the scale (distance) between y points when filling in noise
/// @param time the time position for the noise field
void fill_raw_2dnoise8(uint8_t *pData, int width, int height, uint8_t octaves, uint16_t x, int16_t scalex, uint16_t y, int16_t scaley, uint16_t time);

/// Fill a 2D 8-bit buffer with noise, using inoise16() 
/// @copydetails fill_raw_2dnoise8(uint8_t*, int, int, uint8_t, uint16_t, int16_t, uint16_t, int16_t, uint16_t)
void fill_raw_2dnoise16into8(uint8_t *pData, int width, int height, uint8_t octaves, uint32_t x, int32_t scalex, uint32_t y, int32_t scaley, uint32_t time);

/// Fill a 2D 16-bit buffer with noise, using inoise16() 
/// @copydetails fill_raw_2dnoise8(uint8_t*, int, int, uint8_t, uint16_t, int16_t, uint16_t, int16_t, uint16_t)
/// @param freq88 starting octave frequency
/// @param amplitude noise amplitude
/// @param skip how many noise maps to skip over, incremented recursively per octave
void fill_raw_2dnoise16(uint16_t *pData, int width, int height, uint8_t octaves, q88 freq88, fract16 amplitude, int skip, uint32_t x, int32_t scalex, uint32_t y, int32_t scaley, uint32_t time);

/// Fill a 2D 8-bit buffer with noise, using inoise16() 
/// @copydetails fill_raw_2dnoise8(uint8_t*, int, int, uint8_t, uint16_t, int16_t, uint16_t, int16_t, uint16_t)
/// @param freq44 starting octave frequency
/// @param amplitude noise amplitude
/// @param skip how many noise maps to skip over, incremented recursively per octave
void fill_raw_2dnoise16into8(uint8_t *pData, int width, int height, uint8_t octaves, q44 freq44, fract8 amplitude, int skip, uint32_t x, int32_t scalex, uint32_t y, int32_t scaley, uint32_t time);

/// @} Raw Fill Functions


/// @name Fill Functions
/// Fill an LED array with colors based on noise. 
/// Colors are calculated using noisemaps, randomly selecting hue and value 
/// (brightness) points with full saturation (255).
/// @{

/// Fill an LED array with random colors, using 8-bit noise
/// @param leds pointer to LED array
/// @param num_leds the number of LEDs to fill
/// @param octaves the number of octaves to use for value (brightness) noise
/// @param x x-axis coordinate on noise map for value (brightness) noise
/// @param scale the scale (distance) between x points when filling in value (brightness) noise
/// @param hue_octaves the number of octaves to use for color hue noise
/// @param hue_x x-axis coordinate on noise map for color hue noise
/// @param hue_scale the scale (distance) between x points when filling in color hue noise
/// @param time the time position for the noise field
void fill_noise8(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time);

/// Fill an LED array with random colors, using 16-bit noise
/// @copydetails fill_noise8()
/// @param hue_shift how much to shift the final hues by for every LED
void fill_noise16(CRGB *leds, int num_leds,
            uint8_t octaves, uint16_t x, int scale,
            uint8_t hue_octaves, uint16_t hue_x, int hue_scale,
            uint16_t time, uint8_t hue_shift=0);

/// Fill an LED matrix with random colors, using 8-bit noise
/// @param leds pointer to LED array
/// @param width the width of the LED matrix
/// @param height the height of the LED matrix
/// @param serpentine whether the matrix is laid out in a serpentine pattern (alternating left/right directions per row)
///
/// @param octaves the number of octaves to use for value (brightness) noise
/// @param x x-axis coordinate on noise map for value (brightness) noise
/// @param xscale the scale (distance) between x points when filling in value (brightness) noise
/// @param y y-axis coordinate on noise map for value (brightness) noise
/// @param yscale the scale (distance) between y points when filling in value (brightness) noise
/// @param time the time position for the value (brightness) noise field
///
/// @param hue_octaves the number of octaves to use for color hue noise
/// @param hue_x x-axis coordinate on noise map for color hue noise
/// @param hue_xscale the scale (distance) between x points when filling in color hue noise
/// @param hue_y y-axis coordinate on noise map for color hue noise.
/// @param hue_yscale the scale (distance) between y points when filling in color hue noise
/// @param hue_time the time position for the color hue noise field
/// @param blend if true, will blend the newly generated LED values into the array. If false,
/// will overwrite the array values directly.
void fill_2dnoise8(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint16_t x, int xscale, uint16_t y, int yscale, uint16_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time,bool blend);

/// Fill an LED matrix with random colors, using 16-bit noise
/// @copydetails fill_2dnoise8()
/// @param hue_shift how much to shift the final hues by for every LED
void fill_2dnoise16(CRGB *leds, int width, int height, bool serpentine,
            uint8_t octaves, uint32_t x, int xscale, uint32_t y, int yscale, uint32_t time,
            uint8_t hue_octaves, uint16_t hue_x, int hue_xscale, uint16_t hue_y, uint16_t hue_yscale,uint16_t hue_time, bool blend, uint16_t hue_shift=0);

/// @} Fill Functions

/// @} NoiseFill
/// @} Noise

FASTLED_NAMESPACE_END

