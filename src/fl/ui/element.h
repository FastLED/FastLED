#pragma once

#include "fl/stl/string.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "platforms/ui_defs.h"  // IWYU pragma: keep

#define FL_NO_COPY(CLASS)                                                      \
    CLASS(const CLASS &) = delete;                                             \
    CLASS &operator=(const CLASS &) FL_NOEXCEPT = delete;

namespace fl {

// Virtual interface for physical button input - breaks fl.cpp -> fl.sensors+ link chain.
// Concrete Button class implements this in fl/sensors/button.h.
class IButtonInput {
  public:
    virtual ~IButtonInput() FL_NOEXCEPT = default;
    virtual bool isPressed() const FL_NOEXCEPT = 0;
    virtual bool clicked() const FL_NOEXCEPT = 0;
};

// Forward declaration only - concrete type lives in fl/sensors/button.h
class Button;

// Base class for UI elements that provides string-based group functionality
class UIElement {
  public:
    UIElement() FL_NOEXCEPT;
    VIRTUAL_IF_NOT_AVR ~UIElement() FL_NOEXCEPT {}
    virtual void setGroup(const fl::string& groupName) FL_NOEXCEPT { mGroupName = groupName; }

    fl::string getGroup() const FL_NOEXCEPT { return mGroupName; }
    bool hasGroup() const FL_NOEXCEPT { return !mGroupName.empty(); }

  private:
    fl::string mGroupName;
};

#define FASTLED_UI_DEFINE_OPERATORS(UI_CLASS)                                  \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >=)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <=)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >)                        \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <)                        \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, ==)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, !=)

} // namespace fl
