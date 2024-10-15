#pragma once

#include "engine_events.h"
#include "ptr.h"
#include "singleton.h"
#include <mutex>
#include <set>
#include <string>

FASTLED_NAMESPACE_BEGIN

class jsUI;
DECLARE_SMART_PTR(jsUI)

class jsUiManager : EngineEvents::Listener {
  public:
    typedef std::set<jsUIPtr> jsUIPtrSet;
    static void addComponent(jsUIPtr component);
    static void removeComponent(jsUIPtr component);
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
