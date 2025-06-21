#pragma once

#include <stdint.h>

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsButtonImpl {
  public:
    jsButtonImpl(const fl::string &name);
    ~jsButtonImpl();


    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    bool isPressed() const;
    bool clicked() const;
    int clickedCount() const { return mClickedCount; }
    const fl::string &groupName() const { return mInternal->group(); }

    void click() { mPressed = true; }

  private:
    struct Updater : fl::EngineEvents::Listener {
        void init(jsButtonImpl *owner) {
            mOwner = owner;
            fl::EngineEvents::addListener(this);
        }
        ~Updater() { fl::EngineEvents::removeListener(this); }
        void onPlatformPreLoop2() override {
            mOwner->mClickedHappened =
                mOwner->mPressed && (mOwner->mPressed != mOwner->mPressedLast);
            mOwner->mPressedLast = mOwner->mPressed;
            if (mOwner->mClickedHappened) {
                mOwner->mClickedCount++;
            }
        }
        jsButtonImpl *mOwner = nullptr;
    };

    Updater mUpdater;

    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    bool mPressed = false;
    bool mPressedLast = false;
    bool mClickedHappened = false;
    int mClickedCount = 0;
};

} // namespace fl
