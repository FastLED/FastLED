#pragma once

#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

// Proof of concept FFT using KISS FFT. Right now this is fixed sized blocks of 512. But this is
// intended to change with a C++ wrapper around ot/
typedef int16_t fft_audio_buffer_t[512];
// typedef float fft_output[16];

using fft_output_fixed = fl::vector_fixed<float, 16>;

void fft_init(); // Explicit initialization of FFT, otherwise it will be initialized on first run.
bool fft_is_initialized();
void fft_unit_test(const fft_audio_buffer_t &buffer, fft_output_fixed* out);

};