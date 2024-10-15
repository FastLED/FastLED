#pragma once

#include <stdint.h>
#include <atomic>
#include <string>

#include "ui.h"

class jsSlider : public jsUI {
  public:
    jsSlider(const std::string& name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f)
        : mName(name), mMin(min), mMax(max), mValue(value), mStep(step), mId(sNextId++) {
        jsUiManager::addComponent(jsUIPtr::TakeOwnership(this));
    }

    std::string name() const override { return mName; }
    std::string type() const override { return "slider"; }
    float value() const { return mValue; }
    virtual void update() override {}

    void setValue(float value) {
        mValue = value;
    }
    // Operator for implicit conversion to float
    operator float() const { return mValue; }
    // Getter for the unique identifier
    uint32_t getId() const { return mId; }

  private:
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    std::string mName;
    uint32_t mId;

    static std::atomic<uint32_t> sNextId;
};

inline std::atomic<uint32_t> jsSlider::sNextId(0);

DECLARE_SMART_PTR_NO_FWD(jsSlider);



