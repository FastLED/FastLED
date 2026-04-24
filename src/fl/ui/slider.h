#pragma once

#include "fl/stl/int.h"  // IWYU pragma: keep
#include "fl/stl/function.h"
#include "fl/math/math.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/system/engine_events.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_SLIDER
#define FASTLED_HAS_UI_SLIDER 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_SLIDER

class UISliderImpl {
  public:
    // If step is -1, it will be calculated as (max - min) / 100
    UISliderImpl(const char *name, float value = 128.0f, float min = 1,
                 float max = 255, float step = -1.f)
        : mValue(value), mMin(fl::min(min, max)), mMax(fl::max(min, max)) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(step);
        if (value < min) {
            mValue = min;
        }
        if (value > max) {
            mValue = max;
        }
    }
    ~UISliderImpl() FL_NOEXCEPT {}
    float value() const { return mValue; }
    float getMax() const { return mMax; }
    float getMin() const { return mMin; }
    void setValue(float value) { mValue = fl::max(mMin, fl::min(mMax, value)); }
    operator float() const { return mValue; }
    operator u8() const { return static_cast<u8>(mValue); }
    operator u16() const { return static_cast<u16>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const { return static_cast<int>(mValue); }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

    UISliderImpl &operator=(float value) FL_NOEXCEPT {
        setValue(value);
        return *this;
    }
    UISliderImpl &operator=(int value) FL_NOEXCEPT {
        setValue(static_cast<float>(value));
        return *this;
    }

  private:
    float mValue;
    float mMin;
    float mMax;
};

#endif

class UISlider : public UIElement {
  public:
    FL_NO_COPY(UISlider)
    // If step is -1, it will be calculated as (max - min) / 100
    UISlider(const char *name, float value = 128.0f, float min = 1,
             float max = 255, float step = -1.f) FL_NOEXCEPT;
    float value() const FL_NOEXCEPT { return mImpl.value(); }
    float value_normalized() const FL_NOEXCEPT {
        float min = mImpl.getMin();
        float max = mImpl.getMax();
        if (fl::almost_equal(max, min, 0.0001f)) {
            return 0;
        }
        return (value() - min) / (max - min);
    }
    float getMax() const FL_NOEXCEPT { return mImpl.getMax(); }
    float getMin() const FL_NOEXCEPT { return mImpl.getMin(); }
    void setValue(float value) FL_NOEXCEPT;
    operator float() const FL_NOEXCEPT { return mImpl.value(); }
    operator u8() const FL_NOEXCEPT { return static_cast<u8>(mImpl.value()); }
    operator fl::u16() const FL_NOEXCEPT { return static_cast<fl::u16>(mImpl.value()); }
    operator int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }
    template <typename T> T as() const FL_NOEXCEPT {
        return static_cast<T>(mImpl.value());
    }

    int as_int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }

    UISlider &operator=(float value) FL_NOEXCEPT {
        mImpl.setValue(value);
        return *this;
    }
    UISlider &operator=(int value) FL_NOEXCEPT {
        mImpl.setValue(static_cast<float>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    int onChanged(function<void(UISlider &)> callback) FL_NOEXCEPT {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UISliderImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UISlider *owner) FL_NOEXCEPT : mOwner(owner) {

        }
        ~Listener() FL_NOEXCEPT {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() FL_NOEXCEPT {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() FL_NOEXCEPT override;

      private:
        UISlider *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UISlider &)> mCallbacks;
    float mLastFrameValue = 0;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

FASTLED_UI_DEFINE_OPERATORS(UISlider)

} // namespace fl
