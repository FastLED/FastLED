#pragma once

#include "ui.h"
#include <emscripten.h>

FASTLED_NAMESPACE_BEGIN

inline void jsUiManager::addComponent(jsUIPtr component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.insert(component);
    instance().mItemsAdded = true;
}

inline void jsUiManager::removeComponent(jsUIPtr component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.erase(component);
}

inline void jsUiManager::onEndFrame() {
    if (mItemsAdded) {
        updateJs();
        mItemsAdded = false;
    }
}

inline jsUiManager& jsUiManager::instance() {
    return Singleton<jsUiManager>::instance();
}

inline void jsUiManager::updateAll() {
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
    if (!mComponents.empty()) {
        str.pop_back();
    }
    str += "]";
    return str;
}

inline jsUI::jsUI() : mId(nextId()) {}

inline jsUI::~jsUI() {
    jsUiManager::removeComponent(jsUIPtr::TakeOwnership(this));
}

inline int jsUI::id() const {
    return mId;
}

inline int jsUI::nextId() {
    static std::atomic<uint32_t> sNextId(0);
    return sNextId++;
}

FASTLED_NAMESPACE_END
