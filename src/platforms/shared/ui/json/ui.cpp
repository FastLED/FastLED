#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "fl/warn.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/scoped_ptr.h"
#include "fl/function.h"

namespace fl {

// Temporary storage for UI components that arrive before handlers are set
static fl::vector_inlined<fl::WeakPtr<JsonUiInternal>, 32>& getPendingComponents() {
    static fl::vector_inlined<fl::WeakPtr<JsonUiInternal>, 32> pending;
    return pending;
}

// Internal JsonUiManager instance
static fl::scoped_ptr<JsonUiManager>& getInternalManager() {
    static fl::scoped_ptr<JsonUiManager> manager;
    return manager;
}

JsonUiUpdateInput setJsonUiHandlers(const JsonUiUpdateOutput& updateJsHandler) {
    // Create internal JsonUiManager only if updateJsHandler is valid (not empty)
    if (updateJsHandler) {
        auto& manager = getInternalManager();
        manager.reset(new JsonUiManager(updateJsHandler));
        FL_WARN("Created internal JsonUiManager with updateJs callback");
        
        // Flush any pending components to the internal manager
        auto& pending = getPendingComponents();
        if (!pending.empty()) {
            FL_WARN("Flushing " << pending.size() << " pending UI components to internal JsonUiManager");
            for (const auto& component : pending) {
                // Only add components that are still valid (not destroyed)
                if (component) {
                    manager->addComponent(component);
                }
            }
            pending.clear();
        }
        
        // Return a function that allows updating the engine state
        return [](const char* jsonStr) {
            auto& manager = getInternalManager();
            if (manager) {
                manager->updateUiComponents(jsonStr);
            } else {
                FL_WARN("updateEngineState called but no internal JsonUiManager exists");
            }
        };
    } else {
        // No updateJs handler, clear any existing internal manager
        auto& manager = getInternalManager();
        manager.reset(); // Clear the internal manager
        FL_WARN("No updateJs handler provided, cleared internal JsonUiManager");
        
        // Return an empty function since no manager exists
        return fl::function<void(const char*)>{};
    }
}

void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Check if we have an internal manager first
    auto& manager = getInternalManager();
    if (manager) {
        manager->addComponent(component);
        return;
    }
    
    // No manager exists, store in pending list
    auto& pending = getPendingComponents();
    pending.push_back(component);
    FL_WARN("addJsonUiComponent: no manager exists, component stored in pending list: " << component);
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Check if we have an internal manager first
    auto& manager = getInternalManager();
    if (manager) {
        manager->removeComponent(component);
        return;
    }
    
    // No manager exists, try to remove from pending list
    auto& pending = getPendingComponents();
    auto it = pending.find_if([&component](const fl::WeakPtr<JsonUiInternal>& pending_component) {
        return pending_component == component;
    });
    
    if (it != pending.end()) {
        pending.erase(it);
        FL_WARN("Removed component from pending list: " << component);
    } else {
        FL_WARN("removeJsonUiComponent: no manager exists and component not in pending list: " << component);
    }
}

} // namespace fl
