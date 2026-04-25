#pragma once

#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_TITLE
#define FASTLED_HAS_UI_TITLE 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_TITLE

class UITitleImpl {
  public:
    UITitleImpl(const char *name) FL_NOEXCEPT { FASTLED_UNUSED(name); }
    ~UITitleImpl() FL_NOEXCEPT {}

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) FL_NOEXCEPT { FASTLED_UNUSED(groupName); }
};

#endif

class UITitle : public UIElement {
  public:
    FL_NO_COPY(UITitle);
    UITitle(const char *name) FL_NOEXCEPT;
    ~UITitle() FL_NOEXCEPT;

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

  protected:
    UITitleImpl mImpl;
};

} // namespace fl
