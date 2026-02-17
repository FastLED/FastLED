#pragma once

// Engine core methods for Animartrix2
// Extracted from animartrix_detail.hpp ANIMartRIX class to enable standalone operation

#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "fl/stl/chrono.h"
#include "fl/fx/2d/animartrix2_detail/core_types.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_float.hpp"

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#define FL_SIN_F(x) fl::sinf(x)
#define FL_COS_F(x) fl::cosf(x)

namespace fl {

// Polar coordinate pre-computation
// Builds polar_theta[x][y] and distance[x][y] lookup tables
inline void render_polar_lookup_table(float cx, float cy,
                                      fl::vector<fl::vector<float>> &polar_theta,
                                      fl::vector<fl::vector<float>> &distance,
                                      int num_x, int num_y) {
    polar_theta.resize(num_x, fl::vector<float>(num_y, 0.0f));
    distance.resize(num_x, fl::vector<float>(num_y, 0.0f));

    for (int xx = 0; xx < num_x; xx++) {
        for (int yy = 0; yy < num_y; yy++) {
            float dx = xx - cx;
            float dy = yy - cy;
            distance[xx][yy] = fl::hypotf(dx, dy);
            polar_theta[xx][yy] = fl::atan2f(dy, dx);
        }
    }
}

// Calculate oscillators from timing ratios
// Computes linear, radial, directional, and noise_angle modulators
inline void calculate_oscillators(oscillators &timings, modulators &move,
                                   fl::u32 current_time, float speed_factor) {
    double runtime = current_time * timings.master_speed * speed_factor;

    for (int i = 0; i < num_oscillators; i++) {
        move.linear[i] = (runtime + timings.offset[i]) * timings.ratio[i];
        move.radial[i] = fl::fmodf(move.linear[i], 2 * PI);
        move.directional[i] = FL_SIN_F(move.radial[i]);
        move.noise_angle[i] = PI * (1 + pnoise(move.linear[i], 0, 0));
    }
}

// Set up standard oscillator configuration
inline void run_default_oscillators(oscillators &timings, modulators &move,
                                     fl::u32 current_time, float speed_factor,
                                     float master_speed = 0.005) {
    timings.master_speed = master_speed;

    timings.ratio[0] = 1;
    timings.ratio[1] = 2;
    timings.ratio[2] = 3;
    timings.ratio[3] = 4;
    timings.ratio[4] = 5;
    timings.ratio[5] = 6;
    timings.ratio[6] = 7;
    timings.ratio[7] = 8;
    timings.ratio[8] = 9;
    timings.ratio[9] = 10;

    timings.offset[0] = 000;
    timings.offset[1] = 100;
    timings.offset[2] = 200;
    timings.offset[3] = 300;
    timings.offset[4] = 400;
    timings.offset[5] = 500;
    timings.offset[6] = 600;
    timings.offset[7] = 700;
    timings.offset[8] = 800;
    timings.offset[9] = 900;

    calculate_oscillators(timings, move, current_time, speed_factor);
}

// Float mapping maintaining 32 bit precision
inline float map_float(float x, float in_min, float in_max, float out_min,
                       float out_max) {
    float result =
        (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (result < out_min)
        result = out_min;
    if (result > out_max)
        result = out_max;
    return result;
}

// Main noise field renderer with clamping
// Converts polar coordinates to cartesian, applies transformations, and renders Perlin noise
inline float render_value(render_parameters &animation) {
    // Convert polar coordinates back to cartesian ones
    float newx = (animation.offset_x + animation.center_x -
                  (FL_COS_F(animation.angle) * animation.dist)) *
                 animation.scale_x;
    float newy = (animation.offset_y + animation.center_y -
                  (FL_SIN_F(animation.angle) * animation.dist)) *
                 animation.scale_y;
    float newz = (animation.offset_z + animation.z) * animation.scale_z;

    // Render noise value at this new cartesian point
    float raw_noise_field_value = pnoise(newx, newy, newz);

    // Enhance histogram (improve contrast) by setting black and white point
    if (raw_noise_field_value < animation.low_limit)
        raw_noise_field_value = animation.low_limit;
    if (raw_noise_field_value > animation.high_limit)
        raw_noise_field_value = animation.high_limit;

    float scaled_noise_value =
        map_float(raw_noise_field_value, animation.low_limit,
                  animation.high_limit, 0, 255);

    return scaled_noise_value;
}

// Clamp RGB values to [0, 255]
inline rgb rgb_sanity_check(rgb &pixel) {
    if (pixel.red < 0)
        pixel.red = 0;
    if (pixel.green < 0)
        pixel.green = 0;
    if (pixel.blue < 0)
        pixel.blue = 0;

    if (pixel.red > 255)
        pixel.red = 255;
    if (pixel.green > 255)
        pixel.green = 255;
    if (pixel.blue > 255)
        pixel.blue = 255;

    return pixel;
}

// Color blend functions
inline float subtract(float &a, float &b) { return a - b; }

inline float multiply(float &a, float &b) { return a * b / 255.f; }

inline float add(float &a, float &b) { return a + b; }

inline float screen(float &a, float &b) {
    return (1 - (1 - a / 255.f) * (1 - b / 255.f)) * 255.f;
}

inline float colordodge(float &a, float &b) { return (a / (255.f - b)) * 255.f; }

inline float colorburn(float &a, float &b) {
    return (1 - ((1 - a / 255.f) / (b / 255.f))) * 255.f;
}

// Timing functions (performance measurement)
inline void get_ready(unsigned long &a, unsigned long &b) {
    a = fl::micros();
    b = fl::micros(); // logOutput
}

inline void logOutput(unsigned long &b) {
    b = fl::micros();
}

inline void logFrame(unsigned long &c) {
    c = fl::micros();
}

}  // namespace fl
