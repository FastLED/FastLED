#pragma once

#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_DESCRIPTION
#define FASTLED_HAS_UI_DESCRIPTION 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_DESCRIPTION

class UIDescriptionImpl {
  public:
    UIDescriptionImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIDescriptionImpl() FL_NOEXCEPT {}

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }
};

#endif

class UIDescription : public UIElement {
  public:
    FL_NO_COPY(UIDescription);
    UIDescription(const char *name) FL_NOEXCEPT;
    ~UIDescription() FL_NOEXCEPT;

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

  protected:
    UIDescriptionImpl mImpl;
};

} // namespace fl
