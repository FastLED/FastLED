// IWYU pragma: private

#include "fl/stl/json.h"
#include "fl/stl/json.h"
#include "fl/math/math.h"
#include "platforms/shared/ui/json/number_field.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/stl/noexcept.h"
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
 FL_NOEXCEPT : JsonUiInternal(name), mValue(value), mMin(min), mMax(max) {}

    // Override toJson to serialize the number field's data directly.
    void toJson(fl::json& json) const FL_NOEXCEPT override {
        json.set("name", name());
        json.set("type", "number");
        json.set("group", groupName());
        json.set("id", id());
        json.set("value", mValue);
        json.set("min", mMin);
        json.set("max", mMax);
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

    // Accessors for the number field values.
    float value() const FL_NOEXCEPT { return mValue; }
    float getMin() const FL_NOEXCEPT { return mMin; }
    float getMax() const FL_NOEXCEPT { return mMax; }
    
    void setValue(float value) FL_NOEXCEPT { 
        if (value < mMin) {
            value = mMin;
        } else if (value > mMax) {
            value = mMax;
        }
        mValue = value;
    }
    
    void setMin(float min) FL_NOEXCEPT { mMin = min; }
    void setMax(float max) FL_NOEXCEPT { mMax = max; }
};

JsonNumberFieldImpl::JsonNumberFieldImpl(const fl::string &name, float value,
                                        float min, float max) FL_NOEXCEPT {
    // Create an instance of the new internal class
    mInternal = fl::make_shared<JsonUiNumberFieldInternal>(name, value, min, max);

    // Register the component with the JsonUiManager
    addJsonUiComponent(mInternal);
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { 
    // Ensure the component is removed from the global registry
    removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

int JsonNumberFieldImpl::id() const FL_NOEXCEPT {
    return mInternal->id();
}

JsonNumberFieldImpl &JsonNumberFieldImpl::Group(const fl::string &name) FL_NOEXCEPT {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonNumberFieldImpl::name() const FL_NOEXCEPT { return mInternal->name(); }

void JsonNumberFieldImpl::toJson(fl::json &json) const FL_NOEXCEPT {
    mInternal->toJson(json);
}

float JsonNumberFieldImpl::value() const FL_NOEXCEPT {
    if (!mInternal) {
        FASTLED_ASSERT(false, "JsonNumberFieldImpl::value() called before initialization");
        return 0.0f;  // Return default value if not initialized
    }
    return mInternal->value();
}

void JsonNumberFieldImpl::setValue(float value) FL_NOEXCEPT {
    float oldValue = mInternal->value();
    mInternal->setValue(value);
    
    // If value actually changed, mark this component as changed for polling
    if (!fl::almost_equal(mInternal->value(), oldValue)) {
        mInternal->markChanged();
    }
}

fl::string JsonNumberFieldImpl::groupName() const FL_NOEXCEPT { return mInternal->groupName(); }

void JsonNumberFieldImpl::setGroup(const fl::string &groupName) FL_NOEXCEPT { mInternal->setGroup(groupName); }

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(float value) FL_NOEXCEPT {
    setValue(value);
    return *this;
}

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(int value) FL_NOEXCEPT {
    setValue(static_cast<float>(value));
    return *this;
}

bool JsonNumberFieldImpl::operator==(float v) const FL_NOEXCEPT { return fl::almost_equal(value(), v); }

bool JsonNumberFieldImpl::operator==(int v) const FL_NOEXCEPT { return fl::almost_equal(value(), static_cast<float>(v)); }

bool JsonNumberFieldImpl::operator!=(float v) const FL_NOEXCEPT { return !fl::almost_equal(value(), v); }

bool JsonNumberFieldImpl::operator!=(int v) const FL_NOEXCEPT { return !fl::almost_equal(value(), static_cast<float>(v)); }

} // namespace fl
