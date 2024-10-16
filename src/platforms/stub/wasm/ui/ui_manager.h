#pragma once

#include "engine_events.h"
#include "singleton.h"
#include <mutex>
#include <set>
#include <string>
#include <memory>
#include <map>

FASTLED_NAMESPACE_BEGIN

class jsUiInternal;

class jsUiManager : EngineEvents::Listener {
  public:
    static void addComponent(std::weak_ptr<jsUiInternal> component);
    static void removeComponent(std::weak_ptr<jsUiInternal> component);

    // Called from the JS engine.
    static void updateUiComponents(const std::string& jsonStr);

  private:
  static void executeUiUpdates(const std::string& jsonStr);
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
        if (pendingJsonUpdate.empty()) {
            return;
        }
        jsUiManager::executeUiUpdates(pendingJsonUpdate);
        pendingJsonUpdate.clear();
    }

    void onEndShowLeds() override {
        if (mItemsAdded) {
            updateJs();
            mItemsAdded = false;
        }
    }
    void updateJs();
    static void updateAllFastLedUiComponents(const std::map<int, std::string>& id_val_map);


    std::string toJsonStr();

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance();
    bool mItemsAdded = false;
    std::string pendingJsonUpdate;
};

FASTLED_NAMESPACE_END
