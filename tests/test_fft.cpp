
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
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * PI * 10);
        float sin_x = sin(rot);
        buffer[i] = int16_t(32767 * sin_x);
    }
    FFTBins out(16);
    // fft_unit_test(buffer, &out);
    const int samples = n;
    FFTImpl fft(samples);
    fft.run(buffer, &out);

    FASTLED_WARN("FFTImpl output raw bins: " << out.bins_raw);
    FASTLED_WARN("FFTImpl output db bins: " << out.bins_db);
    const float expected_output[16] = {
        3,       2,    2,     6,    6.08, 15.03, 3078.22, 4346.29,
        4033.16, 3109, 38.05, 4.47, 4,    2,     1.41,    1.41};
    for (int i = 0; i < 16; ++i) {
        // CHECK(out[i] == Approx(expected_output[i]).epsilon(0.1));
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = ALMOST_EQUAL(a, b, 0.1);
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
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * PI * 10);
        float sin_x = sin(rot);
        auto v = int16_t(32767 * sin_x);
        buffer.push_back(v);
    }
    FFTBins out(16);
    // fft_unit_test(buffer, &out);
    const int samples = n;
    FFTImpl fft(samples);
    fft.run(buffer, &out);

    FASTLED_WARN("FFTImpl output: " << out);
    const float expected_output[16] = {
        3,       2,       4,       5,       5.10,    9.06,    11.05,   27.66,
        2779.93, 3811.66, 4176.58, 4185.02, 4174.50, 4017.63, 3638.46, 3327.60};
    for (int i = 0; i < 16; ++i) {
        // CHECK(out[i] == Approx(expected_output[i]).epsilon(0.1));
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = ALMOST_EQUAL(a, b, 0.1);
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
        float rot = fl::map_range<float, float>(i, 0, n - 1, 0, 2 * PI * 10);
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
    FASTLED_WARN("FFTImpl output: " << out);
    const float expected_output[64] = {
        3,       3,       1,       2,       2,       3,       3,
        3,       3,       4,       3,       4,       4,       5,
        5,       3.16,    4.12,    5.10,    5.10,    6.08,    7,
        9.06,    9.06,    9.06,    10.20,   11.18,   15.13,   18.25,
        20.22,   26.31,   30.59,   63.95,   71.85,   2601.78, 2896.46,
        3281.87, 3473.71, 3678.96, 3876.88, 3960.81, 4023.50, 4203.33,
        4176.58, 4286.66, 4199.48, 4273.51, 4274.82, 4155.52, 4170.46,
        4121.19, 4131.86, 4044.44, 4072.49, 4120.38, 3966.60, 4017.84,
        3815.20, 3815.66, 3964.51, 3628.27, 3599.13, 3863.29, 3823.06,
        3327.600};
    for (int i = 0; i < 64; ++i) {
        // CHECK(out[i] == Approx(expected_output[i]).epsilon(0.1));
        float a = out.bins_raw[i];
        float b = expected_output[i];
        bool almost_equal = ALMOST_EQUAL(a, b, 0.1);
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
