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

// Use lazy initialization to avoid global constructors
static JsonUiAddHandler& getAddHandler() {
    static JsonUiAddHandler handler;
    return handler;
}

static JsonUiRemoveHandler& getRemoveHandler() {
    static JsonUiRemoveHandler handler;
    return handler;
}

static JsonUiUpdateJsHandler& getUpdateJsHandler() {
    static JsonUiUpdateJsHandler handler;
    return handler;
}

// Internal JsonUiManager instance
static fl::scoped_ptr<JsonUiManager>& getInternalManager() {
    static fl::scoped_ptr<JsonUiManager> manager;
    return manager;
}

fl::function<void(const char*)> setJsonUiHandlers(const JsonUiAddHandler& addHandler, const JsonUiRemoveHandler& removeHandler, const JsonUiUpdateJsHandler& updateJsHandler) {
    getAddHandler() = addHandler;
    getRemoveHandler() = removeHandler;
    getUpdateJsHandler() = updateJsHandler;
    
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
        // No updateJs handler, clear any existing internal manager and use external handlers if available
        auto& manager = getInternalManager();
        manager.reset(); // Clear the internal manager
        
        auto& pending = getPendingComponents();
        if (addHandler && !pending.empty()) {
            FL_WARN("Flushing " << pending.size() << " pending UI components to external add handler");
            for (const auto& component : pending) {
                // Only add components that are still valid (not destroyed)
                if (component) {
                    addHandler(component);
                }
            }
            pending.clear();
        }
        
        // Return an empty function since external handlers manage state
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
    
    // Fall back to external handler
    const auto& handler = getAddHandler();
    if (!handler) {
        // Handler not set yet, store in pending list
        auto& pending = getPendingComponents();
        pending.push_back(component);
        FL_WARN("addJsonUiComponent handler not set, component stored in pending list: " << component);
        return;
    }
    handler(component);
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Check if we have an internal manager first
    auto& manager = getInternalManager();
    if (manager) {
        manager->removeComponent(component);
        return;
    }
    
    // Fall back to external handler logic
    const auto& handler = getRemoveHandler();
    
    // Always try to remove from pending list first, regardless of handler state
    auto& pending = getPendingComponents();
    auto it = pending.find_if([&component](const fl::WeakPtr<JsonUiInternal>& pending_component) {
        return pending_component == component;
    });
    
    if (it != pending.end()) {
        pending.erase(it);
        FL_WARN("Removed component from pending list: " << component);
        // If we found it in pending, we're done - it was never actually added via handler
        return;
    }
    
    // Not in pending list, try to remove via handler
    if (!handler) {
        FL_WARN("removeJsonUiComponent handler not set, component will be ignored: " << component);
        return;
    }
    handler(component);
}

} // namespace fl
