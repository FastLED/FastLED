#include "platforms/shared/ui/json/slider.h"
#include "fl/json.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/compiler_control.h"

FL_DISABLE_WARNING(deprecated-declarations)

#if FASTLED_ENABLE_JSON

namespace fl {

// Definition of the internal class that was previously in slider_internal.h
class JsonUiSliderInternal : public JsonUiInternal {
private:
    float mMin;
    float mMax;
    float mValue;
    float mStep;
    bool mStepExplicitlySet;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial values.
    JsonUiSliderInternal(const fl::string& name, float value, float min, float max, float step = -1)
        : JsonUiInternal(name), mMin(min), mMax(max), mValue(value), mStep(step), mStepExplicitlySet(false) {
        if (ALMOST_EQUAL_FLOAT(mStep, -1.f) && mMax > mMin) {
            mStep = (mMax - mMin) / 255.0f;
        } else if (!ALMOST_EQUAL_FLOAT(mStep, -1.f)) {
            mStepExplicitlySet = true;
        }
    }

    // Override toJson to serialize the slider's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "slider");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
        json.set("min", mMin);
        json.set("max", mMax);
        // Only output step if it was explicitly set by the user
        if (mStepExplicitlySet) {
            json.set("step", mStep);
        }
    }

    // Override updateInternal to handle updates from JSON.
    void updateInternal(const fl::Json& json) override {
        float value = json | 0.0f;
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }

    // Accessors for the slider values.
    float value() const { return mValue; }
    float getMin() const { return mMin; }
    float getMax() const { return mMax; }
    float step() const { return mStep; }
    
    void setValue(float value) { 
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }
    
    float value_normalized() const {
        if (ALMOST_EQUAL(mMax, mMin, 0.0001f)) {
            return 0;
        }
        return (mValue - mMin) / (mMax - mMin);
    }
    
    void setMin(float min) { mMin = min; }
    void setMax(float max) { mMax = max; }
    void setStep(float step) { 
        mStep = step; 
        mStepExplicitlySet = true;
    }
};

JsonSliderImpl::JsonSliderImpl(const fl::string &name, float value, float min,
                               float max, float step)
    : mInternal(fl::make_shared<JsonUiSliderInternal>(name, value, min, max, step)) {
    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonSliderImpl::~JsonSliderImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonSliderImpl &JsonSliderImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonSliderImpl::name() const { return mInternal->name(); }

void JsonSliderImpl::toJson(fl::Json &json) const {
    mInternal->toJson(json);
}

float JsonSliderImpl::value() const { return mInternal->value(); }

float JsonSliderImpl::value_normalized() const {
    return mInternal->value_normalized();
}

float JsonSliderImpl::getMax() const { return mInternal->getMax(); }

float JsonSliderImpl::getMin() const { return mInternal->getMin(); }

void JsonSliderImpl::setValue(float value) {
    mInternal->setValue(value);
}

const fl::string &JsonSliderImpl::groupName() const {
    return mInternal->groupName();
}

void JsonSliderImpl::setGroup(const fl::string &groupName) {
    mInternal->setGroup(groupName);
}

int JsonSliderImpl::as_int() const { return static_cast<int>(mInternal->value()); }

JsonSliderImpl &JsonSliderImpl::operator=(float value) {
    setValue(value);
    return *this;
}

JsonSliderImpl &JsonSliderImpl::operator=(int value) {
    setValue(static_cast<float>(value));
    return *this;
}

int JsonSliderImpl::id() const {
    return mInternal->id();
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
