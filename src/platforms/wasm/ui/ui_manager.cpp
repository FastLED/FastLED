





#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "ui_manager.h"
#include "fl/json.h"

#if FASTLED_ENABLE_JSON

namespace fl {

void UiManager::addComponent(WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
}

void UiManager::removeComponent(WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.erase(component);
}



fl::vector<jsUiInternalPtr> UiManager::getComponents() {
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

void UiManager::updateUiComponents(const char *jsonStr) {
    FLArduinoJson::JsonDocument doc;
    FLArduinoJson::DeserializationError error =
        FLArduinoJson::deserializeJson(doc, jsonStr);
    if (error) {

        FL_WARN("Error: Failed to parse JSON string: " << error.c_str());
        return;
    }
    mPendingJsonUpdate = doc;
    mHasPendingUpdate = true;
}

void UiManager::executeUiUpdates(const FLArduinoJson::JsonDocument &doc) {
    for (FLArduinoJson::JsonPairConst kv :
         doc.as<FLArduinoJson::JsonObjectConst>()) {
        int id = atoi(kv.key().c_str());
        // double loop to avoid copying the value
        for (auto it = mComponents.begin();
             it != mComponents.end();) {
            if (auto component = it->lock()) {
                ++it;
                if (component->id() == id) {
                    component->update(kv.value());
                }
            } else {
                mComponents.erase(it);
            }
        }
    }
}

void UiManager::toJson(FLArduinoJson::JsonArray &json) {
    fl::vector<jsUiInternalPtr> components = getComponents();
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

void UiManager::onEndShowLeds() {
    if (mItemsAdded) {
        FLArduinoJson::JsonDocument doc;
        FLArduinoJson::JsonArray jarray =
            doc.to<FLArduinoJson::JsonArray>();
        toJson(jarray);
        fl::string buff;
        FLArduinoJson::serializeJson(doc, buff);
        mUpdateJs(buff.c_str());
        mItemsAdded = false;
    }
}



} // namespace fl
#endif // FASTLED_ENABLE_JSON
