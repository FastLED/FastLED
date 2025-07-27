#include "fl/json.h"
#include "fl/json.h"
#include "fl/map.h"
#include "fl/mutex.h"
#include "fl/namespace.h"
#include "ui_manager.h"
#include "fl/compiler_control.h"
#include "fl/warn.h"
#include "fl/assert.h"
#include "fl/string.h"


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

void JsonUiManager::addComponent(fl::weak_ptr<JsonUiInternal> component) {
    //FL_WARN("*** JsonUiManager::addComponent ENTRY ***");
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.insert(component);
    mItemsAdded = true;
    
    // Mark the component as changed so it gets sent to the frontend initially
    if (auto ptr = component.lock()) {
        ptr->markChanged();
        //FL_WARN("*** COMPONENT REGISTERED: ID " << ptr->id() << " name=" << ptr->name() << " (Total: " << mComponents.size() << ")");
    }
}

void JsonUiManager::removeComponent(fl::weak_ptr<JsonUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(mMutex);
    mComponents.erase(component);
}

void JsonUiManager::processPendingUpdates() {
    // Force immediate processing of pending updates (for testing)

    if (mHasPendingUpdate) {
        executeUiUpdates(mPendingJsonUpdate);
        mPendingJsonUpdate = fl::Json(); // Clear the pending update
        mHasPendingUpdate = false;
    }


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
        fl::Json doc = fl::Json::array();
        toJson(doc);
        fl::string jsonStr = doc.to_string();
        //FL_WARN("*** SENDING UI TO FRONTEND: " << jsonStr.substr(0, 100).c_str() << "...");
        mUpdateJs(jsonStr.c_str());

        // Clear the changed flag for all components after sending the update
        fl::lock_guard<fl::mutex> lock(mMutex); // Acquire lock again for modifying mComponents
        for (auto &componentRef : mComponents) {
            if (auto component = componentRef.lock()) {
                component->clearChanged();
            }
        }
    }






}

fl::vector<JsonUiInternalPtr> JsonUiManager::getComponents() {
    fl::lock_guard<fl::mutex> lock(mMutex);
    fl::vector<JsonUiInternalPtr> out;
    for (auto &component : mComponents) {
        if (auto ptr = component.lock()) {
            out.push_back(ptr);
        } else {
            FL_WARN("*** WARNING: Component weak_ptr is expired, skipping");
        }
    }
    // Sort components by ID to ensure consistent serialization order
    fl::sort(out.begin(), out.end(), [](const JsonUiInternalPtr& a, const JsonUiInternalPtr& b) {
        return a->id() < b->id();
    });
    return out;
}

JsonUiInternalPtr JsonUiManager::findUiComponent(const char* id_or_name) {
    auto components = getComponents();
    
    for (auto &component : components) {
        int id = component->id();
        string componentIdStr;
        componentIdStr.append(id);
        
        if (fl::string::strcmp(componentIdStr.c_str(), id_or_name) == 0) {
            //FL_WARN("*** Found component with ID " << id);
            return component;
        }
    }

    // If we didn't find it by id, try to find it by name
    for (auto &component : components) {
        if (fl::string::strcmp(component->name().c_str(), id_or_name) == 0) {
            return component;
        }
    }
    
    return JsonUiInternalPtr(); // Return null pointer if not found
}

void JsonUiManager::updateUiComponents(const char* jsonStr) {
    //FL_WARN("*** JsonUiManager::updateUiComponents ENTRY ***");
    // FL_WARN("*** INCOMING JSON: " << (jsonStr ? jsonStr : "NULL"));
    // FL_WARN("*** JSON LENGTH: " << (jsonStr ? strlen(jsonStr) : 0));
    // FL_WARN("*** CURRENT COMPONENT COUNT: " << mComponents.size());
    
    if (!jsonStr) {
        FL_ASSERT(false, "*** JsonUiManager::updateUiComponents: NULL JSON string provided");
        return;
    }

    // FL_WARN("*** BACKEND RECEIVED UI UPDATE: " << (jsonStr ? jsonStr : "NULL"));
    // FL_WARN("*** JsonUiManager pointer: " << this);
    // FL_WARN("*** BEFORE: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    
    mPendingJsonUpdate = fl::Json::parse(jsonStr);
    mHasPendingUpdate = true;
    // FL_WARN("*** AFTER: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    // FL_WARN("*** BACKEND SET mHasPendingUpdate = true, waiting for onEndFrame()");
}


void JsonUiManager::executeUiUpdates(const fl::Json &doc) {
    
    if (doc.is_object()) {
        
        // Iterate through all keys in the JSON object
        for (auto key : doc.keys()) {
            const char* id_or_name = key.c_str();
            
            //FL_WARN("*** Checking for component with ID: " << idStr);
            
            auto component = findUiComponent(id_or_name);
            if (component) {
                const fl::Json v = doc[key.c_str()];
                component->updateInternal(v);
                //FL_WARN("*** Updated component with ID " << idStr);
            } else {
                FL_WARN("*** ERROR: could not find component with ID or name: " << id_or_name);
            }
        }
    } else {
        // Debug: Show what we actually received instead of just asserting
        fl::string debugJson = doc.to_string();
        FL_WARN("*** UI UPDATE ERROR: Expected JSON object but got " << 
               (doc.is_array() ? "array" : "non-object") << 
               ": " << debugJson.substr(0, 200).c_str() << "...");
        
        // Use a warning instead of assertion to prevent crashes
        // FL_ASSERT(false, "JSON document is not an object, cannot execute UI updates");
    }
}

void JsonUiManager::onEndFrame() {
    processPendingUpdates();
}

void JsonUiManager::toJson(fl::Json &doc) {
    auto components = getComponents();
    for (const auto &component : components) {
        fl::Json componentJson = fl::Json::object();
        component->toJson(componentJson);
        doc.push_back(componentJson);
    }
}

} // namespace fl

#endif
