#include "fl/json.h"
#include "fl/map.h"
#include "fl/mutex.h"
#include "fl/namespace.h"
#include "ui_manager.h"
#include "fl/compiler_control.h"
#include "fl/warn.h"
#include "fl/assert.h"

FL_DISABLE_WARNING(deprecated-declarations)

#if FASTLED_ENABLE_JSON

namespace fl {


// Constructor is inline in header, just add logging to destructor
JsonUiManager::JsonUiManager(Callback updateJs) : mUpdateJs(updateJs) {
    fl::EngineEvents::addListener(this);
}


JsonUiManager::~JsonUiManager() {
    // FL_WARN("*** JsonUiManager: DESTRUCTOR CALLED ***");
    // FL_WARN("*************************************************************");
    // FL_WARN("*** CRITICAL ERROR: JsonUiManager DESTRUCTOR IS RUNNING! ***");
    // FL_WARN("*** THIS SHOULD NOT HAPPEN DURING UI UPDATES!             ***");
    // FL_WARN("*** THE UI MANAGER IS BEING DESTROYED AND RECREATED!      ***");
    // FL_WARN("*** THIS IS THE ROOT CAUSE OF THE UI UPDATE BUG!          ***");
    // FL_WARN("*************************************************************");
    // // FL_WARN("*** STACK TRACE: JsonUiManager destructor at " << this);
    // FL_WARN("*** Component count being lost: " << mComponents.size());
    // FL_WARN("*************************************************************");
    // FL_ASSERT(false, "JsonUiManager destructor should not be running during UI updates");
    fl::EngineEvents::removeListener(this);
}

void JsonUiManager::addComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_WARN("*** JsonUiManager::addComponent ENTRY ***");
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
    
    // Mark the component as changed so it gets sent to the frontend initially
    if (auto ptr = component.lock()) {
        ptr->markChanged();
    }
    
    // FL_WARN("*** COMPONENT REGISTERED: ID " << (component.lock() ? component.lock()->id() : -1) << " (Total: " << mComponents.size() << ")");
}

void JsonUiManager::removeComponent(fl::WeakPtr<JsonUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.erase(component);
}

void JsonUiManager::processPendingUpdates() {
    // Force immediate processing of pending updates (for testing)
    if (mHasPendingUpdate) {
        executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate.clear();
        mHasPendingUpdate = false;
    }
}

fl::vector<JsonUiInternalPtr> JsonUiManager::getComponents() {
    FL_WARN("*** JsonUiManager::getComponents ENTRY ***");
    fl::lock_guard<fl::mutex> lock(mMutex);
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
    // FL_WARN("*** INCOMING JSON: " << (jsonStr ? jsonStr : "NULL"));
    // FL_WARN("*** JSON LENGTH: " << (jsonStr ? strlen(jsonStr) : 0));
    // FL_WARN("*** CURRENT COMPONENT COUNT: " << mComponents.size());
    
    if (!jsonStr) {
        FL_WARN("*** JsonUiManager::updateUiComponents: NULL JSON string provided");
        return;
    }

    // FL_WARN("*** BACKEND RECEIVED UI UPDATE: " << (jsonStr ? jsonStr : "NULL"));
    // FL_WARN("*** JsonUiManager pointer: " << this);
    // FL_WARN("*** BEFORE: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    
    FLArduinoJson::JsonDocument doc;
    deserializeJson(doc, jsonStr);
    
    mPendingJsonUpdate = fl::move(doc);
    mHasPendingUpdate = true;
    // FL_WARN("*** AFTER: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    // FL_WARN("*** BACKEND SET mHasPendingUpdate = true, waiting for onEndFrame()");
}

void JsonUiManager::executeUiUpdates(const FLArduinoJson::JsonDocument &doc) {
    auto type = getJsonType(doc);
    auto components = getComponents();

    if (type == fl::JSON_OBJECT) {
        auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
        bool any_found = false;
        int id = -1;
        for (auto &component : components) {
            id = component->id();
            string idStr = "";
            idStr += id;
            
            if (obj.containsKey(idStr.c_str())) {
                const FLArduinoJson::JsonVariantConst v = obj[idStr.c_str()];
                component->update(v);
                any_found = true;
                break;
            }
        }

        FL_WARN_IF(!any_found, "*** ERROR: could not find any components in the JSON update mapping into internal component ids");

    } else {
        FL_WARN("JSON document is not an object, cannot execute UI updates");
    }
}

void JsonUiManager::onEndFrame() {
    // Don't re-enable this, it fills the screen with spam.
    //FL_WARN("*** onEndFrame CALLED: JsonUiManager=" << this << ", mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    if (!mHasPendingUpdate) {
        return;
    }
    // FL_WARN("*** onEndFrame: Processing pending update");
    executeUiUpdates(mPendingJsonUpdate);
    mPendingJsonUpdate.clear();
    mHasPendingUpdate = false;
    // FL_WARN("*** onEndFrame: Update processed and cleared");
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
        fl::lock_guard<fl::mutex> lock(mMutex);
        // Check if new components were added
        shouldUpdate = mItemsAdded;
        mItemsAdded = false;
        
        // Poll all components for changes (eliminates need for manual notifications)
        if (!shouldUpdate) {
            for (auto &componentRef : mComponents) {
                if (auto component = componentRef.lock()) {
                    if (component->hasChanged()) {
                        shouldUpdate = true;
                        break; // Found at least one change, no need to check more
                    }
                }
            }
        }
    }
    
    if (shouldUpdate) {
        // Clear the changed flags for all components
        {
            fl::lock_guard<fl::mutex> lock(mMutex);
            for (auto &componentRef : mComponents) {
                if (auto component = componentRef.lock()) {
                    component->clearChanged();
                }
            }
        }
        
        FLArduinoJson::JsonDocument doc;
        auto json = doc.to<FLArduinoJson::JsonArray>();
        toJson(json);
        string jsonStr;
        serializeJson(doc, jsonStr);
        //FL_WARN("*** SENDING UI TO FRONTEND: " << jsonStr.substr(0, 100).c_str() << "...");
        mUpdateJs(jsonStr.c_str());
    }
}

} // namespace fl
#endif // FASTLED_ENABLE_JSON
