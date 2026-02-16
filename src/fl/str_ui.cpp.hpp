// UI-specific string functionality
// This file is only compiled/linked when UI features are actually used
// Breaking the dependency chain from core string functionality to UI system

#include "fl/str.h"
// IWYU pragma: begin_keep
#include "platforms/shared/ui/json/ui_internal.h"  // ok platform headers
// IWYU pragma: end_keep // ok platform headers

namespace fl {

// Implementation of UI-specific append method
// This will only be linked if JsonUiInternal is actually used somewhere
string &string::append(const JsonUiInternal& val) {
    append(val.name());
    return *this;
}

} // namespace fl
