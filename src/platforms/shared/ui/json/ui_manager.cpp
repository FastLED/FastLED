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
        mPendingJsonUpdate.clear();
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
        // Clear the changed flags for all components
        {
            fl::lock_guard<fl::mutex> lock(mMutex);
            for (auto &componentRef : mComponents) {
                if (auto component = componentRef.lock()) {
                    component->clearChanged();
                }
            }
        }
        
        // IDEAL API ENHANCEMENT NEEDED: JsonBuilder should support vector<Json> for object arrays
        // For now, use object-wrapper approach which works with current ideal API
        
        fl::JsonBuilder builder;
        builder.set("type", "ui_update");
        builder.set("count", static_cast<int>(getComponents().size()));
        
        // Serialize each component and collect as strings for now
        auto components = getComponents();
        fl::vector<fl::string> componentStrings;
        for (auto &component : components) {
            auto componentJson = component->toJson();
            if (componentJson.is_object()) {
                componentStrings.push_back(componentJson.serialize());
            }
        }
        
        // Store serialized components - frontend will need to parse these
        builder.set("components_serialized", componentStrings);
        
        fl::Json wrapperJson = builder.build();
        fl::string jsonStr = wrapperJson.serialize();
        //FL_WARN("*** SENDING UI TO FRONTEND: " << jsonStr.substr(0, 100).c_str() << "...");
        mUpdateJs(jsonStr.c_str());
    }





}

fl::vector<JsonUiInternalPtr> JsonUiManager::getComponents() {
    //FL_WARN("*** JsonUiManager::getComponents ENTRY ***");
    fl::lock_guard<fl::mutex> lock(mMutex);
    //FL_WARN("*** mComponents.size() = " << mComponents.size());
    fl::vector<JsonUiInternalPtr> out;
    for (auto &component : mComponents) {
        if (auto ptr = component.lock()) {
            out.push_back(ptr);
            //FL_WARN("*** Added component to output: id=" << ptr->id() << " name=" << ptr->name());
        } else {
            FL_WARN("*** WARNING: Component weak_ptr is expired, skipping");
        }
    }
    //FL_WARN("*** Returning " << out.size() << " components");
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
    
    fl::JsonDocument doc;
    deserializeJson(doc, jsonStr);
    
    mPendingJsonUpdate = fl::move(doc);
    mHasPendingUpdate = true;
    // FL_WARN("*** AFTER: mHasPendingUpdate=" << (mHasPendingUpdate ? "true" : "false"));
    // FL_WARN("*** BACKEND SET mHasPendingUpdate = true, waiting for onEndFrame()");
}


void JsonUiManager::executeUiUpdates(const fl::JsonDocument &doc) {
    auto type = getJsonType(doc);
    
    if (type == fl::JSON_OBJECT) {
        // Use fl::Json wrapper - eliminate FLArduinoJson iteration by checking known components
        fl::Json rootJson(const_cast<fl::JsonDocument&>(doc));
        
        // Instead of iterating through JSON keys, iterate through known components
        // and check if they exist in the JSON - this eliminates FLArduinoJson usage
        auto components = getComponents();
        for (auto &component : components) {
            const fl::string& componentName = component->name();
            
            // Check if this component name exists in the JSON
            fl::Json valueJson = rootJson[componentName];
            if (valueJson.has_value()) {
                component->update(valueJson);
                //FL_WARN("*** Updated component: " << componentName.c_str());
            }
            
            // Also check by ID if different from name
            fl::string componentId = to_string(component->id());
            if (componentId != componentName) {
                fl::Json valueByIdJson = rootJson[componentId];
                if (valueByIdJson.has_value()) {
                    component->update(valueByIdJson);
                    //FL_WARN("*** Updated component by ID: " << componentId.c_str());
                }
            }
        }
    } else {
        // Debug: Show what we actually received instead of just asserting
        fl::string debugJson = doc.serialize();
        FL_WARN("*** UI UPDATE ERROR: Expected JSON object but got " << 
               (type == fl::JSON_ARRAY ? "array" : "non-object") << 
               ": " << debugJson.substr(0, 200).c_str() << "...");
        
        // Use a warning instead of assertion to prevent crashes
        // FL_ASSERT(false, "JSON document is not an object, cannot execute UI updates");
    }
}

void JsonUiManager::onEndFrame() {
    processPendingUpdates();
}

// REMOVED: Legacy toJson method no longer needed after FLArduinoJson elimination


} // namespace fl
#endif // FASTLED_ENABLE_JSON
