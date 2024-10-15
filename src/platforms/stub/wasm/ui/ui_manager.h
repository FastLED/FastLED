#pragma once

#include "engine_events.h"
#include "singleton.h"
#include <mutex>
#include <set>
#include <string>
#include <memory>

FASTLED_NAMESPACE_BEGIN

class jsUiInternal;

class jsUiManager : EngineEvents::Listener {
  public:
    struct WeakPtrCompare {
        bool operator()(const std::weak_ptr<jsUiInternal>& lhs, const std::weak_ptr<jsUiInternal>& rhs) const;
    };

    typedef std::set<std::weak_ptr<jsUiInternal>, WeakPtrCompare> jsUIPtrSet;
    static void addComponent(std::weak_ptr<jsUiInternal> component);
    static void removeComponent(std::weak_ptr<jsUiInternal> component);
    void onEndFrame() override;
    void updateJs();
    static void updateAll();
    std::string toJsonStr();

  private:
    friend class Singleton<jsUiManager>;
    jsUiManager() {}
    ~jsUiManager() {}

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance();
    bool mItemsAdded = false;
};

FASTLED_NAMESPACE_END
