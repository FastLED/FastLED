#pragma once

#include "ui_manager.h"
#include <emscripten.h>
#include "json.h"
#include <sstream>

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

inline void jsUiManager::updateAll(const std::string& jsonStr) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    for (auto it = instance().mComponents.begin(); it != instance().mComponents.end(); ) {
        if (auto component = it->lock()) {
            component->update(jsonStr.c_str());
            ++it;
        } else {
            it = instance().mComponents.erase(it);
        }
    }
}

inline void jsUiManager::receiveJsUpdate(const char* jsonStr) {
    updateAll(jsonStr);
}

inline void jsUiManager::updateUiComponents(const std::string& jsonStr) {
    // Check if the input is a valid JSON object or array
    printf("Debug: Received JSON string: %s\n", jsonStr.c_str());
    std::map<int, std::string> id_val_map;
    bool ok = JsonIdValueDecoder::parseJson(jsonStr.c_str(), &id_val_map);
    if (!ok) {
        printf("Error: Invalid JSON string received: %s\n", jsonStr.c_str());
        return;
    }
    /*
    for (const auto& partial : decoder.getPartials()) {
        // updateAll(partial);
    }
    */
   printf("Parse ok - implement what happens next\n");
}

inline void jsUiManager::updateJs() {
    std::string s = jsUiManager::instance().toJsonStr();
    EM_ASM_({
        globalThis.onFastLedUiElementsAdded = globalThis.onFastLedUiElementsAdded || function(jsonData, updateFunc) {
            console.log(new Date().toLocaleTimeString());
            console.log("Missing globalThis.onFastLedUiElementsAdded(jsonData, updateFunc) function");
            console.log("Added ui elements:", jsonData);
        };
        var jsonStr = UTF8ToString($0);
        // try {
        //     var data = JSON.parse(jsonStr);
        //     globalThis.onFastLedUiElementsAdded(data, function(updateData) {
        //         var updateJsonStr = JSON.stringify(updateData);
        //         _jsUiManager_updateUiComponents(updateJsonStr);
        //     });
        // } catch (error) {
        //     console.error("Error parsing JSON:", error);
        //     console.error("Problematic JSON string:", jsonStr);
        // }
        // improve this by seperating out the update function from the parsing of the JSON
        var data = null;
        try {
            data = JSON.parse(jsonStr);
        } catch (error) {
            console.error("Error parsing JSON:", error);
            console.error("Problematic JSON string:", jsonStr);
            return;
        }
        if (data) {
            globalThis.onFastLedUiElementsAdded(data);
        } else {
            console.error("Internal error, data is null");
        }

    }, s.c_str());
}

inline std::string jsUiManager::toJsonStr() {
    std::ostringstream oss;
    oss << "[";
    std::lock_guard<std::mutex> lock(instance().mMutex);
    bool first = true;
    for (auto it = mComponents.begin(); it != mComponents.end(); ) {
        if (auto component = it->lock()) {
            if (!first) {
                oss << ",";
            }
            std::string componentJson = component->toJsonStr();
            if (!componentJson.empty()) {
                oss << componentJson;
                first = false;
            } else {
                printf("Warning: Empty JSON from component\n");
            }
            ++it;
        } else {
            it = mComponents.erase(it);
        }
    }
    oss << "]";
    std::string result = oss.str();
    printf("Debug: Generated JSON: %s\n", result.c_str());
    return result;
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents", &jsUiManager::updateUiComponents);
}


FASTLED_NAMESPACE_END
