#include "slider.h"
#include "ui_manager.h"
#include <sstream>

FASTLED_NAMESPACE_BEGIN

class JsonDictEncoder {
private:
    std::ostringstream oss;
    bool first = true;
    bool begun = false;
    bool ended = false;

    template<typename T>
    void appendJsonField(const char* name, const T& value) {
        oss << "\"" << name << "\":" << value;
    }

    void appendJsonField(const char* name, const std::string& value) {
        oss << "\"" << name << "\":\"" << value << "\"";
    }

    void appendJsonField(const char* name, const char* value) {
        oss << "\"" << name << "\":\"" << value << "\"";
    }

public:
    JsonDictEncoder() = default;

    void begin() {
        if (!begun) {
            oss << "{";
            begun = true;
        }
    }

    void end() {
        if (begun && !ended) {
            oss << "}";
            begun = false;
            ended = true;
        }
    }

    template<typename T>
    void addField(const char* name, const T& value) {
        if (!begun) {
            begin();
        }
        if (!first) {
            oss << ",";
        }
        appendJsonField(name, value);
        first = false;
    }

    const char* str() {
        end();
        return oss.str().c_str();
    }
};

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
    JsonDictEncoder encoder;
    encoder.addField("name", "Slider: " + name());
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
}

void jsSlider::setValue(float value) {
    mValue = std::max(mMin, std::min(mMax, value));
    // TODO: Notify UI of value change
}

jsSlider::operator float() const { 
    return mValue; 
}

FASTLED_NAMESPACE_END
