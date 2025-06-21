#include "fl/json.h"
#include "fl/map.h"
#include "fl/mutex.h"
#include "fl/namespace.h"
#include "ui_manager.h"
#include "fl/compiler_control.h"
#include "fl/warn.h"

FL_DISABLE_WARNING(deprecated-declarations)

#if FASTLED_ENABLE_JSON

namespace fl {


// Constructor is inline in header, just add logging to destructor

JsonUiManager::~JsonUiManager() {
    FL_WARN("*** JsonUiManager: DESTRUCTOR CALLED ***");
    FL_WARN("*************************************************************");
    FL_WARN("*** CRITICAL ERROR: JsonUiManager DESTRUCTOR IS RUNNING! ***");
    FL_WARN("*** THIS SHOULD NOT HAPPEN DURING UI UPDATES!             ***");
    FL_WARN("*** THE UI MANAGER IS BEING DESTROYED AND RECREATED!      ***");
    FL_WARN("*** THIS IS THE ROOT CAUSE OF THE UI UPDATE BUG!          ***");
    FL_WARN("*************************************************************");
    // FL_WARN("*** STACK TRACE: JsonUiManager destructor at " << this);
    FL_WARN("*** Component count being lost: " << mComponents.size());
    FL_WARN("*************************************************************");
    fl::EngineEvents::removeListener(this);
}

void JsonUiManager::addComponent(fl::WeakPtr<JsonUiInternal> component) {
    fl::lock_guard lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
    FL_WARN("*** COMPONENT REGISTERED: ID " << (component.lock() ? component.lock()->id() : -1) << " (Total: " << mComponents.size() << ")");
}

void JsonUiManager::removeComponent(fl::WeakPtr<JsonUiInternal> component) {
    fl::lock_guard lock(mMutex);
    mComponents.erase(component);
}

fl::vector<JsonUiInternalPtr> JsonUiManager::getComponents() {
    fl::lock_guard lock(mMutex);
    fl::vector<JsonUiInternalPtr> out;
    for (auto &component : mComponents) {
        if (auto ptr = component.lock()) {
            out.push_back(ptr);
        }
    }
    return out;
}

void JsonUiManager::updateUiComponents(const char* jsonStr) {
    FL_WARN("*** JsonUiManager::updateUiComponents ENTRY ***");
    FL_WARN("*** INCOMING JSON: " << (jsonStr ? jsonStr : "NULL"));
    FL_WARN("*** JSON LENGTH: " << (jsonStr ? strlen(jsonStr) : 0));
    FL_WARN("*** CURRENT COMPONENT COUNT: " << mComponents.size());
    
    if (!jsonStr) {
        FL_WARN("*** JsonUiManager::updateUiComponents: NULL JSON string provided");
        return;
    }

    FL_WARN("*** BACKEND RECEIVED UI UPDATE: " << (jsonStr ? jsonStr : "NULL"));
    FL_WARN("*** JsonUiManager pointer: " << this);
    FL_WARN("*** BEFORE: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    
    FLArduinoJson::JsonDocument doc;
    auto result = deserializeJson(doc, jsonStr);
    FL_WARN("*** JSON PARSE RESULT: " << (result == FLArduinoJson::DeserializationError::Ok ? "SUCCESS" : "FAILED"));
    
    mPendingJsonUpdate = fl::move(doc);
    mHasPendingUpdate = true;
    FL_WARN("*** AFTER: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    FL_WARN("*** BACKEND SET mHasPendingUpdate = true, waiting for onPlatformPreLoop()");
}

void JsonUiManager::executeUiUpdates(const FLArduinoJson::JsonDocument &doc) {
    auto components = getComponents();
    FL_WARN("*** EXECUTING UI UPDATES: " << components.size() << " components available");
    
    if (doc.is<FLArduinoJson::JsonObject>()) {
        auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
        FL_WARN("*** JSON OBJECT HAS " << obj.size() << " KEYS");
        
        // Log all available keys in the JSON
        for (auto kv : obj) {
            FL_WARN("*** JSON KEY FOUND: '" << kv.key().c_str() << "'");
        }
        
        // Log all available component IDs
        for (auto &component : components) {
            int id = component->id();
            FL_WARN("*** COMPONENT AVAILABLE: ID " << id);
        }
        
        for (auto &component : components) {
            int id = component->id();
            //char idBuffer[32];
            //sprintf(idBuffer, "id_%d", id);
            string idStr = "";
            idStr += id;
            FL_WARN("*** LOOKING FOR KEY: '" << idStr.c_str() << "' for component ID " << id);
            
            if (obj.containsKey(idStr.c_str())) {
                FL_WARN("*** FOUND MATCH! UPDATING COMPONENT: ID " << id);
                component->update(obj[idStr.c_str()]);
                FL_WARN("*** COMPONENT UPDATE COMPLETED: ID " << id);
            } else {
                FL_WARN("*** NO MATCH: Component ID " << id << " (key '" << idStr.c_str() << "') not found in JSON");
            }
        }
    } else {
        FL_WARN("*** ERROR: JSON document is not an object");
    }
}

void JsonUiManager::onPlatformPreLoop() {
    // Don't re-enable this, it fills the screen with spam.
    //FL_WARN("*** onPlatformPreLoop CALLED: JsonUiManager=" << this << ", mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    if (!mHasPendingUpdate) {
        return;
    }
    FL_WARN("*** onPlatformPreLoop: Processing pending update");
    executeUiUpdates(mPendingJsonUpdate);
    mPendingJsonUpdate.clear();
    mHasPendingUpdate = false;
    FL_WARN("*** onPlatformPreLoop: Update processed and cleared");
}

void JsonUiManager::toJson(FLArduinoJson::JsonArray &json) {
    auto components = getComponents();
    for (auto &component : components) {
        auto obj = json.add<FLArduinoJson::JsonObject>();
        component->toJson(obj);
    }
}

void JsonUiManager::onEndShowLeds() {
   // FL_WARN("*** onEndShowLeds CALLED ***");
    bool shouldUpdate = false;
    {
        fl::lock_guard lock(mMutex);
        shouldUpdate = mItemsAdded;
        mItemsAdded = false;
    }
    if (shouldUpdate) {
        FLArduinoJson::JsonDocument doc;
        auto json = doc.to<FLArduinoJson::JsonArray>();
        toJson(json);
        string jsonStr;
        serializeJson(doc, jsonStr);
        FL_WARN("*** SENDING UI TO FRONTEND: " << jsonStr.substr(0, 100).c_str() << "...");
        mUpdateJs(jsonStr.c_str());
    }
}

} // namespace fl
#endif // FASTLED_ENABLE_JSON
