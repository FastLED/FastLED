#include "slider.h"
#include <sstream>

jsSlider::jsSlider(const std::string& name, float min, float max, float value, float step)
    : mName(name), mMin(min), mMax(max), mValue(value), mStep(step) {
    jsUiManager::addComponent(jsUIPtr::TakeOwnership(this));
}

std::string jsSlider::name() const { 
    return mName; 
}

std::string jsSlider::type() const { 
    return "slider"; 
}

std::string jsSlider::toJsonStr() const {
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

float jsSlider::value() const { 
    return mValue; 
}

void jsSlider::update(const char* jsonStr) {
    // Implementation left empty as in the original
}

void jsSlider::setValue(float value) {
    mValue = value;
}

jsSlider::operator float() const { 
    return mValue; 
}
