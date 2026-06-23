#pragma once

#include "fl/stl/stdint.h"

#include "crgb.h"  // IWYU pragma: keep
#include "fl/fx/detail/draw_context.h"
#include "fl/fx/detail/transition.h"  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/string.h"  // IWYU pragma: keep
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(Fx);

// Abstract base class for effects on a strip/grid of LEDs.
class Fx {
  public:
    // Alias so Fx::DrawContext keeps working in existing code.
    using DrawContext = ::fl::DrawContext;

    Fx(u16 numLeds) : mNumLeds(numLeds) {}

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

    u16 getNumLeds() const { return mNumLeds; }

  protected:
    virtual ~Fx() FL_NOEXCEPT {} // Protected destructor
    u16 mNumLeds;
};

} // namespace fl
