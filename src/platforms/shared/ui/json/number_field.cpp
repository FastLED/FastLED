#include "fl/json.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/number_field.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON

namespace fl {

// Definition of the internal class that was previously in number_field_internal.h
class JsonUiNumberFieldInternal : public JsonUiInternal {
private:
    float mValue;
    float mMin;
    float mMax;

public:
    // Constructor: Initializes the base JsonUiInternal with name, and sets initial values.
    JsonUiNumberFieldInternal(const fl::string& name, float value, float min, float max)
        : JsonUiInternal(name), mValue(value), mMin(min), mMax(max) {}

    // Override toJson to serialize the number field's data directly.
    void toJson(fl::Json& json) const override {
        json.set("name", name());
        json.set("type", "number");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
        json.set("min", mMin);
        json.set("max", mMax);
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

    // Accessors for the number field values.
    float value() const { return mValue; }
    float getMin() const { return mMin; }
    float getMax() const { return mMax; }
    
    void setValue(float value) { 
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }
    
    void setMin(float min) { mMin = min; }
    void setMax(float max) { mMax = max; }
};

JsonNumberFieldImpl::JsonNumberFieldImpl(const fl::string &name, float value,
                                        float min, float max) {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiNumberFieldInternal>(name, value, min, max);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

int JsonNumberFieldImpl::id() const {
    return mInternal->id();
}

JsonNumberFieldImpl &JsonNumberFieldImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonNumberFieldImpl::name() const { return mInternal->name(); }

void JsonNumberFieldImpl::toJson(fl::Json &json) const {
    mInternal->toJson(json);
}

float JsonNumberFieldImpl::value() const { return mInternal->value(); }

void JsonNumberFieldImpl::setValue(float value) {
    float oldValue = mInternal->value();
    mInternal->setValue(value);
    
    // If value actually changed, mark this component as changed for polling
    if (!ALMOST_EQUAL_FLOAT(mInternal->value(), oldValue)) {
        mInternal->markChanged();
    }
}

const fl::string &JsonNumberFieldImpl::groupName() const { return mInternal->groupName(); }

void JsonNumberFieldImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(float value) {
    setValue(value);
    return *this;
}

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(int value) {
    setValue(static_cast<float>(value));
    return *this;
}

bool JsonNumberFieldImpl::operator==(float v) const { return ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator==(int v) const { return ALMOST_EQUAL_FLOAT(value(), static_cast<float>(v)); }

bool JsonNumberFieldImpl::operator!=(float v) const { return !ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator!=(int v) const { return !ALMOST_EQUAL_FLOAT(value(), static_cast<float>(v)); }

} // namespace fl

#endif // FASTLED_ENABLE_JSON
