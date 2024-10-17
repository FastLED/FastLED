#pragma once

#include "engine_events.h"
#include "singleton.h"
#include <mutex>
#include <set>
#include <string>
#include <memory>
#include <map>

#include "platforms/wasm/js.h"
#include "third_party/arduinojson/json.h"

FASTLED_NAMESPACE_BEGIN

class jsUiInternal;

class jsUiManager : EngineEvents::Listener {
  public:
    static void addComponent(std::weak_ptr<jsUiInternal> component);
    static void removeComponent(std::weak_ptr<jsUiInternal> component);

    // Called from the JS engine.
    static void updateUiComponents(const std::string& jsonStr);

  private:
   static void executeUiUpdates(const ArduinoJson::JsonDocument& doc);
    struct WeakPtrCompare {
        bool operator()(const std::weak_ptr<jsUiInternal>& lhs, const std::weak_ptr<jsUiInternal>& rhs) const;
    };
    typedef std::set<std::weak_ptr<jsUiInternal>, WeakPtrCompare> jsUIPtrSet;
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
            std::string jsonStr = toJsonStr();
            updateJs(jsonStr.c_str());
            mItemsAdded = false;
        }
    }
    static void updateAllFastLedUiComponents(const std::map<int, std::string>& id_val_map);

    std::vector<std::shared_ptr<jsUiInternal>> getComponents();


    std::string toJsonStr();
    void toJson(ArduinoJson::JsonArray& json);

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance();
    bool mItemsAdded = false;
    ArduinoJson::JsonDocument mPendingJsonUpdate;
    bool mHasPendingUpdate = false;
};

FASTLED_NAMESPACE_END
