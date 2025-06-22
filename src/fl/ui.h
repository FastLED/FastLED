#pragma once

#include "fl/stdint.h"

#include "fl/audio.h"
#include "fl/engine_events.h"
#include "fl/function_list.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/template_magic.h"
#include "fl/ui_impl.h"
#include "fl/unused.h"
#include "platforms/ui_defs.h"
#include "sensors/button.h"

#define FL_NO_COPY(CLASS)                                                      \
    CLASS(const CLASS &) = delete;                                             \
    CLASS &operator=(const CLASS &) = delete;

namespace fl {

// Base class for UI elements that provides string-based group functionality
class UIBase {
  public:
    UIBase() {}
    virtual ~UIBase() {}
    
    virtual void setGroup(const fl::string& groupName) { mGroupName = groupName; }
    virtual void setGroup(const char* groupName) { mGroupName = fl::string(groupName); }
    fl::string getGroup() const { return mGroupName; }
    bool hasGroup() const { return !mGroupName.empty(); }

  private:
    fl::string mGroupName;
};

// If the platform is missing ui components, provide stubs.

class UISlider : protected UISliderImpl, public UIBase {
  public:
    FL_NO_COPY(UISlider)
    using Super = UISliderImpl;
    // If step is -1, it will be calculated as (max - min) / 100
    UISlider(const char *name, float value = 128.0f, float min = 1,
             float max = 255, float step = -1.f)
        : UISliderImpl(name, value, min, max, step), mListener(this) {}
    float value() const { return Super::value(); }
    float value_normalized() const {
        float min = Super::getMin();
        float max = Super::getMax();
        if (ALMOST_EQUAL(max, min, 0.0001f)) {
            return 0;
        }
        return (value() - min) / (max - min);
    }
    float getMax() const { return Super::getMax(); }
    void setValue(float value);
    operator float() const { return Super::value(); }
    operator uint8_t() const { return static_cast<uint8_t>(Super::value()); }
    operator uint16_t() const { return static_cast<uint16_t>(Super::value()); }
    operator int() const { return static_cast<int>(Super::value()); }
    template <typename T> T as() const {
        return static_cast<T>(Super::value());
    }

    int as_int() const { return static_cast<int>(Super::value()); }

    UISlider &operator=(float value) {
        Super::setValue(value);
        return *this;
    }
    UISlider &operator=(int value) {
        Super::setValue(static_cast<float>(value));
        return *this;
    }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }

