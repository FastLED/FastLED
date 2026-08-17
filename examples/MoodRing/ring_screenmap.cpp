// ring_screenmap.cpp
#include "ring_screenmap.h"
#include "FastLED.h"
#include "fl/math/math.h"

#ifndef TWO_PI
#define TWO_PI 6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359
#endif

fl::ScreenMap makeRingScreenMap(int numLeds, int gridWidth, int gridHeight,
                                float diameter) {
    return fl::ScreenMap(numLeds, diameter, [=](int index, fl::vec2f &pt_out) {
        float centerX = gridWidth / 2.0f;
        float centerY = gridHeight / 2.0f;
        float radius = fl::min(gridWidth, gridHeight) / 2.0f - 1;
        float angle = (TWO_PI * index) / numLeds;
        pt_out.x = centerX + fl::cos(angle) * radius;
        pt_out.y = centerY + fl::sin(angle) * radius;
    });
}
