#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/number_field.h"
#include "ui_internal.h"
#include "platforms/shared/ui/json/ui.h"

#if FASTLED_ENABLE_JSON


namespace fl {

JsonNumberFieldImpl::JsonNumberFieldImpl(const fl::string &name, double value,
                                        double min, double max)
    : mValue(value), mMin(min), mMax(max) {
    auto updateFunc = JsonUiInternal::UpdateFunction(
        [this](const fl::Json &json) {
            static_cast<JsonNumberFieldImpl *>(this)->updateInternal(json);
        });

    auto toJsonFunc =
        JsonUiInternal::ToJsonFunction([this]() -> fl::Json {
            return static_cast<JsonNumberFieldImpl *>(this)->toJson();
        });
    mInternal = fl::make_shared<JsonUiInternal>(name, fl::move(updateFunc),
                                     fl::move(toJsonFunc));
    addJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal));
}

JsonNumberFieldImpl::~JsonNumberFieldImpl() { removeJsonUiComponent(fl::weak_ptr<JsonUiInternal>(mInternal)); }

JsonNumberFieldImpl &JsonNumberFieldImpl::Group(const fl::string &name) {
    mInternal->setGroup(name);
    return *this;
}

const fl::string &JsonNumberFieldImpl::name() const { return mInternal->name(); }

fl::Json JsonNumberFieldImpl::toJson() const {
    auto builder = fl::JsonBuilder()
        .set("name", name())
        .set("group", mInternal->groupName())
        .set("type", "number")
        .set("id", mInternal->id());
    
    builder.set("value", static_cast<float>(mValue));
    builder.set("min", static_cast<float>(mMin));
    builder.set("max", static_cast<float>(mMax));
    
    return builder.build();
}

double JsonNumberFieldImpl::value() const { return mValue; }

void JsonNumberFieldImpl::setValue(double value) {
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    double oldValue = mValue;
    mValue = value;
    
    // If value actually changed, mark this component as changed for polling
    if (!ALMOST_EQUAL_FLOAT(mValue, oldValue)) {
        mInternal->markChanged();
    }
}

void JsonNumberFieldImpl::setValueInternal(double value) {
    // Internal method for updates from JSON UI system - no change notification
    if (value < mMin) {
        value = mMin;
    } else if (value > mMax) {
        value = mMax;
    }
    mValue = value;
}

const fl::string &JsonNumberFieldImpl::groupName() const { return mInternal->groupName(); }

void JsonNumberFieldImpl::setGroup(const fl::string &groupName) { mInternal->setGroup(groupName); }

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(double value) {
    setValue(value);
    return *this;
}

JsonNumberFieldImpl &JsonNumberFieldImpl::operator=(int value) {
    setValue(static_cast<double>(value));
    return *this;
}

bool JsonNumberFieldImpl::operator==(double v) const { return ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator==(int v) const { return ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }

bool JsonNumberFieldImpl::operator!=(double v) const { return !ALMOST_EQUAL_FLOAT(value(), v); }

bool JsonNumberFieldImpl::operator!=(int v) const { return !ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }

void JsonNumberFieldImpl::updateInternal(const fl::Json &json) {
    // Use ideal JSON API directly with flexible numeric access - handles both int and double
    auto maybeValue = json.get_flexible<double>();
    if (maybeValue.has_value()) {
        setValueInternal(*maybeValue);  // Use internal method to avoid change notification
    }
    // If not a number, keep current value unchanged
}

} // namespace fl

#endif // __EMSCRIPTEN__
