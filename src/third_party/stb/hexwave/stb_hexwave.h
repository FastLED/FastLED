// stb_hexwave.h - Header for stb_hexwave audio oscillator
//
// This file contains the API declarations extracted from stb_hexwave.cpp.hpp.
// It provides the public interface without including the implementation.
//
// For the implementation, include stb_hexwave.cpp.hpp (which is included
// via the unity build system in _build.hpp).
//
// Original: https://github.com/nothings/stb/blob/master/stb_hexwave.h
// Author: Sean Barrett - http://nothings.org/stb_vorbis/
// License: MIT or Public Domain
//
// stb_hexwave - v0.5 - public domain, initial release 2021-04-01
//
// A flexible anti-aliased (bandlimited) digital audio oscillator.
//
// This library generates waveforms of a variety of shapes made of
// line segments. It does not do envelopes, LFO effects, etc.; it
// merely tries to solve the problem of generating an artifact-free
// morphable digital waveform with a variety of spectra, and leaves
// it to the user to rescale the waveform and mix multiple voices, etc.
//
// Usage:
//
//   Initialization:
//
//     hexwave_init(32, 16, nullptr); // read "header section" for alternatives
//
//   Create oscillator:
//
//     HexWave osc;
//     hexwave_create(&osc, reflect_flag, peak_time, half_height, zero_wait);
//       see "Waveform shapes" section in implementation for parameter meanings
//
//   Generate audio:
//
//     hexwave_generate_samples(output, number_of_samples, &osc, oscillator_freq)
//       where:
//         output is a buffer where the library will store floating point audio samples
//         number_of_samples is the number of audio samples to generate
//         osc is a pointer to a HexWave
//         oscillator_freq is the frequency of the oscillator divided by the sample rate
//
//   Change oscillator waveform:
//
//     hexwave_change(&osc, reflect_flag, peak_time, half_height, zero_wait);
//       can call in between calls to hexwave_generate_samples
//
// Classic waveforms:
//                               peak    half    zero
//                     reflect   time   height   wait
//      Sawtooth          1       0       0       0
//      Square            1       0       1       0
//      Triangle          1       0.5     0       0
//
// LICENSE
//
//   See end of file for license information.

#ifndef FL_STB_HEXWAVE_INCLUDE_FL_STB_HEXWAVE_H
#define FL_STB_HEXWAVE_INCLUDE_FL_STB_HEXWAVE_H

#include "fl/stl/stdint.h"

#ifndef FL_STB_HEXWAVE_MAX_BLEP_LENGTH
#define FL_STB_HEXWAVE_MAX_BLEP_LENGTH   64 // good enough for anybody
#endif

