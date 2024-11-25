#pragma once

#include <stdint.h>

#include "crgb.h"
#include "namespace.h"
#include "fl/ptr.h"
#include "detail/draw_context.h"
#include "detail/transition.h"
#include "fl/str.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_PTR(Fx);

// Abstract base class for effects on a strip/grid of LEDs.
class Fx : public fl::Referent {
  public:
    // Alias DrawContext for use within Fx
    using DrawContext = _DrawContext;

    Fx(uint16_t numLeds) : mNumLeds(numLeds) {}

    /// @param now The current time in milliseconds. Fx writers are encouraged
    /// to use this instead of millis() directly as this will more deterministic
    /// behavior.
    virtual void draw(DrawContext context) = 0;  // This is the only function that needs to be implemented
                                                 // everything else is optional.

    // If true then this fx has a fixed frame rate and the fps parameter will be
    // set to the frame rate.
    virtual bool hasFixedFrameRate(float *fps) const { *fps = 30; return true; }

    // Get the name of the current fx. This is the class name if there is only one.
    // -1 means to get the current fx name if there are multiple fx.
    virtual fl::Str fxName() const = 0;

    // Called when the fx is paused, usually when a transition has finished.
    virtual void pause() {} 
    virtual void resume() {} // Called when the fx is resumed after a pause,
                             // usually when a transition has started.

    uint16_t getNumLeds() const { return mNumLeds; }

  protected:
    virtual ~Fx() {} // Protected destructor
    uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END
