#pragma once

// base class for all ui components

#include "singleton.h"

#include "engine_events.h"
#include "ptr.h"
#include <atomic>
#include <emscripten.h>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(jsUI)

class jsUiManager : EngineEvents::Listener {
  public:
    typedef std::set<jsUIPtr> jsUIPtrSet;
    static void addComponent(jsUIPtr component) {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        instance().mComponents.insert(component);
        instance().mItemsAdded = true;
    }

    static void removeComponent(jsUIPtr component) {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        instance().mComponents.erase(component);
    }

    void onEndFrame() override {
        if (mItemsAdded) {
            updateJs();
            mItemsAdded = false;
        }
    }

    void updateJs();

    static void updateAll();

    std::string toJsonStr();

  private:
    friend class Singleton<jsUiManager>;
    jsUiManager() {}
    ~jsUiManager() {}

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance() {
        return Singleton<jsUiManager>::instance();
    }
    bool mItemsAdded = false;
};

class jsUI : public Referent {
  public:
    jsUI(): mId(nextId()) {}
    virtual ~jsUI() {
        // Note, because this is smart pointer, the destructor will not be
        // called ever.
        jsUiManager::removeComponent(jsUIPtr::TakeOwnership(this));
    }

    virtual std::string type() const = 0;
    virtual std::string name() const = 0;
    virtual void update(const char* jsonStr) = 0;
    virtual std::string toJsonStr() const = 0;
    int id() const { return mId; }

  private:
    static int nextId() {
        static std::atomic<uint32_t> sNextId = 0;
        return sNextId++;
    }
    static std::atomic<uint32_t> sNextId;
    int mId;
};

inline std::atomic<uint32_t> jsUI::sNextId(0);

inline void jsUiManager::updateAll() {
    // Keeping a copy means that objects can potentially remove themselves
    // from the list of components during the update.
    auto copy = jsUiManager::instance().mComponents;
    {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        copy = instance().mComponents;
    }
    for (const auto &component : copy) {
        component->update("{}");  // Todo - replace this with actual json string data.
    }
}

inline void jsUiManager::updateJs() {
    std::string s = jsUiManager::instance().toJsonStr();
    EM_ASM_({
        globalThis.onFastLedUiElementsAdded = globalThis.onFastLedUiElementsAdded || function(jsonData) {
            console.log("Missing globalThis.onFastLedUiElementsAdded(uiList) function");
            console.log("Added ui elements: " + jsonData);
            console.log(jsonData);
        };
        var jsonStr = UTF8ToString($0);
        var data = JSON.parse(jsonStr);
        globalThis.onFastLedUiElementsAdded(data);
    }, s.c_str());
}

inline std::string jsUiManager::toJsonStr() {
    std::string str = "[";
    for (const auto &component : mComponents) {
        str += component->toJsonStr() + ",";
    }
    str.pop_back();
    str += "]";
    return str;
}

FASTLED_NAMESPACE_END
