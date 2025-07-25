// UI-specific string functionality
// This file is only compiled/linked when UI features are actually used
// Breaking the dependency chain from core string functionality to UI system

#include "fl/str.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json.h"

namespace fl {

// Implementation of UI-specific append method
// This will only be linked if JsonUiInternal is actually used somewhere
string &string::append(const JsonUiInternal& val) {
    #if FASTLED_ENABLE_JSON
    append(val.name());
    return *this;
    #else
    return *this;
    #endif
}

} // namespace fl
