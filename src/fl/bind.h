#pragma once

#include "crgb.h"
#include "fl/ui.h"

// Convenience functions for sketches.
namespace fl {

// bind will bind the values which are updated once per frame.
// Binds a uislider to number value, which is auto updated.
template <typename T> int bind(fl::UISlider &slider, T *t) {
    slider.onChanged([t](float value) { *t = value; });
    return 0;
}

// Binds a UIButton to an integer-like value.
template <typename T> int bind(fl::UIButton &button, T *t) {
    button.onChanged([&button, t]() {
        bool clicked = button.clicked();
        *t = clicked;
    });

    return 0;
}
} // namespace fl
