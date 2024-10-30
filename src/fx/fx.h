#pragma once

#include <stdint.h>

#include "crgb.h"
#include "namespace.h"
#include "ref.h"
#include "detail/draw_context.h"
#include "detail/transition.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(Fx);

// Abstract base class for effects on a strip/grid of LEDs.
class Fx : public Referent {
  public:
    // Alias DrawContext for use within Fx
    using DrawContext = _DrawContext;

    Fx(uint16_t numLeds) : mNumLeds(numLeds) {}

    /// @param now The current time in milliseconds. Fx writers are encouraged
    /// to use this instead of millis() directly as this will more deterministic
    /// behavior.
    virtual void draw(DrawContext context) = 0;  // This is the only function that needs to be implemented
                                                 // everything else is optional.

    // capabilities
    virtual bool hasAlphaChannel() const { return false; }

    // If true then this fx has a fixed frame rate and the fps parameter will be
    // set to the frame rate.
    virtual bool hasFixedFrameRate(float *fps) const { *fps = 30; return true; }

    // Get the name of the current fx. This is the class name if there is only one.
    // -1 means to get the current fx name if there are multiple fx.
    virtual const char * fxName(int which = -1) const = 0;
    // Optionally implement these for multi fx classes.
    virtual int fxNum() const {
        return 1;
    }; // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {}; // Set the current fx number.

     // Negative numbers are allowed. -1 means previous fx.
    virtual void fxNext(int fx = 1) {};
    virtual int fxGet() const { return 0; }; // Get the current fx number.

    virtual void pause() {
    } // Called when the fx is paused, usually when a transition has finished.
    virtual void resume() {} // Called when the fx is resumed after a pause,
                             // usually when a transition has started.

    virtual void destroy() {
        delete this;
    } // Public virtual destructor function
    virtual void lazyInit() {}
    uint16_t getNumLeds() const { return mNumLeds; }

  protected:
    virtual ~Fx() {} // Protected destructor
    uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END
