#pragma once

#include <stdint.h>
#include <atomic>
#include <string>

#include "ui.h"


template <typename T>
struct TypeName {
};

template <>
struct TypeName<int> {
    static const char* get() { return "int"; }
};

template <>
struct TypeName<float> {
    static const char* get() { return "float"; }
};

template <>
struct TypeName<uint8_t> {
    static const char* get() { return "uint8_t"; }
};

template <typename T>
class jsSlider : public jsUI {
  public:
    jsSlider(const std::string& name, T min = 0, T max = 255, T value = 128, T step = 1, T* valuePtr = nullptr)
        : mName(name), mMin(min), mMax(max), mValue(value), mStep(step), mValuePtr(valuePtr), mId(sNextId++) {
        updateValuePtr();
        jsUiManager::addComponent(jsUIPtr::TakeOwnership(this));
    }

    std::string name() const { return mName; }
    std::string type() const override { return TypeName<T>::get(); }
    T value() const { return mValue; }
    virtual void update() override {}
    // Operator for implicit conversion to T
    operator T() const { return mValue; }
    // Getter for the unique identifier
    uint32_t getId() const { return mId; }


  private:
    T mMin;
    T mMax;
    T mValue;
    T mStep;
    std::string mName;
    T* mValuePtr;
    uint32_t mId;

    void updateValuePtr() {
        if (mValuePtr) {
            *mValuePtr = mValue;
        }
    }

    static std::atomic<uint32_t> sNextId;
};

template <typename T>
std::atomic<uint32_t> jsSlider<T>::sNextId(0);

typedef jsSlider<int> IntSlider;
typedef jsSlider<float> FloatSlider;
typedef jsSlider<uint8_t> Uint8Slider;

//typedef Ptr<jsSlider<int>> IntSliderPtr;
//typedef Ptr<jsSlider<float>> FloatSliderPtr;
//typedef Ptr<jsSlider<uint8_t>> Uint8SliderPtr;

DECLARE_SMART_PTR_NO_FWD(IntSlider);
DECLARE_SMART_PTR_NO_FWD(FloatSlider);
DECLARE_SMART_PTR_NO_FWD(Uint8Slider);