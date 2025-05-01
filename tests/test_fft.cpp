
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"

#include "fl/fft.h"
#include "fl/math.h"

// // Proof of concept FFT using KISS FFT. Right now this is fixed sized blocks
// of 512. But this is
// // intended to change with a C++ wrapper around ot/
// typedef int16_t fft_audio_buffer_t[512];

// void fft_init(); // Explicit initialization of FFT, otherwise it will be
// initialized on first run. bool fft_is_initialized(); void fft_unit_test(const
// fft_audio_buffer_t &buffer);

using namespace fl;

TEST_CASE("fft tester") {
    

    // fft_init();
    fft_audio_buffer_t buffer = {0};
    // fill in with a sine wave
    for (int i = 0; i < 512; ++i) {
        // buffer[i] = int16_t(32767 * sin(2.0 * M_PI * i / 2));
        float rot = fl::map_range<float, float>(i, 0, 511, 0, 2 * PI * 10);
        float sin_x = sin(rot);
        buffer[i] = int16_t(32767 * sin_x);
    }
    fl::vector_inlined<float, 16> out;
    // fft_unit_test(buffer, &out);
    FFT fft;
    fft.fft_unit_test(buffer, &out);


    FASTLED_WARN("FFT output: " << out);
    const float expected_output[16] = {
        3, 2, 2, 6, 6.08, 15.03, 3078.22, 4346.29, 4033.16, 3109, 38.05, 4.47, 4, 2, 1.41, 1.41};
    for (int i = 0; i < 16; ++i) {
        // CHECK(out[i] == Approx(expected_output[i]).epsilon(0.1));
        float a = out[i];
        float b = expected_output[i];
        bool almost_equal = ALMOST_EQUAL(a, b, 0.1);
        if (!almost_equal) {
            FASTLED_WARN("FFT output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        CHECK(almost_equal);
    }

    fl::Str info = fft.generateHeaderInfo();
    FASTLED_WARN("FFT info: " << info);
    FASTLED_WARN("Done");
}
