#pragma once

// base class for all ui components

#include "singleton.h"

#include "engine_events.h"
#include "ptr.h"
#include <atomic>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>

FASTLED_NAMESPACE_BEGIN

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

class jsUI : public Referent {
  public:
    jsUI();
    virtual ~jsUI();

    virtual std::string type() const = 0;
    virtual std::string name() const = 0;
    virtual void update(const char* jsonStr) = 0;
    virtual std::string toJsonStr() const = 0;
    int id() const;

  private:
    static int nextId();
    static std::atomic<uint32_t> sNextId;
    int mId;
};

FASTLED_NAMESPACE_END
