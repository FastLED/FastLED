#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include "ui_deps.h"
#include "ui_internal.h"
#include "fl/mutex.h"
#include "fl/set.h"

namespace fl {

// Global storage for UI components - matches the jsUiManager implementation
namespace {
    typedef fl::FixedSet<fl::WeakPtr<jsUiInternal>, 64> jsUIRefSet;
    jsUIRefSet gComponents;
    fl::mutex gMutex;
    bool gItemsAdded = false;
}

void addUiComponent(fl::WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(gMutex);
    gComponents.insert(component);
    gItemsAdded = true;
}

void removeUiComponent(fl::WeakPtr<jsUiInternal> component) {
    fl::lock_guard<fl::mutex> lock(gMutex);
    gComponents.erase(component);
}

} // namespace fl

#endif // __EMSCRIPTEN__
