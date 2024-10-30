#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#include <sstream>
#include <vector>



#include "ui_manager.h"
#include "fixed_map.h"
#include "json.h"
#include "namespace.h"


#include <emscripten.h>
#include <sstream>
#include <vector>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

void jsUiManager::addComponent(WeakRef<jsUiInternal> component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.insert(component);
    instance().mItemsAdded = true;
}

void jsUiManager::removeComponent(WeakRef<jsUiInternal> component) {
    std::lock_guard<std::mutex> lock(instance().mMutex);
    instance().mComponents.erase(component);
}

jsUiManager &jsUiManager::instance() {
    return Singleton<jsUiManager>::instance();
}

std::vector<jsUiInternalRef> jsUiManager::getComponents() {
    std::vector<jsUiInternalRef> components;
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
    std::vector<jsUiInternalRef> components = instance().getComponents();
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

FASTLED_NAMESPACE_END

#endif // __EMSCRIPTEN__
