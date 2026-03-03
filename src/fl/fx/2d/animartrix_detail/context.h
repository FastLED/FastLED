#pragma once

// Context: All shared state for animations, passed to free-function visualizers.
// Extracted from animartrix_detail.hpp

#include "crgb.h"
#include "fl/stl/optional.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/stdint.h"

namespace fl {

// Forward declarations
struct Context;
struct Engine;

// Callback for mapping (x,y) to a 1D LED index
using XYMapCallback = fl::u16 (*)(fl::u16 x, fl::u16 y, void *userData);

// Context: All shared state for animations, passed to free-function visualizers.
// Internally wraps an Engine to reuse existing logic.
struct Context {
    // Grid dimensions
    int num_x = 0;
    int num_y = 0;

    // Output target
    CRGB *leds = nullptr;
    XYMapCallback xyMapFn = nullptr;
    void *xyMapUserData = nullptr;

    // Time
    fl::optional<fl::u32> currentTime;

    // Internal engine (reuses original implementation for bit-identical output)
    fl::unique_ptr<Engine> mEngine;

    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
};

}  // namespace fl
