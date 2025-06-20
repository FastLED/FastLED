#pragma once

#include "fl/engine_events.h"
#include "fl/str.h"
#include "platforms/wasm/ui/ui_internal.h"

namespace fl {

class jsNumberFieldImpl {
  public:
    jsNumberFieldImpl(const fl::Str &name, double value, double min, double max);
    ~jsNumberFieldImpl();
    jsNumberFieldImpl &Group(const fl::Str &name) {
        mGroup = name;
        return *this;
    }

    const fl::Str &name() const;
    void toJson(FLArduinoJson::JsonObject &json) const;
    double value() const;
    void setValue(double value);
    const fl::Str &groupName() const { return mGroup; }
    
    // Method to allow parent UIBase class to set the group
    void setGroupInternal(const fl::Str &groupName) { mGroup = groupName; }

    jsNumberFieldImpl &operator=(double value) {
        setValue(value);
        return *this;
    }
    jsNumberFieldImpl &operator=(int value) {
        setValue(static_cast<double>(value));
        return *this;
    }
    bool operator==(double v) const { return value() == v; }
    bool operator==(int v) const { return value() == v; }
    bool operator!=(double v) const { return value() != v; }
    bool operator!=(int v) const { return value() != v; }

  private:
    void updateInternal(const FLArduinoJson::JsonVariantConst &value);

    jsUiInternalPtr mInternal;
    double mValue;
    double mMin;
    double mMax;
    fl::Str mGroup;
};

} // namespace fl