    int onChanged(function<void(UISlider &)> callback) {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(UISlider *owner) : mOwner(owner) {
            EngineEvents::addListener(this);
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
    FunctionList<UISlider &> mCallbacks;
    float mLastFrameValue = 0;
    bool mLastFramevalueValid = false;
    Listener mListener;
};

// template operator for >= against a jsSliderImpl

class UIButton : protected UIButtonImpl, public UIBase {
  public:
    FL_NO_COPY(UIButton)
    using Super = UIButtonImpl;
    UIButton(const char *name) : UIButtonImpl(name), mListener(this) {}
    ~UIButton() {}
    bool isPressed() const {
        if (Super::isPressed()) {
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
        if (Super::clicked()) {
            return true;
        }
        if (mRealButton) {
            // If we have a real button, check if it was clicked
            return mRealButton->isPressed();
        }
        return false;
    }
    int clickedCount() const { return Super::clickedCount(); }
    operator bool() const { return clicked(); }

    void addRealButton(const Button& pin) {
        mRealButton.reset(new Button(pin));
    }

    void click() { Super::click(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
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

    void removeCallback(int id) { mCallbacks.remove(id); }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(UIButton *owner) : mOwner(owner) {
            EngineEvents::addListener(this);
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
    };

  private:
    FunctionList<UIButton &> mCallbacks;
    Listener mListener;
    fl::scoped_ptr<Button> mRealButton;
};

class UICheckbox : protected UICheckboxImpl, public UIBase {
  public:
    FL_NO_COPY(UICheckbox);
    using Super = UICheckboxImpl;
    UICheckbox(const char *name, bool value = false)
        : UICheckboxImpl(name, value), mListener(this) {}
    ~UICheckbox() {}

    operator bool() const { return value(); }
    explicit operator int() const { return static_cast<int>(value()); }
    UICheckbox &operator=(bool value) {
        impl() = value;
        return *this;
    }
    bool value() const { return Super::value(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }

    void onChanged(function<void(UICheckbox &)> callback) {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(UICheckbox *owner) : mOwner(owner) {
            EngineEvents::addListener(this);
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
    Super &impl() { return *this; }
    FunctionList<UICheckbox &> mCallbacks;
    bool mLastFrameValue = false;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

class UINumberField : protected UINumberFieldImpl, public UIBase {
  public:
    FL_NO_COPY(UINumberField);
    using Super = UINumberFieldImpl;
    UINumberField(const char *name, double value, double min = 0,
                  double max = 100)
        : UINumberFieldImpl(name, value, min, max), mListener(this) {}
    ~UINumberField() {}
    double value() const { return Super::value(); }
    void setValue(double value) { Super::setValue(value); }
    operator double() const { return Super::value(); }
    operator int() const { return static_cast<int>(Super::value()); }
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
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }

    void onChanged(function<void(UINumberField &)> callback) {
        mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
    }
    void clearCallbacks() { mCallbacks.clear(); }

  private:
    struct Listener : public EngineEvents::Listener {
        Listener(UINumberField *owner) : mOwner(owner) {
            EngineEvents::addListener(this);
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
    FunctionList<UINumberField &> mCallbacks;

    Super &impl() { return *this; }
};

class UITitle : protected UITitleImpl, public UIBase {
  public:
    FL_NO_COPY(UITitle);
    using Super = UITitleImpl;
    UITitle(const char *name) : UITitleImpl(name) {}
    ~UITitle() {}
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }
};

class UIDescription : protected UIDescriptionImpl, public UIBase {
  public:
    FL_NO_COPY(UIDescription);
    using Super = UIDescriptionImpl;
    UIDescription(const char *name) : UIDescriptionImpl(name) {}
    ~UIDescription() {}
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }
};

class UIAudio : protected UIAudioImpl, public UIBase {
  public:
    FL_NO_COPY(UIAudio)
    using Super = UIAudioImpl;
    UIAudio(const char *name) : UIAudioImpl(name) {}
    ~UIAudio() {}
    AudioSample next() { return Super::next(); }
    bool hasNext() { return Super::hasNext(); }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }
};

class UIDropdown : protected UIDropdownImpl, public UIBase {
  public:
    FL_NO_COPY(UIDropdown)
    using Super = UIDropdownImpl;

    template<typename Iterator>
    UIDropdown(const char *name, Iterator begin, Iterator end)
        : UIDropdownImpl(name, begin, end), mListener(this) {}

    // Constructor with fl::Slice<fl::string> for arrays and containers.
    UIDropdown(const char *name, fl::Slice<fl::string> options)
        : UIDropdownImpl(name, options), mListener(this) {}

    // Constructor with initializer_list
    UIDropdown(const char *name, fl::initializer_list<fl::string> options)
        : UIDropdownImpl(name, options), mListener(this) {}

    ~UIDropdown() {}
    
    fl::string value() const { return Super::value(); }
    int value_int() const { return Super::value_int(); }
    
    void setSelectedIndex(int index) { 
        Super::setSelectedIndex(index); 
    }
    
    size_t getOptionCount() const { return Super::getOptionCount(); }
    fl::string getOption(size_t index) const { return Super::getOption(index); }
    
    operator fl::string() const { return value(); }
    operator int() const { return value_int(); }
    
    UIDropdown &operator=(int index) {
        setSelectedIndex(index);
        return *this;
    }
    
    // Override setGroup to also update the implementation
    void setGroup(const fl::string& groupName) override { 
        UIBase::setGroup(groupName); 
        // Update the implementation's group if it has the method (WASM platforms)
        Super::setGroup(groupName);
    }
    void setGroup(const char* groupName) override { 
        setGroup(fl::string(groupName)); 
    }

    int onChanged(function<void(UIDropdown &)> callback) {
        int out = mCallbacks.add(callback);
        mListener.addToEngineEventsOnce();
        return out;
    }
    void clearCallbacks() { mCallbacks.clear(); }

  protected:
    struct Listener : public EngineEvents::Listener {
        Listener(UIDropdown *owner) : mOwner(owner) {
            EngineEvents::addListener(this);
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
    FunctionList<UIDropdown &> mCallbacks;
    int mLastFrameValue = -1;
    bool mLastFrameValueValid = false;
    Listener mListener;
};

class UIGroup : protected UIGroupImpl {
  public:
    FL_NO_COPY(UIGroup);
    using Super = UIGroupImpl;
    
    // Constructor takes fl::string as the only parameter for grouping name
    UIGroup(const fl::string& groupName) : UIGroupImpl(groupName.c_str()) {}
    
    // Variadic template constructor: first argument is group name, remaining are UI elements
    template<typename... UIElements>
    UIGroup(const fl::string& groupName, UIElements&... elements) 
        : UIGroupImpl(groupName.c_str()) {
        // Add all UI elements to this group
        add(elements...);
    }
    
    ~UIGroup() {}
    
    // Get the group name
    fl::string name() const { return Super::name(); }
    
    // Implicit conversion to string for convenience
    operator fl::string() const { return name(); }

    // Add control to the group
    template<typename T>
    void addControl(T* control) {
        control->setGroup(name());
    }

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
