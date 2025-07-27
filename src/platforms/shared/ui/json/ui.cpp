#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "fl/warn.h"
#include "fl/memory.h"
#include "fl/vector.h"
#include "fl/unique_ptr.h"
#include "fl/function.h"

namespace fl {

// Temporary storage for UI components that arrive before handlers are set
static fl::vector_inlined<fl::weak_ptr<JsonUiInternal>, 32>& getPendingComponents() {
    static fl::vector_inlined<fl::weak_ptr<JsonUiInternal>, 32> pending;
    return pending;
}

// Internal JsonUiManager instance
static fl::unique_ptr<JsonUiManager>& getInternalManager() {
    static fl::unique_ptr<JsonUiManager> manager;
    return manager;
}

JsonUiUpdateInput setJsonUiHandlers(const JsonUiUpdateOutput& updateJsHandler) {
    // FL_WARN("setJsonUiHandlers: ENTRY - updateJsHandler is " << (updateJsHandler ? "VALID" : "NULL/EMPTY"));
    
    // Create internal JsonUiManager only if updateJsHandler is valid (not empty)
    if (updateJsHandler) {
        // FL_WARN("setJsonUiHandlers: updateJsHandler is valid, creating JsonUiManager");
        auto& manager = getInternalManager();
        // FL_WARN("setJsonUiHandlers: Old manager pointer=" << manager.get());
        
        // Only create a new manager if one doesn't exist
        // This prevents destroying existing components when called multiple times

        if (!manager) {
            manager.reset(new JsonUiManager(updateJsHandler));
        } else {
            manager->resetCallback(updateJsHandler);
        }

        // Flush any pending components to the internal manager
        auto& pending = getPendingComponents();
        if (!pending.empty()) {
            // FL_WARN("Flushing " << pending.size() << " pending UI components to internal JsonUiManager");
            for (const auto& component : pending) {
                // Only add components that are still valid (not destroyed)
                if (!component.expired()) {
                    manager->addComponent(component);
                }
            }
            pending.clear();
        } else {
            // FL_WARN("setJsonUiHandlers: No pending components to flush");
        }
        
        // Return a function that allows updating the engine state
        // FL_WARN("setJsonUiHandlers: Creating and returning updateEngineState lambda");
        auto result = fl::function<void(const char*)>([](const char* jsonStr) {
            // FL_WARN("*** updateEngineState lambda CALLED ***");
            // FL_WARN("*** updateEngineState lambda ENTRY: jsonStr=" << (jsonStr ? jsonStr : "NULL"));
            // FL_WARN("*** updateEngineState lambda JSON LENGTH: " << (jsonStr ? strlen(jsonStr) : 0));
            auto& manager = getInternalManager();
            // FL_WARN("*** updateEngineState lambda: manager pointer=" << manager.get());
            if (manager) {
                // FL_WARN("*** updateEngineState lambda: manager exists, calling updateUiComponents");
                // FL_WARN("*** updateEngineState lambda: PASSING JSON TO MANAGER: " << (jsonStr ? jsonStr : "NULL"));
                manager->updateUiComponents(jsonStr);
                // FL_WARN("*** updateEngineState lambda: updateUiComponents completed");
            } else {
                FL_WARN("*** updateEngineState lambda: NO MANAGER EXISTS!");
            }
        });
        // FL_WARN("setJsonUiHandlers: updateEngineState lambda created, returning it (is " << (result ? "VALID" : "NULL") << ")");
        return result;
    } else {
        // No updateJs handler, clear any existing internal manager
        auto& manager = getInternalManager();
        manager.reset(); // Clear the internal manager
        return fl::function<void(const char*)>{};
    }
}

void addJsonUiComponent(fl::weak_ptr<JsonUiInternal> component) {
    // FL_WARN("addJsonUiComponent: ENTRY - component=" << component);
    
    // Check if we have an internal manager first
    auto& manager = getInternalManager();
    // FL_WARN("addJsonUiComponent: manager exists=" << (manager ? "true" : "false"));
    
    if (manager) {
        // FL_WARN("addJsonUiComponent: Adding component to existing manager");
        manager->addComponent(component);
        // FL_WARN("addJsonUiComponent: Component added to manager, RETURNING");
        return;
    }
        
    // Still no manager exists, store in pending list
    // FL_WARN("addJsonUiComponent: No manager exists, storing in pending list");
    auto& pending = getPendingComponents();
    pending.push_back(component);
    // FL_WARN("addJsonUiComponent: no manager exists, component stored in pending list: " << component);
}

void removeJsonUiComponent(fl::weak_ptr<JsonUiInternal> component) {
    // Check if we have an internal manager first
    // No longer need to clear functions as we're not using lambda captures anymore

    auto& manager = getInternalManager();
    if (manager) {
        manager->removeComponent(component);
        return;
    }
    
    // No manager exists, try to remove from pending list
    auto& pending = getPendingComponents();
    auto it = pending.find_if([&component](const fl::weak_ptr<JsonUiInternal>& pending_component) {
        // Compare the weak_ptrs by checking if they refer to the same object
        auto comp_locked = component.lock();
        auto pending_locked = pending_component.lock();
        return comp_locked && pending_locked && comp_locked == pending_locked;
    });
    
    if (it != pending.end()) {
        pending.erase(it);
        // FL_WARN("Removed component from pending list: " << component);
    } else {
        // FL_WARN("removeJsonUiComponent: no manager exists and component not in pending list: " << component);
    }
}

void processJsonUiPendingUpdates() {
    // Force immediate processing of any pending UI updates (for testing)
    auto& manager = getInternalManager();
    if (manager) {
        // If we have a manager, ask it to process updates immediately
        manager->processPendingUpdates();
    }
    // If no manager exists, there are no pending updates to process
}

} // namespace fl
