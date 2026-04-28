#pragma once

#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/system/engine_events.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_BUTTON
#define FASTLED_HAS_UI_BUTTON 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_BUTTON

class UIButtonImpl {
  public:
    UIButtonImpl(const char *name) : mName(name ? name : "") {}
    ~UIButtonImpl() FL_NOEXCEPT {}
    bool isPressed() const { return false; }
    bool clicked() const { return false; }
    int clickedCount() const { return 0; }
    operator bool() const { return false; }
    void click() {}
    fl::string name() const { return mName; }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    fl::string mName;
};

#endif

class UIButton : public UIElement {
  public:
    FL_NO_COPY(UIButton)
    UIButton(const char *name) FL_NOEXCEPT;
    ~UIButton() FL_NOEXCEPT;
    bool isPressed() const FL_NOEXCEPT {
        if (mImpl.isPressed()) {
            return true;
        }
        if (mButtonInput) {
            return mButtonInput->isPressed();
        }
        return false;
    }
    bool clicked() const FL_NOEXCEPT {
        if (mImpl.clicked()) {
            return true;
        }
        if (mButtonInput) {
            return mButtonInput->clicked();
        }
        return false;
    }
    int clickedCount() const FL_NOEXCEPT { return mImpl.clickedCount(); }
    operator bool() const FL_NOEXCEPT { return clicked(); }
    bool value() const FL_NOEXCEPT { return clicked(); }

    void addRealButton(fl::shared_ptr<IButtonInput> button) FL_NOEXCEPT;

    void click() FL_NOEXCEPT { mImpl.click(); }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

    int onChanged(function<void(UIButton &)> callback) FL_NOEXCEPT {
        int id = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onClicked(function<void()> callback) FL_NOEXCEPT {
        int id = mCallbacks.add([callback](UIButton &btn) {
            if (btn.clicked()) {
                callback();
            }
        });
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onPressed(function<void()> callback) FL_NOEXCEPT {
        int id = mPressCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onReleased(function<void()> callback) FL_NOEXCEPT {
        int id = mReleaseCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    // Per-list removers. Each callback list maintains its own ID counter,
    // so an id returned by onChanged()/onPressed()/onReleased() is only
    // meaningful to its originating list — use the matching remover here.
    // Note: onClicked() also registers into the changed-callbacks list, so
    // ids returned by onClicked() are removed via removeClickedCallback
    // (an alias for removeChangedCallback).
    void removeChangedCallback(int id) FL_NOEXCEPT { mCallbacks.remove(id); }
    void removeClickedCallback(int id) FL_NOEXCEPT { mCallbacks.remove(id); }
    void removePressedCallback(int id) FL_NOEXCEPT { mPressCallbacks.remove(id); }
    void removeReleasedCallback(int id) FL_NOEXCEPT { mReleaseCallbacks.remove(id); }

    void clearCallbacks() FL_NOEXCEPT {
        mCallbacks.clear();
        mPressCallbacks.clear();
        mReleaseCallbacks.clear();
    }

  protected:
    UIButtonImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UIButton *owner) FL_NOEXCEPT : mOwner(owner) {
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
        UIButton *mOwner;
        bool added = false;
        bool mClickedLastFrame = false;
        bool mPressedLastFrame = false;
    };

  private:
    function_list<void(UIButton &)> mCallbacks;
    function_list<void()> mPressCallbacks;
    function_list<void()> mReleaseCallbacks;
    Listener mListener;
    fl::shared_ptr<IButtonInput> mButtonInput;
};

FASTLED_UI_DEFINE_OPERATORS(UIButton)

} // namespace fl
