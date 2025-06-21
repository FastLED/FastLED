#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"
#include "fl/math_macros.h"

namespace fl {

class jsNumberFieldImpl {
  public:
    jsNumberFieldImpl(const fl::string &name, double value, double min, double max);
    ~jsNumberFieldImpl();

    void setGroup(const fl::string& name);

    const fl::string &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    double value() const;
    void setValue(double value);
    const fl::string &groupName() const { return mInternal->group(); }

    jsNumberFieldImpl &operator=(double value) {
        setValue(value);
        return *this;
    }
    jsNumberFieldImpl &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }
    // Use ALMOST_EQUAL_FLOAT for floating-point comparison
    bool operator==(double v) const { return ALMOST_EQUAL_FLOAT(value(), v); }
    bool operator==(int v) const { return ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }
    bool operator!=(double v) const { return !ALMOST_EQUAL_FLOAT(value(), v); }
    bool operator!=(int v) const { return !ALMOST_EQUAL_FLOAT(value(), static_cast<double>(v)); }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    double mValue;
    double mMin;
    double mMax;
};

} // namespace fl
