#pragma once

#include <stdint.h>
#include "namespace.h"
#include "crgb.h"
#include "util/transition.h"
#include "util/draw_context.h"
#include "ptr.h"
FASTLED_NAMESPACE_BEGIN


// Abstract base class for effects on a strip/grid of LEDs.
class Fx: public Referent {
  public:
    // Alias DrawContext for use within Fx
    using DrawContext = ::DrawContext;

    template<typename T, typename... Args>
    static RefPtr<T> make(Args... args) {
        return RefPtr<T>::FromHeap(new T(args...));
    }

    // Specialization for RefPtr to unwrap it
    template<typename T, typename... Args>
    static RefPtr<T> make(RefPtr<T>, Args... args) {
        return make<T>(args...);
    }



    Fx(uint16_t numLeds): mNumLeds(numLeds) {}

    /// @param now The current time in milliseconds. Fx writers are encouraged to use this instead of millis() directly
    /// as this will more deterministic behavior.
    virtual void draw(DrawContext context) = 0;
   
    // capabilities
    virtual bool hasAlphaChannel() const { return false; }

    // If true then this fx has a fixed frame rate and the fps parameter will be set to the frame rate.
    virtual bool hasFixedFrameRate(uint8_t* fps) const { return false; }


    virtual const char* fxName() const = 0;  // Get the name of the current fx. This is the class name if there is only one.
    // Optionally implement these for multi fx classes.
    virtual int fxNum() const { return 1; };  // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {};  // Set the current fx number.
    virtual void fxNext(int fx = 1) {};  // Negative numbers are allowed. -1 means previous fx.
    virtual int fxGet() const { return 0; };  // Get the current fx number.

    virtual void pause() {}  // Called when the fx is paused, usually when a transition has finished.
    virtual void resume() {}  // Called when the fx is resumed after a pause, usually when a transition has started.

    virtual void destroy() { delete this; }  // Public virtual destructor function
    virtual void lazyInit() {}
    uint16_t getNumLeds() const { return mNumLeds; }

protected:
    // protect operator new so that it has to go through Fx
    void* operator new(size_t size) { return ::operator new(size); }
    // protect operator delete so that it has to go through Fx
    void operator delete(void* ptr) { ::operator delete(ptr); }
    virtual ~Fx() {}  // Protected destructor
    uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END

