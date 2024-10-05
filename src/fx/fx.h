#pragma once

#include "crgb.h"
#include "namespace.h"
#include "ptr.h"
#include "util/draw_context.h"
#include "util/transition.h"
#include <stdint.h>
FASTLED_NAMESPACE_BEGIN

// FT_PTR(MyFx) will produce MyFxPtr type.
#define FX_PTR(FxClassName)                                                    \
    class FxClassName;                                                         \
    typedef RefPtr<FxClassName> FxClassName##Ptr;

#define FX_PROTECTED_DESTRUCTOR(FxClassName)                                           \
  protected:                                                                   \
    virtual ~FxClassName()



template<typename T>
struct ref_unwrapper {
    using type = T;
    using ref_type = RefPtr<T>;
};

// specialization for RefPtr<RefPtr<T>>
template<typename T>
struct ref_unwrapper<RefPtr<T>> {
    using type = T;
    using ref_type = RefPtr<T>;
};

// Abstract base class for effects on a strip/grid of LEDs.
class Fx : public Referent {
  public:
    // Alias DrawContext for use within Fx
    using DrawContext = ::DrawContext;

    // Works for the intended use case of:
    //  RefPtr<Fx> fx = Fx::make<MyFx>(args...);
    // And also for the common accidental case of:
    //  RefPtr<Fx> fx = Fx::make<RefPtr<Fx>>();
    template <typename T, typename... Args>
    static typename ref_unwrapper<T>::ref_type make(Args... args) {
        using U = typename ref_unwrapper<T>::type;
        return RefPtr<U>::FromHeap(new U(args...));
    }


    Fx(uint16_t numLeds) : mNumLeds(numLeds) {}

    /// @param now The current time in milliseconds. Fx writers are encouraged
    /// to use this instead of millis() directly as this will more deterministic
    /// behavior.
    virtual void draw(DrawContext context) = 0;

    // capabilities
    virtual bool hasAlphaChannel() const { return false; }

    // If true then this fx has a fixed frame rate and the fps parameter will be
    // set to the frame rate.
    virtual bool hasFixedFrameRate(uint8_t *fps) const { return false; }

    virtual const char *
    fxName() const = 0; // Get the name of the current fx. This is the class
                        // name if there is only one.
    // Optionally implement these for multi fx classes.
    virtual int fxNum() const {
        return 1;
    }; // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {}; // Set the current fx number.
    virtual void fxNext(int fx = 1) {
    }; // Negative numbers are allowed. -1 means previous fx.
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
    // protect operator new so that it has to go through Fx
    void *operator new(size_t size) { return ::operator new(size); }
    // protect operator delete so that it has to go through Fx
    void operator delete(void *ptr) { ::operator delete(ptr); }
    virtual ~Fx() {} // Protected destructor
    uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END
