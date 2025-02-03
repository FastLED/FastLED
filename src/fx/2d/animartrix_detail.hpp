#pragma once

/*
  ___        _            ___  ______ _____    _
 / _ \      (_)          / _ \ | ___ \_   _|  (_)
/ /_\ \_ __  _ _ __ ___ / /_\ \| |_/ / | |_ __ ___  __
|  _  | '_ \| | '_ ` _ \|  _  ||    /  | | '__| \ \/ /
| | | | | | | | | | | | | | | || |\ \  | | |  | |>  <
\_| |_/_| |_|_|_| |_| |_\_| |_/\_| \_| \_/_|  |_/_/\_\

by Stefan Petrick 2023.

High quality LED animations for your next project.

This is a Shader and 5D Coordinate Mapper made for realtime
rendering of generative animations & artistic dynamic visuals.

This is also a modular animation synthesizer with waveform
generators, oscillators, filters, modulators, noise generators,
compressors... and much more.

VO.42 beta version

This code is licenced under a Creative Commons Attribution
License CC BY-NC 3.0

*/

#include "fl/vector.h"
#include <math.h>  // ok include
#include <stdint.h>

#ifndef ANIMARTRIX_INTERNAL
#error "This file is not meant to be included directly. Include animartrix.hpp instead."
#endif

// Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Licensed under the Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/
// This header is distributed with FastLED but has a different license that limits commercial use.
// If you include this high quality LED animation library in your project, you must agree to the licensing terms.
// It is not included by FastLED by default, you must include it manually.
// Setting FASTLED_ANIMARTRIX_LICENSING_AGREEMENT=1 will indicate that you agree to the licensing terms of the ANIMartRIX library for non commercial use only.
//
// Like the rest of FastLED, this header is free for non-commercial use and licensed under the Creative Commons Attribution License CC BY-NC 3.0.
// If you are just making art, then by all means do what you want with this library and you can stop reading now.
// If you are using this header for commercial purposes, then you need to contact Stefan Petrick for a commercial use license.




#include "fl/force_inline.h"
#include "crgb.h"
#include "fl/namespace.h"

// Setting this to 1 means you agree to the licensing terms of the ANIMartRIX library for non commercial use only.
#if defined(FASTLED_ANIMARTRIX_LICENSING_AGREEMENT) || (FASTLED_ANIMARTRIX_LICENSING_AGREEMENT != 0)
#warning "Warning: Non-standard license. This fx header is separate from the FastLED driver and carries different licensing terms. On the plus side, IT'S FUCKING AMAZING. ANIMartRIX: free for non-commercial use and licensed under the Creative Commons Attribution License CC BY-NC-SA 4.0. If you'd like to purchase a commercial use license please contact Stefan Petrick. Github: github.com/StefanPetrick/animartrix Reddit: reddit.com/user/StefanPetrick/ Modified by github.com/netmindz for class portability. Ported into FastLED by Zach Vorhies."
#endif  // 


#ifndef PI
#define PI         3.1415926535897932384626433832795
#endif

#ifdef ANIMARTRIX_PRINT_USES_SERIAL
#define ANIMARTRIX_PRINT_USES_SERIAL(S) Serial.print(S)
#else
#define ANIMARTRIX_PRINT(S) (void)(S)
#endif

#define num_oscillators 10

namespace animartrix_detail {
FASTLED_USING_NAMESPACE

struct render_parameters {

    // TODO float center_x = (num_x / 2) - 0.5;   // center of the matrix
    // TODO float center_y = (num_y / 2) - 0.5;
    float center_x = (999 / 2) - 0.5; // center of the matrix
    float center_y = (999 / 2) - 0.5;
    float dist, angle;
    float scale_x = 0.1; // smaller values = zoom in
    float scale_y = 0.1;
    float scale_z = 0.1;
    float offset_x, offset_y, offset_z;
    float z;
    float low_limit = 0; // getting contrast by highering the black point
    float high_limit = 1;
};


struct oscillators {

    float master_speed; // global transition speed
    float
        offset[num_oscillators];  // oscillators can be shifted by a time offset
    float ratio[num_oscillators]; // speed ratios for the individual oscillators
};


struct modulators {

    float linear[num_oscillators];      // returns 0 to FLT_MAX
    float radial[num_oscillators];      // returns 0 to 2*PI
    float directional[num_oscillators]; // returns -1 to 1
    float noise_angle[num_oscillators]; // returns 0 to 2*PI
};




struct rgb {
    float red, green, blue;
};



static const uint8_t PERLIN_NOISE[] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,
    225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190,
    6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117,
    35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136,
    171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158,
    231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,
    245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209,
    76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,
    202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,
    58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,
    154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253,
    19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
    228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,
    145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184,
    84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156,
    180};

FASTLED_FORCE_INLINE uint8_t P(uint8_t x) {
    const uint8_t idx = x & 255;
    const uint8_t* ptr = PERLIN_NOISE + idx;
    return *ptr;
}

class ANIMartRIX {

  public:
    int num_x; // how many LEDs are in one row?
    int num_y; // how many rows?

    float speed_factor = 1; // 0.1 to 10

    float radial_filter_radius = 23.0; // on 32x32, use 11 for 16x16

    bool serpentine;

    render_parameters animation; // all animation parameters in one place
    oscillators timings; // all speed settings in one place
    modulators move; // all oscillator based movers and shifters at one place
    rgb pixel;

    fl::HeapVector<fl::HeapVector<float>>
        polar_theta; // look-up table for polar angles
    fl::HeapVector<fl::HeapVector<float>>
        distance; // look-up table for polar distances

    unsigned long a, b, c; // for time measurements

    float show1, show2, show3, show4, show5, show6, show7, show8, show9, show0;

    ANIMartRIX() {}

    ANIMartRIX(int w, int h) { this->init(w, h); }

    virtual ~ANIMartRIX() {}

    virtual uint16_t xyMap(uint16_t x, uint16_t y) = 0;

    uint32_t currentTime = 0;
    void setTime(uint32_t t) { currentTime = t; }
    uint32_t getTime() { return currentTime ? currentTime : millis(); }


    void init(int w, int h) {
        animation = render_parameters();
        timings = oscillators();
        move = modulators();
        pixel = rgb();

        this->num_x = w;
        this->num_y = h;
        if (w <= 16) {
            this->radial_filter_radius = 11;
        } else {
            this->radial_filter_radius = 23; // on 32x32, use 11 for 16x16
        }
        render_polar_lookup_table(
            (num_x / 2) - 0.5,
            (num_y / 2) - 0.5); // precalculate all polar coordinates
                                // polar origin is set to matrix centre
        // set default speed ratio for the oscillators, not all effects set their own, so start from know state
        timings.master_speed = 0.01;
    }

    /**
     * @brief Set the Speed Factor 0.1 to 10 - 1 for original speed
     *
     * @param speed
     */
    void setSpeedFactor(float speed) { this->speed_factor = speed; }

    // Dynamic darkening methods:

    float subtract(float &a, float &b) { return a - b; }

    float multiply(float &a, float &b) { return a * b / 255.f; }

    // makes low brightness darker
    // sets the black point high = more contrast
    // animation.low_limit should be 0 for best results
    float colorburn(float &a, float &b) {

        return (1 - ((1 - a / 255.f) / (b / 255.f))) * 255.f;
    }

    // Dynamic brightening methods

    float add(float &a, float &b) { return a + b; }

    // makes bright even brighter
    // reduces contrast
    float screen(float &a, float &b) {

        return (1 - (1 - a / 255.f) * (1 - b / 255.f)) * 255.f;
    }

    float colordodge(float &a, float &b) { return (a / (255.f - b)) * 255.f; }
    /*
     Ken Perlins improved noise   -  http://mrl.nyu.edu/~perlin/noise/
     C-port:  http://www.fundza.com/c4serious/noise/perlin/perlin.html
     by Malcolm Kesson;   arduino port by Peter Chiochetti, Sep 2007 :
     -  make permutation constant byte, obsoletes init(), lookup % 256
    */

    float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    float lerp(float t, float a, float b) { return a + t * (b - a); }
    float grad(int hash, float x, float y, float z) {
        int h = hash & 15;       /* CONVERT LO 4 BITS OF HASH CODE */
        float u = h < 8 ? x : y, /* INTO 12 GRADIENT DIRECTIONS.   */
            v = h < 4                ? y
                : h == 12 || h == 14 ? x
                                     : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }


    float pnoise(float x, float y, float z) {

        int X = (int)floorf(x) & 255, /* FIND UNIT CUBE THAT */
            Y = (int)floorf(y) & 255, /* CONTAINS POINT.     */
            Z = (int)floorf(z) & 255;
        x -= floorf(x); /* FIND RELATIVE X,Y,Z */
        y -= floorf(y); /* OF POINT IN CUBE.   */
        z -= floorf(z);
        float u = fade(x), /* COMPUTE FADE CURVES */
            v = fade(y),   /* FOR EACH OF X,Y,Z.  */
            w = fade(z);
        int A = P(X) + Y, AA = P(A) + Z,
            AB = P(A + 1) + Z, /* HASH COORDINATES OF */
            B = P(X + 1) + Y, BA = P(B) + Z,
            BB = P(B + 1) + Z; /* THE 8 CUBE CORNERS, */

        return lerp(w,
                    lerp(v,
                         lerp(u, grad(P(AA), x, y, z),        /* AND ADD */
                              grad(P(BA), x - 1, y, z)),      /* BLENDED */
                         lerp(u, grad(P(AB), x, y - 1, z),    /* RESULTS */
                              grad(P(BB), x - 1, y - 1, z))), /* FROM  8 */
                    lerp(v,
                         lerp(u, grad(P(AA + 1), x, y, z - 1),   /* CORNERS */
                              grad(P(BA + 1), x - 1, y, z - 1)), /* OF CUBE */
                         lerp(u, grad(P(AB + 1), x, y - 1, z - 1),
                              grad(P(BB + 1), x - 1, y - 1, z - 1))));
    }

