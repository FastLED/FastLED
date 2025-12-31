#pragma once


#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/json.h"
#include "fl/str.h"
#include "fl/int.h"
#include "fl/audio.h"
#include "fl/audio_input.h"  // For AudioConfig
#include "fl/engine_events.h"
#include "fl/stl/function.h"
#include "fl/math_macros.h"
#include "fl/stl/type_traits.h"
#include "fl/ui_impl.h"
#include "fl/unused.h"
#include "platforms/ui_defs.h"
#include "fl/sensors/button.h"
#include "fl/virtual_if_not_avr.h"
#include "fl/int.h"

#define FL_NO_COPY(CLASS)                                                      \
    CLASS(const CLASS &) = delete;                                             \
    CLASS &operator=(const CLASS &) = delete;

namespace fl {

// Base class for UI elements that provides string-based group functionality
class UIElement {
  public:
    UIElement();
    VIRTUAL_IF_NOT_AVR ~UIElement() {}
    virtual void setGroup(const fl::string& groupName) { mGroupName = groupName; }

    fl::string getGroup() const { return mGroupName; }
    bool hasGroup() const { return !mGroupName.empty(); }

  private:
    fl::string mGroupName;
};

// If the platform is missing ui components, provide stubs.

class UISlider : public UIElement {
  public:
    FL_NO_COPY(UISlider)
    // If step is -1, it will be calculated as (max - min) / 100
    UISlider(const char *name, float value = 128.0f, float min = 1,
             float max = 255, float step = -1.f);
    float value() const { return mImpl.value(); }
    float value_normalized() const {
        float min = mImpl.getMin();
        float max = mImpl.getMax();
        if (FL_ALMOST_EQUAL(max, min, 0.0001f)) {
            return 0;
        }
        return (value() - min) / (max - min);
    }
    float getMax() const { return mImpl.getMax(); }
    float getMin() const { return mImpl.getMin(); }
    void setValue(float value);
    operator float() const { return mImpl.value(); }
    operator u8() const { return static_cast<u8>(mImpl.value()); }
    operator fl::u16() const { return static_cast<fl::u16>(mImpl.value()); }
    operator int() const { return static_cast<int>(mImpl.value()); }
    template <typename T> T as() const {
        return static_cast<T>(mImpl.value());
    }

    int as_int() const { return static_cast<int>(mImpl.value()); }

