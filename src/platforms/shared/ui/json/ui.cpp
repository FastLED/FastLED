#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {
__attribute__((weak)) void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // In test environment, just return silently - no need to warn
    (void)component; // Suppress unused parameter warning
}

__attribute__((weak)) void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // In test environment, just return silently - no need to warn
    (void)component; // Suppress unused parameter warning
}
} // namespace fl
