// ok no header - implementation for fl/channels/pipeline_binding.h

#include "fl/channels/pipeline_binding.h"

namespace fl {

ColorPipelineHooks& colorPipelineHooks() FL_NO_EXCEPT {
    // Not a function-local static with a non-trivial constructor: this is a
    // zero-initialized aggregate, so there is no guard variable and no
    // Teensy 3.x `__cxa_guard` conflict.
    static ColorPipelineHooks hooks = {};
    return hooks;
}

#if FL_COLOR_PIPELINE_SHARED
PowerFrameHooks& powerFrameHooks() FL_NO_EXCEPT {
    static PowerFrameHooks hooks = {};
    return hooks;
}
#endif

void notifyColorProfileClearedByLegacy() FL_NO_EXCEPT {
    // Null unless a profile was ever bound, and nothing can have been
    // cleared in that case either.
    void (*notify)() = colorPipelineHooks().notifyProfileClearedByLegacy;
    if (notify != nullptr) {
        notify();
    }
}

}  // namespace fl
