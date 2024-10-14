#include "slider.h"

EMSCRIPTEN_BINDINGS(slider_constructors) {
    emscripten::class_<jsSlider>("jsSlider")
        .constructor<const std::string&, float, float, float, float>()
        .function("name", &jsSlider::name)
        .function("value", &jsSlider::value)
        .function("setValue", &jsSlider::setValue)
        .function("getId", &jsSlider::getId);
}
