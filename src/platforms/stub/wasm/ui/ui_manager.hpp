#pragma once

#include "ui_manager.h"
#include <emscripten.h>

FASTLED_NAMESPACE_BEGIN

inline bool jsUiManager::WeakPtrCompare::operator()(const std::weak_ptr<jsUiInternal>& lhs, const std::weak_ptr<jsUiInternal>& rhs) const {
    auto l = lhs.lock();
    auto r = rhs.lock();
    if (!l && !r) return false;
    if (!l) return true;
    if (!r) return false;
    return l->id() < r->id();
}

inline void jsUiManager::addComponent(std::weak_ptr<jsUiInternal> component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.insert(component);
    instance().mItemsAdded = true;
}

inline void jsUiManager::removeComponent(std::weak_ptr<jsUiInternal> component) {
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
    std::lock_guard<std::mutex> lock(instance().mMutex);
    for (auto it = instance().mComponents.begin(); it != instance().mComponents.end(); ) {
        if (auto component = it->lock()) {
            component->update("{}");  // Todo - replace this with actual json string data.
            ++it;
        } else {
            it = instance().mComponents.erase(it);
        }
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
    std::lock_guard<std::mutex> lock(instance().mMutex);
    for (auto it = mComponents.begin(); it != mComponents.end(); ) {
        if (auto component = it->lock()) {
            str += component->toJsonStr() + ",";
            ++it;
        } else {
            it = mComponents.erase(it);
        }
    }
    if (!mComponents.empty()) {
        str.pop_back();
    }
    str += "]";
    return str;
}

FASTLED_NAMESPACE_END
