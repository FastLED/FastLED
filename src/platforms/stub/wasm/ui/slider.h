#pragma once

#include <stdint.h>
#include <atomic>
#include <string>
#include <sstream>

#include "ui.h"

class jsSlider : public jsUI {
  public:
    jsSlider(const std::string& name, float min = 0.0f, float max = 255.0f, float value = 128.0f, float step = 1.0f)
        : mName(name), mMin(min), mMax(max), mValue(value), mStep(step) {
        jsUiManager::addComponent(jsUIPtr::TakeOwnership(this));
    }

    std::string name() const override { return mName; }
    std::string type() const override { return "slider"; }

    std::string toJsonStr() const override {
      std::string str = "{\"type\":\"" + type() + 
                        "\",\"name\":\"" + name() + 
                        "\",\"id\":" + std::to_string(id()) + 
                        ",\"min\":" + std::to_string(mMin) + 
                        ",\"max\":" + std::to_string(mMax) + 
                        ",\"value\":" + std::to_string(mValue) + 
                        ",\"step\":" + std::to_string(mStep) + 
                        "}";
      return str;
    }

    float value() const { return mValue; }
    virtual void update() override {}

    void setValue(float value) {
        mValue = value;
    }
    // Operator for implicit conversion to float
    operator float() const { return mValue; }
    // Getter for the unique identifier

  private:
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    std::string mName;
    

};

DECLARE_SMART_PTR_NO_FWD(jsSlider);