    void calculate_oscillators(oscillators &timings) {

        double runtime = getTime() * timings.master_speed *
                         speed_factor; // global anaimation speed

        for (int i = 0; i < num_oscillators; i++) {

            move.linear[i] =
                (runtime + timings.offset[i]) *
                timings.ratio[i]; // continously rising offsets, returns 0 to
                                  // max_float

            move.radial[i] = fmodf(move.linear[i],
                                   2 * PI); // angle offsets for continous
                                            // rotation, returns    0 to 2 * PI

            move.directional[i] =
                sinf(move.radial[i]); // directional offsets or factors, returns
                                      // -1 to 1

            move.noise_angle[i] =
                PI *
                (1 +
                 pnoise(move.linear[i], 0,
                        0)); // noise based angle offset, returns 0 to 2 * PI
        }
    }

    void run_default_oscillators(float master_speed = 0.005) {        
        timings.master_speed = master_speed;

        timings.ratio[0] = 1; // speed ratios for the oscillators, higher values
                              // = faster transitions
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

        calculate_oscillators(timings);
    }

    // Convert the 2 polar coordinates back to cartesian ones & also apply all
    // 3d transitions. Calculate the noise value at this point based on the 5
    // dimensional manipulation of the underlaying coordinates.

    float render_value(render_parameters &animation) {

        // convert polar coordinates back to cartesian ones

        float newx = (animation.offset_x + animation.center_x -
                      (cosf(animation.angle) * animation.dist)) *
                     animation.scale_x;
        float newy = (animation.offset_y + animation.center_y -
                      (sinf(animation.angle) * animation.dist)) *
                     animation.scale_y;
        float newz = (animation.offset_z + animation.z) * animation.scale_z;

        // render noisevalue at this new cartesian point

        float raw_noise_field_value = pnoise(newx, newy, newz);

        // A) enhance histogram (improve contrast) by setting the black and
        // white point (low & high_limit) B) scale the result to a 0-255 range
        // (assuming you want 8 bit color depth per rgb chanel) Here happens the
        // contrast boosting & the brightness mapping

        if (raw_noise_field_value < animation.low_limit)
            raw_noise_field_value = animation.low_limit;
        if (raw_noise_field_value > animation.high_limit)
            raw_noise_field_value = animation.high_limit;

        float scaled_noise_value =
            map_float(raw_noise_field_value, animation.low_limit,
                      animation.high_limit, 0, 255);

        return scaled_noise_value;
    }

    // given a static polar origin we can precalculate
    // the polar coordinates

    void render_polar_lookup_table(float cx, float cy) {
        polar_theta.resize(num_x, fl::HeapVector<float>(num_y, 0.0f));
        distance.resize(num_x, fl::HeapVector<float>(num_y, 0.0f));

        for (int xx = 0; xx < num_x; xx++) {
            for (int yy = 0; yy < num_y; yy++) {

                float dx = xx - cx;
                float dy = yy - cy;

                distance[xx][yy] = hypotf(dx, dy);
                polar_theta[xx][yy] = atan2f(dy, dx);
            }
        }
    }

    // float mapping maintaining 32 bit precision
    // we keep values with high resolution for potential later usage

    float map_float(float x, float in_min, float in_max, float out_min,
                    float out_max) {

        float result =
            (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
        if (result < out_min)
            result = out_min;
        if (result > out_max)
            result = out_max;

        return result;
    }

    /* unnecessary bloat

    // check result after colormapping and store the newly rendered rgb data

    void write_pixel_to_framebuffer(int x, int y, rgb &pixel) {

          // assign the final color of this one pixel
          CRGB finalcolor = CRGB(pixel.red, pixel.green, pixel.blue);

          // write the rendered pixel into the framebutter
          leds[xyMap(x, y)] = finalcolor;
    }
    */

    // Avoid any possible color flicker by forcing the raw RGB values to be
    // 0-255. This enables to play freely with random equations for the
    // colormapping without causing flicker by accidentally missing the valid
    // target range.

    rgb rgb_sanity_check(rgb &pixel) {

        // rescue data if possible, return absolute value
        // if (pixel.red < 0)     pixel.red = fabsf(pixel.red);
        // if (pixel.green < 0) pixel.green = fabsf(pixel.green);
        // if (pixel.blue < 0)   pixel.blue = fabsf(pixel.blue);

        // Can never be negative colour
        if (pixel.red < 0)
            pixel.red = 0;
        if (pixel.green < 0)
            pixel.green = 0;
        if (pixel.blue < 0)
            pixel.blue = 0;

        // discard everything above the valid 8 bit colordepth 0-255 range
        if (pixel.red > 255)
            pixel.red = 255;
        if (pixel.green > 255)
            pixel.green = 255;
        if (pixel.blue > 255)
            pixel.blue = 255;

        return pixel;
    }

    // find the right led index according to you LED matrix wiring

    void get_ready() { // wait until new buffer is ready, measure time
        a = micros();
        logOutput();
    }

    virtual void setPixelColorInternal(int x, int y, rgb pixel) = 0;

    // virtual void setPixelColorInternal(int index, rgb pixel) = 0;

    void logOutput() { b = micros(); }

    void logFrame() { c = micros(); }

    // Show the current framerate, rendered pixels per second,
    // rendering time & time spend to push the data to the leds.
    // in the serial monitor.

    void report_performance() {

        float calc = b - a;                      // waiting time
        float push = c - b;                      // rendering time
        float total = c - a;                     // time per frame
        int fps = 1000000 / total;               // frames per second
        int kpps = (fps * num_x * num_y) / 1000; // kilopixel per second

        ANIMARTRIX_PRINT(fps);
        ANIMARTRIX_PRINT(" fps  ");
        ANIMARTRIX_PRINT(kpps);
        ANIMARTRIX_PRINT(" kpps @");
        ANIMARTRIX_PRINT(num_x * num_y);
        ANIMARTRIX_PRINT(" LEDs  ");
        ANIMARTRIX_PRINT(round(total));
        ANIMARTRIX_PRINT(" µs per frame  waiting: ");
        ANIMARTRIX_PRINT(round((calc * 100) / total));
        ANIMARTRIX_PRINT("%  rendering: ");
        ANIMARTRIX_PRINT(round((push * 100) / total));
        ANIMARTRIX_PRINT("%  (");
        ANIMARTRIX_PRINT(round(calc));
        ANIMARTRIX_PRINT(" + ");
        ANIMARTRIX_PRINT(round(push));
        ANIMARTRIX_PRINT(" µs)  Core-temp: ");
        // TODO ANIMARTRIX_PRINT( tempmonGetTemp() );
        //Serial.println(" °C");
        ANIMARTRIX_PRINT(" °C\n");
    }

    // Effects

    void Rotating_Blob() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 0.1;      // higher values = faster transitions
        timings.ratio[1] = 0.03;
        timings.ratio[2] = 0.03;
        timings.ratio[3] = 0.03;

        timings.offset[1] = 10;
        timings.offset[2] = 20;
        timings.offset[3] = 30;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_x = 0;
                animation.offset_y = 0;
                animation.offset_z = 100;
                animation.angle = polar_theta[x][y] + move.radial[0];
                animation.dist = distance[x][y];
                animation.z = move.linear[0];
                animation.low_limit = -1;
                float show1 = render_value(animation);

                animation.angle =
                    polar_theta[x][y] - move.radial[1] + show1 / 512.0;
                animation.dist = distance[x][y] * show1 / 255.0;
                animation.low_limit = 0;
                animation.z = move.linear[1];
                float show2 = render_value(animation);

                animation.angle =
                    polar_theta[x][y] - move.radial[2] + show1 / 512.0;
                animation.dist = distance[x][y] * show1 / 220.0;
                animation.z = move.linear[2];
                float show3 = render_value(animation);

                animation.angle =
                    polar_theta[x][y] - move.radial[3] + show1 / 512.0;
                animation.dist = distance[x][y] * show1 / 200.0;
                animation.z = move.linear[3];
                float show4 = render_value(animation);

                // colormapping
                pixel.red = (show2 + show4) / 2;
                pixel.green = show3 / 6;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Chasing_Spirals() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 0.1;      // higher values = faster transitions
        timings.ratio[1] = 0.13;
        timings.ratio[2] = 0.16;

        timings.offset[1] = 10;
        timings.offset[2] = 20;
        timings.offset[3] = 30;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.angle =
                    3 * polar_theta[x][y] + move.radial[0] - distance[x][y] / 3;
                animation.dist = distance[x][y];
                animation.scale_z = 0.1;
                animation.scale_y = 0.1;
                animation.scale_x = 0.1;
                animation.offset_x = move.linear[0];
                animation.offset_y = 0;
                animation.offset_z = 0;
                animation.z = 0;
                float show1 = render_value(animation);

                animation.angle =
                    3 * polar_theta[x][y] + move.radial[1] - distance[x][y] / 3;
                animation.dist = distance[x][y];
                animation.offset_x = move.linear[1];
                float show2 = render_value(animation);

                animation.angle =
                    3 * polar_theta[x][y] + move.radial[2] - distance[x][y] / 3;
                animation.dist = distance[x][y];
                animation.offset_x = move.linear[2];
                float show3 = render_value(animation);

                // colormapping
                float radius = radial_filter_radius;
                float radial_filter = (radius - distance[x][y]) / radius;

                pixel.red = 3 * show1 * radial_filter;
                pixel.green = show2 * radial_filter / 2;
                pixel.blue = show3 * radial_filter / 4;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Rings() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 1;        // higher values = faster transitions
        timings.ratio[1] = 1.1;
        timings.ratio[2] = 1.2;

        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.angle = 5;
                animation.scale_x = 0.2;
                animation.scale_y = 0.2;
                animation.scale_z = 1;
                animation.dist = distance[x][y];
                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                float show1 = render_value(animation);

                // describe and render animation layers
                animation.angle = 10;

                animation.dist = distance[x][y];
                animation.offset_y = -move.linear[1];
                float show2 = render_value(animation);

                // describe and render animation layers
                animation.angle = 12;

                animation.dist = distance[x][y];
                animation.offset_y = -move.linear[2];
                float show3 = render_value(animation);

                // colormapping
                pixel.red = show1;
                pixel.green = show2 / 4;
                pixel.blue = show3 / 4;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Waves() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 2;        // higher values = faster transitions
        timings.ratio[1] = 2.1;
        timings.ratio[2] = 1.2;

        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.angle = polar_theta[x][y];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.scale_z = 0.1;
                animation.dist = distance[x][y];
                animation.offset_y = 0;
                animation.offset_x = 0;
                animation.z = 2 * distance[x][y] - move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y];
                animation.dist = distance[x][y];
                animation.z = 2 * distance[x][y] - move.linear[1];
                float show2 = render_value(animation);

