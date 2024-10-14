#pragma once

// base class for all ui components

#include "singleton.h"

#include <stdint.h>
#include <set>
#include <mutex>
#include <string>
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(jsUI)

class jsUiManager {
  public:
    typedef std::set<jsUIPtr> jsUIPtrSet;
    static void addComponent(jsUIPtr component) {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        instance().mComponents.insert(component);
    }

    static void removeComponent(jsUIPtr component) {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        instance().mComponents.erase(component);
    }

    static void updateAll();

  private:
    friend class Singleton<jsUiManager>;
    jsUiManager() {}
    ~jsUiManager() {}

    jsUIPtrSet mComponents;
    std::mutex mMutex;

    static jsUiManager &instance() {
        return Singleton<jsUiManager>::instance();
    }
};

class jsUI : public Referent {
  public:
    virtual ~jsUI() {
        // Note, because this is smart pointer, the destructor will not be called
        // ever.
        jsUiManager::removeComponent(jsUIPtr::TakeOwnership(this));
    }
    
    virtual std::string type() const = 0;
    virtual void update() = 0;
};



inline void jsUiManager::updateAll() {
    // Keeping a copy means that objects can potentially remove themselves
    // from the list of components during the update.
    auto copy = jsUiManager::instance().mComponents;
    {
        std::lock_guard<std::mutex> lock(instance().mMutex);
        copy = instance().mComponents;
    }
    for (const auto& component : copy) {
        component->update();
    }
}

FASTLED_NAMESPACE_END
