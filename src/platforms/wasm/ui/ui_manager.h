#pragma once

#include <mutex>
#include <set>
#include <string>
#include <memory>
#include <map>

#include "platforms/wasm/engine_listener.h"
#include "fl/singleton.h"


#include "fl/set.h"
#include "fl/map.h"
#include "fl/ptr.h"

#include "platforms/wasm/js.h"
#include "fl/json.h"

FASTLED_NAMESPACE_BEGIN

class jsUiInternal;

class jsUiManager : fl::EngineEvents::Listener {
  public:
    static void addComponent(fl::WeakPtr<jsUiInternal> component);
    static void removeComponent(fl::WeakPtr<jsUiInternal> component);

    // Called from the JS engine.
    static void jsUpdateUiComponents(const std::string& jsonStr) { updateUiComponents(jsonStr.c_str()); }
    // Internal representation.
    static void updateUiComponents(const char* jsonStr) ;

  private:
    static void executeUiUpdates(const FLArduinoJson::JsonDocument& doc);
    typedef fl::FixedSet<fl::WeakPtr<jsUiInternal>, 64> jsUIRefSet;
    friend class fl::Singleton<jsUiManager>;
    jsUiManager() {
        fl::EngineEvents::addListener(this);
    }
    ~jsUiManager() {
        fl::EngineEvents::removeListener(this);
    }

    void onPlatformPreLoop() override {
        if (!mHasPendingUpdate) {
            return;
        }
        jsUiManager::executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }

    void onEndShowLeds() override {
        if (mItemsAdded) {
            // std::string jsonStr = toJsonStr();
            FLArduinoJson::JsonDocument doc;
            FLArduinoJson::JsonArray jarray = doc.to<FLArduinoJson::JsonArray>();
            toJson(jarray);
            fl::Str buff;
            FLArduinoJson::serializeJson(doc, buff);
            fl::updateJs(buff.c_str());
            mItemsAdded = false;
        }
    }

    std::vector<jsUiInternalPtr> getComponents();
    void toJson(FLArduinoJson::JsonArray& json);

    jsUIRefSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance();
    bool mItemsAdded = false;
    FLArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

FASTLED_NAMESPACE_END
