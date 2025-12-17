/*
 * FFT Precision Configuration
 *
 * This header defines the precision mode for the FFT implementation.
 *
 * Usage:
 *   Define FASTLED_FFT_PRECISION before including this header to select precision mode:
 *
 *   FASTLED_FFT_FLOAT    (1) - 32-bit floating point (faster, moderate precision)
 *   FASTLED_FFT_DOUBLE   (2) - 64-bit floating point (slower, high precision)
 *   FASTLED_FFT_FIXED16  (3) - 16-bit fixed point (default, memory efficient)
 *
 *   Example:
 *     #define FASTLED_FFT_PRECISION FASTLED_FFT_FLOAT
 *     #include "fft_precision.h"
 */

#ifndef FFT_PRECISION_H
#define FFT_PRECISION_H

#include "fl/stl/math.h"

// Define precision mode constants
#define FASTLED_FFT_FLOAT   1
#define FASTLED_FFT_DOUBLE  2
#define FASTLED_FFT_FIXED16 3

// Default to FIXED16 if not specified (maintains backward compatibility)
#ifndef FASTLED_FFT_PRECISION
#define FASTLED_FFT_PRECISION FASTLED_FFT_FIXED16
#endif

// Define the floating point type used for math operations based on precision mode
#if FASTLED_FFT_PRECISION == FASTLED_FFT_FLOAT
    typedef float fft_float_t;
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_DOUBLE
    typedef double fft_float_t;
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_FIXED16
    typedef double fft_float_t;  // Fixed point uses double for high precision internal math
#else
    #error "Invalid FASTLED_FFT_PRECISION value"
#endif

// Precision-aware math function macros
// These select the appropriate float or double version based on the precision mode
// Note: FIXED16 uses standard library (::) functions for backward compatibility with existing test expectations
#if FASTLED_FFT_PRECISION == FASTLED_FFT_FLOAT
    #define FFT_COS(x)   fl::cosf((float)(x))
    #define FFT_SIN(x)   fl::sinf((float)(x))
    #define FFT_EXP(x)   fl::expf((float)(x))
    #define FFT_LOG(x)   fl::logf((float)(x))
    #define FFT_SQRT(x)  fl::sqrtf((float)(x))
    #define FFT_POW(x,y) fl::powf((float)(x), (float)(y))
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_DOUBLE
    #define FFT_COS(x)   fl::cos((double)(x))
    #define FFT_SIN(x)   fl::sin((double)(x))
    #define FFT_EXP(x)   fl::exp((double)(x))
    #define FFT_LOG(x)   fl::log((double)(x))
    #define FFT_SQRT(x)  fl::sqrt((double)(x))
    #define FFT_POW(x,y) fl::pow((double)(x), (double)(y))
#elif FASTLED_FFT_PRECISION == FASTLED_FFT_FIXED16
    // Use fl:: namespace functions for consistency with FastLED conventions
    #define FFT_COS(x)   fl::cos((double)(x))
    #define FFT_SIN(x)   fl::sin((double)(x))
    #define FFT_EXP(x)   fl::exp((double)(x))
    #define FFT_LOG(x)   fl::log((double)(x))
    #define FFT_SQRT(x)  fl::sqrt((double)(x))
    #define FFT_POW(x,y) fl::pow((double)(x), (double)(y))
#endif

#endif // FFT_PRECISION_H
