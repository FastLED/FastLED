#pragma once

#include "ui_manager.h"
#include <emscripten.h>
#include "third_party/arduinojson/json.h"
#include <sstream>
#include <vector>

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

inline void jsUiManager::updateAllFastLedUiComponents(const std::map<int, std::string>& id_val_map) {
    std::vector<std::shared_ptr<jsUiInternal>> components = instance().getComponents();
    // Update components with matching ids
    for (const auto& component : components) {
        const auto& it = id_val_map.find(component->id());
        if (it != id_val_map.end()) {
            component->update(it->second.c_str());
        }
    }
}

inline void jsUiManager::updateUiComponents(const std::string& jsonStr) {
    instance().pendingJsonUpdate = jsonStr;
}

inline void jsUiManager::executeUiUpdates(const std::string& jsonStr) {
    std::map<int, std::string> id_val_map;
    ArduinoJson::DynamicJsonDocument doc(1024); // Adjust size as needed
    ArduinoJson::DeserializationError error = ArduinoJson::deserializeJson(doc, jsonStr);

    if (error) {
        printf("Error: Invalid JSON string received: %s\n", error.c_str());
        return;
    }

    for (ArduinoJson::JsonPair kv : doc.as<ArduinoJson::JsonObject>()) {
        int id = atoi(kv.key().c_str());
        id_val_map[id] = kv.value().as<std::string>();
    }

    updateAllFastLedUiComponents(id_val_map);
}

inline std::string jsUiManager::toJsonStr() {
    ArduinoJson::DynamicJsonDocument doc(4096);
    ArduinoJson::JsonArray json = doc.to<ArduinoJson::JsonArray>();
    toJson(json);
    std::string result;
    ArduinoJson::serializeJson(doc, result);
    return result;
}

inline void jsUiManager::toJson(ArduinoJson::JsonArray& json) {
    std::vector<std::shared_ptr<jsUiInternal>> components = instance().getComponents();
    // std::lock_guard<std::mutex> lock(instance().mMutex);
    // for (auto it = mComponents.begin(); it != mComponents.end(); ) {
    //     if (auto component = it->lock()) {
    //         ArduinoJson::JsonObject componentJson = json.createNestedObject();
    //         component->toJson(componentJson);
    //         if (componentJson.size() == 0) {
    //             printf("Warning: Empty JSON from component\n");
    //             json.remove(json.size() - 1);
    //         }
    //         ++it;
    //     } else {
    //         it = mComponents.erase(it);
    //     }
    // }
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
