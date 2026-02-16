#pragma once

// Context: All shared state for animations, passed to free-function visualizers.
// Extracted from animartrix2_detail.hpp

#include "crgb.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"

namespace animartrix2_detail {

// Forward declaration
struct Context;

// Visualizer: a free function that operates on Context to render one frame
using Visualizer = void (*)(Context &ctx);

// Callback for mapping (x,y) to a 1D LED index
using XYMapCallback = fl::u16 (*)(fl::u16 x, fl::u16 y, void *userData);

// Context: All shared state for animations, passed to free-function visualizers.
// Internally wraps an animartrix_detail::ANIMartRIX to reuse existing logic.
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
    struct Engine;
    Engine *mEngine = nullptr;

    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
};

}  // namespace animartrix2_detail
