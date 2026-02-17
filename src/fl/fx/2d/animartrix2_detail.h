#pragma once
// IWYU pragma: private, include "fl/fx/2d/animartrix2.hpp"
// allow-include-after-namespace

// Animartrix2 detail: Refactored version of animartrix_detail.hpp
// Original by Stefan Petrick 2023. Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to free-function architecture 2026.
//
// Licensed under Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/
//
// Architecture: Context struct holds all shared state. Each animation is a
// free function (Visualizer) that operates on Context. Internally delegates
// to animartrix_detail::ANIMartRIX for bit-identical output.
//
// NOTE: This is an internal implementation header. Do not include directly.
//       Include "fl/fx/2d/animartrix2.hpp" instead.

// Include standalone Animartrix2 core functionality
#include "fl/fx/2d/animartrix2_detail/core_types.h"
#include "fl/fx/2d/animartrix2_detail/perlin_float.h"
#include "fl/fx/2d/animartrix2_detail/engine_core.h"

#include "crgb.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/simd.h"


#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef FL_ANIMARTRIX_USES_FAST_MATH
#define FL_ANIMARTRIX_USES_FAST_MATH 1
#endif

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN
#endif

// Helper structs extracted to separate headers for better organization
#include "fl/fx/2d/animartrix2_detail/context.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_pixel_lut.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_q16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s8x8.h"
#include "fl/fx/2d/animartrix2_detail/perlin_i16_optimized.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.h"
#include "fl/fx/2d/animartrix2_detail/engine.h"

