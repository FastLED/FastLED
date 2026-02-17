#pragma once

// Core data structures for Animartrix2
// Extracted from animartrix_detail.hpp to enable standalone operation

#include "fl/stl/stdint.h"

// Use macro for num_oscillators to match original animartrix convention
// and avoid conflicts when both headers are included
#ifndef num_oscillators
#define num_oscillators 10
#endif

namespace fl {

struct render_parameters {
    float center_x = (999 / 2) - 0.5; // center of the matrix
    float center_y = (999 / 2) - 0.5;
    float dist = 0.0f;
    float angle = 0.0f;
    float scale_x = 0.1; // smaller values = zoom in
    float scale_y = 0.1;
    float scale_z = 0.1;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;
    float z = 0.0f;
    float low_limit = 0; // getting contrast by highering the black point
    float high_limit = 1;
};

struct oscillators {
    float master_speed = 0.0f; // global transition speed
    float offset[num_oscillators] = {};  // oscillators can be shifted by a time offset
    float ratio[num_oscillators] = {}; // speed ratios for the individual oscillators
};

struct modulators {
    float linear[num_oscillators] = {};      // returns 0 to FLT_MAX
    float radial[num_oscillators] = {};      // returns 0 to 2*PI
    float directional[num_oscillators] = {}; // returns -1 to 1
    float noise_angle[num_oscillators] = {}; // returns 0 to 2*PI
};

struct rgb {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
};

} // namespace fl
