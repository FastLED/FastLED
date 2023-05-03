/////////////////////////// rgb_2_rgbw ///////////////////////////

// This is a working algorithm of the rgb_2_rgbw() conversion function.
// It is intended to be used for the SK6812 chipset.

#include <assert.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <tuple>

using namespace std;

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct RGBW {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
};

inline uint8_t min3(uint8_t a, uint8_t b, uint8_t c) {
    return std::min(a, min(b, c));
}

// Note that the unused template parameter here is used purely so that the
// user can define their own non-template rgb_2_rgbw() function and have it
// override this function. This works because the C++ compiler will always
// prefer non-template functions to template functions. In this case, the compiler
// will first attempt to bind to a non-template rgb_2_rgbw() function and if it
// does not find it, will default to the one specified here. This is similar to
// the __attribute__((weak)) linker directive.
//
// This function assumes that the white component can be represented by a three
// color LED (rgb) mixing together to produce neutral white. It's also assumed that
// the power output of the white color component is equal to that of one color
// component led.
//
// For RGBW strips that have non-neutral white color components, or white components
// which exceed or are less than the brightness of other color components then this
// function may produce colors that are slightly off.
template <int WHITE_TEMPERATURE = 4000>
inline RGBW rgb_2_rgbw(const RGB rgb) {
    uint8_t min_component = min3(rgb.r, rgb.g, rgb.b);
    uint8_t w;
    bool is_min = true;
    if (min_component <= 84) {
        w = 3 * min_component;
    } else {
        w = 255;
        is_min = false;
    }
    uint8_t r_prime;
    uint8_t g_prime;
    uint8_t b_prime;
    if (is_min) {
        assert(rgb.r >= min_component);
        assert(rgb.g >= min_component);
        assert(rgb.b >= min_component);
        r_prime = rgb.r - min_component;
        g_prime = rgb.g - min_component;
        b_prime = rgb.b - min_component;
    } else {
        uint8_t w3 = w / 3;
        assert(rgb.r >= w3);
        assert(rgb.g >= w3);
        assert(rgb.b >= w3);
        r_prime = rgb.r - w3;
        g_prime = rgb.g - w3;
        b_prime = rgb.b - w3;
    }
    RGBW out = { r_prime, g_prime, b_prime, w };
    return out;
}

/////////////////////////// BEGIN TEST ///////////////////////////

int calc_r(const RGBW input) {
    return int(input.r) + int(input.w) / 3;
}

int calc_g(const RGBW input) {
    return int(input.g) + int(input.w) / 3;
}

int calc_b(const RGBW input) {
    return int(input.b) + int(input.w) / 3;
}

float calc_error(const RGB input_rgb, const RGBW input_rgbw) {
    int r_val = calc_r(input_rgbw);
    int g_val = calc_g(input_rgbw);
    int b_val = calc_b(input_rgbw);
    int r_error = abs(r_val - int(input_rgb.r));
    int g_error = abs(g_val - int(input_rgb.g));
    int b_error = abs(b_val - int(input_rgb.b));
    int rgb_error = r_error + g_error + b_error;
    float error = float(rgb_error) / (float(255) * 3);
    return error;
}

void test_rgb_2_rgbw(const RGB input) {
    RGBW output = rgb_2_rgbw(input);
    cout << "Input " << int(input.r) << ", " << int(input.g) << ", " << int(input.b) << "\n";
    cout << "output " << int(output.r) << ", " << int(output.g) << ", " << int(output.b) << ", " << int(output.w) << "\n";
    int r_val = calc_r(output);
    int g_val = calc_g(output);
    int b_val = calc_b(output);
    int r_error = abs(r_val - int(input.r));
    int g_error = abs(g_val - int(input.g));
    int b_error = abs(b_val - int(input.b));
    int rgb_error = r_error + g_error + b_error;
    float error = float(rgb_error) / (255 * 3);
    cout << "error: " << error << "\n";
    cout << "\n";
}

void test_max_error() {
    float max_error = 0.f;
    RGB max_error_rgb = { 0, 0, 0 };
    for (int r = 0; r < 256; ++r) {
        for (int g = 0; g < 256; g++) {
            for (int b = 0; b < 256; b++) {
                RGB input = { uint8_t(r), uint8_t(g), uint8_t(b) };
                RGBW output = rgb_2_rgbw(input);
                float error = calc_error(input, output);
                if (error > max_error) {
                    max_error = error;
                    max_error_rgb = input;
                }
            }
        }
    }
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "max error is: " << max_error * 100 << "%\n";
    if (max_error > 0.f) {
        std::cout << "max error rgb is: " << int(max_error_rgb.r) << ", " << int(max_error_rgb.g) << ", " << int(max_error_rgb.b) << "\n";
    }
}

int main() {
    test_max_error();
    return 0;
}
