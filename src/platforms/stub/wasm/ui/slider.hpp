#include "slider.h"
#include "ui_manager.h"
#include <sstream>

FASTLED_NAMESPACE_BEGIN

jsSlider::jsSlider(const std::string& name, float min, float max, float value, float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonStrFunc = [this]() { return this->toJsonStr(); };
    mInternal = std::make_shared<jsUiInternal>(name.c_str(), std::move(updateFunc), std::move(toJsonStrFunc));
    jsUiManager::addComponent(mInternal);
}

jsSlider::~jsSlider() {
    jsUiManager::removeComponent(mInternal);
}

std::string jsSlider::name() const {
    return mInternal->name();
}

std::string jsSlider::toJsonStr() const {
    std::ostringstream oss;
    oss << "{"
        << "\"name\":\"Slider: " << name() << "\""
        << ",\"type\":\"slider\""
        << ",\"id\":" << mInternal->id()
        << ",\"min\":" << mMin
        << ",\"max\":" << mMax
        << ",\"value\":" << mValue
        << ",\"step\":" << mStep
        << "}";
    return oss.str();
}

float jsSlider::value() const { 
    return mValue; 
}

void jsSlider::updateInternal(const char* jsonStr) {
    // TODO: Implement the update logic here
}

void jsSlider::setValue(float value) {
    mValue = std::max(mMin, std::min(mMax, value));
    // TODO: Notify UI of value change
}

jsSlider::operator float() const { 
    return mValue; 
}

FASTLED_NAMESPACE_END
