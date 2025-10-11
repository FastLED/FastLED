
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"

#include "fl/fft.h"
#include "fl/fft_impl.h"
#include "fl/math.h"

// // Proof of concept FFTImpl using KISS FFTImpl. Right now this is fixed sized blocks
// of 512. But this is
// // intended to change with a C++ wrapper around ot/
// typedef int16_t fft_audio_buffer_t[512];

// void fft_init(); // Explicit initialization of FFTImpl, otherwise it will be
// initialized on first run. bool fft_is_initialized(); void fft_unit_test(const
// fft_audio_buffer_t &buffer);

using namespace fl;


TEST_CASE("fft tester 512") {
    int16_t buffer[512] = {0};
    const int n = 512;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = sin(rot);
        buffer[i] = int16_t(32767 * sin_x);
    }
    FFTBins out(16);
    // fft_unit_test(buffer, &out);
    const int samples = n;
    FFTImpl fft(samples);
    fft.run(buffer, &out);

    // Test expectations updated to match Q15 fixed-point implementation (2025-01-11)
    const float expected_output[16] = {
        3.00,      2.00,      2.00,      6.00,      6.08,      15.03,     74069.60,  147622.53,
        127123.91, 75557.54,  38.14,     4.47,      4.00,      2.00,      1.41,      1.41};
    for (int i = 0; i < 16; ++i) {
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = FL_ALMOST_EQUAL(a, b, 0.1);
        if (!almost_equal) {
            FASTLED_WARN("FFTImpl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        CHECK(almost_equal);
    }

    fl::string info = fft.info();
    FASTLED_WARN("FFTImpl info: " << info);
    FASTLED_WARN("Done");
}

TEST_CASE("fft tester 256") {
    // fft_audio_buffer_t buffer = {0};
    fl::vector<int16_t> buffer;
    const int n = 256;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = sin(rot);
        auto v = int16_t(32767 * sin_x);
        buffer.push_back(v);
    }
    FFTBins out(16);
    // fft_unit_test(buffer, &out);
    const int samples = n;
    FFTImpl fft(samples);
    fft.run(buffer, &out);

    // Test expectations updated to match Q15 fixed-point implementation (2025-01-11)
    const float expected_output[16] = {
        3.00,      2.00,      4.00,      5.00,      5.10,      9.06,      11.05,     27.66,
        60417.69,  113548.60, 136322.36, 136873.91, 136186.67, 126147.16, 103467.31, 86549.66};
    for (int i = 0; i < 16; ++i) {
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = FL_ALMOST_EQUAL(a, b, 0.1);
        if (!almost_equal) {
            FASTLED_WARN("FFTImpl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        CHECK(almost_equal);
    }

    fl::string info = fft.info();
    FASTLED_WARN("FFTImpl info: " << info);
    FASTLED_WARN("Done");
}

TEST_CASE("fft tester 256 with 64 bands") {
    // fft_audio_buffer_t buffer = {0};
    fl::vector<int16_t> buffer;
    const int n = 256;
    // fill in with a sine wave
    for (int i = 0; i < n; ++i) {
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * FL_PI * 10);
        float sin_x = sin(rot);
        auto v = int16_t(32767 * sin_x);
        buffer.push_back(v);
    }
    FFTBins out(64);
    // fft_unit_test(buffer, &out);
    const int samples = n;
    FFT_Args args(samples, 64);
    FFTImpl fft(args);
    fft.run(buffer, &out);
    // Test expectations updated to match Q15 fixed-point implementation (2025-01-11)
    const float expected_output[64] = {
        3.00,      3.00,      1.00,      2.00,      2.00,      3.00,      3.00,
        3.00,      3.00,      4.00,      3.00,      4.00,      4.00,      5.00,
        5.00,      3.16,      4.12,      5.10,      5.10,      6.08,      7.00,
        9.06,      9.06,      9.06,      10.20,     11.18,     15.13,     18.25,
        20.22,     26.31,     30.61,     66.33,     76.04,     52927.46,  65585.62,
        84188.41,  94313.36,  105783.13, 117466.01, 122605.23, 126515.94, 138073.81,
        136322.36, 143600.75, 137820.73, 142721.47, 142809.06, 134951.34, 135923.25,
        132732.11, 133419.50, 127835.66, 129614.28, 132679.41, 122963.98, 126160.11,
        113759.55, 113786.92, 122834.30, 102889.07, 101243.45, 116644.41, 114228.32,
        86549.66};
    for (int i = 0; i < 64; ++i) {
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = FL_ALMOST_EQUAL(a, b, 0.1);
        if (!almost_equal) {
            FASTLED_WARN("FFTImpl output mismatch at index " << i << ": " << a
                                                         << " != " << b);
        }
        CHECK(almost_equal);
    }
    fl::string info = fft.info();
    FASTLED_WARN("FFTImpl info: " << info);
    FASTLED_WARN("Done");
}