namespace fl {
namespace third_party {
namespace hexwave {

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC TYPES
//
//////////////////////////////////////////////////////////////////////////////

// Internal waveform parameters structure
typedef struct
{
   int32_t reflect;
   float peak_time;
   float zero_wait;
   float half_height;
} HexWaveParameters;

/// Engine state holding BLEP/BLAMP tables
///
/// This structure holds the precomputed band-limited step (BLEP) and ramp (BLAMP)
/// tables used for anti-aliased waveform generation. Previously this was global
/// state, but now each HexWaveEngine instance can have its own configuration.
///
/// You can create multiple engines with different width/oversample settings,
/// or share a single engine among multiple HexWave oscillators.
struct HexWaveEngine
{
   int32_t width;       ///< Width of fixup in samples (4..64)
   int32_t oversample;  ///< Number of oversampled versions
   float *blep;         ///< Band-limited step table
   float *blamp;        ///< Band-limited ramp table
   bool ownsBuffers;    ///< True if engine allocated blep/blamp (vs user-provided)
};

// Main oscillator state structure
struct HexWave
{
   float t, prev_dt;
   HexWaveParameters current, pending;
   int32_t have_pending;
   float buffer[FL_STB_HEXWAVE_MAX_BLEP_LENGTH];
   HexWaveEngine *engine;  ///< Pointer to engine with BLEP/BLAMP tables
};

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC API - Instance-based (Recommended)
//
//////////////////////////////////////////////////////////////////////////////

/// Create and initialize a new HexWaveEngine
/// @param width Size of BLEP, from 4..64, larger is slower & more memory but less aliasing
/// @param oversample 2+, number of subsample positions, larger uses more memory but less noise
/// @param user_buffer Optional, if provided the library will perform no allocations.
///                    16*width*(oversample+1) bytes, must stay allocated as long as engine is used
/// @return Pointer to initialized engine, or nullptr on failure
///
/// Width can be larger than 64 if you define FL_STB_HEXWAVE_MAX_BLEP_LENGTH to a larger value
HexWaveEngine* hexwave_engine_create(int32_t width, int32_t oversample, float *user_buffer);

/// Destroy a HexWaveEngine and free its resources
/// @param engine Pointer to engine to destroy
void hexwave_engine_destroy(HexWaveEngine *engine);

/// Create a new oscillator with the given waveform parameters
/// @param hex Pointer to HexWave structure to initialize
/// @param engine Pointer to initialized HexWaveEngine (required)
/// @param reflect Is tested as 0 or non-zero
/// @param peak_time Is clamped to 0..1
/// @param half_height Is not clamped
/// @param zero_wait Is clamped to 0..1
///
/// Classic waveforms:
///                               peak    half    zero
///                     reflect   time   height   wait
///      Sawtooth          1       0       0       0
///      Square            1       0       1       0
///      Triangle          1       0.5     0       0
void hexwave_create(HexWave *hex, HexWaveEngine *engine, int32_t reflect, float peak_time, float half_height, float zero_wait);

/// Change oscillator waveform parameters (takes effect at next cycle boundary)
/// @param hex Pointer to HexWave structure
/// @param reflect Is tested as 0 or non-zero
/// @param peak_time Is clamped to 0..1
/// @param half_height Is not clamped
/// @param zero_wait Is clamped to 0..1
void hexwave_change(HexWave *hex, int32_t reflect, float peak_time, float half_height, float zero_wait);

/// Generate audio samples
/// @param output Buffer where the library will store generated floating point audio samples
/// @param num_samples The number of audio samples to generate
/// @param hex Pointer to a HexWave initialized with hexwave_create
/// @param freq Frequency of the oscillator divided by the sample rate
///
/// The output samples will continue from where the samples generated by the
/// previous hexwave_generate_samples() on this oscillator ended.
void hexwave_generate_samples(float *output, int32_t num_samples, HexWave *hex, float freq);

//////////////////////////////////////////////////////////////////////////////
//
// LEGACY API - Global state (Deprecated)
//
// These functions use a hidden global engine for backward compatibility.
// New code should use the instance-based API above.
//
//////////////////////////////////////////////////////////////////////////////

/// Initialize the hexwave library (DEPRECATED - use hexwave_engine_create)
/// @param width Size of BLEP, from 4..64, larger is slower & more memory but less aliasing
/// @param oversample 2+, number of subsample positions, larger uses more memory but less noise
/// @param user_buffer Optional, if provided the library will perform no allocations.
void hexwave_init(int32_t width, int32_t oversample, float *user_buffer);

/// Shutdown the hexwave library (DEPRECATED - use hexwave_engine_destroy)
/// @param user_buffer Pass in same parameter as passed to hexwave_init
void hexwave_shutdown(float *user_buffer);

/// Create oscillator using global engine (DEPRECATED)
/// @note This version uses the global engine. Prefer hexwave_create with explicit engine.
void hexwave_create_legacy(HexWave *hex, int32_t reflect, float peak_time, float half_height, float zero_wait);

} // namespace hexwave
} // namespace third_party
} // namespace fl

#endif // FL_STB_HEXWAVE_INCLUDE_FL_STB_HEXWAVE_H

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
