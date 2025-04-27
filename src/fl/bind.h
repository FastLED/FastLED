#pragma once

#include "crgb.h"
#include "fl/ui.h"

// Convenience functions for sketches.
namespace fl {

// bind will bind the values which are updated once per frame.
// Binds a uislider to number value, which is auto updated.
template <typename T> int bind(fl::UISlider &slider, T *t) {
    int jobid = slider.onChanged([t](float value) { *t = value; });
    return jobid;
}

// Binds a UIButton to an integer-like value.
template <typename T> int bind(fl::UIButton &button, T *t) {
    int jobid = button.onChanged([&button, t]() {
        bool clicked = button.clicked();
        *t = clicked;
    });

    return jobid;
}
} // namespace fl
