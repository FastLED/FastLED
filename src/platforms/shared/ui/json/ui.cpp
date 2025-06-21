#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {

// Use lazy initialization to avoid global constructors
static JsonUiAddHandler& getAddHandler() {
    static JsonUiAddHandler handler;
    return handler;
}

static JsonUiRemoveHandler& getRemoveHandler() {
    static JsonUiRemoveHandler handler;
    return handler;
}

void setJsonUiAddHandler(const JsonUiAddHandler& handler) {
    getAddHandler() = handler;
}

void setJsonUiRemoveHandler(const JsonUiRemoveHandler& handler) {
    getRemoveHandler() = handler;
}

void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    const auto& handler = getAddHandler();
    if (!handler) {
        FL_WARN("addJsonUiComponent handler not set, component will be ignored: " << component);
        return;
    }
    handler(component);
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    const auto& handler = getRemoveHandler();
    if (!handler) {
        FL_WARN("removeJsonUiComponent handler not set, component will be ignored: " << component);
        return;
    }
    handler(component);
}

} // namespace fl
