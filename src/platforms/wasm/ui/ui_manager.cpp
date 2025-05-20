#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include <sstream>
#include <vector>

#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "ui_manager.h"

#include <emscripten.h>
#include <sstream>
#include <vector>

#include "fl/namespace.h"

namespace fl {

void jsUiManager::addComponent(WeakPtr<jsUiInternal> component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.insert(component);
    instance().mItemsAdded = true;
}

void jsUiManager::removeComponent(WeakPtr<jsUiInternal> component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.erase(component);
}

jsUiManager &jsUiManager::instance() {
    return fl::Singleton<jsUiManager>::instance();
}

std::vector<jsUiInternalPtr> jsUiManager::getComponents() {
    std::vector<jsUiInternalPtr> components;
    {
        std::lock_guard<std::mutex> lock(mMutex);

        components.reserve(mComponents.size());
        for (auto it = mComponents.begin(); it != mComponents.end();) {
            if (auto component = it->lock()) {
                components.push_back(component);
                ++it;
            } else {
                mComponents.erase(it);
            }
        }
    }
    return components;
}

void jsUiManager::updateUiComponents(const char *jsonStr) {
    FLArduinoJson::JsonDocument doc;
    FLArduinoJson::DeserializationError error =
        FLArduinoJson::deserializeJson(doc, jsonStr);
    if (error) {
        printf("Error: Failed to parse JSON string: %s\n", error.c_str());
        return;
    }
    auto &self = instance();
    self.mPendingJsonUpdate = doc;
    self.mHasPendingUpdate = true;
}

void jsUiManager::executeUiUpdates(const FLArduinoJson::JsonDocument &doc) {
    auto &self = instance();
    for (FLArduinoJson::JsonPairConst kv :
         doc.as<FLArduinoJson::JsonObjectConst>()) {
        int id = atoi(kv.key().c_str());
        // double loop to avoid copying the value
        for (auto it = self.mComponents.begin();
             it != self.mComponents.end();) {
            if (auto component = it->lock()) {
                ++it;
                if (component->id() == id) {
                    component->update(kv.value());
                }
            } else {
                self.mComponents.erase(it);
            }
        }
    }
}

void jsUiManager::toJson(FLArduinoJson::JsonArray &json) {
    std::vector<jsUiInternalPtr> components = instance().getComponents();
    for (const auto &component : components) {
        FLArduinoJson::JsonObject componentJson =
            json.add<FLArduinoJson::JsonObject>();
        component->toJson(componentJson);
        if (componentJson.size() == 0) {
            printf("Warning: Empty JSON from component\n");
            json.remove(json.size() - 1);
        }
    }
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUiManager::jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
