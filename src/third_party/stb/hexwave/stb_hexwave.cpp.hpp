// stb_hexwave.cpp.hpp - Hexwave audio oscillator implementation
//
// This file contains the implementation. For the API declarations,
// include "third_party/stb/hexwave/stb_hexwave.h" instead.
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
// LICENSE
//
//   See end of file for license information.

//////////////////////////////////////////////////////////////////////////////
//
// Include the standalone header for API declarations
//
#include "third_party/stb/hexwave/stb_hexwave.h"

//////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION BEGINS HERE
//
//////////////////////////////////////////////////////////////////////////////

#ifndef FL_STB_HEXWAVE_HEADER_ONLY

// FastLED stl wrappers instead of standard library headers
#include "fl/stl/malloc.h"    // For fl::malloc, fl::free
#include "fl/stl/cstring.h"   // For fl::memset, fl::memcpy, fl::memmove
#include "fl/stl/math.h"      // For fl::sin, fl::cos, fl::fabs

namespace fl {
namespace third_party {
namespace hexwave {

// Import fl:: functions for use within this namespace
using fl::malloc;
using fl::free;
using fl::memset;
using fl::memcpy;
using fl::memmove;
using fl::sin;
using fl::cos;
using fl::fabs;

#define hexwave_clamp(v,a,b)   ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))

void hexwave_change(HexWave *hex, int32_t reflect, float peak_time, float half_height, float zero_wait)
{
   hex->pending.reflect     = reflect;
   hex->pending.peak_time   = hexwave_clamp(peak_time,0.0f,1.0f);
   hex->pending.half_height = half_height;
   hex->pending.zero_wait   = hexwave_clamp(zero_wait,0.0f,1.0f);
   // put a barrier here to allow changing from a different thread than the generator
   hex->have_pending        = 1;
}

void hexwave_create(HexWave *hex, HexWaveEngine *engine, int32_t reflect, float peak_time, float half_height, float zero_wait)
{
   memset(hex, 0, sizeof(*hex));
   hex->engine = engine;
   hexwave_change(hex, reflect, peak_time, half_height, zero_wait);
   hex->current = hex->pending;
   hex->have_pending = 0;
   hex->t = 0;
   hex->prev_dt = 0;
}

// Global engine for legacy API - lazily initialized
static HexWaveEngine* sGlobalEngine = nullptr;

void hexwave_create_legacy(HexWave *hex, int32_t reflect, float peak_time, float half_height, float zero_wait)
{
   hexwave_create(hex, sGlobalEngine, reflect, peak_time, half_height, zero_wait);
}

static void hex_add_oversampled_bleplike(float *output, float time_since_transition, float scale, float *data, HexWaveEngine *engine)
{
   float *d1,*d2;
   float lerpweight;
   int32_t i, bw = engine->width;

   int32_t slot = (int32_t) (time_since_transition * engine->oversample);
   if (slot >= engine->oversample)
      slot = engine->oversample-1; // clamp in case the floats overshoot

   d1 = &data[ slot   *bw];
   d2 = &data[(slot+1)*bw];

   lerpweight = time_since_transition * engine->oversample - slot;
   for (i=0; i < bw; ++i)
      output[i] += scale * (d1[i] + (d2[i]-d1[i])*lerpweight);
}

static void hex_blep (float *output, float time_since_transition, float scale, HexWaveEngine *engine)
{
   hex_add_oversampled_bleplike(output, time_since_transition, scale, engine->blep, engine);
}

static void hex_blamp(float *output, float time_since_transition, float scale, HexWaveEngine *engine)
{
   hex_add_oversampled_bleplike(output, time_since_transition, scale, engine->blamp, engine);
}

typedef struct
{
   float t,v,s; // time, value, slope
} hexvert;

// each half of the waveform needs 4 vertices to represent 3 line
// segments, plus 1 more for wraparound
static void hexwave_generate_linesegs(hexvert vert[9], HexWave *hex, float dt)
{
   int32_t j;
   float min_len = dt / 256.0f;

   vert[0].t = 0;
   vert[0].v = 0;
   vert[1].t = hex->current.zero_wait*0.5f;
   vert[1].v = 0;
   vert[2].t = 0.5f*hex->current.peak_time + vert[1].t*(1-hex->current.peak_time);
   vert[2].v = 1;
   vert[3].t = 0.5f;
   vert[3].v = hex->current.half_height;

   if (hex->current.reflect) {
      for (j=4; j <= 7; ++j) {
         vert[j].t = 1 -  vert[7-j].t;
         vert[j].v =    - vert[7-j].v;
      }
   } else {
      for (j=4; j <= 7; ++j) {
         vert[j].t =  0.5f +  vert[j-4].t;
         vert[j].v =        - vert[j-4].v;
      }
   }
   vert[8].t = 1;
   vert[8].v = 0;

   for (j=0; j < 8; ++j) {
      if (vert[j+1].t <= vert[j].t + min_len) {
          // if change takes place over less than a fraction of a sample treat as discontinuity
          //
          // otherwise the slope computation can blow up to arbitrarily large and we
          // try to generate a huge BLAMP and the result is wrong.
          //
          // why does this happen if the math is right? i believe if done perfectly,
          // the two BLAMPs on either side of the slope would cancel out, but our
          // BLAMPs have only limited sub-sample precision and limited integration
          // accuracy. or maybe it's just the math blowing up w/ floating point precision
          // limits as we try to make x * (1/x) cancel out
          //
          // min_len verified artifact-free even near nyquist with only oversample=4
         vert[j+1].t = vert[j].t;
      }
   }

   if (vert[8].t != 1.0f) {
      // if the above fixup moved the endpoint away from 1.0, move it back,
      // along with any other vertices that got moved to the same time
      float t = vert[8].t;
      for (j=5; j <= 8; ++j)
         if (vert[j].t == t)
            vert[j].t = 1.0f;
   }

   // compute the exact slopes from the final fixed-up positions
   for (j=0; j < 8; ++j)
      if (vert[j+1].t == vert[j].t)
         vert[j].s = 0;
      else
         vert[j].s = (vert[j+1].v - vert[j].v) / (vert[j+1].t - vert[j].t);

   // wraparound at end
   vert[8].t = 1;
   vert[8].v = vert[0].v;
   vert[8].s = vert[0].s;
}

void hexwave_generate_samples(float *output, int32_t num_samples, HexWave *hex, float freq)
{
   HexWaveEngine *engine = hex->engine;
   hexvert vert[9];
   int32_t pass,i,j;
   float t = hex->t;
   float temp_output[2*FL_STB_HEXWAVE_MAX_BLEP_LENGTH];
   int32_t buffered_length = static_cast<int32_t>(sizeof(float)*engine->width);
   float dt = (float) fabs(freq);
   float recip_dt = (dt == 0.0f) ? 0.0f : 1.0f / dt;

   int32_t halfw = engine->width/2;
   // all sample times are biased by halfw to leave room for BLEP/BLAMP to go back in time

   if (num_samples <= 0)
      return;

   // convert parameters to times and slopes
   hexwave_generate_linesegs(vert, hex, dt);

   if (hex->prev_dt != dt) {
      // if frequency changes, add a fixup at the derivative discontinuity starting at now
      float slope;
      for (j=1; j < 6; ++j)
         if (t < vert[j].t)
            break;
      slope = vert[j].s;
      if (slope != 0)
         hex_blamp(output, 0, (dt - hex->prev_dt)*slope, engine);
      hex->prev_dt = dt;
   }

   // copy the buffered data from last call and clear the rest of the output array
   memset(output, 0, sizeof(float)*num_samples);
   memset(temp_output, 0, 2*engine->width*sizeof(float));

   if (num_samples >= engine->width) {
      memcpy(output, hex->buffer, buffered_length);
   } else {
      // if the output is shorter than engine->width, we do all synthesis to temp_output
      memcpy(temp_output, hex->buffer, buffered_length);
   }

   for (pass=0; pass < 2; ++pass) {
      int32_t i0,i1;
      float *out;

      // we want to simulate having one buffer that is num_output + engine->width
      // samples long, without putting that requirement on the user, and without
      // allocating a temp buffer that's as long as the whole thing. so we use two
      // overlapping buffers, one the user's buffer and one a fixed-length temp
      // buffer.

      if (pass == 0) {
         if (num_samples < engine->width)
            continue;
         // run as far as we can without overwriting the end of the user's buffer
         out = output;
         i0 = 0;
         i1 = num_samples - engine->width;
      } else {
         // generate the rest into a temp buffer
         out = temp_output;
         i0 = 0;
         if (num_samples >= engine->width)
            i1 = engine->width;
         else
            i1 = num_samples;
      }

      // determine current segment
      for (j=0; j < 8; ++j)
         if (t < vert[j+1].t)
            break;

      i = i0;
      for(;;) {
         while (t < vert[j+1].t) {
            if (i == i1)
               goto done;
            out[i+halfw] += vert[j].v + vert[j].s*(t - vert[j].t);
            t += dt;
            ++i;
         }
         // transition from lineseg starting at j to lineseg starting at j+1

         if (vert[j].t == vert[j+1].t)
            hex_blep(out+i, recip_dt*(t-vert[j+1].t), (vert[j+1].v - vert[j].v), engine);
         hex_blamp(out+i, recip_dt*(t-vert[j+1].t), dt*(vert[j+1].s - vert[j].s), engine);
         ++j;

         if (j == 8) {
            // change to different waveform if there's a change pending
            j = 0;
            t -= 1.0f; // t was >= 1.f if j==8
            if (hex->have_pending) {
               float prev_s0 = vert[j].s;
               float prev_v0 = vert[j].v;
               hex->current = hex->pending;
               hex->have_pending = 0;
               hexwave_generate_linesegs(vert, hex, dt);
               // the following never occurs with this oscillator, but it makes
               // the code work in more general cases
               if (vert[j].v != prev_v0)
                  hex_blep (out+i, recip_dt*t,    (vert[j].v - prev_v0), engine);
               if (vert[j].s != prev_s0)
                  hex_blamp(out+i, recip_dt*t, dt*(vert[j].s - prev_s0), engine);
            }
         }
      }
     done:
      ;
   }

   // at this point, we've written output[] and temp_output[]
   if (num_samples >= engine->width) {
      // the first half of temp[] overlaps the end of output, the second half will be the new start overlap
      for (i=0; i < engine->width; ++i)
         output[num_samples-engine->width + i] += temp_output[i];
      memcpy(hex->buffer, temp_output+engine->width, buffered_length);
   } else {
      for (i=0; i < num_samples; ++i)
         output[i] = temp_output[i];
      memcpy(hex->buffer, temp_output+num_samples, buffered_length);
   }

   hex->t = t;
}

void hexwave_engine_destroy(HexWaveEngine *engine)
{
   if (engine == nullptr)
      return;

   #ifndef FL_STB_HEXWAVE_NO_ALLOCATION
   if (engine->ownsBuffers) {
      free(engine->blep);
      free(engine->blamp);
   }
   free(engine);
   #endif
}

// buffer should be NULL or must be 4*(width*(oversample+1)*2 +
HexWaveEngine* hexwave_engine_create(int32_t width, int32_t oversample, float *user_buffer)
{
   if (width > FL_STB_HEXWAVE_MAX_BLEP_LENGTH)
      width = FL_STB_HEXWAVE_MAX_BLEP_LENGTH;

   int32_t halfwidth = width/2;
   int32_t half = halfwidth*oversample;
   int32_t blep_buffer_count = width*(oversample+1);
   int32_t n = 2*half+1;

   #ifdef FL_STB_HEXWAVE_NO_ALLOCATION
   (void)blep_buffer_count;
   if (user_buffer == nullptr)
      return nullptr;  // Cannot allocate in no-allocation mode
   float *buffers = user_buffer;
   #else
   float *buffers = user_buffer ? user_buffer : (float *) malloc(sizeof(float) * n * 2);
   #endif

   float *step    = buffers+0*n;
   float *ramp    = buffers+1*n;
   float *blep_buffer, *blamp_buffer;
   double integrate_impulse=0, integrate_step=0;
   int32_t i,j;

   bool ownsBuffers = false;
   if (user_buffer == 0) {
      #ifndef FL_STB_HEXWAVE_NO_ALLOCATION
      blep_buffer  = (float *) malloc(sizeof(float)*blep_buffer_count);
      blamp_buffer = (float *) malloc(sizeof(float)*blep_buffer_count);
      ownsBuffers = true;
      #else
      blep_buffer = nullptr;
      blamp_buffer = nullptr;
      #endif
   } else {
      blep_buffer  = ramp+n;
      blamp_buffer = blep_buffer + blep_buffer_count;
   }

   // compute BLEP and BLAMP by integrating windowed sinc
   for (i=0; i < n; ++i) {
      for (j=0; j < 16; ++j) {
         float sinc_t = 3.141592f* (i-half) / oversample;
         float sinc   = (i==half) ? 1.0f : (float) sin(static_cast<double>(sinc_t)) / (sinc_t);
         float wt     = 2.0f*3.1415926f * i / (n-1);
         float window = (float) (0.355768 - 0.487396*cos(static_cast<double>(wt)) + 0.144232*cos(2*static_cast<double>(wt)) - 0.012604*cos(3*static_cast<double>(wt))); // Nuttall
         double value       =         window * sinc;
         integrate_impulse +=         value/16;
         integrate_step    +=         integrate_impulse/16;
      }
      step[i]            = (float) integrate_impulse;
      ramp[i]            = (float) integrate_step;
   }

   // renormalize
   for (i=0; i < n; ++i) {
      step[i] = step[i] * (float) (1.0       / step[n-1]); // step needs to reach to 1.0
      ramp[i] = ramp[i] * (float) (halfwidth / ramp[n-1]); // ramp needs to become a slope of 1.0 after oversampling
   }

   // deinterleave to allow efficient interpolation e.g. w/SIMD
   for (j=0; j <= oversample; ++j) {
      for (i=0; i < width; ++i) {
         blep_buffer [j*width+i] = step[j+i*oversample];
         blamp_buffer[j*width+i] = ramp[j+i*oversample];
      }
   }

   // subtract out the naive waveform; note we can't do this to the raw data
   // above, because we want the discontinuity to be in a different locations
   // for j=0 and j=oversample (which exists to provide something to interpolate against)
   for (j=0; j <= oversample; ++j) {
      // subtract step
      for (i=halfwidth; i < width; ++i)
         blep_buffer [j*width+i] -= 1.0f;
      // subtract ramp
      for (i=halfwidth; i < width; ++i)
         blamp_buffer[j*width+i] -= (j+i*oversample-half)*(1.0f/oversample);
   }

   // Allocate and initialize engine
   #ifndef FL_STB_HEXWAVE_NO_ALLOCATION
   HexWaveEngine *engine = (HexWaveEngine *) malloc(sizeof(HexWaveEngine));
   engine->blep  = blep_buffer;
   engine->blamp = blamp_buffer;
   engine->width = width;
   engine->oversample = oversample;
   engine->ownsBuffers = ownsBuffers;

   if (user_buffer == 0)
      free(buffers);

   return engine;
   #else
   (void)ownsBuffers;
   return nullptr;
   #endif
}

//////////////////////////////////////////////////////////////////////////////
//
// LEGACY API IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////////

void hexwave_shutdown(float *user_buffer)
{
   (void)user_buffer;
   if (sGlobalEngine != nullptr) {
      hexwave_engine_destroy(sGlobalEngine);
      sGlobalEngine = nullptr;
   }
}

void hexwave_init(int32_t width, int32_t oversample, float *user_buffer)
{
   if (sGlobalEngine != nullptr) {
      hexwave_engine_destroy(sGlobalEngine);
   }
   sGlobalEngine = hexwave_engine_create(width, oversample, user_buffer);
}

#undef hexwave_clamp

} // namespace hexwave
} // namespace third_party
} // namespace fl

#endif // FL_STB_HEXWAVE_HEADER_ONLY

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
