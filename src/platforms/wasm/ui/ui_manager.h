#pragma once

#include "fl/singleton.h"
#include "platforms/wasm/engine_listener.h"

#include "fl/map.h"
#include "fl/ptr.h"
#include "fl/set.h"

#include "fl/json.h"
#include "platforms/wasm/js.h"

namespace fl {
namespace ui_detail {

#ifdef __EMSCRIPTEN__
// use std::string when in emscripten, else the js engine can't bind to this
// class.
#include <string>
using string = std::string;
#else
using string = fl::string;
#endif

} // namespace ui_detail

class jsUiInternal;

class UiManager : fl::EngineEvents::Listener {
  public:
    UiManager() { fl::EngineEvents::addListener(this); }
    ~UiManager() { fl::EngineEvents::removeListener(this); }

    void addComponent(fl::WeakPtr<jsUiInternal> component);
    void removeComponent(fl::WeakPtr<jsUiInternal> component);

    // Internal representation.
    void updateUiComponents(const char *jsonStr);
    void executeUiUpdates(const FLArduinoJson::JsonDocument &doc);

  private:
    
    typedef fl::FixedSet<fl::WeakPtr<jsUiInternal>, 64> jsUIRefSet;

    void onPlatformPreLoop() override {
        if (!mHasPendingUpdate) {
            return;
        }
        executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }

    void onEndShowLeds() override {
        if (mItemsAdded) {
            FLArduinoJson::JsonDocument doc;
            FLArduinoJson::JsonArray jarray =
                doc.to<FLArduinoJson::JsonArray>();
            toJson(jarray);
            fl::string buff;
            FLArduinoJson::serializeJson(doc, buff);
            fl::updateJs(buff.c_str());
            mItemsAdded = false;
        }
    }

    fl::vector<jsUiInternalPtr> getComponents();
    void toJson(FLArduinoJson::JsonArray &json);

    jsUIRefSet mComponents;
    fl::mutex mMutex;

    bool mItemsAdded = false;
    FLArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

class jsUiManager : public UiManager {
  public:
    // Called from the JS engine.
    static void jsUpdateUiComponents(const fl::ui_detail::string &jsonStr) {
        instance().updateUiComponents(jsonStr.c_str());
    }
    static jsUiManager &instance();

  private:
    friend class fl::Singleton<jsUiManager>;
    jsUiManager() = default;
    ~jsUiManager() = default;
};

} // namespace fl
