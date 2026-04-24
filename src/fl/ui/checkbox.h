#pragma once

#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/system/engine_events.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_CHECKBOX
#define FASTLED_HAS_UI_CHECKBOX 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_CHECKBOX

class UICheckboxImpl {
  public:
    UICheckboxImpl(const char *name, bool value = false) : mValue(value) {
        FASTLED_UNUSED(name);
    }
    ~UICheckboxImpl() FL_NOEXCEPT {}
    operator bool() const { return mValue; }
    explicit operator int() const { return mValue ? 1 : 0; }
    UICheckboxImpl &operator=(bool value) FL_NOEXCEPT {
        setValue(value);
        return *this;
    }
    UICheckboxImpl &operator=(int value) FL_NOEXCEPT {
        setValue(value != 0);
        return *this;
    }
    bool value() const { return mValue; }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    void setValue(bool value) { mValue = value; }
    bool mValue;
};

#endif

class UICheckbox : public UIElement {
  public:
    FL_NO_COPY(UICheckbox);
    UICheckbox(const char *name, bool value = false) FL_NOEXCEPT;
    ~UICheckbox() FL_NOEXCEPT;

    operator bool() const FL_NOEXCEPT { return value(); }
    explicit operator int() const FL_NOEXCEPT { return static_cast<int>(value()); }
    UICheckbox &operator=(bool value) FL_NOEXCEPT {
        mImpl = value;
        return *this;
    }
    bool value() const FL_NOEXCEPT { return mImpl.value(); }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UICheckbox &)> callback) FL_NOEXCEPT {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UICheckboxImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UICheckbox *owner) FL_NOEXCEPT : mOwner(owner) {
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
        UICheckbox *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UICheckbox &)> mCallbacks;
    bool mLastFrameValue = false;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

FASTLED_UI_DEFINE_OPERATORS(UICheckbox)

} // namespace fl
