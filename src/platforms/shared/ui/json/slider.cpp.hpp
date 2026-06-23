// IWYU pragma: private

#include "platforms/shared/ui/json/slider.h"
#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "fl/math/math.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING(deprecated-declarations)
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
 FL_NOEXCEPT : JsonUiInternal(name), mMin(min), mMax(max), mValue(value), mStep(step), mStepExplicitlySet(false) {
        if (fl::almost_equal(mStep, -1.f) && mMax > mMin) {
            mStep = (mMax - mMin) / 255.0f;
        } else if (!fl::almost_equal(mStep, -1.f)) {
            mStepExplicitlySet = true;
        }
    }

    // Override toJson to serialize the slider's data directly.
    void toJson(fl::json& json) const FL_NOEXCEPT override {
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
    void updateInternal(const fl::json& json) FL_NOEXCEPT override {
        float value = json | 0.0f;
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }

    // Accessors for the slider values.
    float value() const FL_NOEXCEPT { return mValue; }
    float getMin() const FL_NOEXCEPT { return mMin; }
    float getMax() const FL_NOEXCEPT { return mMax; }
    float step() const FL_NOEXCEPT { return mStep; }
    
    void setValue(float value) FL_NOEXCEPT { 
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }
    
    float value_normalized() const FL_NOEXCEPT {
        if (fl::almost_equal(mMax, mMin, 0.0001f)) {
            return 0;
        }
        return (mValue - mMin) / (mMax - mMin);
    }
    
    void setMin(float min) FL_NOEXCEPT { mMin = min; }
    void setMax(float max) FL_NOEXCEPT { mMax = max; }
    void setStep(float step) FL_NOEXCEPT { 
        mStep = step; 
        mStepExplicitlySet = true;
    }
};

JsonSliderImpl::JsonSliderImpl(const fl::string &name, float value, float min,
                               float max, float step)
 FL_NOEXCEPT : mInternal(fl::make_shared<JsonUiSliderInternal>(name, value, min, max, step)) {
    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonSliderImpl::~JsonSliderImpl() {
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonSliderImpl &JsonSliderImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonSliderImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

void JsonSliderImpl::toJson(fl::json &json) const FL_NOEXCEPT {
    mInternal->toJson(json);
}

float JsonSliderImpl::value() const FL_NOEXCEPT { return mInternal->value(); }

float JsonSliderImpl::value_normalized() const FL_NOEXCEPT {
    return mInternal->value_normalized();
}

float JsonSliderImpl::getMax() const FL_NOEXCEPT { return mInternal->getMax(); }

float JsonSliderImpl::getMin() const FL_NOEXCEPT { return mInternal->getMin(); }

void JsonSliderImpl::setValue(float value) FL_NOEXCEPT {
    mInternal->setValue(value);
}

fl::string JsonSliderImpl::groupName() const FL_NOEXCEPT {
    return mInternal->groupName();
}

void JsonSliderImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT {
    mInternal->setGroup(groupName);
}

int JsonSliderImpl::as_int() const FL_NOEXCEPT { return static_cast<int>(mInternal->value()); }

JsonSliderImpl &JsonSliderImpl::operator=(float value) FL_NOEXCEPT {
    setValue(value);
    return *this;
}

JsonSliderImpl &JsonSliderImpl::operator=(int value) FL_NOEXCEPT {
    setValue(static_cast<float>(value));
    return *this;
}

int JsonSliderImpl::id() const FL_NOEXCEPT {
    return mInternal->id();
}

} // namespace fl
