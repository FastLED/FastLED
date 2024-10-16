#include "slider.h"
#include "ui_manager.h"
#include <sstream>
#include "json.h"

FASTLED_NAMESPACE_BEGIN



jsSlider::jsSlider(const char* name, float min, float max, float value, float step)
    : mMin(min), mMax(max), mValue(value), mStep(step) {
    auto updateFunc = [this](const char* jsonStr) { this->updateInternal(jsonStr); };
    auto toJsonStrFunc = [this]() { return this->toJsonStr(); };
    mInternal = std::make_shared<jsUiInternal>(name, std::move(updateFunc), std::move(toJsonStrFunc));
    jsUiManager::addComponent(mInternal);
}

jsSlider::~jsSlider() {
    jsUiManager::removeComponent(mInternal);
}

const char* jsSlider::name() const {
    return mInternal->name();
}

std::string jsSlider::toJsonStr() const {
    JsonDictEncoder encoder;
    char nameBuf[256];
    snprintf(nameBuf, sizeof(nameBuf), "Slider: %s", name());
    encoder.addField("name", nameBuf);
    encoder.addField("type", "slider");
    encoder.addField("id", mInternal->id());
    encoder.addField("min", mMin);
    encoder.addField("max", mMax);
    encoder.addField("value", mValue);
    encoder.addField("step", mStep);
    return encoder.str();
}

float jsSlider::value() const { 
    return mValue; 
}

void jsSlider::updateInternal(const char* jsonStr) {
    // TODO: Implement the update logic here
    printf("jsSlider::updateInternal: %s\n", jsonStr);
}

void jsSlider::setValue(float value) {
    mValue = std::max(mMin, std::min(mMax, value));
    // TODO: Notify UI of value change
}

jsSlider::operator float() const { 
    return mValue; 
}

FASTLED_NAMESPACE_END
