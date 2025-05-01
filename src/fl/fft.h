#pragma once

#include "fl/slice.h"

namespace fl {



typedef int16_t fft_audio_buffer_t[512];


void fft_init(); // Explicit initialization of FFT, otherwise it will be initialized on first run.
bool fft_is_initialized();
void fft_unit_test(const fft_audio_buffer_t &buffer);




};