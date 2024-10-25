#pragma once

#include "engine_events.h"
#include "singleton.h"
#include <mutex>
#include <set>
#include <string>
#include <memory>
#include <map>

#include "fixed_set.h"
#include "fixed_map.h"
#include "ptr.h"

#include "platforms/wasm/js.h"
#include "json.h"

FASTLED_NAMESPACE_BEGIN

class jsUiInternal;

class jsUiManager : EngineEvents::Listener {
  public:
    static void addComponent(WeakPtr<jsUiInternal> component);
    static void removeComponent(WeakPtr<jsUiInternal> component);

    // Called from the JS engine.
    static void jsUpdateUiComponents(const std::string& jsonStr) { updateUiComponents(jsonStr.c_str()); }
    // Internal representation.
    static void updateUiComponents(const char* jsonStr) ;

  private:
    static void executeUiUpdates(const ArduinoJson::JsonDocument& doc);
    typedef FixedSet<WeakPtr<jsUiInternal>, 64> jsUIPtrSet;
    friend class Singleton<jsUiManager>;
    jsUiManager() {
        EngineEvents::addListener(this);
    }
    ~jsUiManager() {
        EngineEvents::removeListener(this);
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
            ArduinoJson::JsonDocument doc;
            ArduinoJson::JsonArray jarray = doc.to<ArduinoJson::JsonArray>();
            toJson(jarray);
            Str buff;
            ArduinoJson::serializeJson(doc, buff);
            updateJs(buff.c_str());
            mItemsAdded = false;
        }
    }

    std::vector<jsUiInternalPtr> getComponents();
    void toJson(ArduinoJson::JsonArray& json);

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance();
    bool mItemsAdded = false;
    ArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

FASTLED_NAMESPACE_END