    UISlider &operator=(float value) {
        mImpl.setValue(value);
        return *this;
    }
    UISlider &operator=(int value) {
        mImpl.setValue(static_cast<float>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    int onChanged(function<void(UISlider &)> callback) {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    UISliderImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UISlider *owner) : mOwner(owner) {
            
        }
        ~Listener() {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() override;

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
    UIButton(const char *name);
    ~UIButton();
    bool isPressed() const {
        if (mImpl.isPressed()) {
            return true;
        }
        // If we have a real button, check if it's pressed
        if (mRealButton) {
            return mRealButton->isPressed();
        }
        // Otherwise, return the default state
        return false;
    }
    bool clicked() const {
        if (mImpl.clicked()) {
            return true;
        }
        if (mRealButton) {
            // If we have a real button, check if it was clicked
            return mRealButton->isPressed();
        }
        return false;
    }
    int clickedCount() const { return mImpl.clickedCount(); }
    operator bool() const { return clicked(); }
    bool value() const { return clicked(); }

    void addRealButton(fl::shared_ptr<Button> button) {
        mRealButton = button;
    }

    void click() { mImpl.click(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }
    
    int onChanged(function<void(UIButton &)> callback) {
        int id = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onClicked(function<void()> callback) {
        int id = mCallbacks.add([callback](UIButton &btn) {
            if (btn.clicked()) {
                callback();
            }
        });
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onPressed(function<void()> callback) {
        int id = mPressCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    int onReleased(function<void()> callback) {
        int id = mReleaseCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return id;
    }

    void removeCallback(int id) { mCallbacks.remove(id); }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    UIButtonImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UIButton *owner) : mOwner(owner) {
        }
        ~Listener() {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() override;

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
    fl::shared_ptr<Button> mRealButton;
};

class UICheckbox : public UIElement {
  public:
    FL_NO_COPY(UICheckbox);
    UICheckbox(const char *name, bool value = false);
    ~UICheckbox();

    operator bool() const { return value(); }
    explicit operator int() const { return static_cast<int>(value()); }
    UICheckbox &operator=(bool value) {
        mImpl = value;
        return *this;
    }
    bool value() const { return mImpl.value(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UICheckbox &)> callback) {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    UICheckboxImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UICheckbox *owner) : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
        ~Listener() {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() override;

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
                  double max = 100);
    ~UINumberField();
    double value() const { return mImpl.value(); }
    void setValue(double value) { mImpl.setValue(value); }
    operator double() const { return mImpl.value(); }
    operator int() const { return static_cast<int>(mImpl.value()); }
    UINumberField &operator=(double value) {
        setValue(value);
        return *this;
    }
    UINumberField &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }

    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }


    void onChanged(function<void(UINumberField &)> callback) {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    UINumberFieldImpl mImpl;

  private:
    struct Listener : public EngineEvents::Listener {
        Listener(UINumberField *owner) : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
        ~Listener() {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() override;

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
    UITitle(const char *name);
    ~UITitle();
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
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
    UIDescription(const char *name);
    ~UIDescription();
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
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
    UIHelp(const char *markdownContent);
    ~UIHelp();
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

    // Access to the markdown content
    const fl::string& markdownContent() const { return mImpl.markdownContent(); }

  protected:
    UIHelpImpl mImpl;
};

class UIAudio : public UIElement {
  public:
    FL_NO_COPY(UIAudio)
    UIAudio(const char *name);
    UIAudio(const char *name, const fl::AudioConfig& config);
    ~UIAudio();
    AudioSample next() { return mImpl.next(); }
    bool hasNext() { return mImpl.hasNext(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

  protected:
    UIAudioImpl mImpl;
};

class UIDropdown : public UIElement {
  public:
    FL_NO_COPY(UIDropdown)

    

    // Constructor with fl::span<fl::string> for arrays and containers.
    UIDropdown(const char *name, fl::span<fl::string> options);

    // Constructor with initializer_list
    UIDropdown(const char *name, fl::initializer_list<fl::string> options);

    ~UIDropdown();
    
    fl::string value() const { return mImpl.value(); }
    int as_int() const { return mImpl.value_int(); }
    fl::string as_string() const { return value(); }
    
    void setSelectedIndex(int index) { 
        mImpl.setSelectedIndex(index); 
    }
    
    fl::size getOptionCount() const { return mImpl.getOptionCount(); }
    fl::string getOption(fl::size index) const { return mImpl.getOption(index); }
    
    operator fl::string() const { return value(); }
    operator int() const { return as_int(); }
    
    UIDropdown &operator=(int index) {
        setSelectedIndex(index);
        return *this;
    }
    
    // Add a physical button that will advance to the next option when pressed
    void addNextButton(int pin) {
        mNextButton = fl::make_shared<Button>(pin);
    }
    
    // Advance to the next option (cycles back to first option after last)
    void nextOption() {
        int currentIndex = as_int();
        int nextIndex = (currentIndex + 1) % static_cast<int>(getOptionCount());
        setSelectedIndex(nextIndex);
    }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIElement::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        mImpl.setGroup(groupName);
    }

    int onChanged(function<void(UIDropdown &)> callback) {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    UIDropdownImpl mImpl;

    struct Listener : public EngineEvents::Listener {
        Listener(UIDropdown *owner) : mOwner(owner) {
            // Don't register in constructor - prevents callbacks before owner is fully initialized
        }
        ~Listener() {
            if (added) {
                EngineEvents::removeListener(this);
            }
        }
        void addToEngineEventsOnce() {
            if (added) {
                return;
            }
            EngineEvents::addListener(this);
            added = true;
        }
        void onBeginFrame() override;

      private:
        UIDropdown *mOwner;
        bool added = false;
    };

  private:
    function_list<void(UIDropdown &)> mCallbacks;
    int mLastFrameValue = -1;
    bool mLastFrameValueValid = false;
    fl::shared_ptr<Button> mNextButton;  // Must be before mListener to ensure proper initialization order
    Listener mListener;
};

class UIGroup {
  public:
    FL_NO_COPY(UIGroup);

    // Constructor takes fl::string as the only parameter for grouping name
    UIGroup(const fl::string& groupName);

    // Variadic template constructor: first argument is group name, remaining are UI elements
    template<typename... UIElements>
    UIGroup(const fl::string& groupName, UIElements&... elements)
        : mImpl(groupName.c_str()) {
        // Add all UI elements to this group
        add(elements...);
    }

    ~UIGroup();
    
    // Get the group name
    fl::string name() const { return mImpl.name(); }
    
    // Implicit conversion to string for convenience
    operator fl::string() const { return name(); }

    // Add control to the group
    template<typename T>
    void addControl(T* control) {
        control->setGroup(name());
    }

  protected:
    UIGroupImpl mImpl;

private:
    // Helper method to add multiple controls using variadic templates
    template<typename T>
    void add(T& control) {
        // Base case: add single control
        control.setGroup(name());
    }
    
    template<typename T, typename... Rest>
    void add(T& control, Rest&... rest) {
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

FASTLED_UI_DEFINE_OPERATORS(UISlider);
FASTLED_UI_DEFINE_OPERATORS(UINumberField);
FASTLED_UI_DEFINE_OPERATORS(UICheckbox);
FASTLED_UI_DEFINE_OPERATORS(UIButton);
FASTLED_UI_DEFINE_OPERATORS(UIDropdown);

} // end namespace fl
