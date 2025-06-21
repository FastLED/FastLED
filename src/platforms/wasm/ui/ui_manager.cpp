#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)



#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#endif

#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "ui_manager.h"


#include "fl/namespace.h"

namespace fl {

void jsUiManager::addComponent(WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
}

void jsUiManager::removeComponent(WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.erase(component);
}

jsUiManager &jsUiManager::instance() {
    return fl::Singleton<jsUiManager>::instance();
}

fl::vector<jsUiInternalPtr> jsUiManager::getComponents() {
    fl::vector<jsUiInternalPtr> components;
    {
        fl::lock_guard<fl::mutex> lock(mMutex);

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

        FL_WARN("Error: Failed to parse JSON string: " << error.c_str());
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
    fl::vector<jsUiInternalPtr> components = instance().getComponents();
    for (const auto &component : components) {
        FLArduinoJson::JsonObject componentJson =
            json.add<FLArduinoJson::JsonObject>();
        component->toJson(componentJson);
        if (componentJson.size() == 0) {
            FL_WARN("Empty JSON from component");
            json.remove(json.size() - 1);
        }
    }
}


#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUiManager::jsUpdateUiComponents);
}
#endif

} // namespace fl

#endif // __EMSCRIPTEN__
