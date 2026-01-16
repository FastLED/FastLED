#pragma once

#include "crgb.h"
#include "fl/stl/math.h"

namespace noise_test_helpers {
    // Helper function to calculate average color difference between two frames
    inline float calcAverageColorDifference(const fl::CRGB* frame1, const fl::CRGB* frame2, int num_leds) {
        float total_diff = 0.0f;
        for (int i = 0; i < num_leds; i++) {
            // Calculate delta for each channel
            int r_diff = (int)frame1[i].r - (int)frame2[i].r;
            int g_diff = (int)frame1[i].g - (int)frame2[i].g;
            int b_diff = (int)frame1[i].b - (int)frame2[i].b;

            // Use Euclidean distance for color difference
            float pixel_diff = fl::sqrt(r_diff*r_diff + g_diff*g_diff + b_diff*b_diff);
            total_diff += pixel_diff;
        }
        return total_diff / num_leds;
    }
}
