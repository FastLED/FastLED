#pragma once

#include "fl/stl/function.h"
#include "fl/stl/initializer_list.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#include "fl/system/engine_events.h"
#include "fl/ui/element.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_DROPDOWN
#define FASTLED_HAS_UI_DROPDOWN 0
#endif

namespace fl {

#if !FASTLED_HAS_UI_DROPDOWN
class UIDropdownImpl {
  public:
    template<fl::size N>
    UIDropdownImpl(const fl::string &name, const fl::string (&options)[N])
        : UIDropdownImpl(name, options, N) {}

    UIDropdownImpl(const fl::string &name, const fl::vector<fl::string>& options)
        : mSelectedIndex(0) {
        FASTLED_UNUSED(name);
        for (fl::size i = 0; i < options.size(); ++i) {
            mOptions.push_back(options[i]);
        }
        if (mOptions.empty()) {
            mOptions.push_back(fl::string("No options"));
        }
    }

    template<typename Iterator>
    UIDropdownImpl(const fl::string &name, Iterator begin, Iterator end)
        : mSelectedIndex(0) {
        FASTLED_UNUSED(name);
        for (Iterator it = begin; it != end; ++it) {
            mOptions.push_back(*it);
        }
        if (mOptions.empty()) {
            mOptions.push_back(fl::string("No options"));
        }
    }

    UIDropdownImpl(const fl::string &name, fl::span<fl::string> options)
        : mSelectedIndex(0) {
        FASTLED_UNUSED(name);
        for (fl::size i = 0; i < options.size(); ++i) {
            mOptions.push_back(options[i]);
        }
        if (mOptions.empty()) {
            mOptions.push_back(fl::string("No options"));
        }
    }

    UIDropdownImpl(const fl::string &name, fl::initializer_list<fl::string> options)
        : mSelectedIndex(0) {
        FASTLED_UNUSED(name);
        for (const auto& option : options) {
            mOptions.push_back(option);
        }
        if (mOptions.empty()) {
            mOptions.push_back(fl::string("No options"));
        }
    }

    ~UIDropdownImpl() FL_NOEXCEPT {}

    fl::string value() const {
        if (mSelectedIndex < mOptions.size()) {
            return mOptions[mSelectedIndex];
        }
        return fl::string("Invalid");
    }

    int value_int() const { return static_cast<int>(mSelectedIndex); }

    void setSelectedIndex(int index) {
        if (index >= 0 && index < static_cast<int>(mOptions.size())) {
            mSelectedIndex = static_cast<fl::size>(index);
        }
    }

    fl::size getOptionCount() const { return mOptions.size(); }
    fl::string getOption(fl::size index) const {
        if (index < mOptions.size()) {
            return mOptions[index];
        }
        return fl::string("Invalid");
    }

    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    UIDropdownImpl(const fl::string &name, const fl::string* options, fl::size count)
        : mSelectedIndex(0) {
        FASTLED_UNUSED(name);
        for (fl::size i = 0; i < count; ++i) {
            mOptions.push_back(options[i]);
        }
        if (mOptions.empty()) {
            mOptions.push_back(fl::string("No options"));
        }
    }

    fl::vector<fl::string> mOptions;
    fl::size mSelectedIndex;
};
#endif

class UIDropdown : public UIElement {
  public:
    FL_NO_COPY(UIDropdown)

    UIDropdown(const char *name, fl::span<fl::string> options) FL_NOEXCEPT;
    UIDropdown(const char *name, fl::initializer_list<fl::string> options) FL_NOEXCEPT;
    ~UIDropdown() FL_NOEXCEPT;

    fl::string value() const FL_NOEXCEPT { return mImpl.value(); }
    int as_int() const FL_NOEXCEPT { return mImpl.value_int(); }
    fl::string as_string() const FL_NOEXCEPT { return value(); }

    void setSelectedIndex(int index) FL_NOEXCEPT {
        mImpl.setSelectedIndex(index);
    }

    fl::size getOptionCount() const FL_NOEXCEPT { return mImpl.getOptionCount(); }
    fl::string getOption(fl::size index) const FL_NOEXCEPT { return mImpl.getOption(index); }

    operator fl::string() const FL_NOEXCEPT { return value(); }
    operator int() const FL_NOEXCEPT { return as_int(); }

    UIDropdown &operator=(int index) FL_NOEXCEPT {
        setSelectedIndex(index);
        return *this;
    }

    // Add a physical button that will advance to the next option when pressed.
    // Concrete implementation lives in fl/sensors/ui_button_integration.cpp.hpp
    // to keep the Button dependency out of the fl.cpp link chain.
    void addNextButton(int pin) FL_NOEXCEPT;

    void nextOption() FL_NOEXCEPT {
        int currentIndex = as_int();
        int nextIndex = (currentIndex + 1) % static_cast<int>(getOptionCount());
        setSelectedIndex(nextIndex);
    }

    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        mImpl.setGroup(groupName);
    }

    int onChanged(function<void(UIDropdown &)> callback) FL_NOEXCEPT {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UIDropdownImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UIDropdown *owner) FL_NOEXCEPT : mOwner(owner) {}
        ~Listener() FL_NOEXCEPT {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() FL_NOEXCEPT {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() FL_NOEXCEPT override;

      private:
        UIDropdown *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UIDropdown &)> mCallbacks;
    int mLastFrameValue = -1;
    bool mLastFrameValueValid = false;
    fl::shared_ptr<IButtonInput> mNextButton;  // Must be before mListener for init order
    Listener mListener;
};

FASTLED_UI_DEFINE_OPERATORS(UIDropdown)

} // namespace fl
