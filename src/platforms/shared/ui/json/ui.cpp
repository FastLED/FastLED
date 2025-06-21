#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"
#include "fl/vector.h"

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

void setJsonUiHandlers(const JsonUiAddHandler& addHandler, const JsonUiRemoveHandler& removeHandler) {
    getAddHandler() = addHandler;
    getRemoveHandler() = removeHandler;
    
    // Flush any pending components to the new handlers
    auto& pending = getPendingComponents();
    if (addHandler && !pending.empty()) {
        FL_WARN("Flushing " << pending.size() << " pending UI components to new add handler");
        for (const auto& component : pending) {
            // Only add components that are still valid (not destroyed)
            if (component) {
                addHandler(component);
            }
        }
        pending.clear();
    }
}

void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
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
