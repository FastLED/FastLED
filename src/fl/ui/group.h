#pragma once

#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_GROUP
#define FASTLED_HAS_UI_GROUP 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_GROUP

class UIGroupImpl {
  public:
    UIGroupImpl(const char *name) FL_NOEXCEPT : mGroupName(name) {
        FASTLED_UNUSED(name);
    }
    ~UIGroupImpl() FL_NOEXCEPT {}
    fl::string name() const FL_NOEXCEPT { return mGroupName; }

  private:
    fl::string mGroupName;
};

#endif

class UIGroup {
  public:
    FL_NO_COPY(UIGroup);

    UIGroup(const fl::string& groupName) FL_NOEXCEPT;

    template<typename... UIElements>
    UIGroup(const fl::string& groupName, UIElements&... elements) FL_NOEXCEPT
        : mImpl(groupName.c_str()) {
        add(elements...);
    }

    ~UIGroup() FL_NOEXCEPT;

    fl::string name() const FL_NOEXCEPT { return mImpl.name(); }

    operator fl::string() const FL_NOEXCEPT { return name(); }

    template<typename T>
    void addControl(T* control) FL_NOEXCEPT {
        control->setGroup(name());
    }

  protected:
    UIGroupImpl mImpl;

  private:
    template<typename T>
    void add(T& control) FL_NOEXCEPT {
        control.setGroup(name());
    }

    template<typename T, typename... Rest>
    void add(T& control, Rest&... rest) FL_NOEXCEPT {
        control.setGroup(name());
        add(rest...);
    }
};

} // namespace fl
