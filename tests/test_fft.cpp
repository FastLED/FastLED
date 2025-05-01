
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"

#include "fl/math.h"
#include "fl/fft.h"

// // Proof of concept FFT using KISS FFT. Right now this is fixed sized blocks of 512. But this is
// // intended to change with a C++ wrapper around ot/
// typedef int16_t fft_audio_buffer_t[512];

// void fft_init(); // Explicit initialization of FFT, otherwise it will be initialized on first run.
// bool fft_is_initialized();
// void fft_unit_test(const fft_audio_buffer_t &buffer);


using namespace fl;

TEST_CASE("fft tester") {

    fft_init();
    fft_audio_buffer_t buffer = {0};
    // fill in with a sine wave
    for (int i = 0; i < 512; ++i) {
        // buffer[i] = int16_t(32767 * sin(2.0 * M_PI * i / 2));
        float rot = fl::map_range<float, float>(i, 0, 511, 0, 2 * PI * 10);
        float sin_x = sin(rot);
        buffer[i] = int16_t(32767 * sin_x);
    }
    fl::vector_fixed<float, 16> out;
    fft_unit_test(buffer, &out);
    FASTLED_WARN("FFT output: " << out);
    FASTLED_WARN("DONE");
    
}
