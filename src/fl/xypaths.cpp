
#include <math.h>

#include "fl/lut.h"
#include "fl/xypaths.h"

namespace fl {

pair_xy<float> HeartPath::at(float alpha) {
    // 1) raw parametric heart
    constexpr float PI = 3.14159265358979323846f;
    float t = alpha * 2.0f * PI;
    float s = sinf(t);
    float c = cosf(t);
    float xo = c * (1.0f - s);
    float yo = s * (1.0f - s);

    // 2) bounding box of (xo,yo) over t∈[0,2π]
    //    minx ≈ −1.299038, maxx ≈ +1.299038
    //    miny = −2.0,      maxy ≈ +0.25
    constexpr float minx = -1.29903805f, maxx = 1.29903805f;
    constexpr float miny = -2.0f, maxy = 0.25f;

    // 3) remap into [0,1]
    float x = (xo - minx) / (maxx - minx);
    float y = (yo - miny) / (maxy - miny);

    return {x, y};
}

} // namespace fl