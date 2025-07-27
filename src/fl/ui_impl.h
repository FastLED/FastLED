#pragma once

#include "fl/stdint.h"

#include "fl/audio.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "fl/type_traits.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "platforms/ui_defs.h"

#ifndef FASTLED_HAS_UI_SLIDER
#define FASTLED_HAS_UI_SLIDER 0
#endif

#ifndef FASTLED_HAS_UI_BUTTON
#define FASTLED_HAS_UI_BUTTON 0
#endif

#ifndef FASTLED_HAS_UI_CHECKBOX
#define FASTLED_HAS_UI_CHECKBOX 0
#endif

#ifndef FASTLED_HAS_UI_NUMBER_FIELD
#define FASTLED_HAS_UI_NUMBER_FIELD 0
#endif

#ifndef FASTLED_HAS_UI_TITLE
#define FASTLED_HAS_UI_TITLE 0
#endif

#ifndef FASTLED_HAS_UI_DESCRIPTION
#define FASTLED_HAS_UI_DESCRIPTION 0
#endif

#ifndef FASTLED_HAS_UI_AUDIO
#define FASTLED_HAS_UI_AUDIO 0
#endif

#ifndef FASTLED_HAS_UI_DROPDOWN
#define FASTLED_HAS_UI_DROPDOWN 0
#endif

namespace fl {

// If the platform is missing ui components, provide stubs.

#if !FASTLED_HAS_UI_SLIDER

class UISliderImpl {
  public:
    // If step is -1, it will be calculated as (max - min) / 100
    UISliderImpl(const char *name, float value = 128.0f, float min = 1,
                 float max = 255, float step = -1.f)
        : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
        FASTLED_UNUSED(step);
        if (value < min) {
            mValue = min;
        }
        if (value > max) {
            mValue = max;
        }
    }
    ~UISliderImpl() {}
    float value() const { return mValue; }
    float getMax() const { return mMax; }
    float getMin() const { return mMin; }
    void setValue(float value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator float() const { return mValue; }
    operator u8() const { return static_cast<u8>(mValue); }
    operator u16() const { return static_cast<u16>(mValue); }
    operator int() const { return static_cast<int>(mValue); }
    template <typename T> T as() const { return static_cast<T>(mValue); }

    int as_int() const { return static_cast<int>(mValue); }
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

    UISliderImpl &operator=(float value) {
        setValue(value);
        return *this;
    }
    UISliderImpl &operator=(int value) {
        setValue(static_cast<float>(value));
        return *this;
    }

  private:
    float mValue;
    float mMin;
    float mMax;
};

// template operator for >= against a jsSliderImpl

#endif

#if !FASTLED_HAS_UI_BUTTON

class UIButtonImpl {
  public:
    UIButtonImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIButtonImpl() {}
    bool isPressed() const { return false; }
    bool clicked() const { return false; }
    int clickedCount() const { return 0; }
    operator bool() const { return false; }
    void click() {}
    fl::string name() const { return mName; }
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    fl::string mName;
};

#endif

#if !FASTLED_HAS_UI_CHECKBOX

class UICheckboxImpl {
  public:
    UICheckboxImpl(const char *name, bool value = false) : mValue(value) {
        FASTLED_UNUSED(name);
    }
    ~UICheckboxImpl() {}
    operator bool() const { return mValue; }
    explicit operator int() const { return mValue ? 1 : 0; }
    UICheckboxImpl &operator=(bool value) {
        setValue(value);
        return *this;
    }
    UICheckboxImpl &operator=(int value) {
        setValue(value != 0);
        return *this;
    }
    bool value() const { return mValue; }
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    void setValue(bool value) { mValue = value; }
    bool mValue;
};

#endif

#if !FASTLED_HAS_UI_NUMBER_FIELD

class UINumberFieldImpl {
  public:
    UINumberFieldImpl(const char *name, double value, double min = 0,
                      double max = 100)
        : mValue(value), mMin(MIN(min, max)), mMax(MAX(min, max)) {
        FASTLED_UNUSED(name);
    }
    ~UINumberFieldImpl() {}
    double value() const { return mValue; }
    void setValue(double value) { mValue = MAX(mMin, MIN(mMax, value)); }
    operator double() const { return mValue; }
    operator int() const { return static_cast<int>(mValue); }
    UINumberFieldImpl &operator=(double value) {
        setValue(value);
        return *this;
    }
    UINumberFieldImpl &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }

  private:
    double mValue;
    double mMin;
    double mMax;
};

#endif

#if !FASTLED_HAS_UI_TITLE

class UITitleImpl {
  public:
    UITitleImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UITitleImpl() {}
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }
};

#endif

#if !FASTLED_HAS_UI_DESCRIPTION

class UIDescriptionImpl {
  public:
    UIDescriptionImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIDescriptionImpl() {}
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }
};

#endif

#if !FASTLED_HAS_UI_HELP
class UIHelpImpl {
  public:
    UIHelpImpl(const char *markdownContent) : mContent(markdownContent) { FASTLED_UNUSED(markdownContent); }
    ~UIHelpImpl() {}
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }
    
    // Stub method for accessing markdown content
    const fl::string& markdownContent() const { return mContent; }
    
  private:
    fl::string mContent;
};

#endif

#if !FASTLED_HAS_UI_AUDIO
class UIAudioImpl {
  public:
    UIAudioImpl(const char *name) { FASTLED_UNUSED(name); }
    ~UIAudioImpl() {}

    AudioSample next() {
        FASTLED_WARN("Audio sample not implemented");
        return AudioSample();
    }

    bool hasNext() {
        FASTLED_WARN("Audio sample not implemented");
        return false;
    }
    
    // Stub method for group setting (does nothing on non-WASM platforms)
    void setGroup(const fl::string& groupName) { FASTLED_UNUSED(groupName); }
};
#endif

#if !FASTLED_HAS_UI_DROPDOWN
class UIDropdownImpl {
  public:
    // Constructor with array of options (size determined automatically)
    template<fl::size N>
    UIDropdownImpl(const fl::string &name, const fl::string (&options)[N]) 
        : UIDropdownImpl(name, options, N) {}
    
    // Constructor with fl::vector of options
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

    // Constructor with fl::span<fl::string>
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

    // Constructor with initializer_list (only available if C++11 support exists)
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

    ~UIDropdownImpl() {}
    
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
    // Private constructor with array of options and count (used by template constructor)
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

#ifndef FASTLED_HAS_UI_GROUP
#define FASTLED_HAS_UI_GROUP 0
#endif

#if !FASTLED_HAS_UI_GROUP

class UIGroupImpl {
  public:
    UIGroupImpl(const char *name) : mGroupName(name) { 
        FASTLED_UNUSED(name); 
    }
    ~UIGroupImpl() {}
    fl::string name() const { return mGroupName; }

  private:
    fl::string mGroupName;
};

#endif

} // end namespace fl
