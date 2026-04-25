#pragma once

#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/math/math.h"
#include "fl/system/engine_events.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_NUMBER_FIELD
#define FASTLED_HAS_UI_NUMBER_FIELD 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_NUMBER_FIELD

class UINumberFieldImpl {
  public:
    UINumberFieldImpl(const char *name, double value, double min = 0,
                      double max = 100)
        : mMin(fl::min(min, max)),
          mMax(fl::max(min, max)),
          mValue(fl::max(mMin, fl::min(mMax, value))) {
        FASTLED_UNUSED(name);
    }
    ~UINumberFieldImpl() FL_NOEXCEPT {}
    double value() const { return mValue; }
    void setValue(double value) { mValue = fl::max(mMin, fl::min(mMax, value)); }
    operator double() const { return mValue; }
    operator int() const { return static_cast<int>(mValue); }
    UINumberFieldImpl &operator=(double value) FL_NOEXCEPT {
        setValue(value);
        return *this;
    }
    UINumberFieldImpl &operator=(int value) FL_NOEXCEPT {
        setValue(static_cast<double>(value));
        return *this;
    }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    double mMin;
    double mMax;
    double mValue;
};

#endif

class UINumberField : public UIElement {
  public:
    FL_NO_COPY(UINumberField);
    UINumberField(const char *name, double value, double min = 0,
                  double max = 100) FL_NOEXCEPT;
    ~UINumberField() FL_NOEXCEPT;
    double value() const FL_NOEXCEPT { return mImpl.value(); }
    void setValue(double value) FL_NOEXCEPT { mImpl.setValue(value); }
    operator double() const FL_NOEXCEPT { return mImpl.value(); }
    operator int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }
    UINumberField &operator=(double value) FL_NOEXCEPT {
        setValue(value);
        return *this;
    }
    UINumberField &operator=(int value) FL_NOEXCEPT {
        setValue(static_cast<double>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UINumberField &)> callback) FL_NOEXCEPT {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UINumberFieldImpl mImpl;

  private:
    struct Listener : public EngineEvents::Listener {
        Listener(UINumberField *owner) FL_NOEXCEPT : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
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
        UINumberField *mOwner;
        bool added = false;
    };

    Listener mListener;
    double mLastFrameValue = 0;
    bool mLastFrameValueValid = false;
    function_list<void(UINumberField &)> mCallbacks;
};

FASTLED_UI_DEFINE_OPERATORS(UINumberField)

} // namespace fl
