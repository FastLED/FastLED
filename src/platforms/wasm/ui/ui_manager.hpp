#pragma once

#include "ui_manager.h"
#include <emscripten.h>
#include "third_party/arduinojson/json.h"
#include <sstream>
#include <vector>
#include "fixed_map.h"

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

inline jsUiManager& jsUiManager::instance() {
    return Singleton<jsUiManager>::instance();
}

inline std::vector<std::shared_ptr<jsUiInternal>> jsUiManager::getComponents() {
    std::vector<std::shared_ptr<jsUiInternal>> components;
    {
        std::lock_guard<std::mutex> lock(mMutex);

        components.reserve(mComponents.size());
        for (auto it = mComponents.begin(); it != mComponents.end(); ) {
            if (auto component = it->lock()) {
                components.push_back(component);
                ++it;
            } else {
                it = mComponents.erase(it);
            }
        }
    }
    return components;
}

// inline void jsUiManager::updateAllFastLedUiComponents(const std::map<int, std::string>& id_val_map) {
//     std::vector<std::shared_ptr<jsUiInternal>> components = instance().getComponents();
//     // Update components with matching ids
//     for (const auto& component : components) {
//         const auto& it = id_val_map.find(component->id());
//         if (it != id_val_map.end()) {
//             component->update(it->second.c_str());
//         }
//     }
// }

void jsUiManager::updateAllFastLedUiComponents(const FixedMap<int, std::string, 128>& id_val_map) {
    std::vector<std::shared_ptr<jsUiInternal>> components = jsUiManager::instance().getComponents();
    // Update components with matching ids
    for (const auto& component : components) {
        const auto& it = id_val_map.find(component->id());
        if (it != id_val_map.end()) {
            component->update(it->second.c_str());
        }
    }
}

inline void jsUiManager::updateUiComponents(const std::string& jsonStr) {
    const char* cstr = jsonStr.c_str();
    ArduinoJson::JsonDocument doc;
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, cstr);
    if (error) {
        printf("Error: Failed to parse JSON string: %s\n", error.c_str());
        return;
    }
    auto& self = instance();
    self.mPendingJsonUpdate = doc;
    self.mHasPendingUpdate = true;
}

inline void jsUiManager::executeUiUpdates(const ArduinoJson::JsonDocument& doc) {

    FixedMap<int, std::string, 128> id_val_map;
    // std::map<int, std::string> id_val_map;
    for (ArduinoJson::JsonPairConst kv : doc.as<ArduinoJson::JsonObjectConst>()) {
        int id = atoi(kv.key().c_str());
        id_val_map[id] = kv.value().as<std::string>();
    }

    updateAllFastLedUiComponents(id_val_map);
}

inline void jsUiManager::toJson(ArduinoJson::JsonArray& json) {
    std::vector<std::shared_ptr<jsUiInternal>> components = instance().getComponents();
    for (const auto& component : components) {
        ArduinoJson::JsonObject componentJson = json.add<ArduinoJson::JsonObject>();
        component->toJson(componentJson);
        if (componentJson.size() == 0) {
            printf("Warning: Empty JSON from component\n");
            json.remove(json.size() - 1);
        }
    }
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents", &jsUiManager::updateUiComponents);
}


FASTLED_NAMESPACE_END
