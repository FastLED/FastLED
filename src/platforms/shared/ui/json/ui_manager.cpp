#include "fl/json.h"
#include "fl/map.h"
#include "fl/mutex.h"
#include "fl/namespace.h"
#include "ui_manager.h"
#include "fl/json.h"
#include "fl/compiler_control.h"


FL_DISABLE_WARNING(deprecated-declarations)


#if FASTLED_ENABLE_JSON

namespace fl {

void JsonUiManager::addComponent(fl::WeakPtr<JsonUiInternal> component) {
    fl::scoped_lock lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
}

void JsonUiManager::removeComponent(fl::WeakPtr<JsonUiInternal> component) {
    fl::scoped_lock lock(mMutex);
    mComponents.erase(component);
}

fl::vector<JsonUiInternalPtr> JsonUiManager::getComponents() {
    fl::scoped_lock lock(mMutex);
    fl::vector<JsonUiInternalPtr> out;
    for (auto &component : mComponents) {
        if (auto ptr = component.lock()) {
            out.push_back(ptr);
        }
    }
    return out;
}

void JsonUiManager::updateUiComponents(const char *jsonStr) {
    FLArduinoJson::JsonDocument doc;
    deserializeJson(doc, jsonStr);
    mPendingJsonUpdate = fl::move(doc);
    mHasPendingUpdate = true;
}

void JsonUiManager::executeUiUpdates(const FLArduinoJson::JsonDocument &doc) {
    auto components = getComponents();
    if (doc.is<FLArduinoJson::JsonObject>()) {
        auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
        for (auto &component : components) {
            int id = component->id();
            string idStr = "id_";
            idStr.append(id);
            if (obj.containsKey(idStr.c_str())) {
                component->update(obj[idStr.c_str()]);
            }
        }
    }
}

void JsonUiManager::toJson(FLArduinoJson::JsonArray &json) {
    auto components = getComponents();
    for (auto &component : components) {
        auto obj = json.add<FLArduinoJson::JsonObject>();
        component->toJson(obj);
    }
}

void JsonUiManager::onEndShowLeds() {
    bool shouldUpdate = false;
    {
        fl::scoped_lock lock(mMutex);
        shouldUpdate = mItemsAdded;
        mItemsAdded = false;
    }
    if (shouldUpdate) {
        FLArduinoJson::JsonDocument doc;
        auto json = doc.to<FLArduinoJson::JsonArray>();
        toJson(json);
        string jsonStr;
        serializeJson(doc, jsonStr);
        mUpdateJs(jsonStr.c_str());
    }
}

} // namespace fl
#endif // FASTLED_ENABLE_JSON
