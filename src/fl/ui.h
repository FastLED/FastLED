#pragma once


#include "fl/asset.h"
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/json.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/stl/int.h"  // IWYU pragma: keep
#include "fl/stl/optional.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_input.h"  // For audio::Config  // IWYU pragma: keep
#include "fl/system/engine_events.h"
#include "fl/stl/function.h"
#include "fl/math/math.h"
#include "fl/stl/type_traits.h"
#include "fl/ui_impl.h"
#include "fl/stl/compiler_control.h"  // IWYU pragma: keep
#include "platforms/ui_defs.h"
#include "fl/stl/int.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

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

// If the platform is missing ui components, provide stubs.

class UISlider : public UIElement {
  public:
    FL_NO_COPY(UISlider)
    // If step is -1, it will be calculated as (max - min) / 100
    UISlider(const char *name, float value = 128.0f, float min = 1,
             float max = 255, float step = -1.f) FL_NOEXCEPT;
    float value() const FL_NOEXCEPT { return mImpl.value(); }
    float value_normalized() const FL_NOEXCEPT {
        float min = mImpl.getMin();
        float max = mImpl.getMax();
        if (fl::almost_equal(max, min, 0.0001f)) {
            return 0;
        }
        return (value() - min) / (max - min);
    }
    float getMax() const FL_NOEXCEPT { return mImpl.getMax(); }
    float getMin() const FL_NOEXCEPT { return mImpl.getMin(); }
    void setValue(float value) FL_NOEXCEPT;
    operator float() const FL_NOEXCEPT { return mImpl.value(); }
    operator u8() const FL_NOEXCEPT { return static_cast<u8>(mImpl.value()); }
    operator fl::u16() const FL_NOEXCEPT { return static_cast<fl::u16>(mImpl.value()); }
    operator int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }
    template <typename T> T as() const FL_NOEXCEPT {
        return static_cast<T>(mImpl.value());
    }

    int as_int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }

    UISlider &operator=(float value) FL_NOEXCEPT {
        mImpl.setValue(value);
        return *this;
    }
    UISlider &operator=(int value) FL_NOEXCEPT {
        mImpl.setValue(static_cast<float>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    int onChanged(function<void(UISlider &)> callback) FL_NOEXCEPT {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UISliderImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UISlider *owner) FL_NOEXCEPT : mOwner(owner) {
            
        }
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
        UISlider *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UISlider &)> mCallbacks;
    float mLastFrameValue = 0;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

// template operator for >= against a jsSliderImpl

class UIButton : public UIElement {
  public:
    FL_NO_COPY(UIButton)
    UIButton(const char *name) FL_NOEXCEPT;
    ~UIButton() FL_NOEXCEPT;
    bool isPressed() const FL_NOEXCEPT {
        if (mImpl.isPressed()) {
            return true;
        }
        if (mButtonInput) {
            return mButtonInput->isPressed();
        }
        return false;
    }
    bool clicked() const FL_NOEXCEPT {
        if (mImpl.clicked()) {
            return true;
        }
        if (mButtonInput) {
            return mButtonInput->clicked();
        }
        return false;
    }
    int clickedCount() const FL_NOEXCEPT { return mImpl.clickedCount(); }
    operator bool() const FL_NOEXCEPT { return clicked(); }
    bool value() const FL_NOEXCEPT { return clicked(); }

    void addRealButton(fl::shared_ptr<Button> button) FL_NOEXCEPT;

    void click() FL_NOEXCEPT { mImpl.click(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }
    
    int onChanged(function<void(UIButton &)> callback) FL_NOEXCEPT {
        int id = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onClicked(function<void()> callback) FL_NOEXCEPT {
        int id = mCallbacks.add([callback](UIButton &btn) {
            if (btn.clicked()) {
                callback();
            }
        });
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onPressed(function<void()> callback) FL_NOEXCEPT {
        int id = mPressCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onReleased(function<void()> callback) FL_NOEXCEPT {
        int id = mReleaseCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    void removeCallback(int id) FL_NOEXCEPT { mCallbacks.remove(id); }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UIButtonImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UIButton *owner) FL_NOEXCEPT : mOwner(owner) {
        }
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
        UIButton *mOwner;
        bool added = false;
        bool mClickedLastFrame = false;
        bool mPressedLastFrame = false;
    };

  private:
    function_list<void(UIButton &)> mCallbacks;
    function_list<void()> mPressCallbacks;
    function_list<void()> mReleaseCallbacks;
    Listener mListener;
    fl::shared_ptr<IButtonInput> mButtonInput;
};

class UICheckbox : public UIElement {
  public:
    FL_NO_COPY(UICheckbox);
    UICheckbox(const char *name, bool value = false) FL_NOEXCEPT;
    ~UICheckbox() FL_NOEXCEPT;

    operator bool() const FL_NOEXCEPT { return value(); }
    explicit operator int() const FL_NOEXCEPT { return static_cast<int>(value()); }
    UICheckbox &operator=(bool value) FL_NOEXCEPT {
        mImpl = value;
        return *this;
    }
    bool value() const FL_NOEXCEPT { return mImpl.value(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UICheckbox &)> callback) FL_NOEXCEPT {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UICheckboxImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UICheckbox *owner) FL_NOEXCEPT : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
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
        UICheckbox *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UICheckbox &)> mCallbacks;
    bool mLastFrameValue = false;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

class UINumberField : public UIElement {
  public:
    FL_NO_COPY(UINumberField);
    UINumberField(const char *name, double value, double min = 0,
                  double max = 100) FL_NOEXCEPT;
    ~UINumberField() FL_NOEXCEPT;
    double value() const FL_NOEXCEPT { return mImpl.value(); }
    void setValue(double value) FL_NOEXCEPT { mImpl.setValue(value); }
    operator double() const FL_NOEXCEPT { return mImpl.value(); }
    operator int() const FL_NOEXCEPT { return static_cast<int>(mImpl.value()); }
    UINumberField &operator=(double value) FL_NOEXCEPT {
        setValue(value);
        return *this;
    }
    UINumberField &operator=(int value) FL_NOEXCEPT {
        setValue(static_cast<double>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UINumberField &)> callback) FL_NOEXCEPT {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() FL_NOEXCEPT { mCallbacks.clear(); }

  protected:
    UINumberFieldImpl mImpl;

  private:
    struct Listener : public EngineEvents::Listener {
        Listener(UINumberField *owner) FL_NOEXCEPT : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
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
        UINumberField *mOwner;
        bool added = false;
    };

    Listener mListener;
    double mLastFrameValue = 0;
    bool mLastFrameValueValid = false;
    function_list<void(UINumberField &)> mCallbacks;
};

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

class UIAudio : public UIElement {
  public:
    FL_NO_COPY(UIAudio)
    UIAudio(const fl::string& name) FL_NOEXCEPT;
    UIAudio(const fl::string& name, const fl::url& url) FL_NOEXCEPT;
    /// Asset-handle overload (issue #2284). Resolves `asset` via
    /// `fl::resolve_asset()` at construction time: the manifest/registry
    /// is consulted first, then (on host/stub) the local filesystem and
    /// sibling `.lnk`. If resolution succeeds, forwards to the url()
    /// constructor; otherwise behaves like the name-only constructor.
    UIAudio(const fl::string& name, const fl::asset_ref& asset) FL_NOEXCEPT;
    UIAudio(const fl::string& name, const fl::audio::Config& config) FL_NOEXCEPT;
    ~UIAudio() FL_NOEXCEPT;
    audio::Sample next() FL_NOEXCEPT { return mImpl.next(); }
    bool hasNext() FL_NOEXCEPT { return mImpl.hasNext(); }

    // Expose underlying audio input for FastLED.add() auto-pump
    fl::shared_ptr<audio::IInput> audioInput() FL_NOEXCEPT { return mImpl.audioInput(); }

    // Returns the hardware microphone config, if one was provided.
    const fl::optional<audio::Config>& config() const FL_NOEXCEPT { return mConfig; }

    // Lazily registers with CFastLED::add() on first call and returns the
    // auto-pumped audio::Processor. Subsequent calls return the cached processor.
    fl::shared_ptr<audio::Processor> processor() FL_NOEXCEPT;

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName);
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

  protected:
    UIAudioImpl mImpl;
    fl::optional<audio::Config> mConfig;
    fl::shared_ptr<audio::Processor> mProcessor;
};

class UIDropdown : public UIElement {
  public:
    FL_NO_COPY(UIDropdown)

    

    // Constructor with fl::span<fl::string> for arrays and containers.
    UIDropdown(const char *name, fl::span<fl::string> options) FL_NOEXCEPT;

    // Constructor with initializer_list
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
    
    // Add a physical button that will advance to the next option when pressed
    void addNextButton(int pin) FL_NOEXCEPT;
    
    // Advance to the next option (cycles back to first option after last)
    void nextOption() FL_NOEXCEPT {
        int currentIndex = as_int();
        int nextIndex = (currentIndex + 1) % static_cast<int>(getOptionCount());
        setSelectedIndex(nextIndex);
    }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) FL_NOEXCEPT override {
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
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
        Listener(UIDropdown *owner) FL_NOEXCEPT : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
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
    fl::shared_ptr<IButtonInput> mNextButton;  // Must be before mListener to ensure proper initialization order
    Listener mListener;
};

class UIGroup {
  public:
    FL_NO_COPY(UIGroup);

    // Constructor takes fl::string as the only parameter for grouping name
    UIGroup(const fl::string& groupName) FL_NOEXCEPT;

    // Variadic template constructor: first argument is group name, remaining are UI elements
    template<typename... UIElements>
    UIGroup(const fl::string& groupName, UIElements&... elements) FL_NOEXCEPT
        : mImpl(groupName.c_str()) {
        // Add all UI elements to this group
        add(elements...);
    }

    ~UIGroup() FL_NOEXCEPT;
    
    // Get the group name
    fl::string name() const FL_NOEXCEPT { return mImpl.name(); }
    
    // Implicit conversion to string for convenience
    operator fl::string() const FL_NOEXCEPT { return name(); }

    // Add control to the group
    template<typename T>
    void addControl(T* control) FL_NOEXCEPT {
        control->setGroup(name());
    }

  protected:
    UIGroupImpl mImpl;

private:
    // Helper method to add multiple controls using variadic templates
    template<typename T>
    void add(T& control) FL_NOEXCEPT {
        // Base case: add single control
        control.setGroup(name());
    }
    
    template<typename T, typename... Rest>
    void add(T& control, Rest&... rest) FL_NOEXCEPT {
        // Recursive case: add first control, then recurse with remaining
        control.setGroup(name());
        add(rest...);
    }
};

#define FASTLED_UI_DEFINE_OPERATORS(UI_CLASS)                                  \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >=)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <=)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, >)                        \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, <)                        \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, ==)                       \
    FASTLED_DEFINE_POD_COMPARISON_OPERATOR(UI_CLASS, !=)

FASTLED_UI_DEFINE_OPERATORS(UISlider)
FASTLED_UI_DEFINE_OPERATORS(UINumberField)
FASTLED_UI_DEFINE_OPERATORS(UICheckbox)
FASTLED_UI_DEFINE_OPERATORS(UIButton)
FASTLED_UI_DEFINE_OPERATORS(UIDropdown)

} // end namespace fl