                // colormapping
                pixel.red = show1;
                pixel.green = 0;
                pixel.blue = show2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Center_Field() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 1;        // higher values = faster transitions
        timings.ratio[1] = 1.1;
        timings.ratio[2] = 1.2;

        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.angle = polar_theta[x][y];
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.scale_z = 0.1;
                animation.dist = 5 * sqrtf(distance[x][y]);
                animation.offset_y = move.linear[0];
                animation.offset_x = 0;
                animation.z = 0;
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y];
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.scale_z = 0.1;
                animation.dist = 4 * sqrtf(distance[x][y]);
                animation.offset_y = move.linear[0];
                animation.offset_x = 0;
                animation.z = 0;
                float show2 = render_value(animation);

                // colormapping
                pixel.red = show1;
                pixel.green = show2;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Distance_Experiment() {

        get_ready();

        timings.master_speed = 0.01; // speed ratios for the oscillators
        timings.ratio[0] = 0.2;      // higher values = faster transitions
        timings.ratio[1] = 0.13;
        timings.ratio[2] = 0.012;

        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = powf(distance[x][y], 0.5);
                animation.angle = polar_theta[x][y] + move.radial[0];
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.scale_z = 0.1;
                animation.offset_y = move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = 0;
                float show1 = render_value(animation);

                animation.dist = powf(distance[x][y], 0.6);
                animation.angle = polar_theta[x][y] + move.noise_angle[2];
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.scale_z = 0.1;
                animation.offset_y = move.linear[1];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = 0;
                float show2 = render_value(animation);

                // colormapping
                pixel.red = show1 + show2;
                pixel.green = show2;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Caleido1() {

        get_ready();

        timings.master_speed = 0.003; // speed ratios for the oscillators
        timings.ratio[0] = 0.02;      // higher values = faster transitions
        timings.ratio[1] = 0.03;
        timings.ratio[2] = 0.04;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.6;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = distance[x][y] * (2 + move.directional[0]) / 3;
                animation.angle = 3 * polar_theta[x][y] +
                                  3 * move.noise_angle[0] + move.radial[4];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.scale_z = 0.1;
                animation.offset_y = 2 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = move.linear[0];
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[1]) / 3;
                animation.angle = 4 * polar_theta[x][y] +
                                  3 * move.noise_angle[1] + move.radial[4];
                animation.offset_x = 2 * move.linear[1];
                animation.z = move.linear[1];
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[2]) / 3;
                animation.angle = 5 * polar_theta[x][y] +
                                  3 * move.noise_angle[2] + move.radial[4];
                animation.offset_y = 2 * move.linear[2];
                animation.z = move.linear[2];
                float show3 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[3]) / 3;
                animation.angle = 4 * polar_theta[x][y] +
                                  3 * move.noise_angle[3] + move.radial[4];
                animation.offset_x = 2 * move.linear[3];
                animation.z = move.linear[3];
                float show4 = render_value(animation);

                // colormapping
                pixel.red = show1;
                pixel.green = show3 * distance[x][y] / 10;
                pixel.blue = (show2 + show4) / 2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Caleido2() {

        get_ready();

        timings.master_speed = 0.002; // speed ratios for the oscillators
        timings.ratio[0] = 0.02;      // higher values = faster transitions
        timings.ratio[1] = 0.03;
        timings.ratio[2] = 0.04;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.6;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = distance[x][y] * (2 + move.directional[0]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[0] + move.radial[4];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.scale_z = 0.1;
                animation.offset_y = 2 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = move.linear[0];
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[1]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[1] + move.radial[4];
                animation.offset_x = 2 * move.linear[1];
                animation.z = move.linear[1];
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[2]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[2] + move.radial[4];
                animation.offset_y = 2 * move.linear[2];
                animation.z = move.linear[2];
                float show3 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[3]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[3] + move.radial[4];
                animation.offset_x = 2 * move.linear[3];
                animation.z = move.linear[3];
                float show4 = render_value(animation);

                // colormapping
                pixel.red = show1;
                pixel.green = show3 * distance[x][y] / 10;
                pixel.blue = (show2 + show4) / 2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Caleido3() {

        get_ready();

        timings.master_speed = 0.004; // speed ratios for the oscillators
        timings.ratio[0] = 0.02;      // higher values = faster transitions
        timings.ratio[1] = 0.03;
        timings.ratio[2] = 0.04;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.6;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = distance[x][y] * (2 + move.directional[0]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[0] + move.radial[4];
                animation.scale_x = 0.1; // + (move.directional[0] + 2)/100;
                animation.scale_y = 0.1; // + (move.directional[1] + 2)/100;
                animation.scale_z = 0.1;
                animation.offset_y = 2 * move.linear[0];
                animation.offset_x = 2 * move.linear[1];
                animation.offset_z = 0;
                animation.z = move.linear[0];
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[1]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[1] + move.radial[4];
                animation.offset_x = 2 * move.linear[1];
                animation.offset_y = show1 / 20.0;
                animation.z = move.linear[1];
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[2]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[2] + move.radial[4];
                animation.offset_y = 2 * move.linear[2];
                animation.offset_x = show2 / 20.0;
                animation.z = move.linear[2];
                float show3 = render_value(animation);

                animation.dist = distance[x][y] * (2 + move.directional[3]) / 3;
                animation.angle = 2 * polar_theta[x][y] +
                                  3 * move.noise_angle[3] + move.radial[4];
                animation.offset_x = 2 * move.linear[3];
                animation.offset_y = show3 / 20.0;
                animation.z = move.linear[3];
                float show4 = render_value(animation);

                // colormapping
                float radius = radial_filter_radius; // radial mask

                pixel.red = show1 * (y + 1) / num_y;
                pixel.green = show3 * distance[x][y] / 10;
                pixel.blue = (show2 + show4) / 2;
                if (distance[x][y] > radius) {
                    pixel.red = 0;
                    pixel.green = 0;
                    pixel.blue = 0;
                }

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Lava1() {

        get_ready();

        timings.master_speed = 0.0015; // speed ratios for the oscillators
        timings.ratio[0] = 4;          // higher values = faster transitions
        timings.ratio[1] = 1;
        timings.ratio[2] = 1;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.6;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = distance[x][y] * 0.8;
                animation.angle = polar_theta[x][y];
                animation.scale_x = 0.15; // + (move.directional[0] + 2)/100;
                animation.scale_y = 0.12; // + (move.directional[1] + 2)/100;
                animation.scale_z = 0.01;
                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = 30;
                float show1 = render_value(animation);

                animation.offset_y = -move.linear[1];
                animation.scale_x = 0.15; // + (move.directional[0] + 2)/100;
                animation.scale_y = 0.12; // + (move.directional[1] + 2)/100;
                animation.offset_x = show1 / 100;
                animation.offset_y += show1 / 100;

                float show2 = render_value(animation);

                animation.offset_y = -move.linear[2];
                animation.scale_x = 0.15; // + (move.directional[0] + 2)/100;
                animation.scale_y = 0.12; // + (move.directional[1] + 2)/100;
                animation.offset_x = show2 / 100;
                animation.offset_y += show2 / 100;

                float show3 = render_value(animation);

                // colormapping
                float linear = (y) / (num_y - 1.f); // radial mask

                pixel.red = linear * show2;
                pixel.green = 0.1 * linear * (show2 - show3);
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Scaledemo1() {

        get_ready();

        timings.master_speed = 0.000001; // speed ratios for the oscillators
        timings.ratio[0] = 0.4;         // higher values = faster transitions
        timings.ratio[1] = 0.32;
        timings.ratio[2] = 0.10;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.6;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // describe and render animation layers
                animation.dist = 0.3 * distance[x][y] * 0.8;
                animation.angle = 3 * polar_theta[x][y] + move.radial[2];
                animation.scale_x = 0.1 + (move.noise_angle[0]) / 10;
                animation.scale_y =
                    0.1 + (move.noise_angle[1]) /
                              10; // + (move.directional[1] + 2)/100;
                animation.scale_z = 0.01;
                animation.offset_y = 0;
                animation.offset_x = 0;
                animation.offset_z = 100 * move.linear[0];
                animation.z = 30;
                float show1 = render_value(animation);

                animation.angle = 3;
                float show2 = render_value(animation);

                float dist = 1; //(10-distance[x][y])/ 10;
                pixel.red = show1 * dist;
                pixel.green = (show1 - show2) * dist * 0.3;
                pixel.blue = (show2 - show1) * dist;

                if (distance[x][y] > 16) {
                    pixel.red = 0;
                    pixel.green = 0;
                    pixel.blue = 0;
                }

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Yves() {

        get_ready();

        a = micros(); // for time measurement in report_performance()

        timings.master_speed = 0.001; // speed ratios for the oscillators
        timings.ratio[0] = 3;         // higher values = faster transitions
        timings.ratio[1] = 2;
        timings.ratio[2] = 1;
        timings.ratio[3] = 0.13;
        timings.ratio[4] = 0.15;
        timings.ratio[5] = 0.03;
        timings.ratio[6] = 0.025;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;
        timings.offset[5] = 500;
        timings.offset[6] = 600;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle =
                    polar_theta[x][y] + 2 * PI + move.noise_angle[5];
                animation.scale_x = 0.08;
                animation.scale_y = 0.08;
                animation.scale_z = 0.08;
                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = 0;
                float show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle =
                    polar_theta[x][y] + 2 * PI + move.noise_angle[6];
                ;
                animation.scale_x = 0.08;
                animation.scale_y = 0.08;
                animation.scale_z = 0.08;
                animation.offset_y = -move.linear[1];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = 0;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + show1 / 100 +
                                  move.noise_angle[3] + move.noise_angle[4];
                animation.dist = distance[x][y] + show2 / 50;
                animation.offset_y = -move.linear[2];

                animation.offset_y += show1 / 100;
                animation.offset_x += show2 / 100;

                float show3 = render_value(animation);

                animation.offset_y = 0;
                animation.offset_x = 0;

                float show4 = render_value(animation);

                pixel.red = show3;
                pixel.green = show3 * show4 / 255;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Spiralus() {

        get_ready();

        timings.master_speed = 0.0011; // speed ratios for the oscillators
        timings.ratio[0] = 1.5;        // higher values = faster transitions
        timings.ratio[1] = 2.3;
        timings.ratio[2] = 3;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.2;
        timings.ratio[5] = 0.03;
        timings.ratio[6] = 0.025;
        timings.ratio[7] = 0.021;
        timings.ratio[8] = 0.027;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;
        timings.offset[5] = 500;
        timings.offset[6] = 600;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 2 * polar_theta[x][y] + move.noise_angle[5] +
                                  move.directional[3] * move.noise_angle[6] *
                                      animation.dist / 10;
                animation.scale_x = 0.08;
                animation.scale_y = 0.08;
                animation.scale_z = 0.02;
                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = move.linear[1];
                float show1 = render_value(animation);

                animation.angle = 2 * polar_theta[x][y] + move.noise_angle[7] +
                                  move.directional[5] * move.noise_angle[8] *
                                      animation.dist / 10;
                animation.offset_y = -move.linear[1];
                animation.z = move.linear[2];

                float show2 = render_value(animation);

                animation.angle = 2 * polar_theta[x][y] + move.noise_angle[6] +
                                  move.directional[6] * move.noise_angle[7] *
                                      animation.dist / 10;
                animation.offset_y = move.linear[2];
                animation.z = move.linear[0];
                float show3 = render_value(animation);

                float f = 1;

                pixel.red = f * (show1 + show2);
                pixel.green = f * (show1 - show2);
                pixel.blue = f * (show3 - show1);

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Spiralus2() {

        get_ready();

        timings.master_speed = 0.0015; // speed ratios for the oscillators
        timings.ratio[0] = 1.5;        // higher values = faster transitions
        timings.ratio[1] = 2.3;
        timings.ratio[2] = 3;
        timings.ratio[3] = 0.05;
        timings.ratio[4] = 0.2;
        timings.ratio[5] = 0.05;
        timings.ratio[6] = 0.055;
        timings.ratio[7] = 0.06;
        timings.ratio[8] = 0.027;
        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;
        timings.offset[5] = 500;
        timings.offset[6] = 600;

        calculate_oscillators(
            timings); // get linear movers and oscillators going

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] + move.noise_angle[5] +
                                  move.directional[3] * move.noise_angle[6] *
                                      animation.dist / 10;
                animation.scale_x = 0.08;
                animation.scale_y = 0.08;
                animation.scale_z = 0.02;
                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;
                animation.z = move.linear[1];
                float show1 = render_value(animation);

                animation.angle = 6 * polar_theta[x][y] + move.noise_angle[7] +
                                  move.directional[5] * move.noise_angle[8] *
                                      animation.dist / 10;
                animation.offset_y = -move.linear[1];
                animation.z = move.linear[2];

                float show2 = render_value(animation);

                animation.angle = 6 * polar_theta[x][y] + move.noise_angle[6] +
                                  move.directional[6] * move.noise_angle[7] *
                                      animation.dist / 10;
                animation.offset_y = move.linear[2];
                animation.z = move.linear[0];
                animation.dist = distance[x][y] * 0.8;
                float show3 = render_value(animation);

                float f = 1; //(24-distance[x][y])/24;

                pixel.red = f * (show1 + show2);
                pixel.green = f * (show1 - show2);
                pixel.blue = f * (show3 - show1);

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Hot_Blob() { // nice one

        get_ready();
        run_default_oscillators(0.001);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];

                animation.scale_x = 0.07 + move.directional[0] * 0.002;
                animation.scale_y = 0.07;

                animation.offset_y = -move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;

                animation.z = 0;
                animation.low_limit = -1;
                float show1 = render_value(animation);

                animation.offset_y = -move.linear[1];
                float show3 = render_value(animation);

                animation.offset_x = show3 / 20;
                animation.offset_y = -move.linear[0] / 2 + show1 / 70;
                animation.low_limit = 0;
                float show2 = render_value(animation);

                animation.offset_x = show3 / 20;
                animation.offset_y = -move.linear[0] / 2 + show1 / 70;
                animation.z = 100;
                float show4 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - animation.dist) / animation.dist;

                float linear = (y + 1) / (num_y - 1.f);

                pixel.red = radial * show2;
                pixel.green = linear * radial * 0.3 * (show2 - show4);
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Zoom() { // nice one

        get_ready();

        run_default_oscillators();
        timings.master_speed = 0.003;
        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = (distance[x][y] * distance[x][y]) / 2;
                animation.angle = polar_theta[x][y];

                animation.scale_x = 0.005;
                animation.scale_y = 0.005;

                animation.offset_y = -10 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;

                animation.z = 0;
                animation.low_limit = 0;
                float show1 = render_value(animation);

                float linear = 1; //(y+1)/(num_y-1.f);

                pixel.red = show1 * linear;
                pixel.green = 0;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Slow_Fade() { // nice one

        get_ready();

        run_default_oscillators();
        timings.master_speed = 0.00005;
        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist =
                    sqrtf(distance[x][y]) * 0.7 * (move.directional[0] + 1.5);
                animation.angle =
                    polar_theta[x][y] - move.radial[0] + distance[x][y] / 5;

                animation.scale_x = 0.11;
                animation.scale_y = 0.11;

                animation.offset_y = -50 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0;

                animation.z = move.linear[0];
                animation.low_limit = -0.1;
                animation.high_limit = 1;
                float show1 = render_value(animation);

                animation.dist = animation.dist * 1.1;
                animation.angle += move.noise_angle[0] / 10;
                float show2 = render_value(animation);

                animation.dist = animation.dist * 1.1;
                animation.angle += move.noise_angle[1] / 10;

                float show3 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * show1;
                pixel.green = radial * (show1 - show2) / 6;
                pixel.blue = radial * (show1 - show3) / 5;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Polar_Waves() { // nice one

        get_ready();

        timings.master_speed = 0.5; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = (distance[x][y]);
                animation.angle =
                    polar_theta[x][y] - animation.dist * 0.1 + move.radial[0];
                animation.z = (animation.dist * 1.5) - 10 * move.linear[0];
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_x = move.linear[0];

                float show1 = render_value(animation);
                animation.angle =
                    polar_theta[x][y] - animation.dist * 0.1 + move.radial[1];
                animation.z = (animation.dist * 1.5) - 10 * move.linear[1];
                animation.offset_x = move.linear[1];

                float show2 = render_value(animation);
                animation.angle =
                    polar_theta[x][y] - animation.dist * 0.1 + move.radial[2];
                animation.z = (animation.dist * 1.5) - 10 * move.linear[2];
                animation.offset_x = move.linear[2];

                float show3 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * show1;
                pixel.green = radial * show2;
                pixel.blue = radial * show3;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void RGB_Blobs() { // nice one

        get_ready();

        timings.master_speed = 0.2; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + move.radial[0] +
                                  move.noise_angle[0] + move.noise_angle[3];
                animation.z = (sqrtf(animation.dist)); // - 10 * move.linear[0];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 10;
                animation.offset_x = 10 * move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[1] +
                                  move.noise_angle[1] + move.noise_angle[4];
                animation.offset_x = 11 * move.linear[1];
                animation.offset_z = 100;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[2] +
                                  move.noise_angle[2] + move.noise_angle[5];
                animation.offset_x = 12 * move.linear[2];
                animation.offset_z = 300;
                float show3 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * show1;
                pixel.green = radial * show2;
                pixel.blue = radial * show3;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void RGB_Blobs2() { // nice one

        get_ready();

        timings.master_speed = 0.12; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + move.radial[0] +
                                  move.noise_angle[0] + move.noise_angle[3] +
                                  move.noise_angle[1];
                animation.z = (sqrtf(animation.dist)); // - 10 * move.linear[0];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 10;
                animation.offset_x = 10 * move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[1] +
                                  move.noise_angle[1] + move.noise_angle[4] +
                                  move.noise_angle[2];
                animation.offset_x = 11 * move.linear[1];
                animation.offset_z = 100;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[2] +
                                  move.noise_angle[2] + move.noise_angle[5] +
                                  move.noise_angle[3];
                animation.offset_x = 12 * move.linear[2];
                animation.offset_z = 300;
                float show3 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 - show3);
                pixel.green = radial * (show2 - show1);
                pixel.blue = radial * (show3 - show2);

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void RGB_Blobs3() { // nice one

        get_ready();

        timings.master_speed = 0.12; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] + move.noise_angle[4];
                animation.angle = polar_theta[x][y] + move.radial[0] +
                                  move.noise_angle[0] + move.noise_angle[3] +
                                  move.noise_angle[1];
                animation.z = (sqrtf(animation.dist)); // - 10 * move.linear[0];
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 10;
                animation.offset_x = 10 * move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[1] +
                                  move.noise_angle[1] + move.noise_angle[4] +
                                  move.noise_angle[2];
                animation.offset_x = 11 * move.linear[1];
                animation.offset_z = 100;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[2] +
                                  move.noise_angle[2] + move.noise_angle[5] +
                                  move.noise_angle[3];
                animation.offset_x = 12 * move.linear[2];
                animation.offset_z = 300;
                float show3 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 + show3) * 0.5 * animation.dist / 5;
                pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
                pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void RGB_Blobs4() { // nice one

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] + move.noise_angle[4];
                animation.angle = polar_theta[x][y] + move.radial[0] +
                                  move.noise_angle[0] + move.noise_angle[3] +
                                  move.noise_angle[1];
                animation.z = 3 + sqrtf(animation.dist);
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 10;
                animation.offset_x = 50 * move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[1] +
                                  move.noise_angle[1] + move.noise_angle[4] +
                                  move.noise_angle[2];
                animation.offset_x = 50 * move.linear[1];
                animation.offset_z = 100;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[2] +
                                  move.noise_angle[2] + move.noise_angle[5] +
                                  move.noise_angle[3];
                animation.offset_x = 50 * move.linear[2];
                animation.offset_z = 300;
                float show3 = render_value(animation);

                float radius = 23; // radius of a radial brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 + show3) * 0.5 * animation.dist / 5;
                pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
                pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void RGB_Blobs5() { // nice one

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] + move.noise_angle[4];
                animation.angle = polar_theta[x][y] + move.radial[0] +
                                  move.noise_angle[0] + move.noise_angle[3] +
                                  move.noise_angle[1];
                animation.z = 3 + sqrtf(animation.dist);
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 10;
                animation.offset_x = 50 * move.linear[0];
                float show1 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[1] +
                                  move.noise_angle[1] + move.noise_angle[4] +
                                  move.noise_angle[2];
                animation.offset_x = 50 * move.linear[1];
                animation.offset_z = 100;
                float show2 = render_value(animation);

                animation.angle = polar_theta[x][y] + move.radial[2] +
                                  move.noise_angle[2] + move.noise_angle[5] +
                                  move.noise_angle[3];
                animation.offset_x = 50 * move.linear[2];
                animation.offset_z = 300;
                float show3 = render_value(animation);

                float radius = 23; // radius of a radial brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 + show3) * 0.5 * animation.dist / 5;
                pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
                pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Big_Caleido() { // nice one

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] +
                                  5 * move.noise_angle[0] +
                                  animation.dist * 0.1;
                animation.z = 5;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 50 * move.linear[0];
                animation.offset_x = 50 * move.noise_angle[0];
                animation.offset_y = 50 * move.noise_angle[1];
                float show1 = render_value(animation);

                animation.angle = 6 * polar_theta[x][y] +
                                  5 * move.noise_angle[1] +
                                  animation.dist * 0.15;
                animation.z = 5;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 50 * move.linear[1];
                animation.offset_x = 50 * move.noise_angle[1];
                animation.offset_y = 50 * move.noise_angle[2];
                float show2 = render_value(animation);

                animation.angle = 5;
                animation.z = 5;
                animation.scale_x = 0.10;
                animation.scale_y = 0.10;
                animation.offset_z = 10 * move.linear[2];
                animation.offset_x = 10 * move.noise_angle[2];
                animation.offset_y = 10 * move.noise_angle[3];
                float show3 = render_value(animation);

                animation.angle = 15;
                animation.z = 15;
                animation.scale_x = 0.10;
                animation.scale_y = 0.10;
                animation.offset_z = 10 * move.linear[3];
                animation.offset_x = 10 * move.noise_angle[3];
                animation.offset_y = 10 * move.noise_angle[4];
                float show4 = render_value(animation);

                animation.angle = 2;
                animation.z = 15;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_z = 10 * move.linear[4];
                animation.offset_x = 10 * move.noise_angle[4];
                animation.offset_y = 10 * move.noise_angle[5];
                float show5 = render_value(animation);

                pixel.red = show1 - show4;
                pixel.green = show2 - show5;
                pixel.blue = show3 - show2 + show1;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(y, x, pixel);
            }
        }
        // show_frame();
    }

    void SM1() { // nice one

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.0031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x / 2; x++) {
            for (int y = 0; y < num_y / 2; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 5 * move.noise_angle[0];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 50 * move.linear[0];
                animation.offset_x = 150 * move.directional[0];
                animation.offset_y = 150 * move.directional[1];
                float show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 4 * move.noise_angle[1];
                animation.z = 15;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_z = 50 * move.linear[1];
                animation.offset_x = 150 * move.directional[1];
                animation.offset_y = 150 * move.directional[2];
                float show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 5 * move.noise_angle[2];
                animation.z = 25;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = 50 * move.linear[2];
                animation.offset_x = 150 * move.directional[2];
                animation.offset_y = 150 * move.directional[3];
                float show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 5 * move.noise_angle[3];
                animation.z = 35;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_z = 50 * move.linear[3];
                animation.offset_x = 150 * move.directional[3];
                animation.offset_y = 150 * move.directional[4];
                float show4 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 5 * move.noise_angle[4];
                animation.z = 45;
                animation.scale_x = 0.2;
                animation.scale_y = 0.2;
                animation.offset_z = 50 * move.linear[4];
                animation.offset_x = 150 * move.directional[4];
                animation.offset_y = 150 * move.directional[5];
                float show5 = render_value(animation);

                pixel.red = show1 + show2;
                pixel.green = show3 + show4;
                pixel.blue = show5;

                pixel = rgb_sanity_check(pixel);
                // leds[xyMap(x, y)] = CRGB(pixel.red, pixel.green, pixel.blue);
                setPixelColorInternal(x, y, pixel);

                setPixelColorInternal((num_x - 1) - x, y, pixel);
                setPixelColorInternal((num_x - 1) - x, (num_y - 1) - y, pixel);
                setPixelColorInternal(x, (num_y - 1) - y, pixel);
            }
        }
        // show_frame();
    }

    void SM2() {

        get_ready();

        timings.master_speed = 0.03; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] * (move.directional[0]);
                animation.angle = polar_theta[x][y] + move.radial[0];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 5 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[1];
                animation.angle = polar_theta[x][y] + move.radial[1];
                animation.z = 50;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 5 * move.linear[1];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[2];
                animation.angle = polar_theta[x][y] + move.radial[2];
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 5 * move.linear[2];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show3 = render_value(animation);

                pixel.red = show1;
                pixel.green = show2;
                pixel.blue = show3;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
        // show_frame();
    }

    void SM3() {

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.offset_y = -20 * move.linear[0];
                ;
                animation.low_limit = -1;
                animation.high_limit = 1;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 500;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.offset_y = -20 * move.linear[0];
                ;
                animation.low_limit = -1;
                animation.high_limit = 1;
                show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 500 + show1 / 20;
                animation.offset_y = -4 * move.linear[0] + show2 / 20;
                animation.low_limit = 0;
                animation.high_limit = 1;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 500 + show1 / 18;
                animation.offset_y = -4 * move.linear[0] + show2 / 18;
                animation.low_limit = 0;
                animation.high_limit = 1;
                show4 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 500 + show1 / 19;
                animation.offset_y = -4 * move.linear[0] + show2 / 19;
                animation.low_limit = 0.3;
                animation.high_limit = 1;
                show5 = render_value(animation);

                pixel.red = show4;
                pixel.green = show3;
                pixel.blue = show5;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM4() {

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0033; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0036;
        timings.ratio[5] = 0.0039;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.offset_y = -20 * move.linear[0];
                ;
                animation.low_limit = 0;
                animation.high_limit = 1;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 500;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.offset_y = -40 * move.linear[0];
                ;
                animation.low_limit = 0;
                animation.high_limit = 1;
                show2 = render_value(animation);

                pixel.red = add(show2, show1);
                pixel.green = 0;
                pixel.blue = colordodge(show2, show1);

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM5() {

        get_ready();

        timings.master_speed = 0.03; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] * (move.directional[0]);
                animation.angle = polar_theta[x][y] + move.radial[0];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 5 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[1];
                animation.angle = polar_theta[x][y] + move.radial[1];
                animation.z = 50;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 5 * move.linear[1];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[2];
                animation.angle = polar_theta[x][y] + move.radial[2];
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 5 * move.linear[2];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show3 = render_value(animation);

                animation.dist = distance[x][y] * (move.directional[3]);
                animation.angle = polar_theta[x][y] + move.radial[3];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 5 * move.linear[3];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show4 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[4];
                animation.angle = polar_theta[x][y] + move.radial[4];
                animation.z = 50;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 5 * move.linear[4];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show5 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[5];
                animation.angle = polar_theta[x][y] + move.radial[5];
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 5 * move.linear[5];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show6 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * add(show1, show4);
                pixel.green = radial * colordodge(show2, show5);
                pixel.blue = radial * screen(show3, show6);

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM6() {

        get_ready();

        timings.master_speed = 0.03; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.7; // zoom factor

                animation.dist = distance[x][y] * (move.directional[0]) * s;
                animation.angle = polar_theta[x][y] + move.radial[0];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 5 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show1 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[1] * s;
                animation.angle = polar_theta[x][y] + move.radial[1];
                animation.z = 50;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 5 * move.linear[1];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show2 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[2] * s;
                animation.angle = polar_theta[x][y] + move.radial[2];
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 5 * move.linear[2];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show3 = render_value(animation);

                animation.dist = distance[x][y] * (move.directional[3]) * s;
                animation.angle = polar_theta[x][y] + move.radial[3];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 5 * move.linear[3];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show4 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[4] * s;
                animation.angle = polar_theta[x][y] + move.radial[4];
                animation.z = 50;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 5 * move.linear[4];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show5 = render_value(animation);

                animation.dist = distance[x][y] * move.directional[5] * s;
                animation.angle = polar_theta[x][y] + move.radial[5];
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 5 * move.linear[5];
                animation.offset_x = 0;
                animation.offset_y = 0;
                float show6 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                show7 = screen(show1, show4);
                show8 = colordodge(show2, show5);
                show9 = screen(show3, show6);

                pixel.red = radial * (show7 + show8);
                pixel.green = 0;
                pixel.blue = radial * show9;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM8() {

        get_ready();

        timings.master_speed = 0.005; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.01; // original Reddit post had no radial movement!

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                // float s = 0.7; // zoom factor

                animation.dist = distance[x][y];
                animation.angle = 2;
                animation.z = 5;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_z = 0;
                animation.offset_y = 50 * move.linear[0];
                animation.offset_x = 0;
                animation.low_limit = 0;
                float show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 2;
                animation.z = 150;
                animation.offset_x = -50 * move.linear[0];
                float show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 1;
                animation.z = 550;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_x = 0;
                animation.offset_y = -50 * move.linear[1];
                float show4 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 1;
                animation.z = 1250;
                animation.scale_x = 0.15;
                animation.scale_y = 0.15;
                animation.offset_x = 0;
                animation.offset_y = 50 * move.linear[1];
                float show5 = render_value(animation);

                // float radius = radial_filter_radius;   // radius of a radial
                // brightness filter float radial =
                // (radius-distance[x][y])/distance[x][y];

                show3 = add(show1, show2);
                show6 = screen(show4, show5);
                // show9 = screen(show3, show6);

                pixel.red = show3;
                pixel.green = 0;
                pixel.blue = show6;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM9() {

        get_ready();

        timings.master_speed = 0.005; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_y = -30 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = -1;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_y = -30 * move.linear[1];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = -1;
                show2 = render_value(animation);

                animation.dist = distance[x][y]; // + show1/64;
                animation.angle = polar_theta[x][y] + 2 + (show1 / 255) * PI;
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_y = -10 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 2 + (show2 / 255) * PI;
                ;
                animation.z = 5;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_y = -20 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                show5 = screen(show4, show3);
                show6 = colordodge(show5, show3);

                float linear1 = y / 32.f;
                float linear2 = (32 - y) / 32.f;

                pixel.red = show5 * linear1;
                pixel.green = 0;
                pixel.blue = show6 * linear2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void SM10() {

        get_ready();

        timings.master_speed = 0.006; // 0.006

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float scale = 0.6;

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.09 * scale;
                animation.scale_y = 0.09 * scale;
                animation.offset_y = -30 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = -1;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.09 * scale;
                animation.scale_y = 0.09 * scale;
                animation.offset_y = -30 * move.linear[1];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = -1;
                show2 = render_value(animation);

                animation.dist = distance[x][y]; // + show1/64;
                animation.angle = polar_theta[x][y] + 2 + (show1 / 255) * PI;
                animation.z = 5;
                animation.scale_x = 0.09 * scale;
                animation.scale_y = 0.09 * scale;
                animation.offset_y = -10 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + 2 + (show2 / 255) * PI;
                ;
                animation.z = 5;
                animation.scale_x = 0.09 * scale;
                animation.scale_y = 0.09 * scale;
                animation.offset_y = -20 * move.linear[0];
                animation.offset_z = 0;
                animation.offset_x = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                show5 = screen(show4, show3);
                show6 = colordodge(show5, show3);

                // float linear1 = y / 32.f;
                // float linear2 = (32-y) / 32.f;

                pixel.red = (show5 + show6) / 2;
                pixel.green = (show5 - 50) + (show6 / 16);
                pixel.blue = 0; // show6;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido() {

        get_ready();

        timings.master_speed = 0.009; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        // float size = 1.5;

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] + 10 * move.radial[0] +
                                  animation.dist / 2;
                animation.z = 5;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 0;
                animation.offset_x = -30 * move.linear[0];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = -5 * polar_theta[x][y] + 12 * move.radial[1] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.07;
                animation.scale_y = 0.07;
                animation.offset_z = 0;
                animation.offset_x = -30 * move.linear[1];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = -5 * polar_theta[x][y] + 12 * move.radial[2] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.05;
                animation.scale_y = 0.05;
                animation.offset_z = 0;
                animation.offset_x = -40 * move.linear[2];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] + 12 * move.radial[3] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.09;
                animation.scale_y = 0.09;
                animation.offset_z = 0;
                animation.offset_x = -35 * move.linear[3];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                show5 = screen(show4, show3);
                show6 = colordodge(show2, show3);

                // float linear1 = y / 32.f;
                // float linear2 = (32-y) / 32.f;

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 + show2);
                pixel.green = 0.3 * radial * show6; //(radial*(show1))*0.3f;
                pixel.blue = radial * show5;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido_2() {

        get_ready();

        timings.master_speed = 0.009; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.0053; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[4] = 0.0056;
        timings.ratio[5] = 0.0059;

        calculate_oscillators(timings);

        float size = 0.5;

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] + 10 * move.radial[0] +
                                  animation.dist / 2;
                animation.z = 5;
                animation.scale_x = 0.07 * size;
                animation.scale_y = 0.07 * size;
                animation.offset_z = 0;
                animation.offset_x = -30 * move.linear[0];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = -5 * polar_theta[x][y] + 12 * move.radial[1] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.07 * size;
                animation.scale_y = 0.07 * size;
                animation.offset_z = 0;
                animation.offset_x = -30 * move.linear[1];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = -5 * polar_theta[x][y] + 12 * move.radial[2] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.05 * size;
                animation.scale_y = 0.05 * size;
                animation.offset_z = 0;
                animation.offset_x = -40 * move.linear[2];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 5 * polar_theta[x][y] + 12 * move.radial[3] +
                                  animation.dist / 2;
                animation.z = 500;
                animation.scale_x = 0.09 * size;
                animation.scale_y = 0.09 * size;
                animation.offset_z = 0;
                animation.offset_x = -35 * move.linear[3];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                show5 = screen(show4, show3);
                show6 = colordodge(show2, show3);

                // float linear1 = y / 32.f;
                // float linear2 = (32-y) / 32.f;

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = radial * (show1 + show2);
                pixel.green = 0.3 * radial * show6; //(radial*(show1))*0.3f;
                pixel.blue = radial * show5;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido_3() {

        get_ready();

        timings.master_speed = 0.001; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.038;
        timings.ratio[5] = 0.041;

        calculate_oscillators(timings);

        float size = 0.4 + move.directional[0] * 0.1;

        float q = 2;

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle =
                    5 * polar_theta[x][y] + 10 * move.radial[0] +
                    animation.dist / (((move.directional[0] + 3) * 2)) +
                    move.noise_angle[0] * q;
                animation.z = 5;
                animation.scale_x = 0.08 * size * (move.directional[0] + 1.5);
                animation.scale_y = 0.07 * size;
                animation.offset_z = -10 * move.linear[0];
                animation.offset_x = -30 * move.linear[0];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle =
                    -5 * polar_theta[x][y] + 10 * move.radial[1] +
                    animation.dist / (((move.directional[1] + 3) * 2)) +
                    move.noise_angle[1] * q;
                animation.z = 500;
                animation.scale_x = 0.07 * size * (move.directional[1] + 1.1);
                animation.scale_y = 0.07 * size * (move.directional[2] + 1.3);
                ;
                animation.offset_z = -12 * move.linear[1];
                ;
                animation.offset_x = -(num_x - 1) * move.linear[1];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle =
                    -5 * polar_theta[x][y] + 12 * move.radial[2] +
                    animation.dist / (((move.directional[3] + 3) * 2)) +
                    move.noise_angle[2] * q;
                animation.z = 500;
                animation.scale_x = 0.05 * size * (move.directional[3] + 1.5);
                ;
                animation.scale_y = 0.05 * size * (move.directional[4] + 1.5);
                ;
                animation.offset_z = -12 * move.linear[3];
                animation.offset_x = -40 * move.linear[3];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle =
                    5 * polar_theta[x][y] + 12 * move.radial[3] +
                    animation.dist / (((move.directional[5] + 3) * 2)) +
                    move.noise_angle[3] * q;
                animation.z = 500;
                animation.scale_x = 0.09 * size * (move.directional[5] + 1.5);
                ;
                ;
                animation.scale_y = 0.09 * size * (move.directional[6] + 1.5);
                ;
                ;
                animation.offset_z = 0;
                animation.offset_x = -35 * move.linear[3];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                show5 = screen(show4, show3) - show2;
                show6 = colordodge(show4, show1);

                show7 = multiply(show1, show2);

                float linear1 = y / 32.f;
                // float linear2 = (32-y) / 32.f;

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                show7 = multiply(show1, show2) * linear1 * 2;
                show8 = subtract(show7, show5);

                // pixel.red    = radial*(show1+show2);
                pixel.green = 0.2 * show8; //(radial*(show1))*0.3f;
                pixel.blue = show5 * radial;
                pixel.red = (1 * show1 + 1 * show2) - show7 / 2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido_4() {

        get_ready();

        timings.master_speed = 0.01; // master speed 0.01 in the video

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.038;
        timings.ratio[6] = 0.041;

        calculate_oscillators(timings);

        float size = 0.6;

        float q = 1;

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 1 + move.directional[6] * 0.3;

                animation.dist = distance[x][y] * s;
                animation.angle =
                    5 * polar_theta[x][y] + 1 * move.radial[0] -
                    animation.dist / (3 + move.directional[0] * 0.5);
                animation.z = 5;
                animation.scale_x = 0.08 * size + (move.directional[0] * 0.01);
                animation.scale_y = 0.07 * size + (move.directional[1] * 0.01);
                animation.offset_z = -10 * move.linear[0];
                animation.offset_x = 0; //-30 * move.linear[0];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y] * s;
                animation.angle =
                    5 * polar_theta[x][y] + 1 * move.radial[1] +
                    animation.dist / (3 + move.directional[1] * 0.5);
                animation.z = 50;
                animation.scale_x = 0.08 * size + (move.directional[1] * 0.01);
                animation.scale_y = 0.07 * size + (move.directional[2] * 0.01);
                animation.offset_z = -10 * move.linear[1];
                animation.offset_x = 0; //-30 * move.linear[0];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 1;
                animation.z = 500;
                animation.scale_x = 0.2 * size;
                animation.scale_y = 0.2 * size;
                animation.offset_z = 0; //-12 * move.linear[3];
                animation.offset_y = +7 * move.linear[3] + move.noise_angle[3];
                animation.offset_x = 0;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle =
                    5 * polar_theta[x][y] + 12 * move.radial[3] +
                    animation.dist / (((move.directional[5] + 3) * 2)) +
                    move.noise_angle[3] * q;
                animation.z = 500;
                animation.scale_x = 0.09 * size * (move.directional[5] + 1.5);
                ;
                ;
                animation.scale_y = 0.09 * size * (move.directional[6] + 1.5);
                ;
                ;
                animation.offset_z = 0;
                animation.offset_x = -35 * move.linear[3];
                animation.offset_y = 0;
                animation.low_limit = 0;
                show4 = render_value(animation);

                // show5 = screen(show4, show3)-show2;
                // show6 = colordodge(show4, show1);

                // show7 = multiply(show1, show2);
                /*
                float linear1 = y / 32.f;
                float linear2 = (32-y) / 32.f;
                */

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];
                /*
                show7 = multiply(show1, show2) * linear1*2;
                show8 = subtract(show7, show5);
                */

                show5 = ((show1 + show2)) - show3;
                if (show5 > 255)
                    show5 = 255;
                if (show5 < 0)
                    show5 = 0;

                show6 = colordodge(show1, show2);

                pixel.red = show5 * radial;
                pixel.blue = (64 - show5 - show3) * radial;
                pixel.green = 0.5 * (show6);
                // pixel.blue   = show5 * radial;
                // pixel.red    = (1*show1 + 1*show2) - show7/2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido_5() {

        get_ready();

        timings.master_speed = 0.01; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.0038;
        timings.ratio[6] = 0.041;

        calculate_oscillators(timings);

        float size = 0.6;

        // float q = 1;

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 1 + move.directional[6] * 0.8;

                animation.dist = distance[x][y] * s;
                animation.angle = 10 * move.radial[6] +
                                  50 * move.directional[5] * polar_theta[x][y] -
                                  animation.dist / 3;
                animation.z = 5;
                animation.scale_x = 0.08 * size;
                animation.scale_y = 0.07 * size;
                animation.offset_z = -10 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_y = 0;
                animation.low_limit = -0.5;
                show1 = render_value(animation);

                float radius = radial_filter_radius; // radius of a radial
                                                     // brightness filter
                float radial = (radius - distance[x][y]) / distance[x][y];

                pixel.red = show1 * radial;
                pixel.green = 0;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Complex_Kaleido_6() {

        get_ready();

        timings.master_speed = 0.01; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.0038;
        timings.ratio[6] = 0.041;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = 16 * polar_theta[x][y] + 16 * move.radial[0];
                animation.z = 5;
                animation.scale_x = 0.06;
                animation.scale_y = 0.06;
                animation.offset_z = -10 * move.linear[0];
                animation.offset_y = 10 * move.noise_angle[0];
                animation.offset_x = 10 * move.noise_angle[4];
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y];
                animation.angle = 16 * polar_theta[x][y] + 16 * move.radial[1];
                animation.z = 500;
                animation.scale_x = 0.06;
                animation.scale_y = 0.06;
                animation.offset_z = -10 * move.linear[1];
                animation.offset_y = 10 * move.noise_angle[1];
                animation.offset_x = 10 * move.noise_angle[3];
                animation.low_limit = 0;
                show2 = render_value(animation);

                // float radius = radial_filter_radius;   // radius of a radial
                // brightness filter float radial =
                // (radius-distance[x][y])/distance[x][y];

                pixel.red = show1;
                pixel.green = 0;
                pixel.blue = show2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Water() {

        get_ready();

        timings.master_speed = 0.037; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.031;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.1;
        timings.ratio[6] = 0.41;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist =
                    distance[x][y] +
                    4 * sinf(move.directional[5] * PI + (float)x / 2) +
                    4 * cosf(move.directional[6] * PI + float(y) / 2);
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.06;
                animation.scale_y = 0.06;
                animation.offset_z = -10 * move.linear[0];
                animation.offset_y = 10;
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = (10 + move.directional[0]) *
                                 sinf(-move.radial[5] + move.radial[0] +
                                      (distance[x][y] / (3)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = (10 + move.directional[1]) *
                                 sinf(-move.radial[5] + move.radial[1] +
                                      (distance[x][y] / (3)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 500;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[1];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = (10 + move.directional[2]) *
                                 sinf(-move.radial[5] + move.radial[2] +
                                      (distance[x][y] / (3)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 500;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show4 = render_value(animation);

                // float radius = radial_filter_radius;   // radius of a radial
                // brightness filter float radial =
                // (radius-distance[x][y])/distance[x][y];

                // pixel.red    = show2;

                pixel.blue = (0.7 * show2 + 0.6 * show3 + 0.5 * show4);
                pixel.red = pixel.blue - 40;
                pixel.green = 0;
                // pixel.red     = radial*show3;
                // pixel.green     = 0.9*radial*show4;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Parametric_Water() {

        get_ready();

        timings.master_speed = 0.003; // master speed

        timings.ratio[0] = 0.025; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[1] = 0.027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions
        timings.ratio[4] = 0.037;
        timings.ratio[5] = 0.15; // wave speed
        timings.ratio[6] = 0.41;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 4;
                float f = 10 + 2 * move.directional[0];

                animation.dist = (f + move.directional[0]) *
                                 sinf(-move.radial[5] + move.radial[0] +
                                      (distance[x][y] / (s)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = (f + move.directional[1]) *
                                 sinf(-move.radial[5] + move.radial[1] +
                                      (distance[x][y] / (s)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 500;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[1];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show3 = render_value(animation);

                animation.dist = (f + move.directional[2]) *
                                 sinf(-move.radial[5] + move.radial[2] +
                                      (distance[x][y] / (s)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 5000;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show4 = render_value(animation);

                animation.dist = (f + move.directional[3]) *
                                 sinf(-move.radial[5] + move.radial[3] +
                                      (distance[x][y] / (s)));
                animation.angle = 1 * polar_theta[x][y];
                animation.z = 2000;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[3];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show5 = render_value(animation);

                show6 = screen(show4, show5);
                show7 = screen(show2, show3);

                float radius = 40; // radius of a radial brightness filter
                float radial = (radius - distance[x][y]) / radius;

                // pixel.red    = show6;
                // pixel.blue = show7;

                pixel.red = pixel.blue - 40;
                pixel.green = 0;
                pixel.blue = (0.3 * show6 + 0.7 * show7) * radial;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment1() {

        get_ready();

        timings.master_speed = 0.03; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y] + 20 * move.directional[0];
                animation.angle = move.noise_angle[0] + move.noise_angle[1] +
                                  polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                pixel.red = 0;
                pixel.green = 0;
                pixel.blue = show1;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment2() {

        get_ready();

        timings.master_speed = 0.02; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist =
                    distance[x][y] - (16 + move.directional[0] * 16);
                animation.angle = move.noise_angle[0] + move.noise_angle[1] +
                                  polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                pixel.red = show1;
                pixel.green = show1 - 80;
                pixel.blue = show1 - 150;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment3() {

        get_ready();

        timings.master_speed = 0.01; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.033; // speed ratios for the oscillators, higher
                                  // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist =
                    distance[x][y] - (12 + move.directional[3] * 4);
                animation.angle = move.noise_angle[0] + move.noise_angle[1] +
                                  polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1;
                animation.scale_y = 0.1;
                animation.offset_z = -10;
                animation.offset_y = 20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                pixel.red = show1;
                pixel.green = show1 - 80;
                pixel.blue = show1 - 150;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Zoom2() { // nice one

        get_ready();

        run_default_oscillators();
        timings.master_speed = 0.003;
        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = (distance[x][y] * distance[x][y]) / 2;
                animation.angle = polar_theta[x][y];

                animation.scale_x = 0.005;
                animation.scale_y = 0.005;

                animation.offset_y = -10 * move.linear[0];
                animation.offset_x = 0;
                animation.offset_z = 0.1 * move.linear[0];

                animation.z = 0;
                animation.low_limit = 0;
                float show1 = render_value(animation);

                // float linear = 1;//(y+1)/(num_y-1.f);

                pixel.red = show1;
                pixel.green = 0;
                pixel.blue = 40 - show1;

                pixel = rgb_sanity_check(pixel);
                setPixelColorInternal(y, x, pixel);
            }
        }
    }

    void Module_Experiment4() {

        get_ready();

        timings.master_speed = 0.031; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.033;
        timings.ratio[4] = 0.036; // speed ratios for the oscillators, higher
                                  // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.8;

                animation.dist = (distance[x][y] * distance[x][y]) * 0.7;
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.004 * s;
                animation.scale_y = 0.003 * s;
                animation.offset_z = 0.1 * move.linear[2];
                animation.offset_y = -20 * move.linear[2];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = (distance[x][y] * distance[x][y]) * 0.8;
                animation.angle = polar_theta[x][y];
                animation.z = 50;
                animation.scale_x = 0.004 * s;
                animation.scale_y = 0.003 * s;
                animation.offset_z = 0.1 * move.linear[3];
                animation.offset_y = -20 * move.linear[3];
                animation.offset_x = 100;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist = (distance[x][y] * distance[x][y]) * 0.9;
                animation.angle = polar_theta[x][y];
                animation.z = 5000;
                animation.scale_x = 0.004 * s;
                animation.scale_y = 0.003 * s;
                animation.offset_z = 0.1 * move.linear[4];
                animation.offset_y = -20 * move.linear[4];
                animation.offset_x = 1000;
                animation.low_limit = 0;
                show3 = render_value(animation);

                // overlapping color mapping
                /*
                float r = show1;
                float g = show2-show1;
                float b = show3-show1-show2;
                */

                pixel.red = show1 - show2 - show3;
                pixel.blue = show2 - show1 - show3;
                pixel.green = show3 - show1 - show2;
                // pixel.green  = b;
                // pixel.green  = show1 - 80;
                // pixel.blue   = show1 - 150;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment5() {

        get_ready();

        timings.master_speed = 0.031; // master speed

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.33;
        timings.ratio[4] = 0.036; // speed ratios for the oscillators, higher
                                  // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 1.5;

                animation.dist = distance[x][y] +
                                 sinf(0.5 * distance[x][y] - move.radial[3]);
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[0];
                animation.offset_y = -20 * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                pixel.red = show1;
                pixel.green = 0;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment6() {

        get_ready();

        timings.master_speed = 0.01; // master speed 0.031

        float w = 0.7;

        timings.ratio[0] = 0.0025; // speed ratios for the oscillators, higher
                                   // values = faster transitions
        timings.ratio[1] = 0.0027;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.33 * w;
        timings.ratio[4] = 0.36 * w; // speed ratios for the oscillators, higher
                                     // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.8;

                animation.dist = distance[x][y] +
                                 sinf(0.25 * distance[x][y] - move.radial[3]);
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[0];
                animation.offset_y = -20 * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist = distance[x][y] +
                                 sinf(0.24 * distance[x][y] - move.radial[4]);
                animation.angle = polar_theta[x][y];
                animation.z = 10;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[1];
                animation.offset_y = -20 * move.linear[1];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show2 = render_value(animation);

                /*
                pixel.red    = show1;
                pixel.green  = show1 * 0.3;
                pixel.blue   = show2-show1;
                */

                pixel.red = (show1 + show2);
                pixel.green = ((show1 + show2) * 0.6) - 30;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment7() {

        get_ready();

        timings.master_speed = 0.005; // master speed 0.031

        float w = 0.3;

        timings.ratio[0] = 0.01; // speed ratios for the oscillators, higher
                                 // values = faster transitions
        timings.ratio[1] = 0.011;
        timings.ratio[2] = 0.029;
        timings.ratio[3] = 0.33 * w;
        timings.ratio[4] = 0.36 * w; // speed ratios for the oscillators, higher
                                     // values = faster transitions

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.7;

                animation.dist =
                    2 + distance[x][y] +
                    2 * sinf(0.25 * distance[x][y] - move.radial[3]);
                animation.angle = polar_theta[x][y];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 10 * move.linear[0];
                animation.offset_y = -20 * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist =
                    2 + distance[x][y] +
                    2 * sinf(0.24 * distance[x][y] - move.radial[4]);
                animation.angle = polar_theta[x][y];
                animation.z = 10;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[1];
                animation.offset_y = -20 * move.linear[1];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show2 = render_value(animation);

                /*
                pixel.red    = show1;
                pixel.green  = show1 * 0.3;
                pixel.blue   = show2-show1;
                */

                pixel.red = (show1 + show2);
                pixel.green = ((show1 + show2) * 0.6) - 50;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment8() {

        get_ready();

        timings.master_speed = 0.01; // master speed 0.031

        float w = 0.3;

        timings.ratio[0] = 0.01; // speed ratios for the oscillators, higher
                                 // values = faster transitions
        timings.ratio[1] = 0.011;
        timings.ratio[2] = 0.013;
        timings.ratio[3] = 0.33 * w;
        timings.ratio[4] = 0.36 * w; // speed ratios for the oscillators, higher
                                     // values = faster transitions
        timings.ratio[5] = 0.38 * w;
        timings.ratio[6] = 0.0003; // master rotation

        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;
        timings.offset[5] = 500;
        timings.offset[6] = 600;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.4; // scale
                float r = 1.5; // scroll speed

                animation.dist =
                    3 + distance[x][y] +
                    3 * sinf(0.25 * distance[x][y] - move.radial[3]);
                animation.angle = polar_theta[x][y] + move.noise_angle[0] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 10 * move.linear[0];
                animation.offset_y = -5 * r * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist =
                    4 + distance[x][y] +
                    4 * sinf(0.24 * distance[x][y] - move.radial[4]);
                animation.angle = polar_theta[x][y] + move.noise_angle[1] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[1];
                animation.offset_y = -5 * r * move.linear[1];
                animation.offset_x = 100;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist =
                    5 + distance[x][y] +
                    5 * sinf(0.23 * distance[x][y] - move.radial[5]);
                animation.angle = polar_theta[x][y] + move.noise_angle[2] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[2];
                animation.offset_y = -5 * r * move.linear[2];
                animation.offset_x = 1000;
                animation.low_limit = 0;
                show3 = render_value(animation);

                show4 = colordodge(show1, show2);

                float rad = sinf(PI / 2 +
                                 distance[x][y] / 14); // better radial filter?!

                /*
                pixel.red    = show1;
                pixel.green  = show1 * 0.3;
                pixel.blue   = show2-show1;
                */

                pixel.red = rad * ((show1 + show2) + show3);
                pixel.green = (((show2 + show3) * 0.8) - 90) * rad;
                pixel.blue = show4 * 0.2;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment9() {

        get_ready();

        timings.master_speed = 0.03; // master speed 0.031

        float w = 0.3;

        timings.ratio[0] = 0.1; // speed ratios for the oscillators, higher
                                // values = faster transitions
        timings.ratio[1] = 0.011;
        timings.ratio[2] = 0.013;
        timings.ratio[3] = 0.33 * w;
        timings.ratio[4] = 0.36 * w; // speed ratios for the oscillators, higher
                                     // values = faster transitions
        timings.ratio[5] = 0.38 * w;
        timings.ratio[6] = 0.0003;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                animation.dist = distance[x][y];
                animation.angle = polar_theta[x][y] + move.radial[1];
                animation.z = 5;
                animation.scale_x = 0.001;
                animation.scale_y = 0.1;
                animation.scale_z = 0.1;
                animation.offset_y = -10 * move.linear[0];
                animation.offset_x = 20;
                animation.offset_z = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                pixel.red = 10 * show1;
                pixel.green = 0;
                pixel.blue = 0;

                pixel = rgb_sanity_check(pixel);

                setPixelColorInternal(x, y, pixel);
            }
        }
    }

    void Module_Experiment10() {

        get_ready();

        timings.master_speed = 0.01; // master speed 0.031

        float w = 1;

        timings.ratio[0] = 0.01; // speed ratios for the oscillators, higher
                                 // values = faster transitions
        timings.ratio[1] = 0.011;
        timings.ratio[2] = 0.013;
        timings.ratio[3] = 0.33 * w;
        timings.ratio[4] = 0.36 * w; // speed ratios for the oscillators, higher
                                     // values = faster transitions
        timings.ratio[5] = 0.38 * w;
        timings.ratio[6] = 0.0003; // master rotation

        timings.offset[0] = 0;
        timings.offset[1] = 100;
        timings.offset[2] = 200;
        timings.offset[3] = 300;
        timings.offset[4] = 400;
        timings.offset[5] = 500;
        timings.offset[6] = 600;

        calculate_oscillators(timings);

        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {

                float s = 0.4; // scale
                float r = 1.5; // scroll speed

                animation.dist =
                    3 + distance[x][y] +
                    3 * sinf(0.25 * distance[x][y] - move.radial[3]);
                animation.angle = polar_theta[x][y] + move.noise_angle[0] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 10 * move.linear[0];
                animation.offset_y = -5 * r * move.linear[0];
                animation.offset_x = 10;
                animation.low_limit = 0;
                show1 = render_value(animation);

                animation.dist =
                    4 + distance[x][y] +
                    4 * sinf(0.24 * distance[x][y] - move.radial[4]);
                animation.angle = polar_theta[x][y] + move.noise_angle[1] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[1];
                animation.offset_y = -5 * r * move.linear[1];
                animation.offset_x = 100;
                animation.low_limit = 0;
                show2 = render_value(animation);

                animation.dist =
                    5 + distance[x][y] +
                    5 * sinf(0.23 * distance[x][y] - move.radial[5]);
                animation.angle = polar_theta[x][y] + move.noise_angle[2] +
                                  move.noise_angle[6];
                animation.z = 5;
                animation.scale_x = 0.1 * s;
                animation.scale_y = 0.1 * s;
                animation.offset_z = 0.1 * move.linear[2];
                animation.offset_y = -5 * r * move.linear[2];
                animation.offset_x = 1000;
                animation.low_limit = 0;
                show3 = render_value(animation);

                show4 = colordodge(show1, show2);

                float rad = sinf(PI / 2 +
                                 distance[x][y] / 14); // better radial filter?!

                /*
                pixel.red    = show1;
                pixel.green  = show1 * 0.3;
                pixel.blue   = show2-show1;
                */

                CHSV(rad * ((show1 + show2) + show3), 255, 255);

                pixel = rgb_sanity_check(pixel);

                uint8_t a = getTime() / 100;
                CRGB p = CRGB(CHSV(((a + show1 + show2) + show3), 255, 255));
                rgb pixel;
                pixel.red = p.red;
                pixel.green = p.green;
                pixel.blue = p.blue;
                setPixelColorInternal(x, y, pixel);
            }
        }
    }
};

}  // namespace animartrix_detail