namespace fl {

// ============================================================
// Animation free functions (Visualizers)
// Each delegates to the corresponding ANIMartRIX method.
// ============================================================

inline void Rotating_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.03;
    e->timings.ratio[3] = 0.03;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.offset_z = 100;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.dist = e->distance[x][y];
            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[1] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 255.0;
            e->animation.low_limit = 0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[2] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 220.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[3] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 200.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = (show2 + show4) / 2;
            e->pixel.green = show3 / 6;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Chasing_Spirals(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[0] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.scale_z = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_x = 0.1;
            e->animation.offset_x = e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[1] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial_filter = (radius - e->distance[x][y]) / radius;

            e->pixel.red = 3 * show1 * radial_filter;
            e->pixel.green = show2 * radial_filter / 2;
            e->pixel.blue = show3 * radial_filter / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

inline void Rings(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = 5;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.scale_z = 1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 10;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 12;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2 / 4;
            e->pixel.blue = show3 / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 2;
    e->timings.ratio[1] = 2.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.dist = e->distance[x][y];
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = show2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Center_Field(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 5 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 4 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Distance_Experiment(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.2;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.012;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = fl::powf(e->distance[x][y], 0.5);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = fl::powf(e->distance[x][y], 0.6);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 3 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.002;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.004;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_y = show1 / 20.0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.offset_x = show2 / 20.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.offset_y = show3 / 20.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;

            e->pixel.red = show1 * (y + 1) / e->num_y;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;
            if (e->distance[x][y] > radius) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Lava1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 4;
    e->timings.ratio[1] = 1;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show1 / 100;
            e->animation.offset_y += show1 / 100;
            float show2 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[2];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show2 / 100;
            e->animation.offset_y += show2 / 100;
            float show3 = e->render_value(e->animation);

            float linear = (y) / (e->num_y - 1.f);

            e->pixel.red = linear * show2;
            e->pixel.green = 0.1 * linear * (show2 - show3);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Scaledemo1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.000001;
    e->timings.ratio[0] = 0.4;
    e->timings.ratio[1] = 0.32;
    e->timings.ratio[2] = 0.10;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = 0.3 * e->distance[x][y] * 0.8;
            e->animation.angle = 3 * e->polar_theta[x][y] + e->move.radial[2];
            e->animation.scale_x = 0.1 + (e->move.noise_angle[0]) / 10;
            e->animation.scale_y = 0.1 + (e->move.noise_angle[1]) / 10;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.offset_z = 100 * e->move.linear[0];
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 3;
            float show2 = e->render_value(e->animation);

            float dist = 1;
            e->pixel.red = show1 * dist;
            e->pixel.green = (show1 - show2) * dist * 0.3;
            e->pixel.blue = (show2 - show1) * dist;

            if (e->distance[x][y] > 16) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Yves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->a = fl::micros();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 3;
    e->timings.ratio[1] = 2;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.13;
    e->timings.ratio[4] = 0.15;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[5];
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[6];
            ;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + show1 / 100 +
                                 e->move.noise_angle[3] + e->move.noise_angle[4];
            e->animation.dist = e->distance[x][y] + show2 / 50;
            e->animation.offset_y = -e->move.linear[2];

            e->animation.offset_y += show1 / 100;
            e->animation.offset_x += show2 / 100;

            float show3 = e->render_value(e->animation);

            e->animation.offset_y = 0;
            e->animation.offset_x = 0;

            float show4 = e->render_value(e->animation);

            e->pixel.red = show3;
            e->pixel.green = show3 * show4 / 255;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0011;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.ratio[7] = 0.021;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.05;
    e->timings.ratio[6] = 0.055;
    e->timings.ratio[7] = 0.06;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            e->animation.dist = e->distance[x][y] * 0.8;
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Hot_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            float linear = 1;

            e->pixel.red = show1 * linear;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Slow_Fade(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.00005;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist =
                fl::sqrtf(e->distance[x][y]) * 0.7 * (e->move.directional[0] + 1.5);
            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[0] + e->distance[x][y] / 5;

            e->animation.scale_x = 0.11;
            e->animation.scale_y = 0.11;

            e->animation.offset_y = -50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -0.1;
            e->animation.high_limit = 1;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[0] / 10;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[1] / 10;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * (show1 - show2) / 6;
            e->pixel.blue = radial * (show1 - show3) / 5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Polar_Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.5;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y]);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[0];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[0];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = e->move.linear[0];

            float show1 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[1];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[1];
            e->animation.offset_x = e->move.linear[1];

            float show2 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[2];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[2];
            e->animation.offset_x = e->move.linear[2];

            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.2;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 - show3);
            e->pixel.green = radial * (show2 - show1);
            e->pixel.blue = radial * (show3 - show2);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Big_Caleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[0] +
                                 e->animation.dist * 0.1;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 50 * e->move.noise_angle[0];
            e->animation.offset_y = 50 * e->move.noise_angle[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[1] +
                                 e->animation.dist * 0.15;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 50 * e->move.noise_angle[1];
            e->animation.offset_y = 50 * e->move.noise_angle[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 5;
            e->animation.z = 5;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[2];
            e->animation.offset_x = 10 * e->move.noise_angle[2];
            e->animation.offset_y = 10 * e->move.noise_angle[3];
            float show3 = e->render_value(e->animation);

            e->animation.angle = 15;
            e->animation.z = 15;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[3];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.offset_y = 10 * e->move.noise_angle[4];
            float show4 = e->render_value(e->animation);

            e->animation.angle = 2;
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 10 * e->move.linear[4];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.offset_y = 10 * e->move.noise_angle[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 - show4;
            e->pixel.green = show2 - show5;
            e->pixel.blue = show3 - show2 + show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x / 2; x++) {
        for (int y = 0; y < e->num_y / 2; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 150 * e->move.directional[0];
            e->animation.offset_y = 150 * e->move.directional[1];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 4 * e->move.noise_angle[1];
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 150 * e->move.directional[1];
            e->animation.offset_y = 150 * e->move.directional[2];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[2];
            e->animation.z = 25;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[2];
            e->animation.offset_x = 150 * e->move.directional[2];
            e->animation.offset_y = 150 * e->move.directional[3];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[3];
            e->animation.z = 35;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[3];
            e->animation.offset_x = 150 * e->move.directional[3];
            e->animation.offset_y = 150 * e->move.directional[4];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[4];
            e->animation.z = 45;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.offset_z = 50 * e->move.linear[4];
            e->animation.offset_x = 150 * e->move.directional[4];
            e->animation.offset_y = 150 * e->move.directional[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show3 + show4;
            e->pixel.blue = show5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);

            e->setPixelColorInternal((e->num_x - 1) - x, y, e->pixel);
            e->setPixelColorInternal((e->num_x - 1) - x, (e->num_y - 1) - y, e->pixel);
            e->setPixelColorInternal(x, (e->num_y - 1) - y, e->pixel);
        }
    }
}
inline void SpiralMatrix2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = show3;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 20;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 20;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 18;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 18;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 19;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 19;
            e->animation.low_limit = 0.3;
            e->animation.high_limit = 1;
            e->show5 = e->render_value(e->animation);

            e->pixel.red = e->show4;
            e->pixel.green = e->show3;
            e->pixel.blue = e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -40 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->add(e->show2, e->show1);
            e->pixel.green = 0;
            e->pixel.blue = e->colordodge(e->show2, e->show1);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * e->add(show1, show4);
            e->pixel.green = radial * e->colordodge(show2, show5);
            e->pixel.blue = radial * e->screen(show3, show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->screen(show1, show4);
            e->show8 = e->colordodge(show2, show5);
            e->show9 = e->screen(show3, show6);

            e->pixel.red = radial * (e->show7 + e->show8);
            e->pixel.green = 0;
            e->pixel.blue = radial * e->show9;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.01;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 0;
            e->animation.offset_y = 50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 150;
            e->animation.offset_x = -50 * e->move.linear[0];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 550;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = -50 * e->move.linear[1];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 1250;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = 50 * e->move.linear[1];
            float show5 = e->render_value(e->animation);

            e->show3 = e->add(show1, show2);
            e->show6 = e->screen(show4, show5);

            e->pixel.red = e->show3;
            e->pixel.green = 0;
            e->pixel.blue = e->show6;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            float linear1 = y / 32.f;
            float linear2 = (32 - y) / 32.f;

            e->pixel.red = e->show5 * linear1;
            e->pixel.green = 0;
            e->pixel.blue = e->show6 * linear2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.006;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float scale = 0.6;

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            e->pixel.red = (e->show5 + e->show6) / 2;
            e->pixel.green = (e->show5 - 50) + (e->show6 / 16);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    float size = 0.5;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size;
            e->animation.scale_y = 0.05 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size;
            e->animation.scale_y = 0.09 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[5] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.4 + e->move.directional[0] * 0.1;

    float q = 2;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                e->animation.dist / (((e->move.directional[0] + 3) * 2)) +
                e->move.noise_angle[0] * q;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size * (e->move.directional[0] + 1.5);
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 10 * e->move.radial[1] +
                e->animation.dist / (((e->move.directional[1] + 3) * 2)) +
                e->move.noise_angle[1] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size * (e->move.directional[1] + 1.1);
            e->animation.scale_y = 0.07 * size * (e->move.directional[2] + 1.3);
            ;
            e->animation.offset_z = -12 * e->move.linear[1];
            ;
            e->animation.offset_x = -(e->num_x - 1) * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                e->animation.dist / (((e->move.directional[3] + 3) * 2)) +
                e->move.noise_angle[2] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size * (e->move.directional[3] + 1.5);
            ;
            e->animation.scale_y = 0.05 * size * (e->move.directional[4] + 1.5);
            ;
            e->animation.offset_z = -12 * e->move.linear[3];
            e->animation.offset_x = -40 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3) - e->show2;
            e->show6 = e->colordodge(e->show4, e->show1);

            e->show7 = e->multiply(e->show1, e->show2);

            float linear1 = y / 32.f;

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->multiply(e->show1, e->show2) * linear1 * 2;
            e->show8 = e->subtract(e->show7, e->show5);

            e->pixel.green = 0.2 * e->show8;
            e->pixel.blue = e->show5 * radial;
            e->pixel.red = (1 * e->show1 + 1 * e->show2) - e->show7 / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    float q = 1;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.3;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[0] -
                e->animation.dist / (3 + e->move.directional[0] * 0.5);
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size + (e->move.directional[0] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[1] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[1] +
                e->animation.dist / (3 + e->move.directional[1] * 0.5);
            e->animation.z = 50;
            e->animation.scale_x = 0.08 * size + (e->move.directional[1] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[2] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 500;
            e->animation.scale_x = 0.2 * size;
            e->animation.scale_y = 0.2 * size;
            e->animation.offset_z = 0;
            e->animation.offset_y = +7 * e->move.linear[3] + e->move.noise_angle[3];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show5 = ((e->show1 + e->show2)) - e->show3;
            if (e->show5 > 255)
                e->show5 = 255;
            if (e->show5 < 0)
                e->show5 = 0;

            e->show6 = e->colordodge(e->show1, e->show2);

            e->pixel.red = e->show5 * radial;
            e->pixel.blue = (64 - e->show5 - e->show3) * radial;
            e->pixel.green = 0.5 * (e->show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.8;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle = 10 * e->move.radial[6] +
                                 50 * e->move.directional[5] * e->polar_theta[x][y] -
                                 e->animation.dist / 3;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = -0.5;
            e->show1 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = e->show1 * radial;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10 * e->move.noise_angle[0];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[1];
            e->animation.z = 500;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_y = 10 * e->move.noise_angle[1];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.037;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.1;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] +
                4 * fl::sinf(e->move.directional[5] * PI + (float)x / 2) +
                4 * fl::cosf(e->move.directional[6] * PI + float(y) / 2);
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10;
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->pixel.blue = (0.7 * e->show2 + 0.6 * e->show3 + 0.5 * e->show4);
            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Parametric_Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.15;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 4;
            float f = 10 + 2 * e->move.directional[0];

            e->animation.dist = (f + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[3]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[3] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 2000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[3];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show5 = e->render_value(e->animation);

            e->show6 = e->screen(e->show4, e->show5);
            e->show7 = e->screen(e->show2, e->show3);

            float radius = 40;
            float radial = (radius - e->distance[x][y]) / radius;

            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;
            e->pixel.blue = (0.3 * e->show6 + 0.7 * e->show7) * radial;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] + 20 * e->move.directional[0];
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 0;
            e->pixel.green = 0;
            e->pixel.blue = e->show1;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (16 + e->move.directional[0] * 16);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (12 + e->move.directional[3] * 4);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0.1 * e->move.linear[0];

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = 40 - show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.7;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[3];
            e->animation.offset_y = -20 * e->move.linear[3];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.9;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[4];
            e->animation.offset_y = -20 * e->move.linear[4];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->pixel.red = e->show1 - e->show2 - e->show3;
            e->pixel.blue = e->show2 - e->show1 - e->show3;
            e->pixel.green = e->show3 - e->show1 - e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1.5;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.5 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.7;

    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 30;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment7(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 50;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            e->pixel.red = rad * ((e->show1 + e->show2) + e->show3);
            e->pixel.green = (((e->show2 + e->show3) * 0.8) - 90) * rad;
            e->pixel.blue = e->show4 * 0.2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;

    float w = 0.3;

    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 5;
            e->animation.scale_x = 0.001;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 20;
            e->animation.offset_z = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 10 * e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 1;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            CHSV(rad * ((e->show1 + e->show2) + e->show3), 255, 255);

            e->pixel = e->rgb_sanity_check(e->pixel);

            fl::u8 a = e->getTime() / 100;
            CRGB p = CRGB(CHSV(((a + e->show1 + e->show2) + e->show3), 255, 255));
            rgb pixel;
            pixel.red = p.red;
            pixel.green = p.green;
            pixel.blue = p.blue;
            e->setPixelColorInternal(x, y, pixel);
        }
    }
}
inline void Fluffy_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;

    e->timings.master_speed = 0.015;
    float size = 0.15;
    float radial_speed = 1;
    float linear_speed = 5;

    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.026;
    e->timings.ratio[2] = 0.027;
    e->timings.ratio[3] = 0.028;
    e->timings.ratio[4] = 0.029;
    e->timings.ratio[5] = 0.030;
    e->timings.ratio[6] = 0.031;
    e->timings.ratio[7] = 0.032;
    e->timings.ratio[8] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[0]);
            e->animation.z = 5;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = linear_speed * e->move.linear[0];
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[1]);
            e->animation.offset_y = linear_speed * e->move.linear[1];
            e->animation.offset_z = 200;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[2]);
            e->animation.offset_y = linear_speed * e->move.linear[2];
            e->animation.offset_z = 400;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show3 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[3]);
            e->animation.offset_y = linear_speed * e->move.linear[3];
            e->animation.offset_z = 600;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show4 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[4]);
            e->animation.offset_y = linear_speed * e->move.linear[4];
            e->animation.offset_z = 800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show5 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[5]);
            e->animation.offset_y = linear_speed * e->move.linear[5];
            e->animation.offset_z = 1800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show6 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[6]);
            e->animation.offset_y = linear_speed * e->move.linear[6];
            e->animation.offset_z = 2800;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show7 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[7]);
            e->animation.offset_y = linear_speed * e->move.linear[7];
            e->animation.offset_z = 3800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show8 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[8]);
            e->animation.offset_y = linear_speed * e->move.linear[8];
            e->animation.offset_z = 4800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show9 = e->render_value(e->animation);

            e->pixel.red = 0.8 * (e->show1 + e->show2 + e->show3) + (e->show4 + e->show5 + e->show6);
            e->pixel.green = 0.8 * (e->show4 + e->show5 + e->show6);
            e->pixel.blue = 0.3 * (e->show7 + e->show8 + e->show9);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}




// Namespace wrappers for backwards compatibility with test code

namespace q31 {
    using fl::Chasing_Spirals_Q31;
    using fl::Chasing_Spirals_Q31_SIMD;
}

namespace q16 {
    // Q16 implementation aliased to Q31 (Q16 was removed, use Q31 instead)
    inline void Chasing_Spirals_Q16_Batch4_ColorGrouped(Context &ctx) {
        fl::Chasing_Spirals_Q31(ctx);
    }
}


#if FL_ANIMARTRIX_USES_FAST_MATH
FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
#endif

}  // namespace fl
