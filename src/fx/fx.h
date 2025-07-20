#pragma once

#include "fl/stdint.h"

#include "crgb.h"
#include "detail/draw_context.h"
#include "detail/transition.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/str.h"
#include "fl/unused.h"

namespace fl {

FASTLED_SMART_PTR(Fx);

// Abstract base class for effects on a strip/grid of LEDs.
class Fx {
  public:
    // Alias DrawContext for use within Fx
    using DrawContext = _DrawContext;

    Fx(uint16_t numLeds) : mNumLeds(numLeds) {}

    /// @param now The current time in milliseconds. Fx writers are encouraged
    /// to use this instead of millis() directly as this will more deterministic
    /// behavior.
    virtual void
    draw(DrawContext context) = 0; // This is the only function that needs to be
                                   // implemented everything else is optional.

    // If true then this fx has a fixed frame rate and the fps parameter will be
    // set to the frame rate.
    virtual bool hasFixedFrameRate(float *fps) const {
        FASTLED_UNUSED(fps);
        return false;
    }

    // Get the name of the current fx.
    virtual fl::string fxName() const = 0;

    // Called when the fx is paused, usually when a transition has finished.
    virtual void pause(fl::u32 now) { FASTLED_UNUSED(now); }
    virtual void resume(fl::u32 now) {
        FASTLED_UNUSED(now);
    } // Called when the fx is resumed after a pause,
      // usually when a transition has started.

    uint16_t getNumLeds() const { return mNumLeds; }

  protected:
    virtual ~Fx() {} // Protected destructor
    uint16_t mNumLeds;
};

} // namespace fl
