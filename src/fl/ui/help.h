#pragma once

#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_HELP
#define FASTLED_HAS_UI_HELP 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_HELP
class UIHelpImpl {
  public:
    UIHelpImpl(const char *markdownContent) : mContent(markdownContent) { FASTLED_UNUSED(markdownContent); }
    ~UIHelpImpl() FL_NOEXCEPT {}

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

    // Stub method for accessing markdown content
    const fl::string& markdownContent() const { return mContent; }

  private:
    fl::string mContent;
};
#endif

class UIHelp : public UIElement {
  public:
    FL_NO_COPY(UIHelp);
    UIHelp(const char *markdownContent) FL_NOEXCEPT;
    ~UIHelp() FL_NOEXCEPT;

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

    // Access to the markdown content
    const fl::string& markdownContent() const FL_NOEXCEPT { return mImpl.markdownContent(); }

  protected:
    UIHelpImpl mImpl;
};

} // namespace fl
